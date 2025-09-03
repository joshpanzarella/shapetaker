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
};

}} // namespace shapetaker::dsp