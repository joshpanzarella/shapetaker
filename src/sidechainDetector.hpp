#pragma once
#include <rack.hpp>
#include <cmath>

using namespace rack;

/**
 * Sidechain Detector - Advanced Envelope Follower
 * 
 * This class implements a sophisticated envelope follower that tracks
 * the amplitude of the sidechain signal with separate attack and release times.
 */
class SidechainDetector {
private:
    float envelope = 0.0f;
    float attack_coeff = 0.0f;
    float release_coeff = 0.0f;
    
    // Internal sample rate tracking
    float sample_rate = 44100.0f;
    
public:
    /**
     * Set the timing parameters for the envelope follower
     * @param attack_ms Attack time in milliseconds (0.1 - 100)
     * @param release_ms Release time in milliseconds (1 - 1000) 
     * @param sr Sample rate in Hz
     */
    void setTiming(float attack_ms, float release_ms, float sr = 44100.0f) {
        sample_rate = sr;
        
        // Convert milliseconds to coefficient values
        // Using exponential decay formula: coeff = exp(-1 / (time_constant * sample_rate))
        attack_coeff = expf(-1.0f / (attack_ms * 0.001f * sample_rate));
        release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));
        
        // Clamp coefficients to prevent instability
        attack_coeff = clamp(attack_coeff, 0.0f, 0.999f);
        release_coeff = clamp(release_coeff, 0.0f, 0.999f);
    }
    
    /**
     * Process a single sample through the envelope follower
     * @param input Input signal sample (should be pre-scaled to 0.0-1.0 range)
     * @return Current envelope value (0.0 - 1.0)
     */
    float process(float input) {
        float target = fabsf(input); // Use absolute value for envelope
        target = clamp(target, 0.0f, 1.0f); // Ensure target stays in valid range
        
        // Use different coefficients for attack vs release
        if (target > envelope) {
            // Attack phase - rising envelope
            envelope = target + (envelope - target) * attack_coeff;
        } else {
            // Release phase - falling envelope
            envelope = target + (envelope - target) * release_coeff;
        }
        
        // Ensure envelope decays to true zero when input is zero
        if (target < 0.0001f && envelope < 0.001f) {
            envelope = 0.0f;
        }
        
        // Clamp output to valid range
        envelope = clamp(envelope, 0.0f, 1.0f);
        
        return envelope;
    }
    
    /**
     * Get the current envelope value without processing new input
     * @return Current envelope level
     */
    float getEnvelope() const { 
        return envelope; 
    }
    
    /**
     * Reset the envelope to zero (useful for initialization)
     */
    void reset() {
        envelope = 0.0f;
    }
    
    /**
     * Get the current sample rate
     */
    float getSampleRate() const {
        return sample_rate;
    }
};