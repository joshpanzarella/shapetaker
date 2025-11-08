#pragma once
#include <vector>
#include <cstring>

namespace shapetaker {
namespace dissolution {

// Simple circular buffer for audio recording and playback
class RingBuffer {
public:
    RingBuffer() : buffer(nullptr), capacity(0), writePos(0), size(0) {}

    ~RingBuffer() {
        clear();
    }

    // Allocate buffer with given capacity (in samples)
    void allocate(size_t numSamples) {
        if (numSamples == capacity) {
            return;
        }

        clear();

        if (numSamples > 0) {
            buffer = new float[numSamples];
            std::memset(buffer, 0, numSamples * sizeof(float));
            capacity = numSamples;
        }
    }

    // Free buffer memory
    void clear() {
        if (buffer) {
            delete[] buffer;
            buffer = nullptr;
        }
        capacity = 0;
        writePos = 0;
        size = 0;
    }

    // Write a sample to the buffer
    void write(float sample) {
        if (!buffer || capacity == 0) {
            return;
        }

        buffer[writePos] = sample;
        writePos = (writePos + 1) % capacity;

        if (size < capacity) {
            size++;
        }
    }

    // Read a sample at given position (0 = oldest sample)
    float read(size_t position) const {
        if (!buffer || capacity == 0 || position >= size) {
            return 0.f;
        }

        size_t actualPos = (writePos + capacity - size + position) % capacity;
        return buffer[actualPos];
    }

    // Get current number of samples in buffer
    size_t getSize() const { return size; }

    // Get buffer capacity
    size_t getCapacity() const { return capacity; }

    // Reset write position and size (keep buffer allocated)
    void reset() {
        writePos = 0;
        size = 0;
        if (buffer) {
            std::memset(buffer, 0, capacity * sizeof(float));
        }
    }

    // Get the recorded length (for freezing)
    size_t getRecordedLength() const {
        return size;
    }

private:
    float* buffer;
    size_t capacity;
    size_t writePos;
    size_t size;
};

} // namespace dissolution
} // namespace shapetaker
