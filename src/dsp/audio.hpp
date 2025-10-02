#pragma once
#include <rack.hpp>
#include <cmath>

using namespace rack;

namespace shapetaker {
namespace dsp {

// ============================================================================
// AUDIO PROCESSING UTILITIES
// ============================================================================

class AudioProcessor {
public:
    // Crossfade between two signals
    static float crossfade(float a, float b, float mix) {
        mix = rack::math::clamp(mix, 0.f, 1.f);
        return a * (1.f - mix) + b * mix;
    }
    
    // Stereo crossfade maintaining constant power
    static void stereoConstantPowerCrossfade(float aL, float aR, float bL, float bR, 
                                           float mix, float& outL, float& outR) {
        mix = rack::math::clamp(mix, 0.f, 1.f);
        float fadeA = std::cos(mix * M_PI * 0.5f);
        float fadeB = std::sin(mix * M_PI * 0.5f);
        outL = aL * fadeA + bL * fadeB;
        outR = aR * fadeA + bR * fadeB;
    }
    
    // Soft clipping with various curves
    static float softClip(float input, float drive = 1.f) {
        input *= drive;
        return std::tanh(input) / std::tanh(drive);
    }

    static float softLimit(float input, float limit = 10.f) {
        if (limit <= 0.f) {
            return 0.f;
        }
        float scaled = input / limit;
        return limit * std::tanh(scaled);
    }
    
    static float asymmetricClip(float input, float drive = 1.f) {
        input *= drive;
        if (input > 0.f) {
            return input / (1.f + input);
        } else {
            return input / (1.f - input * 0.5f);
        }
    }
    
    // DC blocking filter
    static float processDCBlock(float input, float& lastInput, float& lastOutput, 
                               float coefficient = 0.995f) {
        float output = input - lastInput + coefficient * lastOutput;
        lastInput = input;
        lastOutput = output;
        return output;
    }
    
    // Simple low-pass filter for smoothing
    static float lowPass(float input, float& state, float cutoff) {
        cutoff = rack::math::clamp(cutoff, 0.001f, 0.999f);
        state = input * cutoff + state * (1.f - cutoff);
        return state;
    }
    
    // Simple high-pass filter
    static float highPass(float input, float& lastInput, float& lastOutput, float cutoff) {
        cutoff = rack::math::clamp(cutoff, 0.001f, 0.999f);
        float output = cutoff * (lastOutput + input - lastInput);
        lastInput = input;
        lastOutput = output;
        return output;
    }
    
    // Ring modulation
    static float ringMod(float carrier, float modulator) {
        return carrier * modulator;
    }
    
    // AM modulation
    static float amplitudeModulate(float carrier, float modulator, float depth = 1.f) {
        return carrier * (1.f + depth * modulator);
    }
    
    // Simple delay line for short delays/chorus
    template<int MAX_DELAY>
    class DelayLine {
    private:
        float buffer[MAX_DELAY] = {};
        int writePos = 0;
        
    public:
        float process(float input, int delaySamples) {
            delaySamples = rack::math::clamp(delaySamples, 0, MAX_DELAY - 1);
            int readPos = (writePos - delaySamples + MAX_DELAY) % MAX_DELAY;
            float output = buffer[readPos];
            buffer[writePos] = input;
            writePos = (writePos + 1) % MAX_DELAY;
            return output;
        }
        
        void clear() {
            for (int i = 0; i < MAX_DELAY; i++) {
                buffer[i] = 0.f;
            }
            writePos = 0;
        }
    };
};

}} // namespace shapetaker::dsp
