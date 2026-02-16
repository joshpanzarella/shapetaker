#pragma once
#include <rack.hpp>
#include <algorithm>
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

    // DC blocking filter state (high-pass at ~10 Hz)
    float dcBlocker_x1 = 0.0f;
    float dcBlocker_y1 = 0.0f;

    // Pre-emphasis/de-emphasis filter state
    float preEmph_x1 = 0.0f;
    float deEmph_y1 = 0.0f;

    // Precomputed DC blocker coefficient (sample-rate dependent)
    float dcBlockR = 0.99857f; // Default for 44.1kHz, updated in setSampleRate()

    // Dither noise generator state for bit crush
    unsigned int ditherSeed = 1;

public:
    enum Type {
        HARD_CLIP = 0,  // Aggressive limiting with harsh harmonics
        TUBE_SAT = 1,   // Asymmetric tube-style saturation
        WAVE_FOLD = 2,  // Multi-stage wave folding
        BIT_CRUSH = 3,  // Bit depth + sample rate reduction
        DESTROY = 4,    // Hybrid destruction algorithm
        RING_MOD = 5    // Ring modulation with internal oscillator
    };
    
    /**
     * Set the sample rate for the distortion engine
     * @param sr Sample rate in Hz
     */
    void setSampleRate(float sr) {
        sample_rate = sr;
        // Reset SR-dependent states
        crushCounter = 0;
        // Recompute DC blocker coefficient for ~10 Hz cutoff at this sample rate
        const float dcCutoffHz = 10.0f;
        dcBlockR = rack::math::clamp(1.0f - (2.0f * (float)M_PI * dcCutoffHz / sample_rate), 0.9f, 0.9999f);
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
        dcBlocker_x1 = 0.0f;
        dcBlocker_y1 = 0.0f;
        preEmph_x1 = 0.0f;
        deEmph_y1 = 0.0f;
        ditherSeed = 1;
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
            return dcBlock(input); // Still block DC even on clean signal
        }

        // DC blocking on input (high-pass ~10 Hz)
        float clean = dcBlock(input);

        // Pre-emphasis: boost highs before distortion (gives more "analog" character)
        float emphasized = preEmphasis(clean, drive);

        // Apply distortion algorithm
        float distorted;
        switch(type) {
            case HARD_CLIP:
                distorted = hardClip(emphasized, drive);
                break;
            case WAVE_FOLD:
                distorted = waveFold(emphasized, drive);
                break;
            case BIT_CRUSH:
                distorted = bitCrush(emphasized, drive);
                break;
            case DESTROY:
                distorted = destroy(emphasized, drive);
                break;
            case RING_MOD:
                distorted = ringMod(emphasized, drive);
                break;
            case TUBE_SAT:
                distorted = tubeSat(emphasized, drive);
                break;
            default:
                distorted = emphasized;
                break;
        }

        // De-emphasis: cut highs after distortion to compensate for pre-emphasis
        float deemphasized = deEmphasis(distorted, drive);

        return deemphasized;
    }
    
    /**
     * Get the name of a distortion type
     * @param type Distortion type enum
     * @return Human-readable name
     */
    static const char* getTypeName(Type type) {
        switch(type) {
            case HARD_CLIP: return "Hard Clip";
            case TUBE_SAT: return "Tube Sat";
            case WAVE_FOLD: return "Wave Fold";
            case BIT_CRUSH: return "Bit Crush";
            case DESTROY: return "Destroy";
            case RING_MOD: return "Ring Mod";
            default: return "Unknown";
        }
    }
    
private:
    /**
     * DC blocking filter (1st order high-pass at ~10 Hz)
     * Removes DC offset that can accumulate from asymmetric distortion.
     * Coefficient dcBlockR is precomputed in setSampleRate() for correct
     * behavior at any sample rate (44.1k, 48k, 96k, 192k, etc.).
     */
    float dcBlock(float input) {
        float output = input - dcBlocker_x1 + dcBlockR * dcBlocker_y1;
        dcBlocker_x1 = input;
        dcBlocker_y1 = output;
        return output;
    }

    /**
     * Pre-emphasis filter: boost highs before distortion
     * This creates more "analog" sounding distortion
     */
    float preEmphasis(float input, float drive) {
        // Shelf boost at ~3 kHz, amount scales with drive
        const float a = 0.85f; // Smoothing factor
        float boost = 1.0f + (drive * 0.3f); // Up to +30% high frequency boost

        float highpass = input - preEmph_x1;
        preEmph_x1 = input;

        return input + highpass * boost * (1.0f - a);
    }

    /**
     * De-emphasis filter: cut highs after distortion
     * Mirrors the pre-emphasis structure so only the boosted frequencies are cut,
     * preserving harmonics created by the distortion itself.
     */
    float deEmphasis(float input, float drive) {
        const float a = 0.85f;
        float cut = 1.0f + (drive * 0.3f); // Match pre-emphasis boost amount

        float highpass = input - deEmph_y1;
        deEmph_y1 = input;

        return input - highpass * cut * (1.0f - a);
    }

    /**
     * Simple pseudo-random number generator for dithering
     */
    float dither() {
        // TPDF (Triangular Probability Density Function) dither
        ditherSeed = (ditherSeed * 1664525u + 1013904223u);
        float r1 = (float)(ditherSeed & 0x7FFFFFFF) / 2147483648.0f;
        ditherSeed = (ditherSeed * 1664525u + 1013904223u);
        float r2 = (float)(ditherSeed & 0x7FFFFFFF) / 2147483648.0f;
        return (r1 + r2 - 1.0f) * 0.5f; // -0.5 to +0.5 range
    }

    /**
     * Improved tube saturation curve
     * Models grid current and cathode bias shift
     */
    float tubeCurve(float x) {
        // Asymmetric soft clipping that mimics triode behavior
        if (x > 0.0f) {
            // Positive side: softer saturation
            return x / (1.0f + x * x * 0.5f);
        } else {
            // Negative side: harder saturation (grid current)
            float abs_x = -x;
            return -abs_x / (1.0f + abs_x * abs_x * 0.7f);
        }
    }

    /**
     * Smooth cubic interpolation for wave folding
     * Reduces aliasing at fold points
     */
    float smoothFold(float x) {
        // Wrap x into -1 to +1 range with smooth cubic interpolation at boundaries
        while (x > 1.0f || x < -1.0f) {
            if (x > 1.0f) {
                float excess = x - 1.0f;
                // Cubic ease at fold point
                x = 1.0f - excess * (3.0f - 2.0f * rack::math::clamp(excess, 0.0f, 1.0f));
                if (excess > 1.0f) x = -1.0f + (excess - 1.0f);
            } else if (x < -1.0f) {
                float excess = -1.0f - x;
                x = -1.0f + excess * (3.0f - 2.0f * rack::math::clamp(excess, 0.0f, 1.0f));
                if (excess > 1.0f) x = 1.0f - (excess - 1.0f);
            }
        }
        return x;
    }

    /**
     * Aggressive hard clipping (REDESIGNED)
     * Sharp, punchy, with high-frequency emphasis for maximum aggression
     * Distinctly different from warm tube saturation
     */
    float hardClip(float input, float drive) {
        // High-frequency emphasis BEFORE clipping (adds punch and bite)
        float highFreqBoost = 1.0f + drive * 0.4f; // Up to 40% high freq boost
        float emphasized = input * highFreqBoost;

        // Aggressive pregain - much more extreme than tube sat
        float preGain = 1.0f + drive * 25.0f; // Increased from 18x to 25x
        float x = emphasized * preGain;

        // Very tight threshold for immediate hard clipping
        float threshold = rack::math::crossfade(1.0f, 0.12f, drive); // Tighter than before (0.12 vs 0.18)

        // HARD CLIP - minimal knee for sharp transients
        float clipped;
        float abs_x = fabsf(x);

        if (abs_x <= threshold) {
            // Below threshold: pass through with slight pre-distortion
            clipped = x + x * x * x * 0.1f; // Subtle cubic nonlinearity
        } else {
            // Above threshold: HARD LIMIT with edge enhancement
            clipped = copysignf(threshold, x);

            // Add "edge" at clipping point (emphasizes transients)
            float overshoot = (abs_x - threshold) * 0.15f;
            overshoot = rack::math::clamp(overshoot, 0.0f, threshold * 0.2f);
            clipped += copysignf(overshoot, x) * (1.0f - expf(-overshoot * 5.0f));
        }

        // Add odd harmonics for more aggression (opposite of tube sat's even harmonics)
        float harmonic3 = clipped * clipped * clipped * drive * 0.15f;
        float enhanced = clipped + harmonic3;

        // Aggressive output boost to match other distortion types at 0 dB
        // Simple fixed gain that pushes output to unity
        return rack::math::clamp(enhanced * 3.5f, -1.0f, 1.0f);
    }
    
    /**
     * Multi-stage wave folding for complex harmonics (IMPROVED)
     * Now uses smooth cubic interpolation at fold points to reduce aliasing
     */
    float waveFold(float input, float drive) {
        float x = input * (1.0f + drive * 6.0f); // Up to 7x gain before folding

        // Use smooth folding function that reduces aliasing
        x = smoothFold(x);

        // Additional subtle fold at higher drive levels
        if (drive > 0.5f) {
            float extraFold = (drive - 0.5f) * 2.0f;
            x = x + extraFold * smoothFold(x * 2.0f) * 0.3f;
        }

        return rack::math::clamp(x * 1.4f, -1.0f, 1.0f); // Boosted to hit near 0 dB
    }
    
    /**
     * Bit depth reduction with sample rate crushing (IMPROVED)
     * Now includes TPDF dithering to reduce harsh quantization noise
     */
    float bitCrush(float input, float drive) {
        // 16 down to 4 effective bits for a classic stepped contour
        float bits = rack::math::crossfade(16.0f, 4.0f, drive);
        bits = rack::math::clamp(bits, 4.0f, 16.0f);

        // Add TPDF dithering before quantization (scaled to bit depth)
        float ditherAmount = 1.0f / std::exp2f(bits);
        float dithered = input + dither() * ditherAmount * 0.5f;

        // Quantize to the reduced bit depth
        float scale = std::exp2f(bits - 1.0f);
        float quantized = roundf(dithered * scale) / scale;
        quantized = rack::math::clamp(quantized, -1.0f, 1.0f);

        // Gentle sample-rate crushing that stays subtle at low drive
        float holdNorm = drive * drive; // slow ramp-in for S/R reduction
        int desiredHold = 1 + (int)std::round(holdNorm * 63.0f); // 1..64
        desiredHold = std::max(desiredHold, 1);
        if (desiredHold != crushHold) {
            crushHold = desiredHold;
            crushCounter = std::min(crushCounter, crushHold);
        }

        if (crushCounter <= 0) {
            crushCounter = crushHold;
            crushSample = quantized;
        } else {
            crushCounter--;
        }

        // Boost output to match other types
        return rack::math::clamp(crushSample * 1.1f, -1.0f, 1.0f);
    }
    
    /**
     * Hybrid destruction combining multiple algorithms (IMPROVED)
     * More aggressive and chaotic, with cross-modulated feedback
     */
    float destroy(float input, float drive) {
        // Stage 1: Aggressive wave folding with feedback modulation
        float foldAmount = drive * 0.7f + fabsf(prev_input) * 0.2f;
        float folded = waveFold(input, foldAmount);

        // Stage 2: Hard clipping to add edge
        float clipped = hardClip(folded, drive * 0.6f);

        // Stage 3: Bit crushing for digital grit
        float crushed = bitCrush(clipped, drive * 0.8f);

        // Stage 4: Nonlinear cross-modulated feedback
        float feedback = crushed * drive * 0.35f;
        float modulation = sinf(prev_input * 3.14159f) * drive * 0.15f;
        prev_input = crushed + (feedback * prev_input) + modulation;

        // Soft limiting to prevent runaway
        prev_input = tubeCurve(prev_input * 0.7f) * 1.4f;
        prev_input = rack::math::clamp(prev_input, -2.0f, 2.0f);

        return rack::math::clamp(prev_input * 1.2f, -1.0f, 1.0f); // Boosted for unity gain
    }
    
    /**
     * Ring modulation using internal oscillator (IMPROVED)
     * Carrier morphs from sine -> triangle -> square with drive
     */
    float ringMod(float input, float drive) {
        // Carrier frequency: exponential sweep from 2Hz (tremolo) to 2kHz (metallic)
        float carrier_freq = 2.0f * std::exp2f(drive * 10.0f); // 2Hz -> ~2048Hz

        // Update phase
        phase += 2.0f * M_PI * carrier_freq / sample_rate;
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }

        // Generate carrier wave that morphs with drive
        float sine = sinf(phase);
        float triangle = 2.0f * fabsf(2.0f * (phase / (2.0f * static_cast<float>(M_PI)) - 0.5f)) - 1.0f;
        float square = (phase < static_cast<float>(M_PI)) ? 1.0f : -1.0f;

        // Morph: sine -> triangle -> square as drive increases
        float carrier;
        if (drive < 0.5f) {
            // Sine to triangle
            carrier = rack::math::crossfade(sine, triangle, drive * 2.0f);
        } else {
            // Triangle to square
            carrier = rack::math::crossfade(triangle, square, (drive - 0.5f) * 2.0f);
        }

        // Ring modulation with amplitude scaling and subtle soft clipping
        float modulated = input * carrier * (1.0f + drive);
        return rack::math::clamp(tubeCurve(modulated * 0.8f) * 1.6f, -1.0f, 1.0f); // Boosted for unity gain
    }
    
    /**
     * Asymmetric tube-style saturation (IMPROVED)
     * Models triode grid current, cathode bias shift, and transformer saturation
     */
    float tubeSat(float input, float drive) {
        float preGain = 1.0f + drive * 9.0f;
        float x = input * preGain;

        // Stage 1: Triode saturation with grid current modeling
        float triode = tubeCurve(x);

        // Stage 2: Cathode bias shift (creates even harmonics)
        float bias = drive * 0.5f;
        float biased = tubeCurve(triode + bias) - tubeCurve(bias);

        // Stage 3: Output transformer saturation (smooth limiting)
        float transformer = biased / (1.0f + fabsf(biased) * 0.3f);

        // Stage 4: Add subtle "bloom" at high drive (power supply sag simulation)
        float sag = drive * drive * 0.15f;
        float bloom = transformer * (1.0f - sag * fabsf(transformer));

        // Mix stages based on drive amount
        float output = rack::math::crossfade(triode, bloom, drive * 0.7f);

        // Boost output to unity gain
        return rack::math::clamp(output * 1.15f, -1.0f, 1.0f);
    }
};

}} // namespace shapetaker::dsp
