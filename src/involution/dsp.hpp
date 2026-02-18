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
    float phase = 0.f;
    float lastOutput = 0.f;
    
public:
    float process(float rate, float amount, float sampleTime) {
        phase += rate * sampleTime;
        if (phase >= 1.f) {
            phase -= 1.f;
        }
        
        // Simple chaotic function combining multiple harmonics
        float chaos = std::sin(phase * 2.f * M_PI) * 0.5f +
                     std::sin(phase * 7.f * M_PI) * 0.3f +
                     std::sin(phase * 13.f * M_PI) * 0.2f;
        
        // Apply amount and smooth
        chaos *= amount;
        lastOutput += (chaos - lastOutput) * 0.1f;
        
        return lastOutput;
    }
    
    void reset() {
        phase = 0.f;
        lastOutput = 0.f;
    }
};

/**
 * Cross-feedback processor for dual filter setup
 */
class CrossFeedback {
private:
    float lastOutputA = 0.f;  // previous cycle's filter A output
    float lastOutputB = 0.f;  // previous cycle's filter B output

public:
    struct IO {
        float inputA, inputB;
        float outputA, outputB;
    };

    // Call BEFORE filtering: injects previous cycle's opposite-channel filter
    // output into this cycle's filter input.  Because we inject the FILTER
    // OUTPUT (not the raw dry input), the two filters hear each other's
    // spectrally-shaped signal — if their cutoffs or resonances differ, this
    // creates genuine tonal interaction rather than a level-neutral crossfade
    // of identical dry signals.
    //
    // Additive injection preserves input level (no crossfade volume drop).
    // blend = amount * 0.15 keeps the cross-channel loop gain (blend^2 * Gpeak^2)
    // safely below 1 even at high resonance: 0.15^2 * 4^2 = 0.36 at peak gain 4×.
    IO process(float inputA, float inputB, float amount) {
        amount = rack::math::clamp(amount, 0.f, 1.f);

        float blend = amount * 0.15f;

        float crossA = inputA + lastOutputB * blend;
        float crossB = inputB + lastOutputA * blend;

        return {crossA, crossB, crossA, crossB};
    }

    // Call AFTER filtering: store outputs for next cycle's cross-injection.
    // tanh-limits to ±12V so resonant peaks don't drive runaway injection.
    void storeOutputs(float outputA, float outputB) {
        lastOutputA = std::tanh(outputA * (1.f / 12.f)) * 12.f;
        lastOutputB = std::tanh(outputB * (1.f / 12.f)) * 12.f;
    }

    void reset() {
        lastOutputA = lastOutputB = 0.f;
    }
};

/**
 * Stereo width processor for magical stereo effects
 */
class StereoProcessor {
public:
    struct StereoSignal {
        float left, right;
    };
    
    static StereoSignal processWidth(float mono, float width) {
        width = rack::math::clamp(width, 0.f, 2.f);
        
        if (width < 1.f) {
            // Narrow the stereo image
            float narrowAmount = 1.f - width;
            return {
                mono * (1.f - narrowAmount * 0.5f),
                mono * (1.f - narrowAmount * 0.5f)
            };
        } else {
            // Widen the stereo image using simple phase manipulation
            float widenAmount = width - 1.f;
            return {
                mono * (1.f + widenAmount * 0.3f),
                mono * (1.f - widenAmount * 0.3f)
            };
        }
    }
    
    static StereoSignal processRotation(StereoSignal input, float rotation) {
        rotation = rack::math::clamp(rotation, -1.f, 1.f);
        
        float angle = rotation * M_PI * 0.25f; // Max 45 degrees
        float cosAngle = std::cos(angle);
        float sinAngle = std::sin(angle);
        
        return {
            input.left * cosAngle - input.right * sinAngle,
            input.left * sinAngle + input.right * cosAngle
        };
    }
};

} // namespace involution
} // namespace shapetaker