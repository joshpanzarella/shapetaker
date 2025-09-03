#pragma once
#include <rack.hpp>
#include <cmath>

using namespace rack;

namespace shapetaker {
namespace dsp {

// ============================================================================
// SIDECHAIN DETECTION
// ============================================================================

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
        attack_coeff = rack::math::clamp(attack_coeff, 0.0f, 0.999f);
        release_coeff = rack::math::clamp(release_coeff, 0.0f, 0.999f);
    }
    
    /**
     * Process a single sample through the envelope follower
     * @param input Input signal sample (should be pre-scaled to 0.0-1.0 range)
     * @return Current envelope value (0.0 - 1.0)
     */
    float process(float input) {
        float target = fabsf(input); // Use absolute value for envelope
        target = rack::math::clamp(target, 0.0f, 1.0f); // Ensure target stays in valid range
        
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
        envelope = rack::math::clamp(envelope, 0.0f, 1.0f);
        
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

// ============================================================================
// DISTORTION EFFECTS
// ============================================================================

/**
 * Distortion Engine - Collection of Intense Distortion Algorithms
 * 
 * This class provides 6 different distortion types ranging from
 * aggressive clipping to complex wave manipulation.
 */
class DistortionEngine {
private:
    float phase = 0.0f;          // For oscillator-based effects
    float sample_rate = 44100.0f; // Current sample rate
    float prev_input = 0.0f;     // For feedback effects
    
    // Sample-rate reduction state for bit crush
    int crushCounter = 0;        // Downsample hold counter
    int crushHold = 1;           // Samples to hold
    float crushSample = 0.0f;    // Last held sample
    
public:
    enum Type {
        HARD_CLIP = 0,  // Aggressive limiting with harsh harmonics
        WAVE_FOLD = 1,  // Multi-stage wave folding
        BIT_CRUSH = 2,  // Bit depth + sample rate reduction  
        DESTROY = 3,    // Hybrid destruction algorithm
        RING_MOD = 4,   // Ring modulation with internal oscillator
        TUBE_SAT = 5    // Asymmetric tube-style saturation
    };
    
    /**
     * Set the sample rate for the distortion engine
     * @param sr Sample rate in Hz
     */
    void setSampleRate(float sr) { 
        sample_rate = sr; 
        // Reset SR-dependent states
        crushCounter = 0;
    }
    
    /**
     * Reset internal state (useful for feedback-based algorithms)
     */
    void reset() {
        phase = 0.0f;
        prev_input = 0.0f;
        crushCounter = 0;
        crushHold = 1;
        crushSample = 0.0f;
    }
    
    /**
     * Process audio through the selected distortion algorithm
     * @param input Input audio sample (-10V to +10V typical)
     * @param drive Distortion amount (0.0 - 1.0)
     * @param type Distortion algorithm to use
     * @return Processed audio sample
     */
    float process(float input, float drive, Type type) {
        drive = rack::math::clamp(drive, 0.0f, 1.0f);
        
        // If drive is negligible, return clean signal and reset state
        if (drive < 0.001f) {
            prev_input *= 0.99f; // Slowly decay feedback state
            return input;
        }
        
        switch(type) {
            case HARD_CLIP:
                return hardClip(input, drive);
            case WAVE_FOLD:
                return waveFold(input, drive);
            case BIT_CRUSH:
                return bitCrush(input, drive);
            case DESTROY:
                return destroy(input, drive);
            case RING_MOD:
                return ringMod(input, drive);
            case TUBE_SAT:
                return tubeSat(input, drive);
            default:
                return input;
        }
    }
    
    /**
     * Get the name of a distortion type
     * @param type Distortion type enum
     * @return Human-readable name
     */
    static const char* getTypeName(Type type) {
        switch(type) {
            case HARD_CLIP: return "Hard Clip";
            case WAVE_FOLD: return "Wave Fold"; 
            case BIT_CRUSH: return "Bit Crush";
            case DESTROY: return "Destroy";
            case RING_MOD: return "Ring Mod";
            case TUBE_SAT: return "Tube Sat";
            default: return "Unknown";
        }
    }
    
private:
    /**
     * Aggressive hard clipping with extended drive range
     */
    float hardClip(float input, float drive) {
        float x = input * (1.0f + drive * 8.0f); // Drive up to 9x gain
        return rack::math::clamp(x, -1.0f, 1.0f);
    }
    
    /**
     * Multi-stage wave folding for complex harmonics
     */
    float waveFold(float input, float drive) {
        float x = input * (1.0f + drive * 6.0f); // Up to 7x gain before folding
        
        // Multiple folding stages for increased complexity
        for(int i = 0; i < 3; i++) {
            if (x > 1.0f) {
                x = 2.0f - x;  // Fold down from ceiling
            } else if (x < -1.0f) {
                x = -2.0f - x; // Fold up from floor
            }
        }
        
        return x * 0.5f; // Scale back to reasonable range
    }
    
    /**
     * Bit depth reduction with sample rate crushing
     */
    float bitCrush(float input, float drive) {
        float bits = 16.0f - (drive * 14.0f);  // 16 bits down to 2 bits
        bits = rack::math::clamp(bits, 2.0f, 16.0f);
        
        // Quantize to reduced bit depth
        float levels = powf(2.0f, bits);
        float quantized = roundf(input * levels) / levels;

        // Sample-rate reduction via simple sample-and-hold
        // Higher drive -> stronger reduction (longer hold)
        int desiredHold = 1 + (int) std::round(drive * 127.0f); // 1..128
        // Smoothly adapt the hold length to avoid zipper noise on parameter changes
        if (desiredHold != crushHold) crushHold = desiredHold;

        if (crushCounter <= 0) {
            crushCounter = crushHold;
            crushSample = quantized;
        } else {
            crushCounter--;
        }

        return crushSample;
    }
    
    /**
     * Hybrid destruction combining multiple algorithms
     */
    float destroy(float input, float drive) {
        // Stage 1: Wave folding
        float folded = waveFold(input, drive * 0.7f);
        
        // Stage 2: Bit crushing
        float crushed = bitCrush(folded, drive * 0.8f);
        
        // Stage 3: Nonlinear feedback
        float feedback = crushed * drive * 0.3f;
        prev_input = crushed + (feedback * prev_input);
        
        // Prevent runaway feedback
        prev_input = rack::math::clamp(prev_input, -2.0f, 2.0f);
        
        return prev_input;
    }
    
    /**
     * Ring modulation using internal sine wave oscillator
     */
    float ringMod(float input, float drive) {
        // Carrier frequency increases with drive (50Hz to 550Hz)
        float carrier_freq = 50.0f + (drive * 500.0f);
        
        // Generate carrier wave
        float carrier = sinf(phase);
        
        // Update phase
        phase += 2.0f * M_PI * carrier_freq / sample_rate;
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
        
        // Ring modulation with amplitude scaling
        return input * carrier * (1.0f + drive);
    }
    
    /**
     * Asymmetric tube-style saturation
     */
    float tubeSat(float input, float drive) {
        float x = input * (1.0f + drive * 2.0f);
        
        // Asymmetric saturation (different for positive/negative)
        if (x >= 0.0f) {
            return 1.0f - expf(-x);
        } else {
            return -(1.0f - expf(x));
        }
    }
};

}} // namespace shapetaker::dsp