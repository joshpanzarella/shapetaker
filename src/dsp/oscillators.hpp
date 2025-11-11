#pragma once
#include <rack.hpp>
#include <cmath>

using namespace rack;

namespace shapetaker {
namespace dsp {

// ============================================================================
// OSCILLATOR UTILITIES
// ============================================================================

class OscillatorHelper {
public:
    // Generate basic waveforms
    static float sine(float phase) {
        return std::sin(2.f * M_PI * phase);
    }
    
    static float triangle(float phase) {
        return 2.f * std::abs(2.f * phase - 1.f) - 1.f;
    }
    
    static float saw(float phase) {
        return 2.f * phase - 1.f;
    }
    
    static float square(float phase, float pulseWidth = 0.5f) {
        return (phase < pulseWidth) ? 1.f : -1.f;
    }
    
    // Advance phase with frequency and sample rate
    static float advancePhase(float phase, float frequency, float sampleRate) {
        phase += frequency / sampleRate;
        return phase - std::floor(phase); // Wrap to [0, 1)
    }
    
    // Generate noise
    static float noise() {
        static uint32_t seed = 1;
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (seed & 0xFFFF) / 32767.5f - 1.f;
    }
    
    // Soft sync - reset phase when sync input rises
    static float processSoftSync(float phase, float lastSyncInput, float syncInput) {
        if (syncInput > 0.1f && lastSyncInput <= 0.1f) {
            return 0.f; // Reset phase
        }
        return phase;
    }
    
    // Hard sync - force phase to match sync oscillator
    static float processHardSync(float phase, float syncPhase, float lastSyncPhase) {
        // Detect sync oscillator reset
        if (syncPhase < lastSyncPhase) {
            return 0.f;
        }
        return phase;
    }

    // Sigmoid-morphed saw with subtle organic coloration
    static float organicSigmoidSaw(float phase, float shape, float freq, float sampleRate) {
        shape = rack::math::clamp(shape, 0.f, 1.f);
        // Emphasize the midpoint so modulation sweeps feel more dramatic
        float emphasizedShape = 1.f - std::pow(1.f - shape, 1.5f);

        // Linear sawtooth baseline
        float linearSaw = 2.f * phase - 1.f;
        if (shape < 0.001f) {
            return std::tanh(linearSaw * 1.02f) * 0.98f;
        }

        float range = 3.f + emphasizedShape * 9.f;
        float sigmoidInput = (phase - 0.5f) * range * 2.f;

        // Subtle harmonic bias tied to phase
        float harmonicBias = std::sin(phase * 2.f * M_PI * 3.f) * 0.03f * emphasizedShape;
        sigmoidInput += harmonicBias;

        float sigmoidOutput = std::tanh(sigmoidInput);

        float blend = emphasizedShape * 1.25f + std::sin(phase * 2.f * M_PI) * 0.015f * emphasizedShape;
        blend = rack::math::clamp(blend, 0.f, 1.f);

        float result = linearSaw * (1.f - blend) + sigmoidOutput * blend;

        // Add airy harmonics when not near Nyquist
        float nyquist = sampleRate * 0.5f;
        if (freq < nyquist * 0.35f) {
            float air = std::sin(phase * 2.f * M_PI * 7.f) * 0.008f * emphasizedShape;
            result += air;
        }

        return std::tanh(result * 1.05f) * 0.95f;
    }

    static float equalPowerMix(float a, float b, float t) {
        float angle = rack::math::clamp(t, 0.f, 1.f) * (float)M_PI_2;
        return a * std::cos(angle) + b * std::sin(angle);
    }

    // ========================================================================
    // ANTI-ALIASING UTILITIES
    // ========================================================================

    // PolyBLEP (Polynomial Band-Limited Step) residual calculation
    // Used to reduce aliasing at discontinuities in waveforms
    // t: normalized distance from discontinuity (0 at discontinuity, increases with distance)
    // Returns the correction value to subtract from the naive waveform
    static float polyBLEP(float t) {
        // For t in [0, 1], apply polynomial correction
        if (t < 1.f) {
            t = t + t - t * t - 1.f;
            return t;
        }
        // For t in [-1, 0], apply mirrored correction
        else if (t > -1.f) {
            t = t + t + t * t + 1.f;
            return t;
        }
        // Outside correction range
        return 0.f;
    }

    // Generate PWM (Pulse Width Modulation) waveform with polyBLEP anti-aliasing
    // phase: oscillator phase [0, 1)
    // pulseWidth: duty cycle [0, 1], clamped to [0.05, 0.95] to prevent DC offset
    // freq: oscillator frequency in Hz
    // sampleRate: audio sample rate in Hz
    // Returns anti-aliased pulse wave output in range [-1, 1]
    static float pwmWithPolyBLEP(float phase, float pulseWidth, float freq, float sampleRate) {
        // Clamp pulse width to prevent stuck DC offset
        pulseWidth = rack::math::clamp(pulseWidth, 0.05f, 0.95f);

        // Generate naive square wave
        float output = (phase < pulseWidth) ? 1.f : -1.f;

        // Calculate normalized phase increment per sample
        float dt = freq / sampleRate;

        // Apply polyBLEP correction at rising edge (phase = 0)
        if (phase < dt) {
            float t = phase / dt;
            output -= polyBLEP(t);
        }
        // Apply polyBLEP correction at falling edge (phase = pulseWidth)
        else if (phase > pulseWidth && phase < pulseWidth + dt) {
            float t = (phase - pulseWidth) / dt;
            output += polyBLEP(t);
        }

        return output;
    }

    // Convenience alias for backward compatibility
    static inline float generatePWM(float phase, float pulseWidth, float freq, float sampleRate) {
        return pwmWithPolyBLEP(phase, pulseWidth, freq, sampleRate);
    }
};

}} // namespace shapetaker::dsp
