#pragma once

#include "../utilities.hpp"

namespace shapetaker {
namespace involution {

/**
 * DSP components specific to the Involution module
 */

/**
 * Chaos generator for filter modulation
 */
class ChaosGenerator {
private:
    static constexpr float kFundamentalMul = 2.f * M_PI;
    static constexpr float kHarmonic2Mul = 7.f * M_PI;
    static constexpr float kHarmonic3Mul = 13.f * M_PI;
    static constexpr float kHarmonic1Weight = 0.5f;
    static constexpr float kHarmonic2Weight = 0.3f;
    static constexpr float kHarmonic3Weight = 0.2f;
    static constexpr float kOutputSmoothing = 0.1f;
    float phase = 0.f;
    float lastOutput = 0.f;
    
public:
    float process(float rate, float amount, float sampleTime) {
        phase += rate * sampleTime;
        if (phase >= 1.f) {
            phase -= 1.f;
        }
        
        // Simple chaotic function combining multiple harmonics
        float chaos = std::sin(phase * kFundamentalMul) * kHarmonic1Weight +
                     std::sin(phase * kHarmonic2Mul) * kHarmonic2Weight +
                     std::sin(phase * kHarmonic3Mul) * kHarmonic3Weight;
        
        // Apply amount and smooth
        chaos *= amount;
        lastOutput += (chaos - lastOutput) * kOutputSmoothing;
        
        return lastOutput;
    }
    
    void reset() {
        phase = 0.f;
        lastOutput = 0.f;
    }
};

/**
 * Resonant allpass feedback phaser: 6-stage APF chain with feedback loop.
 * The phaser center frequency is modulated by the same chaos LFO that sweeps
 * the filter cutoff, creating a unified liquid/organic spectral movement.
 */
class AllpassStage {
public:
    float x1 = 0.f;
    float y1 = 0.f;
    // y[n] = c * x[n] + x[n-1] - c * y[n-1]
    float process(float x, float c) {
        float y = c * x + x1 - c * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.f; }
};

class AllpassPhaser {
public:
    static constexpr float kFeedbackClamp = 12.f;
    static constexpr float kDryWetAverage = 0.5f;
    static constexpr int NUM_STAGES = 6;
    AllpassStage stages[NUM_STAGES];
    float feedbackMem = 0.f;

    // 1st-order APF coefficient for given center frequency
    static float apfCoeff(float centerHz, float sampleRate) {
        float g = std::tan((float)M_PI * centerHz / sampleRate);
        return (g - 1.f) / (g + 1.f);
    }

    // Returns 0.5*(dry + APF_out): unity gain at DC/Nyquist, notches at sweep freq.
    // feedback sharpens the notches; clamp protects against runaway at high resonance.
    float process(float input, float c, float fb) {
        float x = input + fb * rack::math::clamp(feedbackMem, -kFeedbackClamp, kFeedbackClamp);
        for (int i = 0; i < NUM_STAGES; i++) {
            x = stages[i].process(x, c);
        }
        feedbackMem = x;
        return kDryWetAverage * (input + x);
    }

    void reset() {
        for (int i = 0; i < NUM_STAGES; i++) stages[i].reset();
        feedbackMem = 0.f;
    }
};

/**
 * Single-sideband frequency shifter using a 4-pair IIR Hilbert approximation.
 * Shifts all frequencies by a constant Hz offset (not pitch-proportional).
 * Run channel A with +shiftHz and channel B with -shiftHz for a counter-rotating
 * stereo image: spectral content drifts in opposite directions, producing slow
 * beating at low amounts and dramatic inharmonic textures at higher settings.
 *
 * The Hilbert pair uses two 4-stage first-order allpass chains whose combined
 * phase response differs by ~90° from ~30 Hz to ~18 kHz (< 2° error across
 * the audio band at 44.1/48 kHz).  The allpass transfer function is:
 *   H(z) = (a - z^-1) / (1 - a·z^-1)
 * which gives unity gain at all frequencies, only shifting phase.
 */
class FrequencyShifter {
private:
    static constexpr int kHilbertStages = 4;
    static constexpr float kPhaseMin = -1.f;
    static constexpr float kPhaseMax = 1.f;
    static constexpr float kPhaseRadians = 2.f * static_cast<float>(M_PI);

    struct Allpass1 {
        float x1 = 0.f, y1 = 0.f;
        float process(float x, float a) {
            float y = a * x + x1 - a * y1;
            x1 = x; y1 = y;
            return y;
        }
        void reset() { x1 = y1 = 0.f; }
    };

    Allpass1 stagesI[kHilbertStages];
    Allpass1 stagesQ[kHilbertStages];
    float phase = 0.f;

public:
    // shiftHz > 0 shifts spectrum up; shiftHz < 0 shifts down
    float process(float input, float shiftHz, float sampleRate) {
        // 4-pair IIR Hilbert approximation coefficients
        // Tuned for <2° quadrature error from ~30 Hz to ~18 kHz
        static const float CI[kHilbertStages] = {0.0721f, 0.3699f, 0.6899f, 0.9150f};
        static const float CQ[kHilbertStages] = {0.2040f, 0.5200f, 0.8192f, 0.9720f};

        float I = input;
        float Q = input;
        for (int i = 0; i < kHilbertStages; i++) {
            I = stagesI[i].process(I, CI[i]);
            Q = stagesQ[i].process(Q, CQ[i]);
        }

        phase += shiftHz / sampleRate;
        if (phase >  kPhaseMax) phase -= kPhaseMax;
        if (phase < kPhaseMin) phase += kPhaseMax;

        // Upper sideband: I·cos(φ) - Q·sin(φ)
        float cosP = std::cos(kPhaseRadians * phase);
        float sinP = std::sin(kPhaseRadians * phase);
        return I * cosP - Q * sinP;
    }

    void reset() {
        for (int i = 0; i < kHilbertStages; i++) {
            stagesI[i].reset();
            stagesQ[i].reset();
        }
        phase = 0.f;
    }
};

} // namespace involution
} // namespace shapetaker
