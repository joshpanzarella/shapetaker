#pragma once

#include <rack.hpp>
#include <cmath>
#include <cstring>

namespace shapetaker {
namespace reverie {

// Double-buffered reverse grain processor
// Captures audio into one buffer while playing the other back in reverse
// Crossfades at buffer boundaries to prevent clicks
class ReverseGrainBuffer {
private:
    static const int MAX_GRAIN = 24000; // ~500ms at 48kHz
    float* bufferA;
    float* bufferB;
    int grainSize; // current grain size in samples
    int writePos;  // write position in active buffer
    int readPos;   // read position in playback buffer (counts down)
    bool writeToA; // which buffer is currently being written to
    bool initialized;

    // Crossfade at boundaries
    static const int CROSSFADE_SAMPLES = 128;

    void allocate() {
        if (bufferA) return;
        bufferA = new float[MAX_GRAIN]();
        bufferB = new float[MAX_GRAIN]();
        initialized = true;
    }

public:
    ReverseGrainBuffer() : bufferA(NULL), bufferB(NULL), initialized(false) {
        grainSize = 4800; // 100ms default
        writePos = 0;
        readPos = 0;
        writeToA = true;
    }

    ~ReverseGrainBuffer() {
        delete[] bufferA;
        delete[] bufferB;
    }

    ReverseGrainBuffer(const ReverseGrainBuffer&) : bufferA(NULL), bufferB(NULL), initialized(false) {
        grainSize = 4800;
        writePos = 0;
        readPos = 0;
        writeToA = true;
    }

    ReverseGrainBuffer& operator=(const ReverseGrainBuffer&) {
        return *this;
    }

    void setSampleRate(float sampleRate) {
        allocate();
        // Default grain size 100ms
        grainSize = (int)(sampleRate * 0.1f);
        if (grainSize > MAX_GRAIN) grainSize = MAX_GRAIN;
        if (grainSize < 256) grainSize = 256;
    }

    // Set window size as 0..1 parameter (maps to 50ms - 500ms)
    void setWindowSize(float param, float sampleRate) {
        float timeMs = 50.0f + param * 450.0f; // 50ms to 500ms
        grainSize = (int)(sampleRate * timeMs * 0.001f);
        if (grainSize > MAX_GRAIN) grainSize = MAX_GRAIN;
        if (grainSize < 256) grainSize = 256;
    }

    void reset() {
        if (bufferA) std::memset(bufferA, 0, MAX_GRAIN * sizeof(float));
        if (bufferB) std::memset(bufferB, 0, MAX_GRAIN * sizeof(float));
        writePos = 0;
        readPos = 0;
        writeToA = true;
    }

    float process(float input) {
        if (!initialized) return 0.0f;
        // Write to active buffer
        float* writeBuffer = writeToA ? bufferA : bufferB;
        float* readBuffer = writeToA ? bufferB : bufferA;

        writeBuffer[writePos] = input;

        // Read from playback buffer in reverse
        float output = 0.0f;
        if (readPos >= 0 && readPos < grainSize) {
            output = readBuffer[readPos];

            // Apply crossfade envelope at boundaries
            if (readPos < CROSSFADE_SAMPLES) {
                float fade = (float)readPos / (float)CROSSFADE_SAMPLES;
                output *= fade;
            } else if (readPos > grainSize - CROSSFADE_SAMPLES) {
                float fade = (float)(grainSize - readPos) / (float)CROSSFADE_SAMPLES;
                output *= fade;
            }
        }

        // Advance positions
        writePos++;
        readPos--;

        // When write buffer is full, swap
        if (writePos >= grainSize) {
            writeToA = !writeToA;
            writePos = 0;
            readPos = grainSize - 1;
        }

        return output;
    }
};

} // namespace reverie
} // namespace shapetaker
