#pragma once
#include "../plugin.hpp"
#include <cmath>

/**
 * SimpleLiquidFilter - 6th-order lowpass WITHOUT oversampling
 * Simplified version to debug crash issues
 */
class SimpleLiquidFilter {
private:
    // Three 2-pole SVF stages
    struct SVF2Pole {
        float ic1eq = 0.f;
        float ic2eq = 0.f;

        void reset() {
            ic1eq = ic2eq = 0.f;
        }

        float process(float input, float g, float k) {
            // Prevent denormals
            if (std::abs(ic1eq) < 1e-20f) ic1eq = 0.f;
            if (std::abs(ic2eq) < 1e-20f) ic2eq = 0.f;

            float v1 = (ic1eq + g * (input - ic2eq)) / (1.f + g * (g + k));
            float v2 = ic2eq + g * v1;

            ic1eq = 2.f * v1 - ic1eq;
            ic2eq = 2.f * v2 - ic2eq;

            return v2;
        }
    };

    SVF2Pole stage1, stage2, stage3;
    float sampleRate = 48000.f;

public:
    SimpleLiquidFilter() {
        reset();
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(sr, 1000.f);  // Safety
    }

    void reset() {
        stage1.reset();
        stage2.reset();
        stage3.reset();
    }

    float process(float input, float cutoff, float resonance) {
        // Safety checks
        if (!std::isfinite(input)) {
            reset();
            return 0.f;
        }

        // Clamp parameters
        cutoff = rack::math::clamp(cutoff, 20.f, sampleRate * 0.45f);
        resonance = rack::math::clamp(resonance, 0.1f, 2.0f);

        // Calculate filter coefficient
        float g = std::tan(M_PI * cutoff / sampleRate);
        g = rack::math::clamp(g, 0.001f, 0.99f);

        // Distribute resonance across stages
        float k1 = 2.f - 0.3f * resonance;
        float k2 = 2.f - 0.8f * resonance;
        float k3 = 2.f - 2.0f * resonance;

        // Process through three stages
        float x = input;
        x = stage1.process(x, g, k1);
        x = stage2.process(x, g, k2);
        x = stage3.process(x, g, k3);

        // Final safety
        if (!std::isfinite(x)) {
            reset();
            return 0.f;
        }

        return x;
    }
};
