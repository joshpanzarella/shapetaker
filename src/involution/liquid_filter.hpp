#pragma once
#include "../plugin.hpp"
#include "rack.hpp"
#include <algorithm>

/**
 * LiquidFilter - 6th-order filter with liquid, resonant character
 *
 * Three cascaded 2-pole SVF stages (k=2.0) with global ladder-style feedback:
 * - k=2.0 is held constant — reducing it shifts the -180° phase crossing to a
 *   higher-gain frequency, cutting the max stable feedbackAmount below 2 and
 *   causing pumping oscillation.  Resonance comes entirely from global feedback.
 * - Feedback is 2nd-order HP'd at 20% of the filter cutoff (-12dB/oct), so
 *   bass below the resonant region is strongly protected; clamped 30–160Hz.
 * - lastFeedback is tanh-limited to ±2.5V — prevents integrator runaway and
 *   gives the feedback loop extra "spring" for an elastic, liquid character.
 * - resonanceNormalized capped at 0.88 → feedbackAmount ceiling 1.76: loop
 *   gain at -180° ≈ 0.74, near-oscillating elastic ring, provably stable.
 * - Tighter inter-stage saturation prevents amplitude buildup through the cascade
 *   and adds organic harmonic compression.
 * - 2x oversampling for alias suppression.
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

    // 2nd-order HP on the feedback path: two cascaded 1-pole LP states.
    // Subtracting only the HP'd feedback from the input preserves bass.
    // Two poles give -12dB/oct below the HP cutoff (vs -6dB/oct with one pole),
    // strongly protecting bass even at very high resonance settings.
    float hpFeedbackLP1 = 0.f;
    float hpFeedbackLP2 = 0.f;

    // Transistor-style saturation curve
    // Linear for small signals (< 0.5), smooth transition, hard clip above 1.0
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
        hpFeedbackLP1 = 0.f;
        hpFeedbackLP2 = 0.f;
    }

    float process(float input, float cutoff, float resonance, float drive = 1.f) {
        // Safety checks
        if (!std::isfinite(input)) return 0.f;
        if (oversampledRate <= 0.f) return input;

        // Clamp parameters to safe ranges
        cutoff = rack::math::clamp(cutoff, 1.f, oversampledRate * 0.45f);
        resonance = rack::math::clamp(resonance, 0.1f, 10.f);
        drive = rack::math::clamp(drive, 0.1f, 10.f);

        // Map resonance to normalized 0..1
        constexpr float resonanceMin    = 0.707f;
        constexpr float resonanceSoftMax = 2.8f;
        const float resonanceRange = std::max(resonanceSoftMax - resonanceMin, 0.001f);
        float resonanceClamped     = rack::math::clamp(resonance, resonanceMin, resonanceSoftMax);
        // Cap at 0.88 so feedbackAmount never exceeds 1.76 — keeps loop gain
        // comfortably below 1 (0.74 at ceiling) and prevents instability above
        // resonance ~2.6 while still delivering near-self-oscillating elasticity.
        float resonanceNormalized  = rack::math::clamp((resonanceClamped - resonanceMin) / resonanceRange, 0.f, 0.88f);

        // Global feedback amount.
        // With k=2.0 per stage, the -180° phase crossing is at ω=0.577·ωc where
        // the three-stage LP gain is 0.422.  Max stable feedbackAmount ≈ 2.37.
        // Ceiling at 1.76 (resonanceNormalized capped 0.88) keeps loop gain ≈ 0.74,
        // well inside stability while delivering an elastic, near-oscillating ring.
        float feedbackAmount = std::pow(resonanceNormalized, 1.1f) * 2.0f;

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
            // 2nd-order HP on feedback at 20% of filter cutoff (-12dB/oct).
            // e.g. cutoff=400Hz → HP at 80Hz; 40Hz is then -24dB down in
            // the feedback signal.  Clamped 30–160Hz to stay well below the
            // musical midrange and protect more of the bass spectrum.
            {
                float HP_CUTOFF_HZ = rack::math::clamp(cutoff * 0.20f, 30.f, 160.f);
                float hpAlpha = rack::math::clamp(
                    (2.f * static_cast<float>(M_PI) * HP_CUTOFF_HZ) / oversampledRate,
                    0.f, 0.99f);
                // Cascade two 1-pole LPs to form a 2nd-order HP
                hpFeedbackLP1 += hpAlpha * (lastFeedback - hpFeedbackLP1);
                float hp1 = lastFeedback - hpFeedbackLP1;
                hpFeedbackLP2 += hpAlpha * (hp1 - hpFeedbackLP2);
                float feedbackHPed = hp1 - hpFeedbackLP2;
                x = x - feedbackHPed * feedbackAmount;
            }

            // Post-injection saturation: base=1.0 so normal audio passes cleanly;
            // drive grows with feedbackAmount, shaping peaks at high resonance.
            x = saturate(x, 1.0f + feedbackAmount * 0.15f);

            // Compute bandpass mix from filter mode
            float bpMix = 0.f;
            if (filterMode == BANDPASS) bpMix = 1.f;
            else if (filterMode == MORPH) bpMix = filterMorph;

            // Cascade three critically-damped 2-pole stages (k=2.0).
            // Keeping k=2.0 is mandatory for stability: reducing k shifts the
            // -180° phase crossing to a higher-gain frequency, dropping the max
            // stable feedbackAmount well below 2 (causes the pumping distortion).
            x = stage1.process(x, g, 2.f, bpMix);

            // Inter-stage saturation: drive scaled up so peaks are compressed
            // between stages, preventing amplitude buildup through the cascade.
            x = saturate(x, 1.0f + feedbackAmount * 0.2f);

            x = stage2.process(x, g, 2.f, bpMix);
            x = saturate(x, 1.0f + feedbackAmount * 0.2f);

            x = stage3.process(x, g, 2.f, bpMix);

            // Store LP integrator state for feedback.
            // tanh soft-limits to ±2.5V — prevents integrator runaway while
            // allowing slightly more resonant swing than a tighter limit.
            // The wider ±2.5V range gives the feedback loop more "spring",
            // producing the elastic ring characteristic of liquid filter sweeps.
            lastFeedback = std::tanh(stage3.lastV2 * 0.4f) * 2.5f;

            // Post-cascade saturation (subtle rounding)
            x = saturate(x, 1.f + feedbackAmount * 0.08f);

            oversampledOutputs[i] = x;
        }

        // Downsample back to base rate
        float output = decimator.process(oversampledOutputs);

        // Resonance gain compensation: the resonant peak adds significant level.
        // Roll back gain progressively so it never hard-clips the output stage.
        float resonanceGainComp = 1.f - 0.18f * resonanceNormalized;
        resonanceGainComp = rack::math::clamp(resonanceGainComp, 0.78f, 1.05f);
        output *= resonanceGainComp;

        // Soft output limiter: tanh never hard-clips, so resonant peaks are
        // shaped smoothly rather than with the harsh transistor knee.
        // ±12V headroom — normal audio (±5V) gets only ~5% compression.
        output = std::tanh(output * (1.0f / 12.0f)) * 12.0f;

        if (!std::isfinite(output)) {
            reset();
            return 0.f;
        }

        return output;
    }
};
