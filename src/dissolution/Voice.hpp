#pragma once

#include "DegradationEngine.hpp"
#include "Envelope.hpp"
#include "RingBuffer.hpp"
#include <rack.hpp>
#include <cmath>

namespace shapetaker {
namespace dissolution {

class Voice {
public:
    enum class State {
        IDLE,
        RECORDING,
        FROZEN,
        FADING,
        RELEASED
    };

    Voice() {
        setSampleRate(sampleRate);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.f, sr);
        envelope.setSampleRate(sampleRate);
        degradationEngine.setSampleRate(sampleRate);
        updateCrossfadeSamples();
    }

    void setAttackTime(float seconds) {
        attackTime = rack::math::clamp(seconds, 0.0005f, 0.1f);
        envelope.setAttackTime(attackTime);
    }

    void setFadeTime(float seconds) {
        fadeTime = rack::math::clamp(seconds, 0.001f, 20.f);
        envelope.setReleaseTime(fadeTime);
    }

    void setCrossfadeTime(float seconds) {
        crossfadeSeconds = rack::math::clamp(seconds, 0.f, 0.1f);
        updateCrossfadeSamples();
    }

    void setWowFlutter(float amount) {
        degradationEngine.setWowFlutter(amount);
    }

    void setLoFi(float amount) {
        degradationEngine.setLoFi(amount);
    }

    void setFilterSweep(float amount) {
        degradationEngine.setFilterSweep(amount);
    }

    void setSaturation(float amount) {
        degradationEngine.setSaturation(amount);
    }

    void setNoise(float amount) {
        degradationEngine.setNoise(amount);
    }

    void setLevel(float value) {
        level = rack::math::clamp(value, 0.f, 1.f);
    }

    float getLevel() const {
        return level;
    }

    void allocateBuffer(size_t samples) {
        buffer.allocate(samples);
    }

    void trigger(float nowSeconds) {
        state = State::RECORDING;
        startTime = nowSeconds;
        freezeTime = 0.f;
        playbackPos = 0.f;
        frozenLength = 0;
        lastOutput = 0.f;
        frozenHoldSample = 0.f;
        pendingFreeze = false;
        pendingFade = false;
        buffer.reset();
        degradationEngine.reset();
        envelope.reset();
        envelope.setAttackTime(attackTime);
        envelope.setReleaseTime(fadeTime);
        envelope.trigger();
    }

    void requestFreeze() {
        pendingFreeze = true;
    }

    void fade() {
        if (state == State::FROZEN) {
            beginFade();
        } else if (state == State::RECORDING) {
            pendingFade = true;
            pendingFreeze = true;
        }
    }

    void reset() {
        state = State::IDLE;
        startTime = 0.f;
        freezeTime = 0.f;
        playbackPos = 0.f;
        frozenLength = 0;
        lastOutput = 0.f;
        frozenHoldSample = 0.f;
        pendingFreeze = false;
        pendingFade = false;
        buffer.reset();
        envelope.reset();
        degradationEngine.reset();
    }

    bool isActive() const {
        return state == State::RECORDING || state == State::FROZEN || state == State::FADING;
    }

    State getState() const {
        return state;
    }

    float getStartTime() const {
        return startTime;
    }

    float getLastOutput() const {
        return lastOutput;
    }

    float process(float input,
                  float nowSeconds,
                  float degradationTime,
                  float sustainTime,
                  DegradationStyle style,
                  float degradeMix,
                  float sampleTime) {
        float output = 0.f;
        degradeMix = rack::math::clamp(degradeMix, 0.f, 1.f);

        switch (state) {
            case State::IDLE:
                lastOutput = 0.f;
                break;

            case State::RECORDING: {
                float elapsed = nowSeconds - startTime;
                float progress = 1.f;
                if (degradationTime > 1e-5f) {
                    progress = rack::math::clamp(elapsed / degradationTime, 0.f, 1.f);
                }

                float degraded = degradationEngine.process(input, progress, style, sampleTime);
                float mixed = rack::math::crossfade(input, degraded, degradeMix);

                buffer.write(mixed);

                float env = envelope.process();
                output = mixed * env;
                lastOutput = output;

                if (pendingFreeze || progress >= 1.f) {
                    freeze(nowSeconds);
                    if (pendingFade && state == State::FROZEN) {
                        beginFade();
                    }
                }
                break;
            }

            case State::FROZEN: {
                if (frozenLength == 0) {
                    beginFade();
                    output = 0.f;
                    lastOutput = 0.f;
                    break;
                }

                // Hold the frozen sample value like an envelope sustain stage
                // The gate is "open" but the level stays constant
                output = frozenHoldSample;
                lastOutput = output;

                if (pendingFade) {
                    beginFade();
                } else if (sustainTime > 0.f) {
                    float elapsed = nowSeconds - freezeTime;
                    if (elapsed >= sustainTime) {
                        beginFade();
                    }
                }
                break;
            }

            case State::FADING: {
                if (frozenLength == 0) {
                    envelope.release();
                    if (envelope.isComplete()) {
                        state = State::RELEASED;
                    }
                    lastOutput = 0.f;
                    output = 0.f;
                    break;
                }

                // During fade, continue holding the frozen sample value
                // and apply the envelope fade-out (like an envelope release stage)
                float env = envelope.process();
                output = frozenHoldSample * env;
                lastOutput = output;

                if (envelope.isComplete()) {
                    state = State::RELEASED;
                }
                break;
            }

            case State::RELEASED:
                reset();
                lastOutput = 0.f;
                break;
        }

        return lastOutput * level;
    }

private:
    void freeze(float nowSeconds) {
        pendingFreeze = false;

        frozenLength = buffer.getRecordedLength();
        if (frozenLength == 0) {
            if (buffer.getCapacity() > 0) {
                buffer.write(lastOutput);
                frozenLength = buffer.getRecordedLength();
            }
            if (frozenLength == 0) {
                state = State::RELEASED;
                return;
            }
        }

        // Capture the final sample value to hold during sustain
        // Like an envelope sustain stage, we hold the last captured level
        frozenHoldSample = lastOutput;

        state = State::FROZEN;
        freezeTime = nowSeconds;
        playbackPos = 0.f;
        envelope.forceSustain();
    }

    void beginFade() {
        pendingFade = false;
        if (state == State::FROZEN) {
            state = State::FADING;
            envelope.release();
        } else if (state == State::RECORDING) {
            pendingFreeze = true;
        }
    }

    void advancePlayback() {
        if (frozenLength == 0) {
            playbackPos = 0.f;
            return;
        }

        playbackPos += 1.f;
        if (playbackPos >= static_cast<float>(frozenLength)) {
            playbackPos = std::fmod(playbackPos, static_cast<float>(frozenLength));
        }
    }

    float readLooped(float position) const {
        if (frozenLength == 0) {
            return lastOutput;
        }

        const float length = static_cast<float>(frozenLength);
        float pos = position;
        pos = std::fmod(pos, length);
        if (pos < 0.f) {
            pos += length;
        }

        size_t i0 = static_cast<size_t>(pos);
        size_t i1 = (i0 + 1) % frozenLength;
        float frac = pos - static_cast<float>(i0);
        float sample = rack::math::crossfade(buffer.read(i0), buffer.read(i1), rack::math::clamp(frac, 0.f, 1.f));

        if (crossfadeSamples > 0 && frozenLength > crossfadeSamples + 2) {
            const float startZone = length - static_cast<float>(crossfadeSamples);
            if (pos >= startZone) {
                float t = (pos - startZone) / static_cast<float>(crossfadeSamples);
                float startSample = readWrapped(position - length);
                sample = rack::math::crossfade(sample, startSample, rack::math::clamp(t, 0.f, 1.f));
            }
        }

        return sample;
    }

    float readWrapped(float position) const {
        if (frozenLength == 0) {
            return lastOutput;
        }

        const float length = static_cast<float>(frozenLength);
        float pos = std::fmod(position, length);
        if (pos < 0.f) {
            pos += length;
        }

        size_t i0 = static_cast<size_t>(pos);
        size_t i1 = (i0 + 1) % frozenLength;
        float frac = pos - static_cast<float>(i0);
        return rack::math::crossfade(buffer.read(i0), buffer.read(i1), rack::math::clamp(frac, 0.f, 1.f));
    }

    void updateCrossfadeSamples() {
        if (crossfadeSeconds <= 0.f) {
            crossfadeSamples = 0;
            return;
        }
        float target = crossfadeSeconds * sampleRate;
        crossfadeSamples = static_cast<size_t>(std::max(1.f, std::round(target)));
    }

    State state = State::IDLE;
    float startTime = 0.f;
    float freezeTime = 0.f;
    float playbackPos = 0.f;
    size_t frozenLength = 0;
    float level = 1.f;
    float lastOutput = 0.f;

    float sampleRate = 44100.f;
    float attackTime = 0.01f;
    float fadeTime = 1.f;
    float crossfadeSeconds = 0.015f;
    size_t crossfadeSamples = 0;

    bool pendingFreeze = false;
    bool pendingFade = false;

    float frozenHoldSample = 0.f;

    RingBuffer buffer;
    Envelope envelope;
    DegradationEngine degradationEngine;
};

} // namespace dissolution
} // namespace shapetaker
