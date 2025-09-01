#pragma once
#include "plugin.hpp"
#include <cmath>
#include <array>

namespace shapetaker {

// ============================================================================
// FILTER UTILITIES
// ============================================================================

// Generic biquad filter with multiple filter types
class BiquadFilter {
public:
    enum Type {
        LOWPASS,
        HIGHPASS,
        BANDPASS,
        NOTCH,
        ALLPASS
    };

private:
    float x1 = 0.f, x2 = 0.f;
    float y1 = 0.f, y2 = 0.f;
    float a0 = 1.f, a1 = 0.f, a2 = 0.f;
    float b1 = 0.f, b2 = 0.f;
    
    // Cache for coefficient optimization
    float lastFreq = -1.f, lastQ = -1.f;
    Type lastType = LOWPASS;

public:
    void reset() {
        x1 = x2 = y1 = y2 = 0.f;
    }
    
    float process(float input) {
        float output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        
        // Stability check
        if (!std::isfinite(output) || std::abs(output) > 10000.f) {
            reset();
            return input; // Pass through when unstable
        }
        
        x2 = x1; x1 = input;
        y2 = y1; y1 = output;
        
        return output;
    }
    
    void setParameters(Type type, float freq, float Q, float sampleRate) {
        // Only recalculate if parameters changed significantly
        const float epsilon = 1e-6f;
        if (type == lastType && 
            std::abs(freq - lastFreq) < epsilon && 
            std::abs(Q - lastQ) < epsilon) {
            return;
        }
        
        lastType = type;
        lastFreq = freq;
        lastQ = Q;
        
        freq = clamp(freq, 1.f, sampleRate * 0.49f);
        Q = clamp(Q, 0.1f, 30.f);
        
        float omega = 2.f * M_PI * freq / sampleRate;
        float sin_omega = std::sin(omega);
        float cos_omega = std::cos(omega);
        float alpha = sin_omega / (2.f * Q);
        
        float norm = 1.f / (1.f + alpha);
        
        switch (type) {
            case LOWPASS:
                a0 = ((1.f - cos_omega) / 2.f) * norm;
                a1 = (1.f - cos_omega) * norm;
                a2 = a0;
                break;
                
            case HIGHPASS:
                a0 = ((1.f + cos_omega) / 2.f) * norm;
                a1 = -(1.f + cos_omega) * norm;
                a2 = a0;
                break;
                
            case BANDPASS:
                a0 = alpha * norm;
                a1 = 0.f;
                a2 = -alpha * norm;
                break;
                
            case NOTCH:
                a0 = norm;
                a1 = -2.f * cos_omega * norm;
                a2 = norm;
                break;
                
            case ALLPASS:
                a0 = (1.f - alpha) * norm;
                a1 = -2.f * cos_omega * norm;
                a2 = (1.f + alpha) * norm;
                break;
        }
        
        b1 = (-2.f * cos_omega) * norm;
        b2 = (1.f - alpha) * norm;
    }
};

// Morphing filter that blends between filter types
class MorphingFilter : public BiquadFilter {
public:
    void setMorphingParameters(float freq, float Q, float morph, float sampleRate) {
        morph = clamp(morph, 0.f, 1.f);
        
        // Create two filters and blend their coefficients
        BiquadFilter lp, bp, hp;
        lp.setParameters(LOWPASS, freq, Q, sampleRate);
        bp.setParameters(BANDPASS, freq, Q, sampleRate);
        hp.setParameters(HIGHPASS, freq, Q, sampleRate);
        
        // For simplicity, use discrete switching based on morph parameter
        // Future enhancement: implement smooth coefficient blending
        if (morph < 0.33f) {
            setParameters(LOWPASS, freq, Q, sampleRate);
        } else if (morph < 0.67f) {
            setParameters(BANDPASS, freq, Q, sampleRate);
        } else {
            setParameters(HIGHPASS, freq, Q, sampleRate);
        }
    }
};

// ============================================================================
// PARAMETER SMOOTHING
// ============================================================================

class ParameterSmoother {
private:
    float value = 0.f;
    bool initialized = false;
    float timeConstant = 0.001f; // 1ms default

public:
    ParameterSmoother(float timeConstant = 0.001f) : timeConstant(timeConstant) {}
    
    void setTimeConstant(float tc) {
        timeConstant = tc;
    }
    
    float process(float target, float sampleTime) {
        if (!initialized) {
            value = target;
            initialized = true;
            return value;
        }
        
        float alpha = sampleTime / (timeConstant + sampleTime);
        value += alpha * (target - value);
        return value;
    }
    
    void reset(float initialValue = 0.f) {
        value = initialValue;
        initialized = false;
    }
    
    float getValue() const {
        return value;
    }
};

// ============================================================================
// LED/LIGHTING UTILITIES
// ============================================================================

struct RGBColor {
    float r, g, b;
    
    RGBColor(float r = 0.f, float g = 0.f, float b = 0.f) : r(r), g(g), b(b) {}
};

class LightingHelper {
public:
    // Chiaroscuro-style color progression: Teal -> Bright Blue-Purple -> Dark Purple
    static RGBColor getChiaroscuroColor(float value, float baseBrightness = 0.6f) {
        value = clamp(value, 0.f, 1.f);
        float maxBrightness = baseBrightness;
        
        if (value <= 0.5f) {
            // 0 to 0.5: Teal to bright blue-purple
            float red = value * 2.0f * maxBrightness;
            float green = maxBrightness;
            float blue = maxBrightness;
            return RGBColor(red, green, blue);
        } else {
            // 0.5 to 1.0: Bright blue-purple to dark purple
            float red = maxBrightness;
            float green = 2.0f * (1.0f - value) * maxBrightness;
            float blue = maxBrightness * (1.7f - value * 0.7f);
            return RGBColor(red, green, blue);
        }
    }
    
    // Set RGB lights on a module
    static void setRGBLight(Module* module, int lightId, const RGBColor& color) {
        if (module) {
            module->lights[lightId].setBrightness(color.r);
            module->lights[lightId + 1].setBrightness(color.g);
            module->lights[lightId + 2].setBrightness(color.b);
        }
    }
    
    // VU meter color progression
    static RGBColor getVUColor(float level) {
        level = clamp(level, 0.f, 1.f);
        if (level < 0.7f) {
            return RGBColor(0.f, level / 0.7f, 0.f); // Green
        } else if (level < 0.9f) {
            float blend = (level - 0.7f) / 0.2f;
            return RGBColor(blend, 1.f, 0.f); // Green to yellow
        } else {
            float blend = (level - 0.9f) / 0.1f;
            return RGBColor(1.f, 1.f - blend, 0.f); // Yellow to red
        }
    }
};

// ============================================================================
// POLYPHONIC UTILITIES
// ============================================================================

class PolyphonicHelper {
public:
    // Get channel count with maximum limit
    static int getChannelCount(Input& input, int maxChannels = 16) {
        return std::min(std::max(input.getChannels(), 1), maxChannels);
    }
    
    // Set up output channels to match input or specified count
    static void setupOutputChannels(Output& output, int channels) {
        output.setChannels(channels);
    }
    
    // Process polyphonic CV with fallback and clamping
    static float getPolyCV(Input& input, int channel, float scale = 1.f, 
                          float min = -10.f, float max = 10.f) {
        if (!input.isConnected()) return 0.f;
        float cv = input.getPolyVoltage(channel) * scale;
        return clamp(cv, min, max);
    }
};

// ============================================================================
// CV PROCESSING UTILITIES
// ============================================================================

class CVProcessor {
public:
    // Process CV with attenuverter
    static float processAttenuverter(Input& cvInput, float attenuverterValue, 
                                   float scale = 0.1f, int channel = 0) {
        if (!cvInput.isConnected()) return 0.f;
        float cv = cvInput.getPolyVoltage(channel) * scale;
        return cv * attenuverterValue;
    }
    
    // Process parameter with CV modulation
    static float processParameter(float baseParam, Input& cvInput, 
                                float attenuverter, float scale = 0.1f, 
                                float min = 0.f, float max = 1.f, int channel = 0) {
        float result = baseParam;
        if (cvInput.isConnected()) {
            result += processAttenuverter(cvInput, attenuverter, scale, channel);
        }
        return clamp(result, min, max);
    }
    
    // Quantize voltage to semitones (for musical applications)
    static float quantizeToSemitones(float voltage) {
        return std::round(voltage * 12.0f) / 12.0f;
    }
    
    // Convert parameter to frequency with exponential scaling
    static float paramToFrequency(float param, float baseFreq = 20.f, float octaves = 10.f) {
        return baseFreq * std::pow(2.f, param * octaves);
    }
};

// ============================================================================
// OSCILLATOR UTILITIES
// ============================================================================

class OscillatorHelper {
public:
    // Increment phase with wraparound
    static void incrementPhase(float& phase, float frequency, float sampleTime) {
        phase += frequency * sampleTime;
        if (phase >= 1.f) {
            phase -= 1.f;
        } else if (phase < 0.f) {
            phase += 1.f;
        }
    }
    
    // Sync phases
    static void syncPhase(float& slavePhase, float masterPhase) {
        slavePhase = masterPhase;
    }
    
    // Basic waveforms
    static float sine(float phase) {
        return std::sin(2.f * M_PI * phase);
    }
    
    static float triangle(float phase) {
        return (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
    }
    
    static float sawtooth(float phase) {
        return 2.f * phase - 1.f;
    }
    
    static float square(float phase, float pulseWidth = 0.5f) {
        return (phase < pulseWidth) ? 1.f : -1.f;
    }
    
    // Anti-aliasing using simple one-pole lowpass
    static float antiAlias(float input, float& z1, float cutoff, float sampleRate) {
        float dt = 1.f / sampleRate;
        float RC = 1.f / (2.f * M_PI * cutoff);
        float alpha = dt / (RC + dt);
        z1 += alpha * (input - z1);
        return z1;
    }
};

// ============================================================================
// TRIGGER/GATE UTILITIES
// ============================================================================

class TriggerHelper {
public:
    // Process multiple trigger sources (button + CV input)
    static bool processTrigger(dsp::SchmittTrigger& trigger, float buttonValue, 
                              Input& cvInput, float threshold = 1.f) {
        float totalValue = buttonValue;
        if (cvInput.isConnected()) {
            totalValue += cvInput.getVoltage();
        }
        return trigger.process(totalValue, threshold);
    }
    
    // Process toggle button with trigger
    static bool processToggle(dsp::SchmittTrigger& trigger, float buttonValue, 
                             bool& toggleState) {
        if (trigger.process(buttonValue)) {
            toggleState = !toggleState;
            return true;
        }
        return false;
    }
};

// ============================================================================
// ENVELOPE UTILITIES
// ============================================================================

class EnvelopeGenerator {
private:
    float phase = 0.f;
    float attackTime = 0.1f;
    float decayTime = 0.5f;
    float sustainLevel = 0.5f;
    float releaseTime = 0.5f;
    
    enum Stage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    Stage stage = IDLE;
    bool gateHigh = false;

public:
    void setADSR(float attack, float decay, float sustain, float release) {
        attackTime = std::max(attack, 0.001f);
        decayTime = std::max(decay, 0.001f);
        sustainLevel = clamp(sustain, 0.f, 1.f);
        releaseTime = std::max(release, 0.001f);
    }
    
    void trigger() {
        if (stage == IDLE || stage == RELEASE) {
            stage = ATTACK;
            phase = 0.f;
        }
        gateHigh = true;
    }
    
    void release() {
        if (stage != IDLE && stage != RELEASE) {
            stage = RELEASE;
            phase = 0.f;
        }
        gateHigh = false;
    }
    
    float process(float sampleTime) {
        switch (stage) {
            case ATTACK:
                phase += sampleTime / attackTime;
                if (phase >= 1.f) {
                    stage = DECAY;
                    phase = 0.f;
                    return 1.f;
                }
                return phase;
                
            case DECAY:
                phase += sampleTime / decayTime;
                if (phase >= 1.f) {
                    stage = gateHigh ? SUSTAIN : RELEASE;
                    phase = 0.f;
                    return sustainLevel;
                }
                return 1.f + (sustainLevel - 1.f) * phase;
                
            case SUSTAIN:
                if (!gateHigh) {
                    stage = RELEASE;
                    phase = 0.f;
                }
                return sustainLevel;
                
            case RELEASE:
                phase += sampleTime / releaseTime;
                if (phase >= 1.f) {
                    stage = IDLE;
                    return 0.f;
                }
                return sustainLevel * (1.f - phase);
                
            default:
                return 0.f;
        }
    }
    
    bool isActive() const {
        return stage != IDLE;
    }
};

// ============================================================================
// AUDIO PROCESSING UTILITIES
// ============================================================================

class AudioProcessor {
public:
    // Soft clipping/saturation
    static float softClip(float input, float threshold = 0.7f) {
        float abs_input = std::abs(input);
        if (abs_input <= threshold) {
            return input;
        } else {
            float sign = (input > 0.f) ? 1.f : -1.f;
            float excess = abs_input - threshold;
            float compressed = threshold + excess / (1.f + excess);
            return sign * compressed;
        }
    }
    
    // Crossfade between two signals
    static float crossfade(float signalA, float signalB, float crossfadeAmount) {
        crossfadeAmount = clamp(crossfadeAmount, 0.f, 1.f);
        return signalA * (1.f - crossfadeAmount) + signalB * crossfadeAmount;
    }
    
    // Simple DC blocking filter
    static float dcBlock(float input, float& z1, float cutoff = 20.f, float sampleRate = 44100.f) {
        float alpha = 1.f / (1.f + sampleRate / (2.f * M_PI * cutoff));
        float output = input - z1 + alpha * z1;
        z1 = input;
        return output;
    }
    
    // Level compensation based on gain
    static float levelCompensation(float input, float gain) {
        if (gain > 1.f) {
            return input / std::sqrt(gain);
        }
        return input;
    }
};

} // namespace shapetaker