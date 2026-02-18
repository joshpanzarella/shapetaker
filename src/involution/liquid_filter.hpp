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
 * - lastFeedback is tanh-limited to ±FEEDBACK_TANH_SWING — prevents integrator
 *   runaway and gives the feedback loop extra "spring" for an elastic, liquid
 *   character.
 * - resonanceNormalized capped at RESONANCE_NORM_CAP → feedbackAmount ceiling
 *   ≈1.76: loop gain at -180° ≈ 0.74, near-oscillating elastic ring, provably
 *   stable.
 * - Dual-envelope cutoff breathing: a fast input follower (ENV_ATTACK_TC /
 *   ENV_RELEASE_TC) opens the cutoff on transients; a slow output follower
 *   (OUT_ENV_ATTACK_TC / OUT_ENV_RELEASE_TC) adds a secondary "bloom" as the
 *   resonant peak itself builds then decays.  The two envelopes create a
 *   multi-stage release — the resonance seeks, detunes slightly, and settles —
 *   which is the defining liquid quality of vintage analog ladder filters
 *   reacting to their own current draw.
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

    // Parameter bounds — referenced by the host module's configParam/clamp calls
    static constexpr float RESONANCE_MIN = 0.707f;
    static constexpr float RESONANCE_MAX = 2.05f;

private:
    // -------------------------------------------------------------------------
    // DSP tuning constants
    // -------------------------------------------------------------------------
    static constexpr float RESONANCE_NORM_CAP   = 0.88f;   // hard cap on normalised resonance
    static constexpr float FEEDBACK_EXP         = 0.85f;   // concentrates resonance in mid-sweep
    static constexpr float FEEDBACK_SCALE       = 2.0f;    // feedbackAmount multiplier
    static constexpr float BREATH_CUTOFF_SCALE  = 0.20f;   // max cutoff shift from input breath (20%)
    static constexpr float BLOOM_CUTOFF_SCALE   = 0.06f;   // max cutoff shift from output bloom (6%)
    static constexpr float ENV_ATTACK_TC        = 0.003f;  // 3ms   — input transient attack
    static constexpr float ENV_RELEASE_TC       = 0.120f;  // 120ms — input breath release
    static constexpr float OUT_ENV_ATTACK_TC    = 0.010f;  // 10ms  — output bloom attack
    static constexpr float OUT_ENV_RELEASE_TC   = 0.250f;  // 250ms — output bloom release
    static constexpr float FEEDBACK_TANH_SWING  = 2.5f;    // ±V limit on tanh-clamped feedback
    static constexpr float FEEDBACK_PRESCALE    = 0.4f;    // pre-tanh scale on stage3 LP output
    static constexpr float SIGNAL_HEADROOM      = 12.f;    // ±V headroom throughout the signal path
    static constexpr float INPUT_PEAK_NORM      = 10.f;    // normalise input envelope to 10V peak = 1.0
    static constexpr float SVF_K                = 2.f;     // critically-damped SVF damping coefficient
    static constexpr float HP_CUTOFF_RATIO      = 0.20f;   // feedback HP as fraction of filter cutoff
    static constexpr float HP_CUTOFF_MIN_HZ     = 30.f;    // lower bound for feedback HP
    static constexpr float HP_CUTOFF_MAX_HZ     = 160.f;   // upper bound for feedback HP
    static constexpr float SAT_DRIVE_PRE         = 0.15f;  // saturation drive growth post-injection
    static constexpr float SAT_DRIVE_INTER      = 0.20f;  // saturation drive growth between stages
    static constexpr float SAT_DRIVE_POST       = 0.08f;  // saturation drive growth post-cascade
    static constexpr float BREATH_RESONANCE_DAMP = 0.4f;  // how much breath modulation scales back at max resonance

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

    // Input envelope follower: fast attack, medium release
    // Tracks the incoming signal level to open the cutoff on transients.
    float signalEnvelope  = 0.f;
    float envAttackCoeff  = 0.f;
    float envReleaseCoeff = 0.f;

    // Output envelope follower: slow attack, slow release ("bloom")
    // Tracks the filter output level — when the resonant peak builds up,
    // the cutoff shifts slightly, detuning the peak and creating the
    // liquid "seeking-and-settling" motion of vintage ladder filters.
    float outputEnvelope     = 0.f;
    float outEnvAttackCoeff  = 0.f;
    float outEnvReleaseCoeff = 0.f;

    // 2nd-order HP on the feedback path: two cascaded 1-pole LP states.
    // Subtracting only the HP'd feedback from the input preserves bass.
    // Two poles give -12dB/oct below the HP cutoff (vs -6dB/oct with one pole),
    // strongly protecting bass even at very high resonance settings.
    float hpFeedbackLP1 = 0.f;
    float hpFeedbackLP2 = 0.f;

    // Transistor-style saturation curve
    // Linear for small signals (< 0.5), smooth transition, hard clip above 1.0
    float saturate(float input, float drive) {
        drive = rack::math::clamp(drive, 0.1f, SIGNAL_HEADROOM);

        float normalized = (input * drive) / SIGNAL_HEADROOM;
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

        float output = (normalized * SIGNAL_HEADROOM) / drive;
        return rack::math::clamp(output, -SIGNAL_HEADROOM, SIGNAL_HEADROOM);
    }

    // Smooth tanh-based limiter used for in-filter saturation.
    // Unlike saturate(), this has no hard-clip region — it compresses asymptotically
    // toward SIGNAL_HEADROOM/drive.  This prevents the resonant ring-up from
    // transient-heavy inputs (PWM, hard sync) from triggering harsh clipping artefacts
    // while still providing effective amplitude control inside the cascade.
    float filterSaturate(float input, float drive) {
        drive = rack::math::clamp(drive, 0.1f, SIGNAL_HEADROOM);
        return std::tanh(input * drive / SIGNAL_HEADROOM) * SIGNAL_HEADROOM / drive;
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
        return rack::math::clamp(result, -SIGNAL_HEADROOM, SIGNAL_HEADROOM);
    }

public:
    LiquidFilter() : decimator(0.9f), upsampler(0.9f) {
        oversampledRate    = baseSampleRate * OVERSAMPLE_FACTOR;
        envAttackCoeff     = std::exp(-1.f / (baseSampleRate * ENV_ATTACK_TC));
        envReleaseCoeff    = std::exp(-1.f / (baseSampleRate * ENV_RELEASE_TC));
        outEnvAttackCoeff  = std::exp(-1.f / (baseSampleRate * OUT_ENV_ATTACK_TC));
        outEnvReleaseCoeff = std::exp(-1.f / (baseSampleRate * OUT_ENV_RELEASE_TC));
        reset();
    }

    void setSampleRate(float sr) {
        baseSampleRate     = sr;
        oversampledRate    = sr * OVERSAMPLE_FACTOR;
        envAttackCoeff     = std::exp(-1.f / (sr * ENV_ATTACK_TC));
        envReleaseCoeff    = std::exp(-1.f / (sr * ENV_RELEASE_TC));
        outEnvAttackCoeff  = std::exp(-1.f / (sr * OUT_ENV_ATTACK_TC));
        outEnvReleaseCoeff = std::exp(-1.f / (sr * OUT_ENV_RELEASE_TC));
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
        signalEnvelope = 0.f;
        outputEnvelope = 0.f;
    }

    float process(float input, float cutoff, float resonance, float drive = 1.f) {
        // Safety checks
        if (!std::isfinite(input)) return 0.f;
        if (oversampledRate <= 0.f) return input;

        // Clamp parameters to safe ranges
        cutoff    = rack::math::clamp(cutoff,    1.f, oversampledRate * 0.45f);
        resonance = rack::math::clamp(resonance, 0.1f, 10.f);
        drive     = rack::math::clamp(drive,     0.1f, 10.f);

        // Map resonance to normalized 0..1
        const float resonanceRange = std::max(RESONANCE_MAX - RESONANCE_MIN, 0.001f);
        float resonanceClamped    = rack::math::clamp(resonance, RESONANCE_MIN, RESONANCE_MAX);
        // Cap at RESONANCE_NORM_CAP so feedbackAmount never exceeds ~1.76 — keeps
        // loop gain comfortably below 1 (≈0.74 at ceiling) and prevents instability
        // while still delivering near-self-oscillating elasticity.
        float resonanceNormalized = rack::math::clamp(
            (resonanceClamped - RESONANCE_MIN) / resonanceRange, 0.f, RESONANCE_NORM_CAP);

        // Global feedback amount.
        // Exponent FEEDBACK_EXP (vs 1.0 linear) concentrates resonance presence in the
        // lower-middle of the knob range, so the elastic ring is audible across more of
        // the sweep rather than appearing only near the top.
        float feedbackAmount = std::pow(resonanceNormalized, FEEDBACK_EXP) * FEEDBACK_SCALE;

        // ====================================================================
        // DUAL-ENVELOPE CUTOFF BREATHING
        // ====================================================================
        // Input follower: fast attack / medium release.
        // Opens the cutoff on incoming transients, then exhales over ~120ms.
        {
            float envIn   = std::abs(input) * (1.f / INPUT_PEAK_NORM);
            float envCoeff = (envIn > signalEnvelope) ? envAttackCoeff : envReleaseCoeff;
            signalEnvelope += (1.f - envCoeff) * (envIn - signalEnvelope);
            signalEnvelope  = rack::math::clamp(signalEnvelope, 0.f, 1.f);
        }
        // outputEnvelope holds the previous cycle's tracked output level (0-1).
        // It is updated after decimation (below) so this cycle uses last cycle's
        // value — a 1-sample delay that avoids an algebraic loop.  The bloom
        // effect is too slow (OUT_ENV_RELEASE_TC) to be sensitive to 1-sample jitter.

        // Input breath: up to BREATH_CUTOFF_SCALE cutoff shift at full signal (≈3.2 semitones).
        // Scale back with resonance: PWM and sync produce dense transients that trigger the
        // breath follower continuously, causing rapid cutoff modulation that interacts badly
        // with the near-oscillating loop.  At max resonance the shift is ~60% of its base value.
        // Output bloom: up to BLOOM_CUTOFF_SCALE additional shift as resonance builds (≈1 semitone)
        float breathScale = BREATH_CUTOFF_SCALE * (1.f - resonanceNormalized * BREATH_RESONANCE_DAMP);
        float breathCutoff = cutoff * (1.f + signalEnvelope * breathScale
                                           + outputEnvelope * BLOOM_CUTOFF_SCALE);
        breathCutoff = rack::math::clamp(breathCutoff, 1.f, oversampledRate * 0.45f);

        // Upsample
        float upsampledBuffer[OVERSAMPLE_FACTOR];
        upsampler.process(input, upsampledBuffer);

        float oversampledOutputs[OVERSAMPLE_FACTOR];

        for (int i = 0; i < OVERSAMPLE_FACTOR; i++) {
            float x = upsampledBuffer[i];

            // Pre-filter drive saturation
            x = driveSaturate(x, drive);

            // Filter coefficients at oversampled rate.
            // breathCutoff carries the envelope-modulated cutoff for elasticity.
            float g = std::tan(M_PI * breathCutoff / oversampledRate);
            g = rack::math::clamp(g, 0.f, 0.99f);

            // ================================================================
            // GLOBAL FEEDBACK TOPOLOGY (ladder-style)
            // ================================================================
            // 2nd-order HP on feedback at HP_CUTOFF_RATIO of filter cutoff (-12dB/oct).
            // e.g. cutoff=400Hz → HP at 80Hz; 40Hz is then -24dB down in the feedback
            // signal.  Clamped HP_CUTOFF_MIN_HZ–HP_CUTOFF_MAX_HZ to stay well below
            // the musical midrange and protect more of the bass spectrum.
            {
                float HP_CUTOFF_HZ = rack::math::clamp(
                    breathCutoff * HP_CUTOFF_RATIO, HP_CUTOFF_MIN_HZ, HP_CUTOFF_MAX_HZ);
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

            // Post-injection saturation: smooth tanh limit so feedback peaks at high
            // resonance are rounded rather than hard-clipped.
            x = filterSaturate(x, 1.0f + feedbackAmount * SAT_DRIVE_PRE);

            // Compute bandpass mix from filter mode
            float bpMix = 0.f;
            if (filterMode == BANDPASS) bpMix = 1.f;
            else if (filterMode == MORPH) bpMix = filterMorph;

            // Cascade three critically-damped 2-pole stages (k=SVF_K).
            // Keeping SVF_K=2.0 is mandatory for stability: reducing k shifts the
            // -180° phase crossing to a higher-gain frequency, dropping the max
            // stable feedbackAmount well below 2 (causes the pumping distortion).
            x = stage1.process(x, g, SVF_K, bpMix);

            // Inter-stage saturation: smooth tanh prevents amplitude buildup through
            // the cascade without adding hard-clip artefacts to the resonant ring.
            x = filterSaturate(x, 1.0f + feedbackAmount * SAT_DRIVE_INTER);

            x = stage2.process(x, g, SVF_K, bpMix);
            x = filterSaturate(x, 1.0f + feedbackAmount * SAT_DRIVE_INTER);

            x = stage3.process(x, g, SVF_K, bpMix);

            // Store LP integrator state for feedback.
            // tanh soft-limits to ±FEEDBACK_TANH_SWING — prevents integrator runaway
            // while allowing slightly more resonant swing than a tighter limit.
            // The wider swing gives the feedback loop more "spring", producing the
            // elastic ring characteristic of liquid filter sweeps.
            lastFeedback = std::tanh(stage3.lastV2 * FEEDBACK_PRESCALE) * FEEDBACK_TANH_SWING;

            // Post-cascade saturation (subtle rounding — smooth tanh, no hard clip)
            x = filterSaturate(x, 1.f + feedbackAmount * SAT_DRIVE_POST);

            oversampledOutputs[i] = x;
        }

        // Downsample back to base rate
        float output = decimator.process(oversampledOutputs);

        // Update output bloom envelope for next cycle.
        // Tracks filter output level (normalized to 0-1 at ±SIGNAL_HEADROOM peak).
        // Slow attack ignores transients; slow release holds the bloom long enough
        // to create the liquid "seeking-and-settling" motion.
        {
            float envOut  = std::abs(output) * (1.f / SIGNAL_HEADROOM);
            float outCoeff = (envOut > outputEnvelope) ? outEnvAttackCoeff : outEnvReleaseCoeff;
            outputEnvelope += (1.f - outCoeff) * (envOut - outputEnvelope);
            outputEnvelope  = rack::math::clamp(outputEnvelope, 0.f, 1.f);
        }

        // Soft output limiter: tanh handles any resonant peak buildup smoothly.
        // No separate gain compensation — blanket gain rolloff was causing the
        // perceived -3dB drop at high resonance.  The tanh only compresses the
        // loudest peaks (resonant spikes) while leaving the passband at full level,
        // which is more authentic to analog behavior and sounds more alive.
        // ±SIGNAL_HEADROOM — normal audio (±5V) gets only ~5% compression.
        output = std::tanh(output * (1.0f / SIGNAL_HEADROOM)) * SIGNAL_HEADROOM;

        if (!std::isfinite(output)) {
            reset();
            return 0.f;
        }

        return output;
    }
};
