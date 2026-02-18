#pragma once

#include <rack.hpp>
#include <cmath>
#include <cstring>

namespace shapetaker {
namespace reverie {

// Granular pitch shifter using two overlapping Hann-windowed grains
// Counter-based grain management for click-free crossfades
// Designed for octave shifts (+12 or -12 semitones)
class GranularPitchShifter {
private:
    static const int MAX_BUFFER = 8192; // ~170ms at 48kHz
    float* buffer;
    int writePos;
    int grainSizeSamples;
    float pitchRatio; // 2.0 = +1 octave, 0.5 = -1 octave
    bool initialized;

    // Two grains with deterministic counter-based lifecycle
    int grainAge[2];       // counter: 0 to grainSizeSamples-1
    int grainStartPos[2];  // write position when grain was born

    float hannWindow(float phase) {
        return 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * phase));
    }

    float readInterpolated(float pos) {
        if (!buffer) return 0.0f;
        // Wrap to buffer range
        while (pos >= MAX_BUFFER) pos -= MAX_BUFFER;
        while (pos < 0) pos += MAX_BUFFER;
        int idx1 = (int)pos;
        float frac = pos - (float)idx1;
        idx1 = idx1 % MAX_BUFFER;
        if (idx1 < 0) idx1 += MAX_BUFFER;
        int idx2 = (idx1 + 1) % MAX_BUFFER;
        return buffer[idx1] * (1.0f - frac) + buffer[idx2] * frac;
    }

    void allocate() {
        if (buffer) return;
        buffer = new float[MAX_BUFFER]();
        initialized = true;
    }

    void resetGrain(int idx) {
        grainAge[idx] = 0;
        // Start reading from grainSizeSamples behind the write head
        grainStartPos[idx] = writePos - grainSizeSamples;
        if (grainStartPos[idx] < 0) grainStartPos[idx] += MAX_BUFFER;
    }

public:
    GranularPitchShifter() : buffer(NULL), initialized(false) {
        writePos = 0;
        grainSizeSamples = 2048;
        pitchRatio = 2.0f;
        grainAge[0] = 0;
        grainAge[1] = 0;
        grainStartPos[0] = 0;
        grainStartPos[1] = 0;
    }

    ~GranularPitchShifter() {
        delete[] buffer;
    }

    GranularPitchShifter(const GranularPitchShifter&) : buffer(NULL), initialized(false) {
        writePos = 0;
        grainSizeSamples = 2048;
        pitchRatio = 2.0f;
        grainAge[0] = 0;
        grainAge[1] = 0;
        grainStartPos[0] = 0;
        grainStartPos[1] = 0;
    }

    GranularPitchShifter& operator=(const GranularPitchShifter&) {
        return *this;
    }

    void setSampleRate(float sampleRate) {
        allocate();
        // Grain size ~40ms for clean octave shifts
        grainSizeSamples = (int)(sampleRate * 0.04f);
        if (grainSizeSamples < 64) grainSizeSamples = 64;
        if (grainSizeSamples > MAX_BUFFER / 2) grainSizeSamples = MAX_BUFFER / 2;

        // Initialize grain 1 at age 0, grain 2 at half-grain offset
        resetGrain(0);
        grainAge[1] = grainSizeSamples / 2;
        grainStartPos[1] = writePos - grainSizeSamples;
        if (grainStartPos[1] < 0) grainStartPos[1] += MAX_BUFFER;
    }

    void setPitchRatio(float ratio) {
        pitchRatio = ratio;
    }

    void reset() {
        if (buffer) {
            std::memset(buffer, 0, MAX_BUFFER * sizeof(float));
        }
        writePos = 0;
        resetGrain(0);
        grainAge[1] = grainSizeSamples / 2;
        grainStartPos[1] = 0;
    }

    float process(float input) {
        if (!initialized) return 0.0f;

        // Write input to circular buffer
        buffer[writePos] = input;

        float output = 0.0f;

        // Process both grains
        for (int g = 0; g < 2; g++) {
            // Phase 0..1 over grain lifetime
            float phase = (float)grainAge[g] / (float)grainSizeSamples;
            float window = hannWindow(phase);

            // Read position: start + age * pitchRatio
            float readPos = (float)grainStartPos[g] + (float)grainAge[g] * pitchRatio;
            output += readInterpolated(readPos) * window;

            // Advance grain age
            grainAge[g]++;

            // When grain completes its full window, reset it
            if (grainAge[g] >= grainSizeSamples) {
                resetGrain(g);
            }
        }

        // Advance write position
        writePos = (writePos + 1) % MAX_BUFFER;

        return output;
    }
};

} // namespace reverie
} // namespace shapetaker
