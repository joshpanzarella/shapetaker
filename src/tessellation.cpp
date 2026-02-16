#include "plugin.hpp"
#include "componentlibrary.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace tessellation {
    constexpr float MIN_DELAY_SECONDS = 0.02f;
    constexpr float MAX_DELAY_SECONDS = 1.6f;
    constexpr float DEFAULT_DELAY_SECONDS = 0.35f;
    constexpr float MAX_MOD_DEPTH_SECONDS = 0.02f; // 20 ms swing
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 6.28318530717958647692f;
    constexpr float TAP_RESET_SECONDS = 2.5f;

    inline float subdivisionMultiplier(int index) {
        switch (index) {
            case 0: return 1.f / 3.f;          // Triplet
            case 1: return 0.5f;               // Eighth
            case 2: return 1.f / 1.618034f;    // Golden ratio
            case 3: return 0.75f;              // Dotted eighth
            case 4: return 1.5f;               // Dotted quarter
            default: return 1.f;               // Free / manual
        }
    }
}

// Human-readable subdivision names for tooltips
inline const char* subdivisionName(int index) {
    switch (index) {
        case 0: return "Triplet";
        case 1: return "Eighth";
        case 2: return "Golden";
        case 3: return "Dotted 8th";
        case 4: return "Dotted Quarter";
        default: return "Free";
    }
}

struct Tessellation;

// Custom ParamQuantity declarations (defined after Tessellation is complete)
struct TessTime2Quantity : rack::engine::ParamQuantity {
    std::string getLabel() override;
    std::string getDisplayValueString() override;
};

struct TessTime3Quantity : rack::engine::ParamQuantity {
    std::string getLabel() override;
    std::string getDisplayValueString() override;
};

struct Tessellation : Module {
    enum ParamId {
        TIME1_PARAM,
        MIX1_PARAM,
        REPEATS1_PARAM,
        TONE1_PARAM,
        VOICE1_PARAM,
        SUBDIV2_PARAM,
        TIME2_PARAM,
        MIX2_PARAM,
        REPEATS2_PARAM,
        TONE2_PARAM,
        VOICE2_PARAM,
        SUBDIV3_PARAM,
        TIME3_PARAM,
        MIX3_PARAM,
        REPEATS3_PARAM,
        TONE3_PARAM,
        VOICE3_PARAM,
        MOD_DEPTH_PARAM,
        MOD_RATE_PARAM,
        TAP_PARAM,
        PINGPONG_PARAM,
        XFEED_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        IN_L_INPUT,
        IN_R_INPUT,
        CLOCK_INPUT,
        TIME1_CV_INPUT,
        TIME2_CV_INPUT,
        TIME3_CV_INPUT,
        REPEATS_CV_INPUT,
        MOD_CV_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        OUT_L_OUTPUT,
        OUT_R_OUTPUT,
        DELAY1_OUTPUT,
        DELAY2_OUTPUT,
        DELAY3_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        ENUMS(TEMPO_LIGHT, 3),      // RGB for green tempo light
        ENUMS(MIX1_LIGHT, 3),       // RGB for Teal
        ENUMS(MIX2_LIGHT, 3),       // RGB for Magenta
        ENUMS(MIX3_LIGHT, 3),       // RGB for Amber
        LIGHTS_LEN
    };

    enum class VoiceType {
        Voice24_96 = 0,
        VoiceADM = 1,
        Voice12Bit = 2
    };

    enum class PingPongMode {
        Off = 0,
        PingPong = 1,
        PingPongInverted = 2
    };

    struct StereoDelayLine {
        static constexpr int MAX_CHANNELS = 16;

        float sampleRate = 44100.f;
        int bufferSize = 1;
        std::array<std::vector<float>, MAX_CHANNELS> bufferL;
        std::array<std::vector<float>, MAX_CHANNELS> bufferR;
        std::array<size_t, MAX_CHANNELS> writeIndex{};
        std::array<float, MAX_CHANNELS> delaySamples{};
        std::array<float, MAX_CHANNELS> targetDelaySamples{};  // Target delay time for smoothing
        std::array<float, MAX_CHANNELS> toneStateL{};
        std::array<float, MAX_CHANNELS> toneStateR{};
        std::array<float, MAX_CHANNELS> modPhase{};
        VoiceType voice = VoiceType::Voice24_96;
        float enginePhaseOffset = 0.f;
        PingPongMode pingPongMode = PingPongMode::Off;
        float smoothingCoeff = 0.9995f;  // Smoothing coefficient for delay time changes

        // Tone filter coefficient cache
        float cachedTone = -1.f;
        float cachedAlpha = 0.f;
        float cachedTilt = 0.f;

        // LFO decimation for modulation (optimization: update every N samples)
        int lfoDecimationCounter = 0;
        static constexpr int kLfoDecimation = 8;  // Update every 8 samples
        float cachedModSamples = 0.f;

        void init(float sr, float phaseOffset = 0.f) {
            sampleRate = std::max(sr, 1.f);
            bufferSize = std::max(2, static_cast<int>(std::ceil(tessellation::MAX_DELAY_SECONDS * sampleRate)) + 2);
            for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
                bufferL[ch].assign(bufferSize, 0.f);
                bufferR[ch].assign(bufferSize, 0.f);
            }
            writeIndex.fill(0);
            toneStateL.fill(0.f);
            toneStateR.fill(0.f);
            modPhase.fill(rack::math::clamp(phaseOffset, 0.f, 1.f));
            enginePhaseOffset = phaseOffset;
            float defaultSamples = tessellation::DEFAULT_DELAY_SECONDS * sampleRate;
            delaySamples.fill(defaultSamples);
            targetDelaySamples.fill(defaultSamples);
        }

        void setDelaySeconds(int channel, float seconds) {
            channel = rack::math::clamp(channel, 0, MAX_CHANNELS - 1);
            float samples = rack::math::clamp(seconds * sampleRate, 1.f, static_cast<float>(bufferSize - 2));
            targetDelaySamples[channel] = samples;  // Set target instead of directly changing delay
        }

        void setVoice(int v) {
            voice = static_cast<VoiceType>(rack::math::clamp(v, 0, 2));
        }

        void setPingPong(int mode) {
            pingPongMode = static_cast<PingPongMode>(rack::math::clamp(mode, 0, 2));
        }

        void resetChannel(int channel, float delaySeconds) {
            channel = rack::math::clamp(channel, 0, MAX_CHANNELS - 1);
            std::fill(bufferL[channel].begin(), bufferL[channel].end(), 0.f);
            std::fill(bufferR[channel].begin(), bufferR[channel].end(), 0.f);
            toneStateL[channel] = 0.f;
            toneStateR[channel] = 0.f;
            modPhase[channel] = enginePhaseOffset;
            writeIndex[channel] = 0;
            float samples = rack::math::clamp(delaySeconds * sampleRate, 1.f, static_cast<float>(bufferSize - 2));
            delaySamples[channel] = samples;
            targetDelaySamples[channel] = samples;
        }

        struct Result {
            float wetL = 0.f;
            float wetR = 0.f;
            float tapL = 0.f;
            float tapR = 0.f;
        };

        Result process(int channel, float inL, float inR, float feedback, float tone,
                        float modDepthSeconds, float modRateHz, float sampleTime) {
            channel = rack::math::clamp(channel, 0, MAX_CHANNELS - 1);

            // Smooth delay time changes to avoid artifacts when modulating
            delaySamples[channel] = delaySamples[channel] * smoothingCoeff +
                                    targetDelaySamples[channel] * (1.f - smoothingCoeff);

            float depthSamples = rack::math::clamp(modDepthSeconds * sampleRate, 0.f, static_cast<float>(bufferSize) * 0.45f);
            float phase = modPhase[channel];

            // Optimization: Decimate LFO calculation (update every N samples)
            // LFO rates are slow (0.1-5 Hz), so we don't need sample-accurate modulation
            if (lfoDecimationCounter == 0) {
                if (depthSamples > 0.f && modRateHz > 0.f) {
                    phase += modRateHz * sampleTime * kLfoDecimation;
                    // Optimize phase wrapping: simple subtraction instead of floor()
                    if (phase >= 1.f) {
                        phase -= 1.f;
                    }
                }
                modPhase[channel] = phase;
                float lfoPhase = phase + enginePhaseOffset;
                // Optimize phase wrapping: simple subtraction instead of floor()
                if (lfoPhase >= 1.f) {
                    lfoPhase -= 1.f;
                }
                cachedModSamples = std::sin(tessellation::TWO_PI * lfoPhase) * depthSamples;
            }

            float modSamples = cachedModSamples;
            // Cache stereoOffset - this is constant per sample rate
            static float cachedStereoOffsetSampleRate = -1.f;
            static float cachedStereoOffset = 0.f;
            if (sampleRate != cachedStereoOffsetSampleRate) {
                cachedStereoOffsetSampleRate = sampleRate;
                cachedStereoOffset = sampleRate * 0.00075f;
            }
            float delaySamplesL = rack::math::clamp(delaySamples[channel] + modSamples - cachedStereoOffset, 1.f,
                static_cast<float>(bufferSize - 2));
            float delaySamplesR = rack::math::clamp(delaySamples[channel] - modSamples + cachedStereoOffset, 1.f,
                static_cast<float>(bufferSize - 2));

            auto readSample = [&](std::vector<float>& buffer, float delaySamples) {
                float readIndex = static_cast<float>(writeIndex[channel]) - delaySamples;
                while (readIndex < 0.f) {
                    readIndex += bufferSize;
                }
                size_t index0 = static_cast<size_t>(readIndex) % bufferSize;
                size_t index1 = (index0 + 1) % bufferSize;
                float frac = readIndex - std::floor(readIndex);
                return rack::math::crossfade(buffer[index0], buffer[index1], rack::math::clamp(frac, 0.f, 1.f));
            };

            float delayedL = readSample(bufferL[channel], delaySamplesL);
            float delayedR = readSample(bufferR[channel], delaySamplesR);

            tone = rack::math::clamp(tone, 0.f, 1.f);

            // Cache tone filter coefficients to avoid repeated exp() calls
            if (tone != cachedTone) {
                cachedTone = tone;
                float cutoffHz = rack::math::clamp(400.f + tone * 18000.f, 200.f, 20000.f);
                cachedAlpha = std::exp(-2.f * tessellation::PI * cutoffHz / sampleRate);
                cachedAlpha = rack::math::clamp(cachedAlpha, 0.f, 0.999f);
                cachedTilt = tone * 2.f - 1.f;
            }

            float& lowL = toneStateL[channel];
            float& lowR = toneStateR[channel];
            lowL = rack::math::crossfade(delayedL, lowL, cachedAlpha);
            lowR = rack::math::crossfade(delayedR, lowR, cachedAlpha);
            float highL = delayedL - lowL;
            float highR = delayedR - lowR;
            float tonedL = (cachedTilt <= 0.f)
                ? rack::math::crossfade(delayedL, lowL, -cachedTilt)
                : rack::math::crossfade(delayedL, highL, cachedTilt);
            float tonedR = (cachedTilt <= 0.f)
                ? rack::math::crossfade(delayedR, lowR, -cachedTilt)
                : rack::math::crossfade(delayedR, highR, cachedTilt);

            applyVoicing(tonedL, tonedR);

            Result res;
            res.tapL = tonedL;
            res.tapR = tonedR;

            // Apply ping-pong routing if enabled
            // Normal: L→L, R→R
            // PingPong: L→R, R→L (delays bounce between channels)
            // PingPongInverted: R→L, L→R (reverse phase)
            if (pingPongMode == PingPongMode::PingPong) {
                res.wetL = tonedR;
                res.wetR = tonedL;
            } else if (pingPongMode == PingPongMode::PingPongInverted) {
                res.wetL = tonedL;
                res.wetR = tonedR;
            } else {
                res.wetL = tonedL;
                res.wetR = tonedR;
            }

            float writeL = rack::math::clamp(tonedL * feedback + inL, -10.f, 10.f);
            float writeR = rack::math::clamp(tonedR * feedback + inR, -10.f, 10.f);

            bufferL[channel][writeIndex[channel]] = writeL;
            bufferR[channel][writeIndex[channel]] = writeR;
            writeIndex[channel] = (writeIndex[channel] + 1) % bufferSize;

            // Increment LFO decimation counter
            lfoDecimationCounter = (lfoDecimationCounter + 1) % kLfoDecimation;

            return res;
        }

        void applyVoicing(float& left, float& right) const {
            switch (voice) {
                case VoiceType::VoiceADM: {
                    auto adm = [](float x) {
                        float driven = std::tanh(x * 1.6f);
                        return 0.65f * x + 0.35f * driven;
                    };
                    left = adm(left);
                    right = adm(right);
                    break;
                }
                case VoiceType::Voice12Bit: {
                    constexpr float fullScale = 10.f; // ±5 V audio range
                    constexpr float step = fullScale / 4096.f; // 12-bit quantization
                    auto quantize = [](float sample) {
                        float clamped = rack::math::clamp(sample, -5.f, 5.f);
                        return std::round(clamped / step) * step;
                    };
                    left = quantize(left);
                    right = quantize(right);
                    break;
                }
                case VoiceType::Voice24_96:
                default:
                    break;
            }
        }
    };

    std::array<StereoDelayLine, 3> delayLines;
    float sampleRate = 44100.f;
    rack::dsp::SchmittTrigger tapButtonTrigger;
    rack::dsp::SchmittTrigger clockTrigger;
    rack::dsp::PulseGenerator delay1Pulse;
    rack::dsp::PulseGenerator delay2Pulse;
    rack::dsp::PulseGenerator delay3Pulse;
    float tapTimer = 0.f;
    float clockTimer = 0.f;
    float lastClockPeriod = 0.35f;
    float delay1Phase = 0.f;
    float delay2Phase = 0.f;
    float delay3Phase = 0.f;

    // Cross-feedback state: previous sample's delay 3 output (for Delay 3 → 1 feedback)
    static constexpr int MAX_CHANNELS = 16;
    std::array<float, MAX_CHANNELS> xfeedDelay3L{};
    std::array<float, MAX_CHANNELS> xfeedDelay3R{};

    // Parameter decimation for performance (update every N samples instead of every sample)
    static constexpr int kParamDecimation = 32;  // ~0.7ms at 44.1kHz - imperceptible latency
    int paramDecimationCounter = 0;

    // Cached parameter values (updated every kParamDecimation samples)
    float cachedDelay1Seconds = 0.35f;
    float cachedDelay2Seconds = 0.35f;
    float cachedDelay3Seconds = 0.35f;
    float cachedFeedback1 = 0.35f;
    float cachedFeedback2 = 0.35f;
    float cachedFeedback3 = 0.35f;
    float cachedTone1 = 0.5f;
    float cachedTone2 = 0.5f;
    float cachedTone3 = 0.5f;
    float cachedMix1 = 0.5f;
    float cachedMix2 = 0.45f;
    float cachedMix3 = 0.45f;
    float cachedModDepthSeconds = 0.002f;
    float cachedModRateHz = 1.57f;
    float cachedCrossFeedback = 0.0f;
    // Input de-click
    bool lastLeftConnected = false;
    bool lastRightConnected = false;
    float leftFade = 1.f;
    float rightFade = 1.f;
    int cachedVoice1 = 0;
    int cachedVoice2 = 1;
    int cachedVoice3 = 2;
    int cachedPingPongMode = 0;
    int activeChannels = 1;

    void initDelayLines(float sr) {
        sampleRate = sr;
        constexpr float phaseOffsets[3] = {0.f, 0.33f, 0.66f};
        for (int i = 0; i < 3; ++i) {
            delayLines[i].init(sr, phaseOffsets[i]);
        }
    }

    Tessellation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(TIME1_PARAM,
            tessellation::MIN_DELAY_SECONDS,
            tessellation::MAX_DELAY_SECONDS,
            tessellation::DEFAULT_DELAY_SECONDS,
            "Delay 1 time", " s");
        shapetaker::ParameterHelper::configGain(this, MIX1_PARAM, "Delay 1 mix", 0.5f);
        shapetaker::ParameterHelper::configGain(this, REPEATS1_PARAM, "Delay 1 repeats", 0.35f);
        shapetaker::ParameterHelper::configGain(this, TONE1_PARAM, "Delay 1 tone", 0.5f);
        configSwitch(VOICE1_PARAM, 0.f, 2.f, 0.f, "Delay 1 voicing", {"24/96", "ADM", "12-bit"});

        configSwitch(SUBDIV2_PARAM, 0.f, 5.f, 1.f, "Delay 2 subdivision",
            {"Triplet", "Eighth", "Golden", "Dotted 8th", "Dotted Quarter", "Free"});
        configParam<TessTime2Quantity>(TIME2_PARAM,
            tessellation::MIN_DELAY_SECONDS,
            tessellation::MAX_DELAY_SECONDS,
            tessellation::DEFAULT_DELAY_SECONDS,
            "Delay 2 time (Free)", " s");
        shapetaker::ParameterHelper::configGain(this, MIX2_PARAM, "Delay 2 mix", 0.45f);
        shapetaker::ParameterHelper::configGain(this, REPEATS2_PARAM, "Delay 2 repeats", 0.35f);
        shapetaker::ParameterHelper::configGain(this, TONE2_PARAM, "Delay 2 tone", 0.5f);
        configSwitch(VOICE2_PARAM, 0.f, 2.f, 1.f, "Delay 2 voicing", {"24/96", "ADM", "12-bit"});

        configSwitch(SUBDIV3_PARAM, 0.f, 5.f, 2.f, "Delay 3 subdivision",
            {"Triplet", "Eighth", "Golden", "Dotted 8th", "Dotted Quarter", "Free"});
        configParam<TessTime3Quantity>(TIME3_PARAM,
            tessellation::MIN_DELAY_SECONDS,
            tessellation::MAX_DELAY_SECONDS,
            tessellation::DEFAULT_DELAY_SECONDS,
            "Delay 3 time (Free)", " s");
        shapetaker::ParameterHelper::configGain(this, MIX3_PARAM, "Delay 3 mix", 0.45f);
        shapetaker::ParameterHelper::configGain(this, REPEATS3_PARAM, "Delay 3 repeats", 0.35f);
        shapetaker::ParameterHelper::configGain(this, TONE3_PARAM, "Delay 3 tone", 0.5f);
        configSwitch(VOICE3_PARAM, 0.f, 2.f, 2.f, "Delay 3 voicing", {"24/96", "ADM", "12-bit"});

        shapetaker::ParameterHelper::configGain(this, MOD_DEPTH_PARAM, "Mod depth", 0.1f);
        shapetaker::ParameterHelper::configGain(this, MOD_RATE_PARAM, "Mod rate", 0.3f);
        configButton(TAP_PARAM, "Tap tempo");
        configSwitch(PINGPONG_PARAM, 0.f, 2.f, 0.f, "Ping-pong mode", {"Off", "Ping-pong", "Inverted"});
        shapetaker::ParameterHelper::configGain(this, XFEED_PARAM, "Cross-feedback", 0.0f);

        shapetaker::ParameterHelper::configAudioInput(this, IN_L_INPUT, "Left audio");
        shapetaker::ParameterHelper::configAudioInput(this, IN_R_INPUT, "Right audio");
        configInput(CLOCK_INPUT, "External clock (sets delay 1 tempo)");
        shapetaker::ParameterHelper::configCVInput(this, TIME1_CV_INPUT, "Delay 1 time CV");
        shapetaker::ParameterHelper::configCVInput(this, TIME2_CV_INPUT, "Delay 2 time CV");
        shapetaker::ParameterHelper::configCVInput(this, TIME3_CV_INPUT, "Delay 3 time CV");
        shapetaker::ParameterHelper::configCVInput(this, REPEATS_CV_INPUT, "Repeats CV");
        shapetaker::ParameterHelper::configCVInput(this, MOD_CV_INPUT, "Mod depth CV");

        shapetaker::ParameterHelper::configAudioOutput(this, OUT_L_OUTPUT, "Left output");
        shapetaker::ParameterHelper::configAudioOutput(this, OUT_R_OUTPUT, "Right output");
        shapetaker::ParameterHelper::configAudioOutput(this, DELAY1_OUTPUT, "Delay 1 tap output");
        shapetaker::ParameterHelper::configAudioOutput(this, DELAY2_OUTPUT, "Delay 2 tap output");
        shapetaker::ParameterHelper::configAudioOutput(this, DELAY3_OUTPUT, "Delay 3 tap output");

        float sr = sampleRate;
        if (APP && APP->engine) {
            sr = APP->engine->getSampleRate();
        }
        initDelayLines(sr);

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void onSampleRateChange() override {
        float sr = sampleRate;
        if (APP && APP->engine) {
            sr = APP->engine->getSampleRate();
        }
        initDelayLines(sr);
    }

    void process(const ProcessArgs& args) override {
        sampleRate = args.sampleRate;

        tapTimer += args.sampleTime;
        if (tapTimer > tessellation::TAP_RESET_SECONDS) {
            tapTimer = 0.f;
        }
        if (tapButtonTrigger.process(params[TAP_PARAM].getValue())) {
            if (tapTimer > 0.02f) {
                float tapped = rack::math::clamp(tapTimer,
                    tessellation::MIN_DELAY_SECONDS,
                    tessellation::MAX_DELAY_SECONDS);
                params[TIME1_PARAM].setValue(tapped);
                float tapPulseDuration = rack::math::clamp(tapped * 0.15f, 0.03f, 0.12f);
                delay1Pulse.trigger(tapPulseDuration);
            }
            tapTimer = 0.f;
        }

        // External clock input processing: measure clock period and set delay 1 time
        clockTimer += args.sampleTime;
        if (inputs[CLOCK_INPUT].isConnected()) {
            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
                // Clock pulse received - measure period
                if (clockTimer > 0.02f) {  // Ignore very fast pulses (< 20ms)
                    float measuredPeriod = rack::math::clamp(clockTimer,
                        tessellation::MIN_DELAY_SECONDS,
                        tessellation::MAX_DELAY_SECONDS);
                    lastClockPeriod = measuredPeriod;
                    params[TIME1_PARAM].setValue(measuredPeriod);
                    float pulseDuration = rack::math::clamp(measuredPeriod * 0.15f, 0.03f, 0.12f);
                    delay1Pulse.trigger(pulseDuration);
                }
                clockTimer = 0.f;
            }
            // Clock timeout: if no pulse for 3 seconds, reset
            if (clockTimer > 3.f) {
                clockTimer = 0.f;
            }
        } else {
            // Clock disconnected - reset timer
            clockTimer = 0.f;
        }

        // Parameter decimation: only read parameters every N samples for performance
        // ~0.7ms latency at 44.1kHz is imperceptible but saves ~15-20% CPU
        if (paramDecimationCounter == 0) {
            // Delay times with CV
            float time1CV = inputs[TIME1_CV_INPUT].isConnected() ? inputs[TIME1_CV_INPUT].getVoltage() * 0.25f : 0.f;
            cachedDelay1Seconds = rack::math::clamp(params[TIME1_PARAM].getValue() + time1CV,
                tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);

            float time2CV = inputs[TIME2_CV_INPUT].isConnected() ? inputs[TIME2_CV_INPUT].getVoltage() * 0.25f : 0.f;
            int subdiv2 = rack::math::clamp(static_cast<int>(std::round(params[SUBDIV2_PARAM].getValue())), 0, 5);
            if (subdiv2 == 5) {
                // Free mode: TIME2 directly controls delay time
                cachedDelay2Seconds = rack::math::clamp(params[TIME2_PARAM].getValue() + time2CV,
                    tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);
            } else {
                // Musical mode: TIME2 nudges the subdivision (±2 steps) around the selected value,
                // then applies a gentle 0.5x–2x multiplier for fine trim.
                float normalized = rack::math::rescale(params[TIME2_PARAM].getValue() + time2CV,
                    tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS, 0.f, 1.f);
                normalized = rack::math::clamp(normalized, 0.f, 1.f);
                int subdivisionOffset = rack::math::clamp(static_cast<int>(std::round((normalized - 0.5f) * 4.f)), -2, 2);
                int effectiveSubdiv = rack::math::clamp(subdiv2 + subdivisionOffset, 0, 4);

                float multiplier = rack::math::clamp(normalized * 1.5f + 0.5f, 0.5f, 2.0f);
                float baseMusicalTime = cachedDelay1Seconds * tessellation::subdivisionMultiplier(effectiveSubdiv);
                cachedDelay2Seconds = rack::math::clamp(baseMusicalTime * multiplier,
                    tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);
            }

            float time3CV = inputs[TIME3_CV_INPUT].isConnected() ? inputs[TIME3_CV_INPUT].getVoltage() * 0.25f : 0.f;
            int subdiv3 = rack::math::clamp(static_cast<int>(std::round(params[SUBDIV3_PARAM].getValue())), 0, 5);
            if (subdiv3 == 5) {
                // Free mode: TIME3 directly controls delay time
                cachedDelay3Seconds = rack::math::clamp(params[TIME3_PARAM].getValue() + time3CV,
                    tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);
            } else {
                // Musical mode: TIME3 nudges the subdivision (±2 steps) around the selected value,
                // then applies a gentle 0.5x–2x multiplier for fine trim.
                float normalized = rack::math::rescale(params[TIME3_PARAM].getValue() + time3CV,
                    tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS, 0.f, 1.f);
                normalized = rack::math::clamp(normalized, 0.f, 1.f);
                int subdivisionOffset = rack::math::clamp(static_cast<int>(std::round((normalized - 0.5f) * 4.f)), -2, 2);
                int effectiveSubdiv = rack::math::clamp(subdiv3 + subdivisionOffset, 0, 4);

                float multiplier = rack::math::clamp(normalized * 1.5f + 0.5f, 0.5f, 2.0f);
                float baseMusicalTime = cachedDelay1Seconds * tessellation::subdivisionMultiplier(effectiveSubdiv);
                cachedDelay3Seconds = rack::math::clamp(baseMusicalTime * multiplier,
                    tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);
            }

            // Voice and ping-pong modes
            cachedVoice1 = static_cast<int>(std::round(params[VOICE1_PARAM].getValue()));
            cachedVoice2 = static_cast<int>(std::round(params[VOICE2_PARAM].getValue()));
            cachedVoice3 = static_cast<int>(std::round(params[VOICE3_PARAM].getValue()));
            cachedPingPongMode = static_cast<int>(std::round(params[PINGPONG_PARAM].getValue()));

            // Feedback/repeats with CV
            float repeatsMod = inputs[REPEATS_CV_INPUT].isConnected() ? inputs[REPEATS_CV_INPUT].getVoltage() * 0.1f : 0.f;
            cachedFeedback1 = rack::math::clamp(params[REPEATS1_PARAM].getValue() + repeatsMod, 0.f, 0.97f);
            cachedFeedback2 = rack::math::clamp(params[REPEATS2_PARAM].getValue() + repeatsMod, 0.f, 0.97f);
            cachedFeedback3 = rack::math::clamp(params[REPEATS3_PARAM].getValue() + repeatsMod, 0.f, 0.97f);

            // Tone controls
            cachedTone1 = rack::math::clamp(params[TONE1_PARAM].getValue(), 0.f, 1.f);
            cachedTone2 = rack::math::clamp(params[TONE2_PARAM].getValue(), 0.f, 1.f);
            cachedTone3 = rack::math::clamp(params[TONE3_PARAM].getValue(), 0.f, 1.f);

            // Mix levels
            cachedMix1 = rack::math::clamp(params[MIX1_PARAM].getValue(), 0.f, 1.f);
            cachedMix2 = rack::math::clamp(params[MIX2_PARAM].getValue(), 0.f, 1.f);
            cachedMix3 = rack::math::clamp(params[MIX3_PARAM].getValue(), 0.f, 1.f);

            // Modulation with CV
            float modDepth = params[MOD_DEPTH_PARAM].getValue();
            if (inputs[MOD_CV_INPUT].isConnected()) {
                modDepth += inputs[MOD_CV_INPUT].getVoltage() * 0.1f;
            }
            modDepth = rack::math::clamp(modDepth, 0.f, 1.f);
            cachedModDepthSeconds = modDepth * tessellation::MAX_MOD_DEPTH_SECONDS;

            float modRate = rack::math::clamp(params[MOD_RATE_PARAM].getValue(), 0.f, 1.f);
            cachedModRateHz = 0.1f + modRate * 4.9f;

            // Cross-feedback
            cachedCrossFeedback = rack::math::clamp(params[XFEED_PARAM].getValue(), 0.f, 0.7f);
        }
        paramDecimationCounter = (paramDecimationCounter + 1) % kParamDecimation;

        // Use cached values for all processing
        delayLines[0].setVoice(cachedVoice1);
        delayLines[1].setVoice(cachedVoice2);
        delayLines[2].setVoice(cachedVoice3);

        delayLines[0].setPingPong(cachedPingPongMode);
        delayLines[1].setPingPong(cachedPingPongMode);
        delayLines[2].setPingPong(cachedPingPongMode);

        // Detect input (dis)connects and ramp to avoid clicks
        bool leftConnectedNow = inputs[IN_L_INPUT].isConnected();
        bool rightConnectedNow = inputs[IN_R_INPUT].isConnected();
        if (leftConnectedNow != lastLeftConnected) {
            if (!leftConnectedNow) {
                // Disconnecting: Clear delay buffers to prevent feedback clicks
                for (int c = 0; c < activeChannels; ++c) {
                    delayLines[0].resetChannel(c, cachedDelay1Seconds);
                    delayLines[1].resetChannel(c, cachedDelay2Seconds);
                    delayLines[2].resetChannel(c, cachedDelay3Seconds);
                    xfeedDelay3L[c] = 0.f;
                    xfeedDelay3R[c] = 0.f;
                }
            }
            leftFade = 0.f;
            lastLeftConnected = leftConnectedNow;
        }
        if (rightConnectedNow != lastRightConnected) {
            if (!rightConnectedNow && !leftConnectedNow) {
                // Both disconnected: Clear delay buffers
                for (int c = 0; c < activeChannels; ++c) {
                    delayLines[0].resetChannel(c, cachedDelay1Seconds);
                    delayLines[1].resetChannel(c, cachedDelay2Seconds);
                    delayLines[2].resetChannel(c, cachedDelay3Seconds);
                    xfeedDelay3L[c] = 0.f;
                    xfeedDelay3R[c] = 0.f;
                }
            }
            rightFade = 0.f;
            lastRightConnected = rightConnectedNow;
        }
        auto advanceFade = [&](float& fade) {
            fade += args.sampleTime * 400.f; // ~2.5 ms ramp (faster fade)
            if (fade > 1.f) fade = 1.f;
            return fade;
        };
        float leftGain = advanceFade(leftFade);
        float rightGain = advanceFade(rightFade);

        int lChannels = inputs[IN_L_INPUT].getChannels();
        int rChannels = inputs[IN_R_INPUT].getChannels();
        int channels = std::max(std::max(lChannels, rChannels), 1);
        if (channels != activeChannels) {
            int maxCh = std::min(StereoDelayLine::MAX_CHANNELS, std::max(channels, activeChannels));
            for (int c = 0; c < maxCh; ++c) {
                if (c >= channels) {
                    // Channel going inactive: clear state
                    delayLines[0].resetChannel(c, cachedDelay1Seconds);
                    delayLines[1].resetChannel(c, cachedDelay2Seconds);
                    delayLines[2].resetChannel(c, cachedDelay3Seconds);
                    xfeedDelay3L[c] = 0.f;
                    xfeedDelay3R[c] = 0.f;
                } else if (c >= activeChannels) {
                    // New channel becoming active: clear before use
                    delayLines[0].resetChannel(c, cachedDelay1Seconds);
                    delayLines[1].resetChannel(c, cachedDelay2Seconds);
                    delayLines[2].resetChannel(c, cachedDelay3Seconds);
                    xfeedDelay3L[c] = 0.f;
                    xfeedDelay3R[c] = 0.f;
                }
            }
            activeChannels = channels;
        }

        outputs[OUT_L_OUTPUT].setChannels(channels);
        outputs[OUT_R_OUTPUT].setChannels(channels);
        outputs[DELAY1_OUTPUT].setChannels(channels);
        outputs[DELAY2_OUTPUT].setChannels(channels);
        outputs[DELAY3_OUTPUT].setChannels(channels);

        float wetGainComp = 1.f / std::max(1.f, cachedMix1 + cachedMix2 + cachedMix3);
        wetGainComp = rack::math::clamp(wetGainComp, 0.5f, 1.f);
        float dryFactor = 1.f;

        // Hoist lambdas outside channel loop for better performance
        auto tapAvg = [](const StereoDelayLine::Result& res) {
            return (res.tapL + res.tapR) * 0.5f;
        };

        for (int c = 0; c < channels; ++c) {
            float inL = (lChannels > 0) ? inputs[IN_L_INPUT].getVoltage(c % lChannels) : 0.f;
            float inR;
            if (inputs[IN_R_INPUT].isConnected()) {
                inR = (rChannels > 0) ? inputs[IN_R_INPUT].getVoltage(c % rChannels)
                                      : inputs[IN_R_INPUT].getVoltage(0);
            }
            else {
                inR = inL;
            }
            inL *= leftGain;
            inR *= rightGain;

            delayLines[0].setDelaySeconds(c, cachedDelay1Seconds);
            delayLines[1].setDelaySeconds(c, cachedDelay2Seconds);
            delayLines[2].setDelaySeconds(c, cachedDelay3Seconds);

            // Optimization: Conditional cross-feedback processing
            // When crossFeedback is zero, skip the multiplication operations
            StereoDelayLine::Result res1, res2, res3;

            if (cachedCrossFeedback > 0.f) {
                // Cross-feedback matrix: Delay 1 → 2 → 3 → 1 (circular)
                // Process delays sequentially to implement cross-feedback routing

                // Delay 1 gets input + cross-fed signal from Delay 3 (previous sample)
                float in1L = inL + xfeedDelay3L[c] * cachedCrossFeedback;
                float in1R = inR + xfeedDelay3R[c] * cachedCrossFeedback;
                res1 = delayLines[0].process(c, in1L, in1R, cachedFeedback1, cachedTone1,
                    cachedModDepthSeconds, cachedModRateHz, args.sampleTime);

                // Delay 2 gets input + cross-fed signal from Delay 1
                float in2L = inL + res1.tapL * cachedCrossFeedback;
                float in2R = inR + res1.tapR * cachedCrossFeedback;
                res2 = delayLines[1].process(c, in2L, in2R, cachedFeedback2, cachedTone2,
                    cachedModDepthSeconds, cachedModRateHz, args.sampleTime);

                // Delay 3 gets input + cross-fed signal from Delay 2
                float in3L = inL + res2.tapL * cachedCrossFeedback;
                float in3R = inR + res2.tapR * cachedCrossFeedback;
                res3 = delayLines[2].process(c, in3L, in3R, cachedFeedback3, cachedTone3,
                    cachedModDepthSeconds, cachedModRateHz, args.sampleTime);

                // Store Delay 3 output for next sample's Delay 1 feedback
                xfeedDelay3L[c] = res3.tapL;
                xfeedDelay3R[c] = res3.tapR;
            } else {
                // No cross-feedback: process delays independently (faster)
                res1 = delayLines[0].process(c, inL, inR, cachedFeedback1, cachedTone1,
                    cachedModDepthSeconds, cachedModRateHz, args.sampleTime);
                res2 = delayLines[1].process(c, inL, inR, cachedFeedback2, cachedTone2,
                    cachedModDepthSeconds, cachedModRateHz, args.sampleTime);
                res3 = delayLines[2].process(c, inL, inR, cachedFeedback3, cachedTone3,
                    cachedModDepthSeconds, cachedModRateHz, args.sampleTime);
            }

            float wetL = (res1.wetL * cachedMix1 + res2.wetL * cachedMix2 + res3.wetL * cachedMix3) * wetGainComp;
            float wetR = (res1.wetR * cachedMix1 + res2.wetR * cachedMix2 + res3.wetR * cachedMix3) * wetGainComp;

            float outL = rack::math::clamp(inL * dryFactor + wetL, -10.f, 10.f);
            float outR = rack::math::clamp(inR * dryFactor + wetR, -10.f, 10.f);

            outputs[OUT_L_OUTPUT].setVoltage(outL, c);
            outputs[OUT_R_OUTPUT].setVoltage(outR, c);

            float send1 = rack::math::clamp(tapAvg(res1) * wetGainComp, -10.f, 10.f);
            float send2 = rack::math::clamp(tapAvg(res2) * wetGainComp, -10.f, 10.f);
            float send3 = rack::math::clamp(tapAvg(res3) * wetGainComp, -10.f, 10.f);
            outputs[DELAY1_OUTPUT].setVoltage(send1, c);
            outputs[DELAY2_OUTPUT].setVoltage(send2, c);
            outputs[DELAY3_OUTPUT].setVoltage(send3, c);
        }

        // Track each delay's phase for LED pulsing
        // Pulse duration scales with delay time: shorter delays = shorter pulses
        // Note: Delay 1 phase tracking is disabled when external clock is connected
        if (!inputs[CLOCK_INPUT].isConnected()) {
            delay1Phase += args.sampleTime;
            float period1 = rack::math::clamp(cachedDelay1Seconds, 0.05f, tessellation::MAX_DELAY_SECONDS);
            if (delay1Phase >= period1) {
                delay1Phase -= period1;
                float pulseDuration1 = rack::math::clamp(cachedDelay1Seconds * 0.15f, 0.03f, 0.12f);
                delay1Pulse.trigger(pulseDuration1);
            }
        }

        delay2Phase += args.sampleTime;
        float period2 = rack::math::clamp(cachedDelay2Seconds, 0.05f, tessellation::MAX_DELAY_SECONDS);
        if (delay2Phase >= period2) {
            delay2Phase -= period2;
            float pulseDuration2 = rack::math::clamp(cachedDelay2Seconds * 0.15f, 0.03f, 0.12f);
            delay2Pulse.trigger(pulseDuration2);
        }

        delay3Phase += args.sampleTime;
        float period3 = rack::math::clamp(cachedDelay3Seconds, 0.05f, tessellation::MAX_DELAY_SECONDS);
        if (delay3Phase >= period3) {
            delay3Phase -= period3;
            float pulseDuration3 = rack::math::clamp(cachedDelay3Seconds * 0.15f, 0.03f, 0.12f);
            delay3Pulse.trigger(pulseDuration3);
        }

        // Tempo light: Light up when tap button is pressed
        float tapPressed = params[TAP_PARAM].getValue();
        lights[TEMPO_LIGHT + 0].setBrightness(tapPressed);
        lights[TEMPO_LIGHT + 1].setBrightness(tapPressed);
        lights[TEMPO_LIGHT + 2].setBrightness(tapPressed);

        // Mix LEDs: Pulse brightness based on mix level (off when not pulsing)
        auto mixBrightness = [](float v) {
            float clamped = rack::math::clamp(v, 0.f, 1.f);
            return std::pow(clamped, 0.7f);
        };
        float mix1Led = mixBrightness(cachedMix1);
        float mix2Led = mixBrightness(cachedMix2);
        float mix3Led = mixBrightness(cachedMix3);

        // LEDs only light up when pulsing, brightness scaled by mix level
        float bright1 = delay1Pulse.process(args.sampleTime) ? mix1Led : 0.f;
        float bright2 = delay2Pulse.process(args.sampleTime) ? mix2Led : 0.f;
        float bright3 = delay3Pulse.process(args.sampleTime) ? mix3Led : 0.f;

        // Mix 1: Teal
        lights[MIX1_LIGHT + 0].setBrightnessSmooth(0.f, args.sampleTime);
        lights[MIX1_LIGHT + 1].setBrightnessSmooth(bright1, args.sampleTime);
        lights[MIX1_LIGHT + 2].setBrightnessSmooth(bright1 * 0.7f, args.sampleTime);

        // Mix 2: Magenta
        lights[MIX2_LIGHT + 0].setBrightnessSmooth(bright2, args.sampleTime);
        lights[MIX2_LIGHT + 1].setBrightnessSmooth(0.f, args.sampleTime);
        lights[MIX2_LIGHT + 2].setBrightnessSmooth(bright2, args.sampleTime);

        // Mix 3: Amber
        lights[MIX3_LIGHT + 0].setBrightnessSmooth(bright3, args.sampleTime);
        lights[MIX3_LIGHT + 1].setBrightnessSmooth(bright3 * 0.7f, args.sampleTime);
        lights[MIX3_LIGHT + 2].setBrightnessSmooth(0.f, args.sampleTime);
    }
};

#ifndef SHAPETAKER_TESSELLATION_NO_WIDGET

// Custom jewel LED sized between Small (10mm) and Medium (12mm) - trimmed to 8mm
class TessellationJewelLED : public shapetaker::ui::JewelLEDBase<18> {
private:
    std::shared_ptr<window::Svg> housingSvg;

    void drawHousing(const DrawArgs& args) const {
        if (!housingSvg || !housingSvg->handle) {
            return;
        }

        nvgSave(args.vg);
        const float scaleX = box.size.x / housingSvg->handle->width;
        const float scaleY = box.size.y / housingSvg->handle->height;
        nvgScale(args.vg, scaleX, scaleY);
        svgDraw(args.vg, housingSvg->handle);
        nvgRestore(args.vg);
    }

public:
    TessellationJewelLED() {
        bgColor = nvgRGBA(0, 0, 0, 0);
        borderColor = nvgRGBA(0, 0, 0, 0);
        // Add RGB base colors for the MultiLightWidget
        addBaseColor(nvgRGB(255, 0, 0));   // Red
        addBaseColor(nvgRGB(0, 255, 0));   // Green
        addBaseColor(nvgRGB(0, 0, 255));   // Blue
        // Hardware-friendly lens: 8mm for a tighter fit
        box.size = mm2px(Vec(8.f, 8.f));

        // Use medium bezel artwork scaled down so the ring frames the glow cleanly
        housingSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));
    }

    void draw(const DrawArgs& args) override {
        // Draw the LED/glow first, then overlay the bezel so everything stays concentric
        ModuleLightWidget::draw(args);
        drawHousing(args);
    }
};

struct TessellationWidget : ModuleWidget {
    // Match Clairaudient background rendering
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            // Keep the same leather grain density as Clairaudient by tiling at
            // fixed height/aspect (no horizontal stretch on wider panels).
            // A second low-alpha offset pass helps hide repeat seams.
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;
            nvgSave(args.vg);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
            nvgFill(args.vg);
            nvgRestore(args.vg);
        }
        ModuleWidget::draw(args);

        // Draw a black inner frame to fully mask any edge tinting
        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }

    TessellationWidget(Tessellation* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Tessellation.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;

        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        auto svgPath = asset::plugin(pluginInstance, "res/panels/Tessellation.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        // 26HP layout: 132.08mm wide × 128.5mm tall
        // Control sizes: Medium knob ~= 20mm, Small knob = 15mm, Jack = 8mm
        // Safe zone: 8mm margin on each edge (knob radius) = 116mm usable width

        // Custom RGB light colors matching the screen visualization
        // Delay 1: Teal (#00ffb4)
        // Delay 2: Magenta (#ff00ff)
        // Delay 3: Amber (#ffb400)
        const std::array<const char*, 3> mixLightPositions = {
            "tess-mix1-light", "tess-mix2-light", "tess-mix3-light"
        };
        const std::array<Vec, 3> mixLightFallbackMm = {
            Vec(59.925152f, 31.278572f),
            Vec(59.925152f, 56.580421f),
            Vec(59.925152f, 81.884483f)
        };
        auto addMixLights = [&](int mixLightId, int delayIndex) {
            if (!module) return;
            // Use RGB lights - all use the same type, color is set by brightness values
            addChild(createLightCentered<TessellationJewelLED>(
                centerPx(mixLightPositions[delayIndex], mixLightFallbackMm[delayIndex].x, mixLightFallbackMm[delayIndex].y),
                module, mixLightId));
        };

        // Fallback coordinates mirror Tessellation.svg anchors exactly.
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("tess-time1", 15.710328f, 19.843622f), module, Tessellation::TIME1_PARAM));

        auto* tapBtn = createParamCentered<ShapetakerVintageMomentaryLight>(
            centerPx("tess-tap", 110.98453f, 19.031929f), module, Tessellation::TAP_PARAM);
        tapBtn->module = module;
        tapBtn->lightId = Tessellation::TEMPO_LIGHT;
        addParam(tapBtn);

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-subdiv-2", 117.38332f, 45.146576f), module, Tessellation::SUBDIV2_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("tess-time-2", 15.710328f, 45.146576f), module, Tessellation::TIME2_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-subdiv3", 117.38332f, 70.449532f), module, Tessellation::SUBDIV3_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("tess-time3", 15.710328f, 70.449532f), module, Tessellation::TIME3_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-mix1", 48.675289f, 19.843622f), module, Tessellation::MIX1_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-repeats1", 71.577965f, 19.843622f), module, Tessellation::REPEATS1_PARAM));
        addMixLights(Tessellation::MIX1_LIGHT, 0);

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-mix2", 48.675289f, 45.146576f), module, Tessellation::MIX2_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-repeats2", 71.577965f, 45.146576f), module, Tessellation::REPEATS2_PARAM));
        addMixLights(Tessellation::MIX2_LIGHT, 1);

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-mix3", 48.675289f, 70.449532f), module, Tessellation::MIX3_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-repeats3", 71.577965f, 70.449532f), module, Tessellation::REPEATS3_PARAM));
        addMixLights(Tessellation::MIX3_LIGHT, 2);

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-tone1", 94.480644f, 19.843622f), module, Tessellation::TONE1_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-tone2", 94.480644f, 45.146576f), module, Tessellation::TONE2_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-tone3", 94.480644f, 70.449532f), module, Tessellation::TONE3_PARAM));

        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("tess-voice1", 32.399029f, 19.843622f), module, Tessellation::VOICE1_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("tess-voice2", 32.399029f, 45.146576f), module, Tessellation::VOICE2_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("tess-voice3", 32.399029f, 70.449532f), module, Tessellation::VOICE3_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-mod-depth", 48.675289f, 95.752487f), module, Tessellation::MOD_DEPTH_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-mod-rate", 71.577965f, 95.752487f), module, Tessellation::MOD_RATE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tess-xfeed", 15.710328f, 95.752487f), module, Tessellation::XFEED_PARAM));

        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("tess-pingpong", 32.399029f, 95.752487f), module, Tessellation::PINGPONG_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("tess-ext-clk-in", 122.61442f, 19.031929f), module, Tessellation::CLOCK_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("tess-in-l", 13.622764f, 115.07108f), module, Tessellation::IN_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("tess-in-r", 26.84543f, 115.07108f), module, Tessellation::IN_R_INPUT));

        const std::array<const char*, 5> cvIds = {
            "tess-time1-cv", "tess-time2-cv", "tess-time3-cv", "tess-repeats-cv", "tess-mod-cv"
        };
        const std::array<Vec, 5> cvFallbackMm = {
            Vec(40.068096f, 115.07108f),
            Vec(53.29076f, 115.07108f),
            Vec(66.51342f, 115.07108f),
            Vec(92.958755f, 115.07108f),
            Vec(79.736084f, 115.07108f)
        };
        for (size_t i = 0; i < cvIds.size(); ++i) {
            addInput(createInputCentered<ShapetakerBNCPort>(
                centerPx(cvIds[i], cvFallbackMm[i].x, cvFallbackMm[i].y),
                module, Tessellation::TIME1_CV_INPUT + i));
        }

        const std::array<const char*, 5> outputIds = {
            "tess-out-l", "tess-out-r", "tess-delay1-out", "tess-delay2-out", "tess-delay3-out"
        };
        const std::array<Vec, 5> outputFallbackMm = {
            Vec(106.18141f, 115.07108f),
            Vec(119.40408f, 115.07108f),
            Vec(92.958755f, 95.752487f),
            Vec(106.18141f, 95.752487f),
            Vec(119.40408f, 95.752487f)
        };
        const std::array<int, 5> outputParams = {
            Tessellation::OUT_L_OUTPUT,
            Tessellation::OUT_R_OUTPUT,
            Tessellation::DELAY1_OUTPUT,
            Tessellation::DELAY2_OUTPUT,
            Tessellation::DELAY3_OUTPUT
        };
        for (size_t i = 0; i < outputIds.size(); ++i) {
            addOutput(createOutputCentered<ShapetakerBNCPort>(
                centerPx(outputIds[i], outputFallbackMm[i].x, outputFallbackMm[i].y),
                module, outputParams[i]));
        }
    }
};

Model* modelTessellation = createModel<Tessellation, TessellationWidget>("Tessellation");
#endif

// ParamQuantity label implementations (after Tessellation definition)
std::string TessTime2Quantity::getLabel() {
    auto* m = dynamic_cast<Tessellation*>(module);
    if (m) {
        int subdiv = rack::math::clamp(static_cast<int>(std::round(m->params[Tessellation::SUBDIV2_PARAM].getValue())), 0, 5);
        if (subdiv != 5) {
            return std::string("Delay 2 trim (") + subdivisionName(subdiv) + " subdivision)";
        }
    }
    return "Delay 2 time (Free)";
}

std::string TessTime3Quantity::getLabel() {
    auto* m = dynamic_cast<Tessellation*>(module);
    if (m) {
        int subdiv = rack::math::clamp(static_cast<int>(std::round(m->params[Tessellation::SUBDIV3_PARAM].getValue())), 0, 5);
        if (subdiv != 5) {
            return std::string("Delay 3 trim (") + subdivisionName(subdiv) + " subdivision)";
        }
    }
    return "Delay 3 time (Free)";
}

namespace {
    std::string divisionLabel(int subdivIndex) {
        switch (subdivIndex) {
            case 0: return "1/8T";         // Triplet
            case 1: return "1/8";          // Eighth
            case 2: return "5/8";          // Golden (rounded to nearest usable fraction)
            case 3: return "1/8.";         // Dotted eighth (3/16)
            case 4: return "3/8";          // Dotted quarter
            default: return "Free";
        }
    }

    // Compute the effective subdivision and fine multiplier using the same mapping as process(),
    // but ignoring CV (tooltip can't see CV).
    std::pair<int, float> computeEffectiveSubdiv(Tessellation* m, rack::engine::ParamQuantity* q, int baseParamId, int timeParamId) {
        if (!m || !q) return {5, 1.f};
        int subdiv = rack::math::clamp(static_cast<int>(std::round(m->params[baseParamId].getValue())), 0, 5);
        if (subdiv == 5) {
            return {5, 1.f};
        }
        float minV = q->getMinValue();
        float maxV = q->getMaxValue();
        float normalized = rack::math::clamp((q->getValue() - minV) / std::max(1e-6f, maxV - minV), 0.f, 1.f);
        int subdivisionOffset = rack::math::clamp(static_cast<int>(std::round((normalized - 0.5f) * 4.f)), -2, 2);
        int effectiveSubdiv = rack::math::clamp(subdiv + subdivisionOffset, 0, 4);
        float multiplier = rack::math::clamp(normalized * 1.5f + 0.5f, 0.5f, 2.0f);
        return {effectiveSubdiv, multiplier};
    }

    std::string formatTrimmedDivision(Tessellation* m, rack::engine::ParamQuantity* q, int baseParamId, int timeParamId) {
        auto [effSubdiv, multiplier] = computeEffectiveSubdiv(m, q, baseParamId, timeParamId);
        if (effSubdiv == 5 || !q) {
            return q ? q->ParamQuantity::getDisplayValueString() : std::string();
        }
        std::string base = divisionLabel(effSubdiv);
        if (std::abs(multiplier - 1.f) < 0.01f) {
            return base;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s ×%.2f", base.c_str(), multiplier);
        return std::string(buf);
    }
}

std::string TessTime2Quantity::getDisplayValueString() {
    auto* m = dynamic_cast<Tessellation*>(module);
    return formatTrimmedDivision(m, this, Tessellation::SUBDIV2_PARAM, Tessellation::TIME2_PARAM);
}

std::string TessTime3Quantity::getDisplayValueString() {
    auto* m = dynamic_cast<Tessellation*>(module);
    return formatTrimmedDivision(m, this, Tessellation::SUBDIV3_PARAM, Tessellation::TIME3_PARAM);
}
