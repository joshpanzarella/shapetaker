#include "plugin.hpp"
#include "componentlibrary.hpp"
#include <algorithm>
#include <array>
#include <cmath>
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
        FREEZE_PARAM,
        TAP_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        IN_L_INPUT,
        IN_R_INPUT,
        TIME1_CV_INPUT,
        TIME2_CV_INPUT,
        TIME3_CV_INPUT,
        REPEATS_CV_INPUT,
        MOD_CV_INPUT,
        FREEZE_GATE_INPUT,
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
        TEMPO_LIGHT,
        DELAY1_VU_LIGHT,
        DELAY2_VU_LIGHT,
        DELAY3_VU_LIGHT,
        MIX1_LIGHT,
        MIX2_LIGHT,
        MIX3_LIGHT,
        LIGHTS_LEN
    };

    enum class VoiceType {
        Voice24_96 = 0,
        VoiceADM = 1,
        Voice12Bit = 2
    };

    struct StereoDelayLine {
        static constexpr int MAX_CHANNELS = 16;

        float sampleRate = 44100.f;
        int bufferSize = 1;
        std::array<std::vector<float>, MAX_CHANNELS> bufferL;
        std::array<std::vector<float>, MAX_CHANNELS> bufferR;
        std::array<size_t, MAX_CHANNELS> writeIndex{};
        std::array<float, MAX_CHANNELS> delaySamples{};
        std::array<float, MAX_CHANNELS> toneStateL{};
        std::array<float, MAX_CHANNELS> toneStateR{};
        std::array<float, MAX_CHANNELS> modPhase{};
        VoiceType voice = VoiceType::Voice24_96;
        float enginePhaseOffset = 0.f;

        // Tone filter coefficient cache
        float cachedTone = -1.f;
        float cachedAlpha = 0.f;
        float cachedTilt = 0.f;

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
        }

        void setDelaySeconds(int channel, float seconds) {
            channel = rack::math::clamp(channel, 0, MAX_CHANNELS - 1);
            float samples = rack::math::clamp(seconds * sampleRate, 1.f, static_cast<float>(bufferSize - 2));
            delaySamples[channel] = samples;
        }

        void setVoice(int v) {
            voice = static_cast<VoiceType>(rack::math::clamp(v, 0, 2));
        }

        struct Result {
            float wetL = 0.f;
            float wetR = 0.f;
            float tapL = 0.f;
            float tapR = 0.f;
        };

        Result process(int channel, float inL, float inR, float feedback, float tone,
                        float modDepthSeconds, float modRateHz, bool freeze, float sampleTime) {
            channel = rack::math::clamp(channel, 0, MAX_CHANNELS - 1);

            float depthSamples = rack::math::clamp(modDepthSeconds * sampleRate, 0.f, static_cast<float>(bufferSize) * 0.45f);
            float phase = modPhase[channel];
            if (depthSamples > 0.f && modRateHz > 0.f) {
                phase += modRateHz * sampleTime;
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
            float modSamples = std::sin(tessellation::TWO_PI * lfoPhase) * depthSamples;
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
            res.wetL = tonedL;
            res.wetR = tonedR;

            float writeL = tonedL * feedback;
            float writeR = tonedR * feedback;
            if (!freeze) {
                writeL += inL;
                writeR += inR;
            }
            else {
                writeL = tonedL;
                writeR = tonedR;
            }

            writeL = rack::math::clamp(writeL, -10.f, 10.f);
            writeR = rack::math::clamp(writeR, -10.f, 10.f);

            bufferL[channel][writeIndex[channel]] = writeL;
            bufferR[channel][writeIndex[channel]] = writeR;
            writeIndex[channel] = (writeIndex[channel] + 1) % bufferSize;

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
                    constexpr float step = 1.f / 2048.f;
                    left = std::round(rack::math::clamp(left, -5.f, 5.f) / step) * step;
                    right = std::round(rack::math::clamp(right, -5.f, 5.f) / step) * step;
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
    rack::dsp::SchmittTrigger freezeButtonTrigger;
    rack::dsp::SchmittTrigger tapButtonTrigger;
    rack::dsp::PulseGenerator tempoLightPulse;
    float tapTimer = 0.f;
    float tempoPhase = 0.f;
    bool freezeLatched = false;

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
        configParam(TIME2_PARAM,
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
        configParam(TIME3_PARAM,
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
        configButton(FREEZE_PARAM, "Freeze");
        configButton(TAP_PARAM, "Tap tempo");

        shapetaker::ParameterHelper::configAudioInput(this, IN_L_INPUT, "Left audio");
        shapetaker::ParameterHelper::configAudioInput(this, IN_R_INPUT, "Right audio");
        shapetaker::ParameterHelper::configCVInput(this, TIME1_CV_INPUT, "Delay 1 time CV");
        shapetaker::ParameterHelper::configCVInput(this, TIME2_CV_INPUT, "Delay 2 time CV");
        shapetaker::ParameterHelper::configCVInput(this, TIME3_CV_INPUT, "Delay 3 time CV");
        shapetaker::ParameterHelper::configCVInput(this, REPEATS_CV_INPUT, "Repeats CV");
        shapetaker::ParameterHelper::configCVInput(this, MOD_CV_INPUT, "Mod depth CV");
        shapetaker::ParameterHelper::configGateInput(this, FREEZE_GATE_INPUT, "Freeze gate");

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

        if (freezeButtonTrigger.process(params[FREEZE_PARAM].getValue())) {
            freezeLatched = !freezeLatched;
        }
        bool freezeGate = inputs[FREEZE_GATE_INPUT].getVoltage() >= 1.f;
        bool freezeActive = freezeLatched || freezeGate;

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
                tempoLightPulse.trigger(0.06f);
            }
            tapTimer = 0.f;
        }

        float time1CV = inputs[TIME1_CV_INPUT].isConnected() ? inputs[TIME1_CV_INPUT].getVoltage() * 0.25f : 0.f;
        float delay1Seconds = rack::math::clamp(params[TIME1_PARAM].getValue() + time1CV,
            tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);

        float time2CV = inputs[TIME2_CV_INPUT].isConnected() ? inputs[TIME2_CV_INPUT].getVoltage() * 0.25f : 0.f;
        int subdiv2 = rack::math::clamp(static_cast<int>(std::round(params[SUBDIV2_PARAM].getValue())), 0, 5);
        float delay2Seconds = (subdiv2 == 5)
            ? rack::math::clamp(params[TIME2_PARAM].getValue() + time2CV,
                tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS)
            : rack::math::clamp(delay1Seconds * tessellation::subdivisionMultiplier(subdiv2),
                tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);

        float time3CV = inputs[TIME3_CV_INPUT].isConnected() ? inputs[TIME3_CV_INPUT].getVoltage() * 0.25f : 0.f;
        int subdiv3 = rack::math::clamp(static_cast<int>(std::round(params[SUBDIV3_PARAM].getValue())), 0, 5);
        float delay3Seconds = (subdiv3 == 5)
            ? rack::math::clamp(params[TIME3_PARAM].getValue() + time3CV,
                tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS)
            : rack::math::clamp(delay1Seconds * tessellation::subdivisionMultiplier(subdiv3),
                tessellation::MIN_DELAY_SECONDS, tessellation::MAX_DELAY_SECONDS);

        delayLines[0].setVoice(static_cast<int>(std::round(params[VOICE1_PARAM].getValue())));
        delayLines[1].setVoice(static_cast<int>(std::round(params[VOICE2_PARAM].getValue())));
        delayLines[2].setVoice(static_cast<int>(std::round(params[VOICE3_PARAM].getValue())));

        float repeatsMod = inputs[REPEATS_CV_INPUT].isConnected() ? inputs[REPEATS_CV_INPUT].getVoltage() * 0.1f : 0.f;
        float feedback1 = rack::math::clamp(params[REPEATS1_PARAM].getValue() + repeatsMod, 0.f, 0.97f);
        float feedback2 = rack::math::clamp(params[REPEATS2_PARAM].getValue() + repeatsMod, 0.f, 0.97f);
        float feedback3 = rack::math::clamp(params[REPEATS3_PARAM].getValue() + repeatsMod, 0.f, 0.97f);

        float tone1 = rack::math::clamp(params[TONE1_PARAM].getValue(), 0.f, 1.f);
        float tone2 = rack::math::clamp(params[TONE2_PARAM].getValue(), 0.f, 1.f);
        float tone3 = rack::math::clamp(params[TONE3_PARAM].getValue(), 0.f, 1.f);

        float mix1 = rack::math::clamp(params[MIX1_PARAM].getValue(), 0.f, 1.f);
        float mix2 = rack::math::clamp(params[MIX2_PARAM].getValue(), 0.f, 1.f);
        float mix3 = rack::math::clamp(params[MIX3_PARAM].getValue(), 0.f, 1.f);

        float modDepth = params[MOD_DEPTH_PARAM].getValue();
        if (inputs[MOD_CV_INPUT].isConnected()) {
            modDepth += inputs[MOD_CV_INPUT].getVoltage() * 0.1f;
        }
        modDepth = rack::math::clamp(modDepth, 0.f, 1.f);
        float modDepthSeconds = modDepth * tessellation::MAX_MOD_DEPTH_SECONDS;

        float modRate = rack::math::clamp(params[MOD_RATE_PARAM].getValue(), 0.f, 1.f);
        float modRateHz = 0.1f + modRate * 4.9f;

        int lChannels = inputs[IN_L_INPUT].getChannels();
        int rChannels = inputs[IN_R_INPUT].getChannels();
        int channels = std::max(std::max(lChannels, rChannels), 1);

        outputs[OUT_L_OUTPUT].setChannels(channels);
        outputs[OUT_R_OUTPUT].setChannels(channels);
        outputs[DELAY1_OUTPUT].setChannels(channels);
        outputs[DELAY2_OUTPUT].setChannels(channels);
        outputs[DELAY3_OUTPUT].setChannels(channels);

        std::array<float, 3> maxVU = {0.f, 0.f, 0.f};
        float wetGainComp = 1.f / std::max(1.f, mix1 + mix2 + mix3);
        wetGainComp = rack::math::clamp(wetGainComp, 0.5f, 1.f);
        float dryFactor = 1.f;

        // Hoist lambdas outside channel loop for better performance
        auto tapAvg = [](const StereoDelayLine::Result& res) {
            return (res.tapL + res.tapR) * 0.5f;
        };
        auto vuBrightness = [](const StereoDelayLine::Result& res) {
            return rack::math::clamp((std::fabs(res.tapL) + std::fabs(res.tapR)) * 0.1f, 0.f, 1.f);
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

            delayLines[0].setDelaySeconds(c, delay1Seconds);
            delayLines[1].setDelaySeconds(c, delay2Seconds);
            delayLines[2].setDelaySeconds(c, delay3Seconds);

            auto res1 = delayLines[0].process(c, inL, inR, feedback1, tone1,
                modDepthSeconds, modRateHz, freezeActive, args.sampleTime);
            auto res2 = delayLines[1].process(c, inL, inR, feedback2, tone2,
                modDepthSeconds, modRateHz, freezeActive, args.sampleTime);
            auto res3 = delayLines[2].process(c, inL, inR, feedback3, tone3,
                modDepthSeconds, modRateHz, freezeActive, args.sampleTime);

            float wetL = (res1.wetL * mix1 + res2.wetL * mix2 + res3.wetL * mix3) * wetGainComp;
            float wetR = (res1.wetR * mix1 + res2.wetR * mix2 + res3.wetR * mix3) * wetGainComp;

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

            maxVU[0] = std::max(maxVU[0], vuBrightness(res1));
            maxVU[1] = std::max(maxVU[1], vuBrightness(res2));
            maxVU[2] = std::max(maxVU[2], vuBrightness(res3));
        }

        tempoPhase += args.sampleTime;
        float tempoPeriod = rack::math::clamp(delay1Seconds, 0.05f, tessellation::MAX_DELAY_SECONDS);
        if (tempoPhase >= tempoPeriod) {
            tempoPhase -= tempoPeriod;
            tempoLightPulse.trigger(0.06f);
        }
        float tempoBrightness = tempoLightPulse.process(args.sampleTime) ? 1.f : 0.f;
        lights[TEMPO_LIGHT].setBrightness(tempoBrightness);

        lights[DELAY1_VU_LIGHT].setBrightness(maxVU[0]);
        lights[DELAY2_VU_LIGHT].setBrightness(maxVU[1]);
        lights[DELAY3_VU_LIGHT].setBrightness(maxVU[2]);
        lights[MIX1_LIGHT].setBrightness(mix1);
        lights[MIX2_LIGHT].setBrightness(mix2);
        lights[MIX3_LIGHT].setBrightness(mix3);
    }
};

#ifndef SHAPETAKER_TESSELLATION_NO_WIDGET
struct TessellationWidget : ModuleWidget {
    TessellationWidget(Tessellation* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Tessellation.svg")));

        shapetaker::ui::LayoutHelper::ScrewPositions::addStandardScrews<ScrewSilver>(
            this,
            shapetaker::ui::LayoutHelper::getModuleWidth(
                shapetaker::ui::LayoutHelper::ModuleWidth::WIDTH_14HP));

        auto mm = [](float x, float y) {
            return mm2px(Vec(x, y));
        };

        const float colLeft = 14.f;
        const float colCenter = 30.f;
        const float colRight = 46.f;
        const float colFar = 60.f;
        const float colUltra = 66.f;

        auto addMixLights = [&](float mixCol, float vuCol, int mixLightId, int vuLightId, float rowPos) {
            if (!module) return;
            addChild(createLightCentered<SmallLight<YellowLight>>(mm(mixCol - 8.f, rowPos), module, mixLightId));
            addChild(createLightCentered<SmallLight<RedLight>>(mm(vuCol + 8.f, rowPos), module, vuLightId));
        };

        const float rowTime1 = 12.f;
        const float rowMix1 = 21.f;
        const float rowTone1 = 30.f;
        const float rowSub2 = 40.f;
        const float rowMix2 = 49.f;
        const float rowTone2 = 58.f;
        const float rowSub3 = 68.f;
        const float rowMix3 = 77.f;
        const float rowTone3 = 86.f;
        const float rowMod = 96.f;
        const float rowFreeze = 104.f;
        const float rowJacksA = 112.f;
        const float rowJacksB = 120.f;
        const float rowJacksC = 128.f;

        addParam(createParamCentered<ShapetakerKnobAltMedium>(mm(colCenter, rowTime1), module, Tessellation::TIME1_PARAM));
        addParam(createParamCentered<rack::componentlibrary::LEDButton>(mm(colFar, rowTime1 - 4.f), module, Tessellation::TAP_PARAM));
        if (module) addChild(createLightCentered<SmallLight<GreenLight>>(mm(colFar + 5.f, rowTime1 - 4.f), module, Tessellation::TEMPO_LIGHT));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowMix1), module, Tessellation::MIX1_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colRight, rowMix1), module, Tessellation::REPEATS1_PARAM));
        addMixLights(colLeft, colRight, Tessellation::MIX1_LIGHT, Tessellation::DELAY1_VU_LIGHT, rowMix1);

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowTone1), module, Tessellation::TONE1_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(mm(colRight, rowTone1), module, Tessellation::VOICE1_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowSub2), module, Tessellation::SUBDIV2_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colRight, rowSub2), module, Tessellation::TIME2_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowMix2), module, Tessellation::MIX2_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colRight, rowMix2), module, Tessellation::REPEATS2_PARAM));
        addMixLights(colLeft, colRight, Tessellation::MIX2_LIGHT, Tessellation::DELAY2_VU_LIGHT, rowMix2);

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowTone2), module, Tessellation::TONE2_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(mm(colRight, rowTone2), module, Tessellation::VOICE2_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowSub3), module, Tessellation::SUBDIV3_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colRight, rowSub3), module, Tessellation::TIME3_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowMix3), module, Tessellation::MIX3_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colRight, rowMix3), module, Tessellation::REPEATS3_PARAM));
        addMixLights(colLeft, colRight, Tessellation::MIX3_LIGHT, Tessellation::DELAY3_VU_LIGHT, rowMix3);

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowTone3), module, Tessellation::TONE3_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(mm(colRight, rowTone3), module, Tessellation::VOICE3_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colLeft, rowMod), module, Tessellation::MOD_DEPTH_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colRight, rowMod), module, Tessellation::MOD_RATE_PARAM));

        addParam(createParamCentered<rack::componentlibrary::LEDButton>(mm(colLeft, rowFreeze), module, Tessellation::FREEZE_PARAM));

        addInput(createInputCentered<ShapetakerBNCPort>(mm(colLeft, rowJacksA), module, Tessellation::IN_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colCenter, rowJacksA), module, Tessellation::IN_R_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colRight, rowJacksA), module, Tessellation::TIME1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colFar, rowJacksA), module, Tessellation::TIME2_CV_INPUT));

        addInput(createInputCentered<ShapetakerBNCPort>(mm(colLeft, rowJacksB), module, Tessellation::TIME3_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colCenter, rowJacksB), module, Tessellation::REPEATS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colRight, rowJacksB), module, Tessellation::MOD_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colFar, rowJacksB), module, Tessellation::FREEZE_GATE_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colLeft, rowJacksC), module, Tessellation::OUT_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colCenter, rowJacksC), module, Tessellation::OUT_R_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colRight, rowJacksC), module, Tessellation::DELAY1_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colFar, rowJacksC), module, Tessellation::DELAY2_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colUltra, rowJacksC), module, Tessellation::DELAY3_OUTPUT));
    }
};

Model* modelTessellation = createModel<Tessellation, TessellationWidget>("Tessellation");
#endif
