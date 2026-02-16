#pragma once
#include "../plugin.hpp"
#include "rack.hpp"
#include <algorithm>

/**
 * LiquidFilter - 6th-order filter with liquid, resonant character
 *
 * Three cascaded 2-pole SVF stages with global feedback topology:
 * - Global feedback from stage 3 output to stage 1 input (like a ladder filter)
 * - Individual stages run clean (no per-stage resonance) preserving bass
 * - Resonance emerges from the feedback loop, not from individual stage peaking
 * - Transistor-style saturation: linear for small signals, shaped for peaks
 * - 2x oversampling for stability
 * - Inter-stage saturation for analog character
 */
class LiquidFilter {
public:
    enum FilterMode {
        LOWPASS,        // Classic 6th-order lowpass
        BANDPASS,       // 6th-order bandpass (nasal, vocal)
        MORPH           // Continuous LP→BP blend via filterMorph parameter
    };

private:
    // Three 2-pole SVF stages
    struct SVF2Pole {
        float ic1eq = 0.f;
        float ic2eq = 0.f;
        float lastV1 = 0.f;  // Bandpass output
        float lastV2 = 0.f;  // Lowpass output

        float process(float input, float g, float k, float bpMix) {
            float v1 = (ic1eq + g * (input - ic2eq)) / (1.f + g * (g + k));
            float v2 = ic2eq + g * v1;

            ic1eq = 2.f * v1 - ic1eq;
            ic2eq = 2.f * v2 - ic2eq;

            lastV1 = v1;
            lastV2 = v2;

            // Crossfade between lowpass (v2) and bandpass (v1)
            return v2 + (v1 - v2) * bpMix;
        }

        void reset() {
            ic1eq = ic2eq = 0.f;
            lastV1 = lastV2 = 0.f;
        }
    };

    SVF2Pole stage1, stage2, stage3;

    // VCV Rack's built-in oversampling
    static const int OVERSAMPLE_FACTOR = 2;
    static const int OVERSAMPLE_QUALITY = 4;
    rack::dsp::Decimator<OVERSAMPLE_FACTOR, OVERSAMPLE_QUALITY> decimator;
    rack::dsp::Upsampler<OVERSAMPLE_FACTOR, OVERSAMPLE_QUALITY> upsampler;

    float baseSampleRate = 48000.f;
    float oversampledRate = 48000.f * OVERSAMPLE_FACTOR;
    FilterMode filterMode = LOWPASS;
    float filterMorph = 0.f;  // 0 = LP, 1 = BP (used in MORPH mode)

    // Global feedback state (ladder-style resonance)
    float lastFeedback = 0.f;

    // Transistor-style saturation curve
    // Linear for small signals (< 0.5), smooth transition, hard clip above 1.0
    // This is what gives the filter its "liquid" character:
    // bass and detail pass through clean, only resonant peaks get shaped
    float saturate(float input, float drive) {
        drive = rack::math::clamp(drive, 0.1f, 12.f);

        constexpr float SATURATION_HEADROOM = 12.f;
        float normalized = (input * drive) / SATURATION_HEADROOM;
        normalized = rack::math::clamp(normalized, -2.8f, 2.8f);

        // Transistor curve: linear below 0.5, smooth compression 0.5-1.0, clip above 1.0
        float abs_x = std::abs(normalized);
        if (abs_x < 0.5f) {
            // Linear region — small signals pass through untouched
        }
        else if (abs_x > 1.f) {
            normalized = (normalized > 0.f) ? 1.f : -1.f;
        }
        else {
            float sign = normalized > 0.f ? 1.f : -1.f;
            normalized = sign * (0.75f + 0.25f * (1.f - std::pow(2.f - 2.f * abs_x, 2.f)));
        }

        float output = (normalized * SATURATION_HEADROOM) / drive;
        return rack::math::clamp(output, -12.f, 12.f);
    }

    float driveSaturate(float input, float drive) {
        drive = rack::math::clamp(drive, 1.f, 9.f);
        float driveMix = rack::math::clamp((drive - 1.f) / 8.f, 0.f, 1.f);

        auto lerp = [](float a, float b, float t) {
            return a + (b - a) * t;
        };

        float driven = input * lerp(1.f, drive * 0.9f + 0.2f, driveMix);
        float shaped = saturate(driven, 1.0f);

        float makeup = lerp(1.f, 1.0f / (drive * 0.5f + 0.5f), driveMix);
        float wet = lerp(0.35f, 0.95f, driveMix);
        float dry = 1.f - wet;

        float result = dry * input + wet * shaped * makeup;
        return rack::math::clamp(result, -12.f, 12.f);
    }

public:
    LiquidFilter() : decimator(0.9f), upsampler(0.9f) {
        oversampledRate = baseSampleRate * OVERSAMPLE_FACTOR;
        reset();
    }

    void setSampleRate(float sr) {
        baseSampleRate = sr;
        oversampledRate = sr * OVERSAMPLE_FACTOR;
    }

    void setFilterMode(FilterMode mode) {
        filterMode = mode;
    }

    void setFilterMorph(float morph) {
        filterMorph = rack::math::clamp(morph, 0.f, 1.f);
    }

    void reset() {
        stage1.reset();
        stage2.reset();
        stage3.reset();
        decimator.reset();
        upsampler.reset();
        lastFeedback = 0.f;
    }

    float process(float input, float cutoff, float resonance, float drive = 1.f) {
        // Safety checks
        if (!std::isfinite(input)) return 0.f;
        if (oversampledRate <= 0.f) return input;

        // Clamp parameters to safe ranges
        cutoff = rack::math::clamp(cutoff, 1.f, oversampledRate * 0.45f);
        resonance = rack::math::clamp(resonance, 0.1f, 10.f);
        drive = rack::math::clamp(drive, 0.1f, 10.f);

        // Map resonance to global feedback amount
        constexpr float resonanceMin = 0.707f;
        constexpr float resonanceSoftMax = 2.8f;
        const float resonanceRange = std::max(resonanceSoftMax - resonanceMin, 0.001f);
        float resonanceClamped = rack::math::clamp(resonance, resonanceMin, resonanceSoftMax);
        float resonanceNormalized = rack::math::clamp((resonanceClamped - resonanceMin) / resonanceRange, 0.f, 1.f);

        // Exponential feedback curve: subtle at low settings, aggressive at top
        float feedbackAmount = std::pow(resonanceNormalized, 1.6f) * 4.0f;

        // Gentle low-frequency feedback reduction
        float cutoffNormalized = rack::math::clamp(cutoff / 900.f, 0.f, 1.f);
        float lowFreqPenalty = 1.f - cutoffNormalized;
        float feedbackScale = 1.f - 0.2f * lowFreqPenalty * resonanceNormalized;
        feedbackScale = rack::math::clamp(feedbackScale, 0.8f, 1.05f);
        feedbackAmount *= feedbackScale;

        // Upsample
        float upsampledBuffer[OVERSAMPLE_FACTOR];
        upsampler.process(input, upsampledBuffer);

        float oversampledOutputs[OVERSAMPLE_FACTOR];

        for (int i = 0; i < OVERSAMPLE_FACTOR; i++) {
            float x = upsampledBuffer[i];

            // Pre-filter drive saturation
            x = driveSaturate(x, drive);

            // Filter coefficients at oversampled rate
            float g = std::tan(M_PI * cutoff / oversampledRate);
            g = rack::math::clamp(g, 0.f, 0.99f);

            // ================================================================
            // GLOBAL FEEDBACK TOPOLOGY (ladder-style)
            // ================================================================
            // Subtract feedback from input — resonance emerges from the loop,
            // not from individual stages. Bass flows through clean stages.
            x = x - lastFeedback * feedbackAmount;

            // Transistor saturation in the feedback path is what makes it
            // "liquid" — small signals stay clean, only peaks get shaped
            x = saturate(x, 1.f + feedbackAmount * 0.15f);

            // Compute bandpass mix from filter mode
            float bpMix = 0.f;
            if (filterMode == BANDPASS) bpMix = 1.f;
            else if (filterMode == MORPH) bpMix = filterMorph;

            // Cascade three clean 2-pole stages (k=2.0 = critically damped)
            // No individual resonance — bass passes through unimpeded
            x = stage1.process(x, g, 2.f, bpMix);

            // Inter-stage saturation adds analog warmth
            x = saturate(x, 1.f + feedbackAmount * 0.12f);

            x = stage2.process(x, g, 2.f, bpMix);
            x = saturate(x, 1.f + feedbackAmount * 0.12f);

            x = stage3.process(x, g, 2.f, bpMix);

            // Store lowpass output for feedback (always LP for loop stability)
            lastFeedback = stage3.lastV2;

            if (!std::isfinite(lastFeedback)) {
                lastFeedback = 0.f;
            }

            // Post-cascade saturation (subtle)
            x = saturate(x, 1.f + feedbackAmount * 0.08f);

            oversampledOutputs[i] = x;
        }

        // Downsample back to base rate
        float output = decimator.process(oversampledOutputs);

        // Gentle resonance-dependent gain compensation
        float resonanceGainComp = 1.f - 0.1f * resonanceNormalized;
        resonanceGainComp = rack::math::clamp(resonanceGainComp, 0.85f, 1.05f);
        output *= resonanceGainComp;

        // Final safety shaping
        output = saturate(output, 1.0f);

        // Final safety check
        if (!std::isfinite(output)) {
            reset();
            return 0.f;
        }

        return output;
    }
};
