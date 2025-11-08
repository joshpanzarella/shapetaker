#pragma once

#include <rack.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace shapetaker {
namespace dissolution {

enum class DegradationStyle {
    TAPE,      // Heavy wow/flutter, warm filtering
    DIGITAL,   // Bit crushing, aliasing
    AMBIENT,   // Subtle, smooth degradation
    CHAOS,     // Randomized, aggressive effects
    COUNT
};

class DegradationEngine {
public:
    DegradationEngine() {
        setSampleRate(44100.f);
        reset();
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.f, sr);
        const int wowBufferSamples = std::max(32, static_cast<int>(sampleRate * maxWowSeconds));
        wowBuffer.assign(static_cast<size_t>(wowBufferSamples), 0.f);
        wowWriteIndex = 0;
    }

    void setWowFlutter(float amount) {
        wowAmount = rack::math::clamp(amount, 0.f, 1.f);
    }

    void setLoFi(float amount) {
        lofiAmount = rack::math::clamp(amount, 0.f, 1.f);
    }

    void setFilterSweep(float amount) {
        filterAmount = rack::math::clamp(amount, 0.f, 1.f);
    }

    void setSaturation(float amount) {
        saturationAmount = rack::math::clamp(amount, 0.f, 1.f);
    }

    void setNoise(float amount) {
        noiseAmount = rack::math::clamp(amount, 0.f, 1.f);
    }

    float process(float input, float progress, DegradationStyle style, float sampleTime) {
        progress = rack::math::clamp(progress, 0.f, 1.f);
        const StyleProfile profile = getProfile(style);

        const float wowIntensity = rack::math::clamp(progress * wowAmount * profile.wowWeight, 0.f, 1.2f);
        const float lofiIntensity = rack::math::clamp(progress * lofiAmount * profile.lofiWeight, 0.f, 1.2f);
        const float filterIntensity = rack::math::clamp(progress * filterAmount * profile.filterWeight, 0.f, 1.2f);
        const float saturationIntensity = rack::math::clamp(progress * saturationAmount * profile.saturationWeight, 0.f, 1.5f);
        const float noiseIntensity = rack::math::clamp(progress * noiseAmount * profile.noiseWeight, 0.f, 1.5f);

        float signal = input;

        signal = applyWowFlutter(signal, wowIntensity, profile, sampleTime);
        signal = applySaturation(signal, saturationIntensity, profile);
        signal = applyFilter(signal, filterIntensity, profile, sampleTime);
        signal = applyLoFi(signal, lofiIntensity, style);
        signal = applyNoise(signal, noiseIntensity, profile, sampleTime);

        if (style == DegradationStyle::CHAOS) {
            chaosPhase += sampleTime * 1.5f;
            if (chaosPhase > 1.f) {
                chaosPhase -= 1.f;
                // occasional abrupt flutter resets for chaos flavour
                wowPhase = rack::random::uniform();
                flutterPhase = rack::random::uniform();
            }
        }

        lastOutput = rack::math::clamp(signal, -12.f, 12.f);
        return lastOutput;
    }

    void reset() {
        wowPhase = 0.f;
        flutterPhase = 0.f;
        sampleHold = 0.f;
        decimationCounter = 0;
        lowpassState = 0.f;
        highpassState = 0.f;
        noiseHighpassState = 0.f;
        prevFilterInput = 0.f;
        prevNoiseInput = 0.f;
        lastOutput = 0.f;
        chaosPhase = 0.f;
        if (!wowBuffer.empty()) {
            std::fill(wowBuffer.begin(), wowBuffer.end(), 0.f);
        }
        wowWriteIndex = 0;
    }

private:
    struct StyleProfile {
        float wowWeight;
        float flutterWeight;
        float lofiWeight;
        float filterWeight;
        float saturationWeight;
        float noiseWeight;
        float wowRateHz;
        float flutterRateHz;
        float wowDepthSeconds;
        float flutterDepthSeconds;
        float saturationTrim;
        float noiseColor;
    };

    static constexpr float maxWowSeconds = 0.02f; // 20 ms buffer
    static constexpr float TWO_PI = 6.28318530717958647692f;

    float sampleRate = 44100.f;
    float wowAmount = 0.f;
    float lofiAmount = 0.f;
    float filterAmount = 0.f;
    float saturationAmount = 0.f;
    float noiseAmount = 0.f;

    float wowPhase = 0.f;
    float flutterPhase = 0.f;
    float sampleHold = 0.f;
    int decimationCounter = 0;

    float lowpassState = 0.f;
    float highpassState = 0.f;
    float noiseHighpassState = 0.f;
    float prevFilterInput = 0.f;
    float prevNoiseInput = 0.f;
    float lastOutput = 0.f;
    float chaosPhase = 0.f;

    std::vector<float> wowBuffer;
    int wowWriteIndex = 0;

    StyleProfile getProfile(DegradationStyle style) const {
        switch (style) {
            case DegradationStyle::DIGITAL:
                return StyleProfile{
                    0.25f, // wowWeight
                    0.35f, // flutterWeight
                    1.15f, // lofiWeight
                    0.45f, // filterWeight
                    0.55f, // saturationWeight
                    0.35f, // noiseWeight
                    0.35f, // wowRateHz
                    5.8f,  // flutterRateHz
                    0.0025f, // wowDepthSeconds
                    0.0008f, // flutterDepthSeconds
                    0.85f, // saturationTrim
                    0.2f   // noiseColor
                };
            case DegradationStyle::AMBIENT:
                return StyleProfile{
                    0.4f,
                    0.4f,
                    0.35f,
                    1.15f,
                    0.45f,
                    0.25f,
                    0.22f,
                    1.6f,
                    0.0035f,
                    0.0010f,
                    0.95f,
                    0.1f
                };
            case DegradationStyle::CHAOS:
                return StyleProfile{
                    0.8f,
                    1.1f,
                    1.25f,
                    0.65f,
                    1.25f,
                    0.95f,
                    0.65f,
                    7.5f,
                    0.0065f,
                    0.0035f,
                    0.8f,
                    0.6f
                };
            case DegradationStyle::TAPE:
            default:
                return StyleProfile{
                    1.0f,
                    0.75f,
                    0.25f,
                    0.9f,
                    1.15f,
                    0.6f,
                    0.32f,
                    3.2f,
                    0.0055f,
                    0.0018f,
                    0.92f,
                    0.4f
                };
        }
    }

    float applyWowFlutter(float input, float intensity, const StyleProfile& profile, float sampleTime) {
        if (wowBuffer.empty()) {
            return input;
        }

        wowBuffer[static_cast<size_t>(wowWriteIndex)] = input;
        wowWriteIndex = (wowWriteIndex + 1) % static_cast<int>(wowBuffer.size());

        if (intensity <= 1e-5f) {
            return input;
        }

        wowPhase += profile.wowRateHz * sampleTime;
        if (wowPhase >= 1.f) {
            wowPhase -= std::floor(wowPhase);
        }

        flutterPhase += profile.flutterRateHz * sampleTime;
        if (flutterPhase >= 1.f) {
            flutterPhase -= std::floor(flutterPhase);
        }

        const float wowLfo = std::sin(TWO_PI * wowPhase);
        const float flutterLfo = std::sin(TWO_PI * flutterPhase);

        const float wowDepthSamples = intensity * profile.wowDepthSeconds * sampleRate;
        const float flutterDepthSamples = intensity * profile.flutterDepthSeconds * sampleRate * profile.flutterWeight;

        float totalDelay = wowDepthSamples * (0.5f * (wowLfo + 1.f));
        totalDelay += flutterDepthSamples * (0.5f * (flutterLfo + 1.f));
        float maxDelay = static_cast<float>(std::max<int>(0, static_cast<int>(wowBuffer.size()) - 2));
        totalDelay = rack::math::clamp(totalDelay, 0.f, maxDelay);

        float readPos = static_cast<float>(wowWriteIndex) - totalDelay;
        const float bufferSize = static_cast<float>(wowBuffer.size());
        while (readPos < 0.f) {
            readPos += bufferSize;
        }

        const int index0 = static_cast<int>(readPos) % static_cast<int>(bufferSize);
        const int index1 = (index0 + 1) % static_cast<int>(bufferSize);
        const float frac = readPos - std::floor(readPos);
        const float delayed = rack::math::crossfade(
            wowBuffer[static_cast<size_t>(index0)],
            wowBuffer[static_cast<size_t>(index1)],
            frac);

        const float mix = rack::math::clamp(intensity * 0.85f + 0.1f * profile.wowWeight, 0.f, 1.f);
        return rack::math::crossfade(input, delayed, mix);
    }

    float applySaturation(float input, float intensity, const StyleProfile& profile) {
        if (intensity <= 1e-5f) {
            return input;
        }

        const float drive = 1.f + intensity * 6.f;
        const float driven = std::tanh(input * drive);
        const float norm = std::tanh(drive);
        float saturated = (norm > 1e-5f) ? (driven / norm) : input;
        saturated *= profile.saturationTrim;

        const float mix = rack::math::clamp(intensity * 0.9f, 0.f, 1.f);
        return rack::math::crossfade(input, saturated, mix);
    }

    float applyFilter(float input, float intensity, const StyleProfile& profile, float sampleTime) {
        // keep filter state running softly even if intensity low to avoid zipper
        const float minLowCut = 400.f;
        const float maxLowCut = 18000.f;
        const float lowCut = rack::math::clamp(
            rack::math::crossfade(maxLowCut, minLowCut, rack::math::clamp(intensity, 0.f, 1.f)),
            minLowCut, maxLowCut);

        const float rcLow = 1.f / (TWO_PI * lowCut);
        const float alphaLow = sampleTime / (rcLow + sampleTime);
        lowpassState += alphaLow * (input - lowpassState);

        const float minHighCut = 20.f;
        const float maxHighCut = 650.f;
        const float highCut = rack::math::clamp(
            rack::math::crossfade(minHighCut, maxHighCut,
                rack::math::clamp(intensity * profile.filterWeight, 0.f, 1.f)),
            minHighCut, maxHighCut);
        const float rcHigh = 1.f / (TWO_PI * highCut);
        const float alphaHigh = rcHigh / (rcHigh + sampleTime);
        highpassState = alphaHigh * (highpassState + lowpassState - prevFilterInput);
        prevFilterInput = lowpassState;

        const float scooped = lowpassState - highpassState * 0.5f;
        const float mix = rack::math::clamp(intensity, 0.f, 1.f);
        return rack::math::crossfade(input, scooped, mix);
    }

    float applyLoFi(float input, float intensity, DegradationStyle style) {
        if (intensity <= 1e-5f) {
            decimationCounter = 0;
            sampleHold = input;
            return input;
        }

        const int maxDecimation = (style == DegradationStyle::CHAOS) ? 64 :
                                  (style == DegradationStyle::DIGITAL ? 48 : 24);
        const int decimation = std::max(1, static_cast<int>(std::round(1 + intensity * maxDecimation)));

        decimationCounter++;
        if (decimationCounter >= decimation) {
            decimationCounter = 0;
            sampleHold = input;
        }

        float reduced = sampleHold;

        const float minBits = (style == DegradationStyle::DIGITAL || style == DegradationStyle::CHAOS) ? 4.f : 8.f;
        const float bitsf = rack::math::crossfade(16.f, minBits, rack::math::clamp(intensity, 0.f, 1.f));
        const int bits = rack::math::clamp(static_cast<int>(std::round(bitsf)), 2, 16);
        const float levels = static_cast<float>(1 << bits);
        reduced = std::round(reduced * levels) / levels;

        if (style == DegradationStyle::CHAOS) {
            const float jitter = (rack::random::uniform() * 2.f - 1.f) * intensity * 0.25f;
            reduced += jitter;
        }

        return reduced;
    }

    float applyNoise(float input, float intensity, const StyleProfile& profile, float sampleTime) {
        if (intensity <= 1e-5f) {
            return input;
        }

        float hiss = (rack::random::uniform() * 2.f - 1.f) * (0.02f + intensity * 0.15f);
        hiss *= profile.noiseWeight;

        if (profile.noiseColor > 0.f) {
            // simple one-pole highpass to tilt noise brighter
            const float targetCut = 500.f + 3000.f * profile.noiseColor;
            const float rc = 1.f / (TWO_PI * targetCut);
            const float alpha = rc / (rc + sampleTime);
            noiseHighpassState = alpha * (noiseHighpassState + hiss - prevNoiseInput);
            prevNoiseInput = hiss;
            hiss = noiseHighpassState;
        }

        if (profile.noiseWeight > 0.8f && rack::random::uniform() < intensity * 0.002f) {
            hiss += (rack::random::uniform() * 2.f - 1.f) * 0.4f;
        }

        return input + hiss * intensity;
    }

};

} // namespace dissolution
} // namespace shapetaker
