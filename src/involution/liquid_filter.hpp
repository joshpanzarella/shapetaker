#pragma once
#include "../plugin.hpp"
#include "rack.hpp"
#include <algorithm>

/**
 * LiquidFilter - 6th-order lowpass filter with liquid, resonant character
 *
 * Three cascaded 2-pole SVF stages with:
 * - 4x oversampling for stability at high resonance
 * - Inter-stage soft saturation for character
 * - Distributed resonance for smooth buildup
 * - Multiple saturation curves for different analog characteristics
 */
class LiquidFilter {
public:
    enum SaturationMode {
        SOFT_CLIP,      // Smooth, gentle saturation
        TUBE,           // Warm tube-like (odd harmonics)
        TRANSISTOR,     // Hard-soft transistor-like
        ASYMMETRIC,     // Asymmetric (more realistic)
        POLY            // Polynomial (fast, smooth)
    };

private:
    // Three 2-pole SVF stages
    struct SVF2Pole {
        float ic1eq = 0.f;
        float ic2eq = 0.f;

        float process(float input, float g, float k) {
            float v1 = (ic1eq + g * (input - ic2eq)) / (1.f + g * (g + k));
            float v2 = ic2eq + g * v1;

            ic1eq = 2.f * v1 - ic1eq;
            ic2eq = 2.f * v2 - ic2eq;

            return v2;
        }

        void reset() {
            ic1eq = ic2eq = 0.f;
        }
    };

    SVF2Pole stage1, stage2, stage3;

    // VCV Rack's built-in oversampling
    static const int OVERSAMPLE_FACTOR = 4;
    static const int OVERSAMPLE_QUALITY = 8;
    rack::dsp::Decimator<OVERSAMPLE_FACTOR, OVERSAMPLE_QUALITY> decimator;
    rack::dsp::Upsampler<OVERSAMPLE_FACTOR, OVERSAMPLE_QUALITY> upsampler;

    float baseSampleRate = 48000.f;
    float oversampledRate = 48000.f * OVERSAMPLE_FACTOR;
    SaturationMode satMode = TUBE;  // Default to tube saturation for warm, liquid character

    // Multiple saturation curves for different analog characteristics
    float saturate(float input, float drive) {
        drive = rack::math::clamp(drive, 0.1f, 10.f);

        // Expand the saturation headroom so nominal ±5V signals remain full-scale
        // while still letting the shaper tame runaway resonance peaks.
        constexpr float SATURATION_HEADROOM = 18.f;
        float normalized = (input * drive) / SATURATION_HEADROOM;
        normalized = rack::math::clamp(normalized, -2.5f, 2.5f);

        switch (satMode) {
            case SOFT_CLIP:
                // Smooth, gentle saturation
                if (normalized > 1.f) normalized = 2.f / 3.f + (normalized - 1.f) / 3.f;
                else if (normalized < -1.f) normalized = -2.f / 3.f + (normalized + 1.f) / 3.f;
                else normalized = normalized - normalized * normalized * normalized / 3.f;
                break;

            case TUBE:
                // Warm tube-like (odd harmonics)
                normalized = normalized / std::sqrt(1.f + normalized * normalized);
                break;

            case TRANSISTOR: {
                // Hard-soft transistor-like
                float abs_x = std::abs(normalized);
                if (abs_x < 0.5f) {
                    // Keep the linear region untouched for small signals
                }
                else if (abs_x > 1.f) {
                    normalized = (normalized > 0.f) ? 1.f : -1.f;
                }
                else {
                    float sign = normalized > 0.f ? 1.f : -1.f;
                    normalized = sign * (0.75f + 0.25f * (1.f - std::pow(2.f - 2.f * abs_x, 2.f)));
                }
                break;
            }

            case ASYMMETRIC:
                // Asymmetric (more realistic)
                if (normalized > 0.f)
                    normalized = normalized / (1.f + 0.8f * normalized);
                else
                    normalized = normalized / (1.f + 0.5f * std::abs(normalized));
                break;

            case POLY:
                // Polynomial (fast, smooth)
                normalized = rack::math::clamp(normalized, -1.5f, 1.5f);
                normalized = normalized - (normalized * normalized * normalized) / 3.f;
                break;

            default:
                break;
        }

        float output = (normalized * SATURATION_HEADROOM) / drive;
        return rack::math::clamp(output, -12.f, 12.f);
    }

    float driveSaturate(float input, float drive) {
        drive = rack::math::clamp(drive, 1.f, 8.f);
        float driveMix = rack::math::clamp((drive - 1.f) / 7.f, 0.f, 1.f);

        // Pre-emphasis: add a small boost before saturation to get character.
        auto lerp = [](float a, float b, float t) {
            return a + (b - a) * t;
        };

        float driven = input * lerp(1.f, drive * 0.8f + 0.2f, driveMix);
        float shaped = saturate(driven, 1.f);

        // Drive-dependent gain scaling keeps unity at low settings and normalises hot drive.
        float makeup = lerp(1.f, 1.0f / (drive * 0.6f + 0.4f), driveMix);
        float wet = lerp(0.25f, 0.85f, driveMix);
        float dry = 1.f - wet;

        float result = dry * input + wet * shaped * makeup;
        return rack::math::clamp(result, -12.f, 12.f);
    }

public:
    LiquidFilter() : decimator(0.9f), upsampler(0.9f) {
        // Initialize to default sample rate
        oversampledRate = baseSampleRate * OVERSAMPLE_FACTOR;
        reset();
    }

    void setSampleRate(float sr) {
        baseSampleRate = sr;
        oversampledRate = sr * OVERSAMPLE_FACTOR;
    }

    void setSaturationMode(SaturationMode mode) {
        satMode = mode;
    }

    void reset() {
        stage1.reset();
        stage2.reset();
        stage3.reset();
        decimator.reset();
        upsampler.reset();
    }

    float process(float input, float cutoff, float resonance, float drive = 1.f) {
        // Safety checks
        if (!std::isfinite(input)) return 0.f;
        if (oversampledRate <= 0.f) return input;

        // Clamp parameters to safe ranges
        cutoff = rack::math::clamp(cutoff, 1.f, oversampledRate * 0.45f);
        resonance = rack::math::clamp(resonance, 0.1f, 10.f);
        drive = rack::math::clamp(drive, 0.1f, 10.f);

        // Apply soft limiting to resonance at extreme settings (including CV) and reduce
        // runaway gain for very low cutoff frequencies.
        constexpr float resonanceMin = 0.707f;
        constexpr float resonanceSoftMax = 1.8f;
        const float resonanceRange = std::max(resonanceSoftMax - resonanceMin, 0.001f);
        float resonanceClamped = rack::math::clamp(resonance, resonanceMin, resonanceSoftMax);
        float resonanceNormalized = rack::math::clamp((resonanceClamped - resonanceMin) / resonanceRange, 0.f, 1.f);

        float cutoffNormalized = rack::math::clamp(cutoff / 1200.f, 0.f, 1.f);
        float lowFreqPenalty = 1.f - cutoffNormalized;
        float resonanceScale = 1.f - 0.8f * lowFreqPenalty * resonanceNormalized;
        resonanceScale = rack::math::clamp(resonanceScale, 0.5f, 1.f);

        resonance = resonanceClamped * resonanceScale;

        // Upsample: produces OVERSAMPLE_FACTOR samples
        float upsampledBuffer[OVERSAMPLE_FACTOR];
        upsampler.process(input, upsampledBuffer);

        // Process at higher sample rate
        float oversampledOutputs[OVERSAMPLE_FACTOR];

        for (int i = 0; i < OVERSAMPLE_FACTOR; i++) {
            float x = upsampledBuffer[i];

            // Pre-filter saturation (input drive)
            x = driveSaturate(x, drive);

            // Calculate filter coefficients at oversampled rate
            float g = std::tan(M_PI * cutoff / oversampledRate);
            g = rack::math::clamp(g, 0.f, 0.99f);  // Prevent instability

            // Distribute resonance across stages with guarded damping factors
            constexpr float stageWeights[3] = {0.25f, 0.45f, 0.6f};
            float k1 = 2.f * (1.f - rack::math::clamp(resonance * stageWeights[0], 0.f, 0.95f));
            float k2 = 2.f * (1.f - rack::math::clamp(resonance * stageWeights[1], 0.f, 0.95f));
            float k3 = 2.f * (1.f - rack::math::clamp(resonance * stageWeights[2], 0.f, 0.95f));
            k1 = rack::math::clamp(k1, 0.2f, 1.999f);
            k2 = rack::math::clamp(k2, 0.2f, 1.999f);
            k3 = rack::math::clamp(k3, 0.2f, 1.999f);

            // Cascade the three 2-pole stages
            x = stage1.process(x, g, k1);

            // Inter-stage saturation (subtle)
            x = saturate(x, 1.f + resonance * 0.3f);

            x = stage2.process(x, g, k2);
            x = saturate(x, 1.f + resonance * 0.3f);

            x = stage3.process(x, g, k3);
            x = saturate(x, 1.f + resonance * 0.2f);

            oversampledOutputs[i] = x;
        }

        // Downsample back to base rate
        float output = decimator.process(oversampledOutputs);

        // Apply resonance-dependent gain compensation so peak levels stay near ±5V
        // even when the control is maxed and the cutoff is very low.
        float resonanceGainComp = 1.f - 0.25f * resonanceNormalized;
        float lowFreqComp = 1.f - 0.4f * resonanceNormalized * lowFreqPenalty;
        resonanceGainComp = rack::math::clamp(resonanceGainComp, 0.55f, 1.f);
        lowFreqComp = rack::math::clamp(lowFreqComp, 0.5f, 1.f);
        output *= resonanceGainComp * lowFreqComp;

        // Final safety shaping to corral extreme resonant peaks without
        // impacting normal operating levels.
        output = saturate(output, 1.0f);

        // Final safety check
        if (!std::isfinite(output)) {
            reset();
            return 0.f;
        }

        return output;
    }
};
