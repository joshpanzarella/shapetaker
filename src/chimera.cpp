#include "plugin.hpp"
#include "ui/layout.hpp"
#include "dsp/polyphony.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace chimera {
    inline float dbToLinear(float db) {
        return std::pow(10.f, db / 20.f);
    }

    constexpr float MAX_LOOP_SECONDS = 32.f;
    constexpr float DEFAULT_BPM = 120.f;
}

struct Chimera : Module {
    static constexpr int kNumChannels = 4;
    static constexpr int kMaxPoly = shapetaker::PolyphonicProcessor::MAX_VOICES;

    enum ParamId {
        CH_LEVEL_PARAM,
        CH_PAN_PARAM = CH_LEVEL_PARAM + kNumChannels,
        CH_TILT_PARAM = CH_PAN_PARAM + kNumChannels,
        CH_MORPH_PARAM = CH_TILT_PARAM + kNumChannels,
        CH_BUS_PARAM = CH_MORPH_PARAM + kNumChannels,
        CH_LOOP_THRESHOLD_PARAM = CH_BUS_PARAM + kNumChannels,
        CH_LOOP_ARM_PARAM = CH_LOOP_THRESHOLD_PARAM + kNumChannels,
        SLOT_A_MODE_PARAM = CH_LOOP_ARM_PARAM + kNumChannels,
        SLOT_A_RATE_PARAM,
        SLOT_A_DEPTH_PARAM,
        SLOT_A_TEXTURE_PARAM,
        SLOT_B_MODE_PARAM,
        SLOT_B_RATE_PARAM,
        SLOT_B_DEPTH_PARAM,
        SLOT_B_TEXTURE_PARAM,
        MORPH_MASTER_PARAM,
        GLUE_THRESHOLD_PARAM,
        GLUE_RATIO_PARAM,
        GLUE_ATTACK_PARAM,
        GLUE_RELEASE_PARAM,
        GLUE_MIX_PARAM,
        GLUE_MAKEUP_PARAM,
        GLUE_HPF_PARAM,
        GLUE_SIDECHAIN_SRC_PARAM,
        LOOP_BARS_PARAM,
        CLOCK_BPM_PARAM,
        CLOCK_RUN_PARAM,
        CLOCK_MODE_PARAM,
        CLOCK_CLICK_PARAM,
        CLOCK_CLICK_LEVEL_PARAM,
        CLOCK_MIX_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        CH_INPUT_L,
        CH_INPUT_R = CH_INPUT_L + kNumChannels,
        SLOT_A_RATE_CV_INPUT = CH_INPUT_R + kNumChannels,
        SLOT_A_DEPTH_CV_INPUT,
        SLOT_A_TEXTURE_CV_INPUT,
        SLOT_B_RATE_CV_INPUT,
        SLOT_B_DEPTH_CV_INPUT,
        SLOT_B_TEXTURE_CV_INPUT,
        GESTURE_INPUT,
        GLUE_SC_INPUT,
        EXT_CLOCK_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        OUT_L_OUTPUT,
        OUT_R_OUTPUT,
        MORPH_A_OUTPUT,
        MORPH_B_OUTPUT,
        CLICK_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        LIGHTS_LEN
    };

    struct ChannelState {
        shapetaker::dsp::VoiceArray<float> tiltLowL;
        shapetaker::dsp::VoiceArray<float> tiltLowR;
    };

    struct LoopTrack {
        enum class State {
            Idle,
            Armed,
            Recording,
            Playing
        };

        State state = State::Idle;
        std::vector<float> bufferL;
        std::vector<float> bufferR;
        size_t recordIndex = 0;
        size_t playIndex = 0;
        size_t targetSamples = 0;
        size_t lengthSamples = 0;
        float detector = 0.f;

        void reset() {
            state = State::Idle;
            recordIndex = playIndex = targetSamples = lengthSamples = 0;
            detector = 0.f;
        }
    };

    struct MorphSlot {
        enum class Flavor {
            Argent,
            Aurum
        };

        float sampleRate = 44100.f;
        float phase = 0.f;
        float phase2 = 0.33f;
        float lagL = 0.f;
        float lagR = 0.f;
        float combL = 0.f;
        float combR = 0.f;
        float ap1L = 0.f;
        float ap1R = 0.f;
        float ap2L = 0.f;
        float ap2R = 0.f;
        float diffL = 0.f;
        float diffR = 0.f;
        float shimmerL = 0.f;
        float shimmerR = 0.f;
        int mode = 0;
        Flavor flavor = Flavor::Argent;

        void setSampleRate(float sr) {
            sampleRate = std::max(sr, 1.f);
        }

        void setMode(int newMode) {
            mode = rack::math::clamp(newMode, 0, 2);
        }

        void setFlavor(Flavor f) {
            flavor = f;
        }

        void reset() {
            phase = 0.f;
            phase2 = 0.33f;
            lagL = lagR = 0.f;
            combL = combR = 0.f;
            ap1L = ap1R = ap2L = ap2R = 0.f;
            diffL = diffR = 0.f;
            shimmerL = shimmerR = 0.f;
        }

        void process(float& left, float& right, float rateParam, float depthParam,
                     float textureParam, float sampleTime) {
            float rateHz = 0.15f + rack::math::clamp(rateParam, 0.f, 1.f) * 5.0f;
            float depth = rack::math::clamp(depthParam, 0.f, 1.f);
            float texture = rack::math::clamp(textureParam, 0.f, 1.f);

            phase += rateHz * sampleTime;
            if (phase >= 1.f) phase -= 1.f;
            phase2 += rateHz * (1.35f + 0.4f * texture) * sampleTime;
            if (phase2 >= 1.f) phase2 -= 1.f;

            float lfoA = std::sin(2.f * float(M_PI) * phase);
            float lfoB = std::sin(2.f * float(M_PI) * phase2);

            auto allpass = [](float input, float coeff, float& state) {
                float y = -coeff * input + state;
                state = input + coeff * y;
                return y;
            };

            auto tapeSat = [](float x, float drive) {
                return std::tanh(x * (1.f + drive * 1.2f));
            };

            if (flavor == Flavor::Argent) {
                switch (mode) {
                    case 0: { // Ensemble chorus
                        float smear = 0.15f + texture * 0.55f;
                        lagL = rack::math::crossfade(lagL, left, smear * 0.5f);
                        lagR = rack::math::crossfade(lagR, right, smear * 0.5f);
                        float mixAmt = depth * (0.45f + 0.4f * texture);
                        float voiceL = rack::math::crossfade(left, lagR, 0.5f + 0.5f * lfoA);
                        float voiceR = rack::math::crossfade(right, lagL, 0.5f + 0.5f * lfoB);
                        float spread = 0.3f + texture * 0.4f;
                        float crossL = left + (voiceR - left) * spread;
                        float crossR = right + (voiceL - right) * spread;
                        left = rack::math::crossfade(left, crossL, mixAmt);
                        right = rack::math::crossfade(right, crossR, mixAmt);
                        break;
                    }
                    case 1: { // Phasewash
                        float apCoeff = 0.1f + texture * 0.75f;
                        float sweep = depth * (0.4f + 0.4f * texture);
                        float phL = allpass(left + lfoA * sweep, apCoeff, ap1L);
                        float phR = allpass(right - lfoB * sweep, apCoeff, ap1R);
                        phL = allpass(phL, apCoeff * 0.6f, ap2L);
                        phR = allpass(phR, apCoeff * 0.6f, ap2R);
                        float mixAmt = 0.35f + depth * 0.5f;
                        left = rack::math::crossfade(left, phL, mixAmt);
                        right = rack::math::crossfade(right, phR, mixAmt);
                        break;
                    }
                    case 2:
                    default: { // Tape diffusion
                        float wowAmt = 0.002f + texture * 0.006f;
                        float wowL = left + (lagL - left) * (wowAmt * (1.2f + lfoA));
                        float wowR = right + (lagR - right) * (wowAmt * (1.2f + lfoB));
                        float smear = depth * (0.4f + 0.4f * texture);
                        float satL = tapeSat(wowL, texture);
                        float satR = tapeSat(wowR, texture);
                        diffL = rack::math::crossfade(diffL, satL, 0.2f + smear);
                        diffR = rack::math::crossfade(diffR, satR, 0.2f + smear);
                        left = rack::math::crossfade(left, diffL, smear);
                        right = rack::math::crossfade(right, diffR, smear);
                        break;
                    }
                }
            } else {
                switch (mode) {
                    case 0: { // Jet flanger
                        float feedback = 0.2f + texture * 0.55f;
                        combL = rack::math::crossfade(combL, left, 0.2f + texture * 0.5f);
                        combR = rack::math::crossfade(combR, right, 0.2f + texture * 0.5f);
                        float sweepL = left + (combL - left) * (0.5f + 0.5f * lfoA) * depth;
                        float sweepR = right + (combR - right) * (0.5f + 0.5f * lfoB) * depth;
                        left = rack::math::crossfade(left, sweepL + combR * feedback * 0.2f, depth);
                        right = rack::math::crossfade(right, sweepR + combL * feedback * 0.2f, depth);
                        break;
                    }
                    case 1: { // Trem / pan ribbon
                        float trem = 0.5f + 0.5f * lfoA;
                        float panLfo = 0.5f + 0.5f * lfoB;
                        float tremDepth = depth * (0.6f + 0.3f * texture);
                        float gainL = rack::math::crossfade(1.f, trem, tremDepth);
                        float gainR = rack::math::crossfade(1.f, 1.f - trem, tremDepth);
                        float panOffset = (panLfo - 0.5f) * (texture * 0.9f);
                        float panL = rack::math::clamp(1.f - panOffset, 0.f, 1.5f);
                        float panR = rack::math::clamp(1.f + panOffset, 0.f, 1.5f);
                        left *= gainL * panL;
                        right *= gainR * panR;
                        break;
                    }
                    case 2:
                    default: { // Shimmer bloom
                        float soak = 0.12f + texture * 0.4f;
                        shimmerL = rack::math::crossfade(shimmerL, left + right, soak);
                        shimmerR = rack::math::crossfade(shimmerR, right + left, soak);
                        float octave = shimmerL * (0.25f + 0.35f * texture);
                        float fifth = shimmerR * (0.18f + 0.32f * texture);
                        float mixAmt = depth * (0.45f + 0.4f * texture);
                        left = rack::math::crossfade(left, left + octave + (shimmerR - shimmerL) * 0.15f, mixAmt);
                        right = rack::math::crossfade(right, right + fifth - (shimmerR - shimmerL) * 0.15f, mixAmt);
                        break;
                    }
                }
            }
        }
    };

    struct GlueCompressor {
        float sampleRate = 44100.f;
        float env = 0.f;
        float scHpState = 0.f;

        void setSampleRate(float sr) {
            sampleRate = std::max(sr, 1.f);
        }

        void reset() {
            env = 0.f;
            scHpState = 0.f;
        }

        void process(float& left, float& right, float sidechainSample, float attackParam,
                     float releaseParam, float thresholdDb, float makeupDb, float mix,
                     float ratio, int hpfMode) {
            float attackMs = rack::math::rescale(rack::math::clamp(attackParam, 0.f, 1.f), 0.f, 1.f, 0.1f, 30.f);
            float releaseMs = rack::math::rescale(rack::math::clamp(releaseParam, 0.f, 1.f), 0.f, 1.f, 50.f, 1500.f);

            float cutoff = 0.f;
            if (hpfMode == 1) cutoff = 60.f;
            else if (hpfMode == 2) cutoff = 120.f;
            if (cutoff > 0.f) {
                float coeff = std::exp(-2.f * float(M_PI) * cutoff / sampleRate);
                scHpState = rack::math::crossfade(sidechainSample, scHpState, coeff);
                sidechainSample = sidechainSample - scHpState;
            }

            float attackCoeff = std::exp(-1.f / std::max(attackMs * 0.001f * sampleRate, 1e-6f));
            float releaseCoeff = std::exp(-1.f / std::max(releaseMs * 0.001f * sampleRate, 1e-6f));
            float detector = std::fabs(sidechainSample);
            if (detector > env) {
                env = attackCoeff * env + (1.f - attackCoeff) * detector;
            } else {
                env = releaseCoeff * env + (1.f - releaseCoeff) * detector;
            }

            float threshold = chimera::dbToLinear(thresholdDb);
            float gain = 1.f;
            if (threshold > 0.f && env > threshold) {
                float over = env / threshold;
                float overDb = 20.f * std::log10(over + 1e-12f);
                float reducedDb = overDb * (1.f - 1.f / std::max(ratio, 1.f));
                gain = chimera::dbToLinear(-reducedDb);
            }

            float makeup = chimera::dbToLinear(makeupDb);
            float wetL = left * gain * makeup;
            float wetR = right * gain * makeup;
            float blend = rack::math::clamp(mix, 0.f, 1.f);
            left = rack::math::crossfade(left, wetL, blend);
            right = rack::math::crossfade(right, wetR, blend);
        }
    };

    std::array<ChannelState, kNumChannels> channelState{};
    std::array<LoopTrack, kNumChannels> loopTracks{};
    std::array<MorphSlot, kMaxPoly> slotAVoices{};
    std::array<MorphSlot, kMaxPoly> slotBVoices{};
    std::array<GlueCompressor, kMaxPoly> glueVoices{};

    struct ClockState {
        float phase = 0.f;
        float sampleRate = 44100.f;
        float effectiveBpm = chimera::DEFAULT_BPM;
        float timeSinceLastTick = 0.f;
        rack::dsp::SchmittTrigger extTrigger;
        rack::dsp::PulseGenerator clickPulse;
    } clockState;

    float sampleRate = 44100.f;
    size_t maxLoopSamples = 0;

    Chimera() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        for (int i = 0; i < kNumChannels; ++i) {
            configParam(CH_LEVEL_PARAM + i, -60.f, 6.f, -6.f,
                        string::f("Channel %d level", i + 1), " dB");
            configParam(CH_PAN_PARAM + i, -1.f, 1.f, 0.f,
                        string::f("Channel %d pan", i + 1));
            configParam(CH_TILT_PARAM + i, -1.f, 1.f, 0.f,
                        string::f("Channel %d tilt", i + 1));
            configParam(CH_MORPH_PARAM + i, 0.f, 1.f, 0.5f,
                        string::f("Channel %d morph mix", i + 1));
            configSwitch(CH_BUS_PARAM + i, 0.f, 2.f, 1.f,
                         string::f("Channel %d bus assign", i + 1),
                         {"Bus A", "A+B", "Bus B"});
            configParam(CH_LOOP_THRESHOLD_PARAM + i, 0.01f, 1.f, 0.2f,
                        string::f("Channel %d loop threshold", i + 1));
            configSwitch(CH_LOOP_ARM_PARAM + i, 0.f, 1.f, 0.f,
                         string::f("Channel %d loop arm", i + 1),
                         {"Off", "Arm"});
            configInput(CH_INPUT_L + i, string::f("Channel %d left", i + 1));
            configInput(CH_INPUT_R + i, string::f("Channel %d right", i + 1));
        }

        configSwitch(SLOT_A_MODE_PARAM, 0.f, 2.f, 0.f, "Slot A mode",
                     {"Ensemble", "Phasewash", "Tape"});
        configParam(SLOT_A_RATE_PARAM, 0.f, 1.f, 0.4f, "Slot A rate");
        configParam(SLOT_A_DEPTH_PARAM, 0.f, 1.f, 0.6f, "Slot A depth");
        configParam(SLOT_A_TEXTURE_PARAM, 0.f, 1.f, 0.5f, "Slot A texture");
        configInput(SLOT_A_RATE_CV_INPUT, "Slot A rate CV");
        configInput(SLOT_A_DEPTH_CV_INPUT, "Slot A depth CV");
        configInput(SLOT_A_TEXTURE_CV_INPUT, "Slot A texture CV");

        configSwitch(SLOT_B_MODE_PARAM, 0.f, 2.f, 1.f, "Slot B mode",
                     {"Jet", "Trem/Pan", "Shimmer"});
        configParam(SLOT_B_RATE_PARAM, 0.f, 1.f, 0.55f, "Slot B rate");
        configParam(SLOT_B_DEPTH_PARAM, 0.f, 1.f, 0.65f, "Slot B depth");
        configParam(SLOT_B_TEXTURE_PARAM, 0.f, 1.f, 0.45f, "Slot B texture");
        configInput(SLOT_B_RATE_CV_INPUT, "Slot B rate CV");
        configInput(SLOT_B_DEPTH_CV_INPUT, "Slot B depth CV");
        configInput(SLOT_B_TEXTURE_CV_INPUT, "Slot B texture CV");

        configParam(MORPH_MASTER_PARAM, 0.f, 1.f, 0.5f, "Global morph mix");

        configParam(GLUE_THRESHOLD_PARAM, -36.f, 0.f, -14.f, "Glue threshold", " dB");
        configSwitch(GLUE_RATIO_PARAM, 0.f, 2.f, 0.f, "Glue ratio",
                     {"2:1", "4:1", "Crush"});
        configParam(GLUE_ATTACK_PARAM, 0.f, 1.f, 0.3f, "Glue attack");
        configParam(GLUE_RELEASE_PARAM, 0.f, 1.f, 0.5f, "Glue release");
        configParam(GLUE_MIX_PARAM, 0.f, 1.f, 0.5f, "Glue dry/wet");
        configParam(GLUE_MAKEUP_PARAM, 0.f, 18.f, 3.f, "Glue makeup", " dB");
        configSwitch(GLUE_HPF_PARAM, 0.f, 2.f, 0.f, "Glue sidechain HPF",
                     {"Off", "60 Hz", "120 Hz"});
        configSwitch(GLUE_SIDECHAIN_SRC_PARAM, 0.f, 2.f, 0.f, "Glue sidechain source",
                     {"Mix", "Morph", "Mix+Morph"});
        configSwitch(LOOP_BARS_PARAM, 0.f, 2.f, 1.f, "Loop length",
                     {"1 bar", "2 bars", "4 bars"});
        configParam(CLOCK_BPM_PARAM, 40.f, 200.f, 120.f, "Clock tempo", " BPM");
        configSwitch(CLOCK_RUN_PARAM, 0.f, 1.f, 1.f, "Clock run",
                     {"Stop", "Run"});
        configSwitch(CLOCK_MODE_PARAM, 0.f, 1.f, 0.f, "Clock source",
                     {"Internal", "External"});
        configSwitch(CLOCK_CLICK_PARAM, 0.f, 1.f, 1.f, "Click enable",
                     {"Off", "On"});
        configParam(CLOCK_CLICK_LEVEL_PARAM, 0.f, 1.5f, 0.75f, "Click level", " V");
        configSwitch(CLOCK_MIX_PARAM, 0.f, 1.f, 0.f, "Click to mix",
                     {"Off", "On"});
        configInput(GLUE_SC_INPUT, "Glue external sidechain");
        configInput(GESTURE_INPUT, "Gesture CV");
        configInput(EXT_CLOCK_INPUT, "External clock");

        configOutput(OUT_L_OUTPUT, "Mix left");
        configOutput(OUT_R_OUTPUT, "Mix right");
        configOutput(MORPH_A_OUTPUT, "Slot A return");
        configOutput(MORPH_B_OUTPUT, "Slot B return");
        configOutput(CLICK_OUTPUT, "Metronome click");

        for (int v = 0; v < kMaxPoly; ++v) {
            slotAVoices[v].setFlavor(MorphSlot::Flavor::Argent);
            slotBVoices[v].setFlavor(MorphSlot::Flavor::Aurum);
        }

        onSampleRateChange();
    }

    void onSampleRateChange() override {
        float sr = APP && APP->engine ? APP->engine->getSampleRate() : 44100.f;
        sampleRate = sr;
        for (auto& slot : slotAVoices) {
            slot.setSampleRate(sr);
            slot.reset();
        }
        for (auto& slot : slotBVoices) {
            slot.setSampleRate(sr);
            slot.reset();
        }
        for (auto& comp : glueVoices) {
            comp.setSampleRate(sr);
            comp.reset();
        }
        clockState.sampleRate = sr;
        clockState.phase = 0.f;
        clockState.timeSinceLastTick = 0.f;
        clockState.effectiveBpm = rack::math::clamp(params[CLOCK_BPM_PARAM].getValue(), 40.f, 200.f);
        maxLoopSamples = std::max<size_t>(1, static_cast<size_t>(chimera::MAX_LOOP_SECONDS * sampleRate));
        for (auto& track : loopTracks) {
            track.bufferL.assign(maxLoopSamples, 0.f);
            track.bufferR.assign(maxLoopSamples, 0.f);
            track.reset();
        }
    }

    void process(const ProcessArgs& args) override {
        const float sampleTime = args.sampleTime;

        float bpmParam = rack::math::clamp(params[CLOCK_BPM_PARAM].getValue(), 40.f, 200.f);
        bool clockRun = params[CLOCK_RUN_PARAM].getValue() > 0.5f;
        bool useExternalClock = params[CLOCK_MODE_PARAM].getValue() > 0.5f && inputs[EXT_CLOCK_INPUT].isConnected();
        clockState.timeSinceLastTick += sampleTime;
        bool clockTick = false;

        if (useExternalClock) {
            if (clockState.extTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage())) {
                clockTick = true;
                if (clockState.timeSinceLastTick > 1e-3f) {
                    float extBpm = 60.f / clockState.timeSinceLastTick;
                    clockState.effectiveBpm = rack::math::clamp(extBpm, 30.f, 240.f);
                }
                clockState.timeSinceLastTick = 0.f;
            }
        } else if (clockRun) {
            float freq = bpmParam / 60.f;
            clockState.phase += freq * sampleTime;
            if (clockState.phase >= 1.f) {
                clockState.phase -= 1.f;
                clockTick = true;
                clockState.effectiveBpm = bpmParam;
            }
        } else {
            clockState.phase = 0.f;
            clockState.effectiveBpm = bpmParam;
            clockState.timeSinceLastTick = 0.f;
        }

        if (clockTick) {
            clockState.clickPulse.trigger(0.002f);
            clockState.timeSinceLastTick = 0.f;
        }

        bool clickGate = clockState.clickPulse.process(sampleTime);
        bool clickEnabled = params[CLOCK_CLICK_PARAM].getValue() > 0.5f;
        float clickLevel = rack::math::clamp(params[CLOCK_CLICK_LEVEL_PARAM].getValue(), 0.f, 1.5f);
        float clickVoltage = (clickGate && clickEnabled) ? clickLevel : 0.f;
        outputs[CLICK_OUTPUT].setChannels(1);
        outputs[CLICK_OUTPUT].setVoltage(clickVoltage);
        bool clickToMix = params[CLOCK_MIX_PARAM].getValue() > 0.5f;
        float clickContribution = (clickEnabled && clickToMix) ? clickVoltage : 0.f;

        size_t loopTargetSamples = std::max<size_t>(1, std::min(getLoopTargetSamples(), maxLoopSamples));

        struct ChannelIO {
            bool hasL = false;
            bool hasR = false;
            int channels = 0;
            bool active = false;
        };

        std::array<ChannelIO, kNumChannels> channelCfg{};
        std::array<bool, kNumChannels> channelActiveForMix{};
        bool anyLoopActive = false;
        int voiceCount = 0;

        for (int ch = 0; ch < kNumChannels; ++ch) {
            auto& cfg = channelCfg[ch];
            cfg.hasL = inputs[CH_INPUT_L + ch].isConnected();
            cfg.hasR = inputs[CH_INPUT_R + ch].isConnected();
            int lChannels = cfg.hasL ? inputs[CH_INPUT_L + ch].getChannels() : 0;
            int rChannels = cfg.hasR ? inputs[CH_INPUT_R + ch].getChannels() : 0;
            cfg.channels = std::max(lChannels, rChannels);
            cfg.active = cfg.hasL || cfg.hasR;
            voiceCount = std::max(voiceCount, cfg.channels);
            anyLoopActive |= loopTracks[ch].state != LoopTrack::State::Idle;
        }
        if (anyLoopActive) {
            voiceCount = std::max(voiceCount, 1);
        }
        voiceCount = rack::math::clamp(voiceCount, 1, kMaxPoly);

        std::array<std::array<float, kMaxPoly>, kNumChannels> channelVoiceOutL{};
        std::array<std::array<float, kMaxPoly>, kNumChannels> channelVoiceOutR{};
        std::array<float, kNumChannels> channelAggregateL{};
        std::array<float, kNumChannels> channelAggregateR{};
        std::array<float, kNumChannels> channelDetectorSum{};
        std::array<float, kNumChannels> channelMorphMix{};
        std::array<int, kNumChannels> channelBusMode{};

        for (int ch = 0; ch < kNumChannels; ++ch) {
            channelMorphMix[ch] = rack::math::clamp(params[CH_MORPH_PARAM + ch].getValue(), 0.f, 1.f);
            channelBusMode[ch] = rack::math::clamp(static_cast<int>(std::round(params[CH_BUS_PARAM + ch].getValue())), 0, 2);
        }

        for (int ch = 0; ch < kNumChannels; ++ch) {
            auto& cfg = channelCfg[ch];
            auto& loop = loopTracks[ch];
            bool processChannel = cfg.active || loop.state != LoopTrack::State::Idle;
            if (!processChannel) {
                continue;
            }
            channelActiveForMix[ch] = true;

            float tiltParam = params[CH_TILT_PARAM + ch].getValue();
            float tiltDark = rack::math::clamp(-tiltParam, 0.f, 1.f);
            float tiltBright = rack::math::clamp(tiltParam, 0.f, 1.f);
            float toneCutoff = 400.f + std::fabs(tiltParam) * 3000.f;
            float lpCoeff = std::exp(-2.f * float(M_PI) * toneCutoff * sampleTime);

            float levelDb = params[CH_LEVEL_PARAM + ch].getValue();
            float gain = chimera::dbToLinear(levelDb);

            float pan = params[CH_PAN_PARAM + ch].getValue();
            float panOffset = rack::math::clamp(pan, -1.f, 1.f);
            float leftGain = rack::math::clamp(1.f - 0.5f * panOffset, 0.f, 1.5f);
            float rightGain = rack::math::clamp(1.f + 0.5f * panOffset, 0.f, 1.5f);

            for (int voice = 0; voice < voiceCount; ++voice) {
                float inL = 0.f;
                float inR = 0.f;
                if (cfg.hasL) {
                    inL = inputs[CH_INPUT_L + ch].getVoltage(voice);
                }
                if (cfg.hasR) {
                    inR = inputs[CH_INPUT_R + ch].getVoltage(voice);
                }
                if (!cfg.hasL && cfg.hasR) {
                    inL = inR;
                } else if (!cfg.hasR && cfg.hasL) {
                    inR = inL;
                }

                auto& state = channelState[ch];
                float& tiltLowL = state.tiltLowL[voice];
                float& tiltLowR = state.tiltLowR[voice];
                tiltLowL = rack::math::crossfade(inL, tiltLowL, lpCoeff);
                tiltLowR = rack::math::crossfade(inR, tiltLowR, lpCoeff);
                float lowL = tiltLowL;
                float lowR = tiltLowR;
                float highL = inL - lowL;
                float highR = inR - lowR;

                float shapedL = inL;
                float shapedR = inR;
                if (tiltDark > 0.f) {
                    shapedL = rack::math::crossfade(shapedL, lowL, tiltDark);
                    shapedR = rack::math::crossfade(shapedR, lowR, tiltDark);
                }
                if (tiltBright > 0.f) {
                    shapedL = rack::math::crossfade(shapedL, highL, tiltBright);
                    shapedR = rack::math::crossfade(shapedR, highR, tiltBright);
                }

                shapedL *= gain;
                shapedR *= gain;

                channelDetectorSum[ch] += 0.5f * (std::fabs(shapedL) + std::fabs(shapedR));

                float outL = shapedL * leftGain;
                float outR = shapedR * rightGain;

                channelVoiceOutL[ch][voice] = outL;
                channelVoiceOutR[ch][voice] = outR;
                channelAggregateL[ch] += outL;
                channelAggregateR[ch] += outR;
            }

            float detectorSample = channelDetectorSum[ch] / std::max(1, cfg.channels);
            loop.detector = 0.995f * loop.detector + 0.005f * detectorSample;
            bool loopArmed = params[CH_LOOP_ARM_PARAM + ch].getValue() > 0.5f;

            if (!loopArmed) {
                if (loop.state != LoopTrack::State::Idle) {
                    loop.reset();
                }
            } else {
                if (loop.state == LoopTrack::State::Idle) {
                    loop.state = LoopTrack::State::Armed;
                }
                if (loop.state == LoopTrack::State::Armed) {
                    float thresholdVoltage = rack::math::clamp(params[CH_LOOP_THRESHOLD_PARAM + ch].getValue(), 0.01f, 1.f) * 5.f;
                    if (loop.detector >= thresholdVoltage) {
                        loop.state = LoopTrack::State::Recording;
                        loop.recordIndex = 0;
                        loop.playIndex = 0;
                        loop.lengthSamples = 0;
                        loop.targetSamples = std::max<size_t>(1, std::min(loopTargetSamples, maxLoopSamples));
                    }
                }
            }

            if (loop.state == LoopTrack::State::Recording) {
                size_t limit = std::min(loop.targetSamples, maxLoopSamples);
                if (loop.recordIndex < limit && loop.recordIndex < loop.bufferL.size()) {
                    loop.bufferL[loop.recordIndex] = channelAggregateL[ch];
                    loop.bufferR[loop.recordIndex] = channelAggregateR[ch];
                    loop.recordIndex++;
                }
                if (loop.recordIndex >= limit || !loopArmed) {
                    if (loop.recordIndex > 0) {
                        loop.lengthSamples = loop.recordIndex;
                        loop.playIndex = 0;
                        loop.state = LoopTrack::State::Playing;
                    } else {
                        loop.reset();
                        if (loopArmed) {
                            loop.state = LoopTrack::State::Armed;
                        }
                    }
                }
            }

            if (loop.state == LoopTrack::State::Playing && loop.lengthSamples > 0) {
                float loopL = loop.bufferL[loop.playIndex];
                float loopR = loop.bufferR[loop.playIndex];
                loop.playIndex = (loop.playIndex + 1) % loop.lengthSamples;

                channelAggregateL[ch] = loopL;
                channelAggregateR[ch] = loopR;
                for (int voice = 0; voice < voiceCount; ++voice) {
                    channelVoiceOutL[ch][voice] = loopL;
                    channelVoiceOutR[ch][voice] = loopR;
                }
            }
        }

        std::array<float, kMaxPoly> busAL{};
        std::array<float, kMaxPoly> busAR{};
        std::array<float, kMaxPoly> busBL{};
        std::array<float, kMaxPoly> busBR{};
        std::array<float, kMaxPoly> morphSendAL{};
        std::array<float, kMaxPoly> morphSendAR{};
        std::array<float, kMaxPoly> morphSendBL{};
        std::array<float, kMaxPoly> morphSendBR{};

        for (int voice = 0; voice < voiceCount; ++voice) {
            for (int ch = 0; ch < kNumChannels; ++ch) {
                if (!channelActiveForMix[ch]) {
                    continue;
                }

                float outL = channelVoiceOutL[ch][voice];
                float outR = channelVoiceOutR[ch][voice];

                auto accumulateBus = [&](float& busL, float& busR) {
                    busL += outL;
                    busR += outR;
                };

                int busMode = channelBusMode[ch];
                if (busMode == 0) {
                    accumulateBus(busAL[voice], busAR[voice]);
                } else if (busMode == 1) {
                    accumulateBus(busAL[voice], busAR[voice]);
                    accumulateBus(busBL[voice], busBR[voice]);
                } else {
                    accumulateBus(busBL[voice], busBR[voice]);
                }

                float morphMix = channelMorphMix[ch];
                morphSendAL[voice] += outL * (1.f - morphMix);
                morphSendAR[voice] += outR * (1.f - morphMix);
                morphSendBL[voice] += outL * morphMix;
                morphSendBR[voice] += outR * morphMix;
            }
        }

        auto readNormalized = [&](float base, int inputId) {
            float value = base;
            if (inputId >= 0 && inputId < INPUTS_LEN && inputs[inputId].isConnected()) {
                value += inputs[inputId].getVoltage() * 0.1f;
            }
            return rack::math::clamp(value, 0.f, 1.f);
        };

        float slotARate = readNormalized(params[SLOT_A_RATE_PARAM].getValue(), SLOT_A_RATE_CV_INPUT);
        float slotADepth = readNormalized(params[SLOT_A_DEPTH_PARAM].getValue(), SLOT_A_DEPTH_CV_INPUT);
        float slotATexture = readNormalized(params[SLOT_A_TEXTURE_PARAM].getValue(), SLOT_A_TEXTURE_CV_INPUT);

        float slotBRate = readNormalized(params[SLOT_B_RATE_PARAM].getValue(), SLOT_B_RATE_CV_INPUT);
        float slotBDepth = readNormalized(params[SLOT_B_DEPTH_PARAM].getValue(), SLOT_B_DEPTH_CV_INPUT);
        float slotBTexture = readNormalized(params[SLOT_B_TEXTURE_PARAM].getValue(), SLOT_B_TEXTURE_CV_INPUT);

        int slotAMode = rack::math::clamp(static_cast<int>(std::round(params[SLOT_A_MODE_PARAM].getValue())), 0, 2);
        int slotBMode = rack::math::clamp(static_cast<int>(std::round(params[SLOT_B_MODE_PARAM].getValue())), 0, 2);
        for (int v = 0; v < kMaxPoly; ++v) {
            slotAVoices[v].setMode(slotAMode);
            slotBVoices[v].setMode(slotBMode);
        }

        float morphMaster = params[MORPH_MASTER_PARAM].getValue();
        if (inputs[GESTURE_INPUT].isConnected()) {
            morphMaster = rack::math::clamp(morphMaster + inputs[GESTURE_INPUT].getVoltage() * 0.05f, 0.f, 1.f);
        }

        std::array<float, kMaxPoly> mixOutL{};
        std::array<float, kMaxPoly> mixOutR{};

        int scMode = rack::math::clamp(static_cast<int>(std::round(params[GLUE_SIDECHAIN_SRC_PARAM].getValue())), 0, 2);
        static constexpr float ratioMap[3] = {2.f, 4.f, 10.f};
        int ratioIndex = rack::math::clamp(static_cast<int>(std::round(params[GLUE_RATIO_PARAM].getValue())), 0, 2);
        float ratio = ratioMap[ratioIndex];
        int hpfMode = rack::math::clamp(static_cast<int>(std::round(params[GLUE_HPF_PARAM].getValue())), 0, 2);

        for (int voice = 0; voice < voiceCount; ++voice) {
            slotAVoices[voice].process(morphSendAL[voice], morphSendAR[voice],
                                       slotARate, slotADepth, slotATexture, sampleTime);
            slotBVoices[voice].process(morphSendBL[voice], morphSendBR[voice],
                                       slotBRate, slotBDepth, slotBTexture, sampleTime);

            float morphL = rack::math::crossfade(morphSendAL[voice], morphSendBL[voice], morphMaster);
            float morphR = rack::math::crossfade(morphSendAR[voice], morphSendBR[voice], morphMaster);

            float mixL = busAL[voice] + busBL[voice] + morphL + clickContribution;
            float mixR = busAR[voice] + busBR[voice] + morphR + clickContribution;

            float mixEnergy = 0.5f * (std::fabs(mixL) + std::fabs(mixR));
            float morphEnergy = 0.5f * (std::fabs(morphL) + std::fabs(morphR));
            float scSource = 0.f;
            if (scMode == 0) scSource = mixEnergy;
            else if (scMode == 1) scSource = morphEnergy;
            else scSource = 0.5f * mixEnergy + 0.5f * morphEnergy;
            if (inputs[GLUE_SC_INPUT].isConnected()) {
                scSource = 0.5f * scSource + 0.5f * std::fabs(inputs[GLUE_SC_INPUT].getVoltage(voice));
            }

            glueVoices[voice].process(mixL, mixR, scSource,
                                      params[GLUE_ATTACK_PARAM].getValue(),
                                      params[GLUE_RELEASE_PARAM].getValue(),
                                      params[GLUE_THRESHOLD_PARAM].getValue(),
                                      params[GLUE_MAKEUP_PARAM].getValue(),
                                      params[GLUE_MIX_PARAM].getValue(),
                                      ratio,
                                      hpfMode);

            mixOutL[voice] = mixL;
            mixOutR[voice] = mixR;
        }

        outputs[OUT_L_OUTPUT].setChannels(voiceCount);
        outputs[OUT_R_OUTPUT].setChannels(voiceCount);
        for (int voice = 0; voice < voiceCount; ++voice) {
            outputs[OUT_L_OUTPUT].setVoltage(mixOutL[voice], voice);
            outputs[OUT_R_OUTPUT].setVoltage(mixOutR[voice], voice);
        }

        outputs[MORPH_A_OUTPUT].setChannels(voiceCount * 2);
        outputs[MORPH_B_OUTPUT].setChannels(voiceCount * 2);
        for (int voice = 0; voice < voiceCount; ++voice) {
            outputs[MORPH_A_OUTPUT].setVoltage(morphSendAL[voice], 2 * voice);
            outputs[MORPH_A_OUTPUT].setVoltage(morphSendAR[voice], 2 * voice + 1);
            outputs[MORPH_B_OUTPUT].setVoltage(morphSendBL[voice], 2 * voice);
            outputs[MORPH_B_OUTPUT].setVoltage(morphSendBR[voice], 2 * voice + 1);
        }
    }

    size_t getLoopTargetSamples() {
        static constexpr int barMultipliers[3] = {1, 2, 4};
        int index = rack::math::clamp(static_cast<int>(std::round(params[LOOP_BARS_PARAM].getValue())), 0, 2);
        float bpm = std::max(clockState.effectiveBpm, 30.f);
        float beatsPerBar = 4.f;
        float secondsPerBeat = 60.f / bpm;
        float seconds = secondsPerBeat * beatsPerBar * barMultipliers[index];
        seconds = rack::math::clamp(seconds, 0.1f, chimera::MAX_LOOP_SECONDS);
        return static_cast<size_t>(seconds * sampleRate);
    }
};

struct ChimeraWidget : ModuleWidget {
    ChimeraWidget(Chimera* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Chimera.svg")));

        shapetaker::ui::LayoutHelper::ScrewPositions::addStandardScrews<ScrewBlack>(this, box.size.x);

        shapetaker::ui::LayoutHelper::PanelSVGParser parser(
            asset::plugin(pluginInstance, "res/panels/Chimera.svg"));
        auto centerPx = shapetaker::ui::LayoutHelper::createCenterPxHelper(parser);

        const std::array<const char*, Chimera::kNumChannels> levelIds = {"ch1_level", "ch2_level", "ch3_level", "ch4_level"};
        const std::array<const char*, Chimera::kNumChannels> panIds = {"ch1_pan", "ch2_pan", "ch3_pan", "ch4_pan"};
        const std::array<const char*, Chimera::kNumChannels> morphIds = {"ch1_morph", "ch2_morph", "ch3_morph", "ch4_morph"};
        const std::array<const char*, Chimera::kNumChannels> tiltIds = {"ch1_tilt", "ch2_tilt", "ch3_tilt", "ch4_tilt"};
        const std::array<const char*, Chimera::kNumChannels> busIds = {"ch1_bus", "ch2_bus", "ch3_bus", "ch4_bus"};
        const std::array<const char*, Chimera::kNumChannels> loopThresholdIds = {"ch1_loop_thresh", "ch2_loop_thresh", "ch3_loop_thresh", "ch4_loop_thresh"};
        const std::array<const char*, Chimera::kNumChannels> loopArmIds = {"ch1_loop_arm", "ch2_loop_arm", "ch3_loop_arm", "ch4_loop_arm"};
        const std::array<const char*, Chimera::kNumChannels> inputLeftIds = {"ch1_in_l", "ch2_in_l", "ch3_in_l", "ch4_in_l"};
        const std::array<const char*, Chimera::kNumChannels> inputRightIds = {"ch1_in_r", "ch2_in_r", "ch3_in_r", "ch4_in_r"};
        const std::array<float, Chimera::kNumChannels> channelFallbackX = {20.f, 48.f, 76.f, 104.f};

        for (int i = 0; i < Chimera::kNumChannels; ++i) {
            addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltLarge>(
                centerPx(levelIds[i], channelFallbackX[i], 20.f), module, Chimera::CH_LEVEL_PARAM + i));
            addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
                centerPx(panIds[i], channelFallbackX[i], 44.f), module, Chimera::CH_PAN_PARAM + i));
            addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
                centerPx(morphIds[i], channelFallbackX[i], 68.f), module, Chimera::CH_MORPH_PARAM + i));
            addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
                centerPx(tiltIds[i], channelFallbackX[i], 92.f), module, Chimera::CH_TILT_PARAM + i));
            addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
                centerPx(busIds[i], channelFallbackX[i], 116.f), module, Chimera::CH_BUS_PARAM + i));
            addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
                centerPx(loopThresholdIds[i], channelFallbackX[i], 80.f), module, Chimera::CH_LOOP_THRESHOLD_PARAM + i));
            addParam(createParamCentered<rack::componentlibrary::CKSS>(
                centerPx(loopArmIds[i], channelFallbackX[i], 102.f), module, Chimera::CH_LOOP_ARM_PARAM + i));

            addInput(createInputCentered<ShapetakerBNCPort>(
                centerPx(inputLeftIds[i], channelFallbackX[i] - 6.f, 110.f), module, Chimera::CH_INPUT_L + i));
            addInput(createInputCentered<ShapetakerBNCPort>(
                centerPx(inputRightIds[i], channelFallbackX[i] + 6.f, 110.f), module, Chimera::CH_INPUT_R + i));
        }

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("slot_a_rate", 120.f, 22.f), module, Chimera::SLOT_A_RATE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("slot_a_depth", 120.f, 44.f), module, Chimera::SLOT_A_DEPTH_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("slot_a_texture", 120.f, 66.f), module, Chimera::SLOT_A_TEXTURE_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("slot_b_rate", 138.f, 22.f), module, Chimera::SLOT_B_RATE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("slot_b_depth", 138.f, 44.f), module, Chimera::SLOT_B_DEPTH_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("slot_b_texture", 138.f, 66.f), module, Chimera::SLOT_B_TEXTURE_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("morph_master_knob", 156.f, 90.f), module, Chimera::MORPH_MASTER_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(
            centerPx("glue_threshold_knob", 170.f, 90.f), module, Chimera::GLUE_THRESHOLD_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("glue_attack_knob", 156.f, 110.f), module, Chimera::GLUE_ATTACK_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("glue_release_knob", 170.f, 110.f), module, Chimera::GLUE_RELEASE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("glue_mix_knob", 156.f, 126.f), module, Chimera::GLUE_MIX_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("glue_makeup_knob", 170.f, 126.f), module, Chimera::GLUE_MAKEUP_PARAM));

        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("slot_a_mode_switch", 120.f, 32.f), module, Chimera::SLOT_A_MODE_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("slot_b_mode_switch", 138.f, 32.f), module, Chimera::SLOT_B_MODE_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("glue_ratio_switch", 156.f, 54.f), module, Chimera::GLUE_RATIO_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("glue_hpf_switch", 170.f, 54.f), module, Chimera::GLUE_HPF_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("glue_sidechain_switch", 163.f, 78.f), module, Chimera::GLUE_SIDECHAIN_SRC_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("clock_bpm_knob", 178.f, 24.f), module, Chimera::CLOCK_BPM_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSS>(
            centerPx("clock_run_switch", 178.f, 38.f), module, Chimera::CLOCK_RUN_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSS>(
            centerPx("clock_mode_switch", 178.f, 48.f), module, Chimera::CLOCK_MODE_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSS>(
            centerPx("clock_click_switch", 178.f, 58.f), module, Chimera::CLOCK_CLICK_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSS>(
            centerPx("clock_mix_switch", 178.f, 68.f), module, Chimera::CLOCK_MIX_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(
            centerPx("loop_bar_switch", 178.f, 80.f), module, Chimera::LOOP_BARS_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("clock_click_level_knob", 178.f, 92.f), module, Chimera::CLOCK_CLICK_LEVEL_PARAM));

        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("slot_a_rate_cv", 122.f, 94.f), module, Chimera::SLOT_A_RATE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("slot_a_depth_cv", 134.f, 94.f), module, Chimera::SLOT_A_DEPTH_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("slot_a_texture_cv", 146.f, 94.f), module, Chimera::SLOT_A_TEXTURE_CV_INPUT));

        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("slot_b_rate_cv", 122.f, 106.f), module, Chimera::SLOT_B_RATE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("slot_b_depth_cv", 134.f, 106.f), module, Chimera::SLOT_B_DEPTH_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("slot_b_texture_cv", 146.f, 106.f), module, Chimera::SLOT_B_TEXTURE_CV_INPUT));

        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("glue_sc_input", 170.f, 118.f), module, Chimera::GLUE_SC_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("gesture_input", 178.f, 106.f), module, Chimera::GESTURE_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("ext_clock_input", 178.f, 98.f), module, Chimera::EXT_CLOCK_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("morph_a_out", 122.f, 120.f), module, Chimera::MORPH_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("morph_b_out", 134.f, 120.f), module, Chimera::MORPH_B_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_out_l", 146.f, 120.f), module, Chimera::OUT_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_out_r", 158.f, 120.f), module, Chimera::OUT_R_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("click_output", 178.f, 116.f), module, Chimera::CLICK_OUTPUT));
    }
};

Model* modelChimera = createModel<Chimera, ChimeraWidget>("Chimera");
