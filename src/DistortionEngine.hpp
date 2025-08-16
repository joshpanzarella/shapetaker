#pragma once
#include <rack.hpp>
#include <cmath>

using namespace rack;

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
    }
    
    /**
     * Reset internal state (useful for feedback-based algorithms)
     */
    void reset() {
        phase = 0.0f;
        prev_input = 0.0f;
    }
    
    /**
     * Process audio through the selected distortion algorithm
     * @param input Input audio sample (-10V to +10V typical)
     * @param drive Distortion amount (0.0 - 1.0)
     * @param type Distortion algorithm to use
     * @return Processed audio sample
     */
    float process(float input, float drive, Type type) {
        drive = clamp(drive, 0.0f, 1.0f);
        
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
        return clamp(x, -1.0f, 1.0f);
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
        bits = clamp(bits, 2.0f, 16.0f);
        
        // Quantize to reduced bit depth
        float levels = powf(2.0f, bits);
        float quantized = roundf(input * levels) / levels;
        
        // Optional: Add sample rate reduction effect
        // (This would require a counter in a real implementation)
        
        return quantized;
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
        prev_input = clamp(prev_input, -2.0f, 2.0f);
        
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