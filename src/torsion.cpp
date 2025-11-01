#include "plugin.hpp"
#include "dsp/audio.hpp"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float OUTPUT_SCALE = 5.f;
    constexpr float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    enum class CZWarpShape {
        Single,
        Resonant,
        Double,
        SawPulse,
        Pulse,
        Count
    };

    float shapeBias(float symmetry, float amount) {
        symmetry = rack::math::clamp(symmetry, 0.f, 1.f);
        amount = rack::math::clamp(amount, 0.f, 1.f);
        float centered = symmetry - 0.5f;
        return centered * (0.35f + 0.25f * amount);
    }

    float warpSegment(float phase, float breakPoint, float attackCurve, float releaseCurve) {
        const float minBreak = 0.02f;
        const float maxBreak = 0.98f;
        breakPoint = rack::math::clamp(breakPoint, minBreak, maxBreak);
        attackCurve = rack::math::clamp(attackCurve, 0.05f, 4.f);
        releaseCurve = rack::math::clamp(releaseCurve, 0.05f, 4.f);

        if (phase < breakPoint) {
            float t = breakPoint <= 0.f ? 0.f : phase / breakPoint;
            t = rack::math::clamp(t, 0.f, 1.f);
            return 0.5f * std::pow(t, attackCurve);
        }

        float denom = 1.f - breakPoint;
        float t = denom <= 0.f ? 1.f : (phase - breakPoint) / denom;
        t = rack::math::clamp(t, 0.f, 1.f);
        return 0.5f + 0.5f * (1.f - std::pow(1.f - t, releaseCurve));
    }

    float computeBreakPoint(float amount, float bias) {
        float baseBreak = rack::math::clamp(1.f - amount * 0.98f, 0.02f, 0.98f);
        return rack::math::clamp(baseBreak + bias, 0.02f, 0.98f);
    }

    float applyCZWarp(float phase, float amount, float bias, CZWarpShape shape) {
        amount = rack::math::clamp(amount, 0.f, 1.f);
        phase = rack::math::eucMod(phase, 1.f);
        if (amount <= 1e-5f) {
            return phase;
        }

        float breakPoint = computeBreakPoint(amount, bias);

        switch (shape) {
            case CZWarpShape::Single:
                return warpSegment(phase, breakPoint, 1.f, 1.f);
            case CZWarpShape::Resonant: {
                float attackCurve = lerp(1.f, 0.22f, amount);
                float releaseCurve = lerp(1.f, 2.8f, amount);
                return warpSegment(phase, breakPoint, attackCurve, releaseCurve);
            }
            case CZWarpShape::Double: {
                float localPhase = (phase < 0.5f) ? phase * 2.f : (phase - 0.5f) * 2.f;
                float localBreak = rack::math::clamp(breakPoint * lerp(0.9f, 0.55f, amount), 0.02f, 0.98f);
                float warped = warpSegment(localPhase, localBreak, 1.f, 1.f);
                return (phase < 0.5f) ? warped * 0.5f : 0.5f + warped * 0.5f;
            }
            case CZWarpShape::SawPulse: {
                float sawBreak = rack::math::clamp(breakPoint * lerp(0.85f, 0.5f, amount), 0.02f, 0.95f);
                float attackCurve = lerp(1.f, 0.4f, amount);
                float releaseCurve = lerp(1.f, 0.2f, amount);
                return warpSegment(phase, sawBreak, attackCurve, releaseCurve);
            }
            case CZWarpShape::Pulse: {
                float pulseBreak = rack::math::clamp(breakPoint * lerp(0.7f, 0.18f, amount), 0.02f, 0.9f);
                float attackCurve = lerp(1.f, 0.6f, amount);
                float releaseCurve = lerp(1.f, 2.2f, amount);
                return warpSegment(phase, pulseBreak, attackCurve, releaseCurve);
            }
            default:
                break;
        }
        return phase;
    }

    float generateSine(float phase) {
        // Sine wave is naturally bandlimited
        return std::sin(2.f * M_PI * phase);
    }

    // Curve shaping for stage transitions
    // curve: -1 = exponential (fast start), 0 = linear, +1 = logarithmic (slow start)
    float applyCurve(float t, float curve) {
        t = rack::math::clamp(t, 0.f, 1.f);
        curve = rack::math::clamp(curve, -1.f, 1.f);

        if (curve < -0.01f) {
            float amount = -curve;
            float exponent = lerp(1.f, 5.f, amount);
            float shaped = 1.f - std::pow(1.f - t, exponent);
            float mix = lerp(0.6f, 0.9f, amount);
            return rack::math::crossfade(t, shaped, mix);
        } else if (curve > 0.01f) {
            float amount = curve;
            float exponent = lerp(1.f, 5.f, amount);
            float shaped = std::pow(t, exponent);
            float mix = lerp(0.6f, 0.9f, amount);
            return rack::math::crossfade(t, shaped, mix);
        }
        return t; // Linear
    }
}

struct Torsion : Module {
    enum InteractionMode {
        INTERACTION_NONE,
        INTERACTION_RESET_SYNC,
        INTERACTION_DCW_FOLLOW,
        INTERACTION_RING_MOD,
        INTERACTION_MODES_LEN
    };

    enum LoopMode {
        LOOP_FORWARD,
        LOOP_REVERSE,
        LOOP_PINGPONG,
        LOOP_RANDOM,
        LOOP_MODES_LEN
    };

    enum ParamId {
        COARSE_PARAM,
        DETUNE_PARAM,
        TORSION_PARAM,
        SYMMETRY_PARAM,
        TORSION_ATTEN_PARAM,
        SYMMETRY_ATTEN_PARAM,
        WARP_SHAPE_PARAM,
        STAGE_RATE_PARAM,
        STAGE_TIME_PARAM,
        STAGE1_PARAM,
        STAGE2_PARAM,
        STAGE3_PARAM,
        STAGE4_PARAM,
        STAGE5_PARAM,
        STAGE6_PARAM,
        CURVE1_PARAM,
        CURVE2_PARAM,
        CURVE3_PARAM,
        CURVE4_PARAM,
        CURVE5_PARAM,
        CURVE6_PARAM,
        LOOP_MODE_PARAM,
        FEEDBACK_PARAM,
        SAW_WAVE_PARAM,
        TRIANGLE_WAVE_PARAM,
        SQUARE_WAVE_PARAM,
        DIRTY_MODE_PARAM,
        SUB_LEVEL_PARAM,
        SUB_WARP_PARAM,
        SUB_SYNC_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        VOCT_INPUT,
        TORSION_CV_INPUT,
        SYMMETRY_CV_INPUT,
        STAGE_TRIG_INPUT,
        GATE_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        MAIN_OUTPUT,
        EDGE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        STAGE_LIGHT_1,
        STAGE_LIGHT_2,
        STAGE_LIGHT_3,
        STAGE_LIGHT_4,
        STAGE_LIGHT_5,
        STAGE_LIGHT_6,
        LOOP_FORWARD_LIGHT,
        LOOP_REVERSE_LIGHT,
        LIGHTS_LEN
    };

    shapetaker::PolyphonicProcessor polyProcessor;
    shapetaker::dsp::VoiceArray<float> primaryPhase;
    shapetaker::dsp::VoiceArray<float> secondaryPhase;
    shapetaker::dsp::VoiceArray<float> subPhase;
    shapetaker::dsp::VoiceArray<float> feedbackSignal;

    shapetaker::dsp::VoiceArray<float> stagePositions;
    shapetaker::dsp::VoiceArray<bool> stageActive;
    shapetaker::dsp::VoiceArray<float> stageEnvelope;
    shapetaker::dsp::VoiceArray<int> loopDirection;  // 1 = forward, -1 = reverse
    shapetaker::dsp::VoiceArray<rack::dsp::SchmittTrigger> stageTriggers;
    shapetaker::dsp::VoiceArray<bool> gateHeld;

    shapetaker::dsp::VoiceArray<float> vintageDrift;
    shapetaker::dsp::VoiceArray<float> vintageDriftTimer;
    shapetaker::dsp::VoiceArray<float> velocityHold;

    static constexpr int kChorusMaxDelaySamples = 4096;
    static constexpr float kChorusBaseDelayMs = 14.f;
    static constexpr float kChorusDepthMs = 4.2f;
    static constexpr float kChorusRateHz = 0.42f;
    static constexpr float kChorusMix = 0.35f;
    static constexpr float kChorusCrossMix = 0.25f;

    struct ChorusVoiceState {
        shapetaker::dsp::AudioProcessor::DelayLine<kChorusMaxDelaySamples> delayL;
        shapetaker::dsp::AudioProcessor::DelayLine<kChorusMaxDelaySamples> delayR;
        float phase = 0.f;

        void reset() {
            delayL.clear();
            delayR.clear();
            phase = 0.f;
        }
    };
    shapetaker::dsp::VoiceArray<ChorusVoiceState> chorusVoices;

    // DC blocking filters for clean output (prevents clicks/pops)
    shapetaker::dsp::VoiceArray<float> dcBlockerX1;  // Previous input
    shapetaker::dsp::VoiceArray<float> dcBlockerY1;  // Previous output

    // Click suppression fade-out ramp for smooth envelope endings
    shapetaker::dsp::VoiceArray<float> clickSuppressor;  // 1.0 = normal, 0.0 = fully faded

    InteractionMode interactionMode = INTERACTION_NONE;
    LoopMode loopMode = LOOP_FORWARD;
    bool vintageMode = false;
    bool dcwKeyTrackEnabled = false;
    bool dcwVelocityEnabled = false;
    bool chorusEnabled = false;
    static constexpr int kNumStages = 6;
    float vintageClockPhase = 0.f;

    static constexpr float kVintageHissLevel = 0.0045f;
    static constexpr float kVintageClockLevel = 0.0024f;
    static constexpr float kVintageClockFreq = 9000.f;  // Hz
    static constexpr float kVintageDriftRange = 0.0045f; // +/- range in octaves (~5.5 cents)
    static constexpr float kVintageDriftHoldMin = 0.18f;
    static constexpr float kVintageDriftHoldMax = 0.45f;
    static constexpr float kVintageIdleHissLevel = 0.0012f;

    Torsion() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(COARSE_PARAM, -4.f, 4.f, 0.f, "Octave", " oct");
        if (auto* quantity = paramQuantities[COARSE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        configParam(DETUNE_PARAM, -20.f, 20.f, 0.f, "Detune", " cents");

        shapetaker::ParameterHelper::configGain(this, TORSION_PARAM, "Torsion depth", 0.0f);
        shapetaker::ParameterHelper::configGain(this, SYMMETRY_PARAM, "Symmetry warp", 0.0f);

        shapetaker::ParameterHelper::configAttenuverter(this, TORSION_ATTEN_PARAM, "Torsion CV");
        shapetaker::ParameterHelper::configAttenuverter(this, SYMMETRY_ATTEN_PARAM, "Symmetry CV");

        configSwitch(WARP_SHAPE_PARAM, 0.f, (float)((int)CZWarpShape::Count - 1), 0.f, "Warp shape",
            {"Single sine", "Resonant", "Double sine", "Saw pulse", "Pulse"});
        if (auto* quantity = paramQuantities[WARP_SHAPE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        // Stage envelope controls
        shapetaker::ParameterHelper::configDiscrete(this, STAGE_RATE_PARAM, "DCW cycle rate", 1, 30, 10);

        shapetaker::ParameterHelper::configAttenuverter(this, STAGE_TIME_PARAM, "Stage time scale");

        // Stage levels for DCW envelope - ADSR-like shape by default
        shapetaker::ParameterHelper::configGain(this, STAGE1_PARAM, "Stage 1 level", 1.0f);
        shapetaker::ParameterHelper::configGain(this, STAGE2_PARAM, "Stage 2 level", 1.0f);
        shapetaker::ParameterHelper::configGain(this, STAGE3_PARAM, "Stage 3 level", 0.5f);
        shapetaker::ParameterHelper::configGain(this, STAGE4_PARAM, "Stage 4 level", 0.5f);
        shapetaker::ParameterHelper::configGain(this, STAGE5_PARAM, "Stage 5 level", 0.0f);
        shapetaker::ParameterHelper::configGain(this, STAGE6_PARAM, "Stage 6 level", 0.0f);

        // Curve shapers (-1 = exp, 0 = linear, +1 = log)
        configParam(CURVE1_PARAM, -1.f, 1.f, 0.f, "Stage 1 curve");
        configParam(CURVE2_PARAM, -1.f, 1.f, 0.f, "Stage 2 curve");
        configParam(CURVE3_PARAM, -1.f, 1.f, 0.f, "Stage 3 curve");
        configParam(CURVE4_PARAM, -1.f, 1.f, 0.f, "Stage 4 curve");
        configParam(CURVE5_PARAM, -1.f, 1.f, 0.f, "Stage 5 curve");
        configParam(CURVE6_PARAM, -1.f, 1.f, 0.f, "Stage 6 curve");

        configSwitch(LOOP_MODE_PARAM, 0.f, LOOP_MODES_LEN - 1.f, 0.f, "Loop mode",
            {"Forward", "Reverse", "Ping-Pong", "Random"});

        shapetaker::ParameterHelper::configGain(this, FEEDBACK_PARAM, "Feedback amount", 0.0f);

        configParam(SAW_WAVE_PARAM, 0.f, 1.f, 0.f, "Sawtooth wave");
        configParam(TRIANGLE_WAVE_PARAM, 0.f, 1.f, 0.f, "Triangle wave");
        configParam(SQUARE_WAVE_PARAM, 0.f, 1.f, 0.f, "Square wave");
        configSwitch(DIRTY_MODE_PARAM, 0.f, 1.f, 0.f, "Saturation mode", {"Clean", "Dirty"});
        if (auto* quantity = paramQuantities[DIRTY_MODE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        // Sub oscillator with extended range for powerful bass
        configParam(SUB_LEVEL_PARAM, 0.f, 2.0f, 0.0f, "Sub oscillator level", "%", 0.f, 100.f);
        configParam(SUB_WARP_PARAM, 0.f, 1.f, 0.f, "Sub DCW depth");
        configSwitch(SUB_SYNC_PARAM, 0.f, 1.f, 0.f, "Sub sync mode", {"Free-run", "Hard sync"});
        if (auto* quantity = paramQuantities[SUB_SYNC_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        shapetaker::ParameterHelper::configCVInput(this, VOCT_INPUT, "Pitch (V/Oct)");
        shapetaker::ParameterHelper::configCVInput(this, TORSION_CV_INPUT, "Torsion CV");
        shapetaker::ParameterHelper::configCVInput(this, SYMMETRY_CV_INPUT, "Symmetry CV");
        shapetaker::ParameterHelper::configGateInput(this, STAGE_TRIG_INPUT, "DCW trigger");
        shapetaker::ParameterHelper::configGateInput(this, GATE_INPUT, "DCW gate");

        shapetaker::ParameterHelper::configAudioOutput(this, MAIN_OUTPUT, "Main");
        shapetaker::ParameterHelper::configAudioOutput(this, EDGE_OUTPUT, "Edge");

        velocityHold.forEach([](float& v) { v = 1.f; });
        resetChorusState();
    }

    void resetChorusState() {
        chorusVoices.forEach([](ChorusVoiceState& voice) {
            voice.reset();
        });
    }

    void onReset() override {
        primaryPhase.reset();
        secondaryPhase.reset();
        subPhase.reset();
        feedbackSignal.reset();
        stagePositions.reset();
        stageActive.reset();
        stageEnvelope.reset();
        loopDirection.reset();
        dcBlockerX1.reset();
        dcBlockerY1.reset();
        clickSuppressor.reset();
        for (int i = 0; i < 16; i++) {
            loopDirection[i] = 1;  // Initialize to forward
            clickSuppressor[i] = 1.0f;  // Start fully active
        }
        gateHeld.reset();
        stageTriggers.reset();
        vintageDrift.reset();
        vintageDriftTimer.reset();
        velocityHold.forEach([](float& v) { v = 1.f; });
        interactionMode = INTERACTION_NONE;
        loopMode = LOOP_FORWARD;
        vintageMode = false;
        dcwKeyTrackEnabled = false;
        dcwVelocityEnabled = false;
        chorusEnabled = false;
        vintageClockPhase = 0.f;
        resetChorusState();
        for (int i = 0; i < LIGHTS_LEN; ++i) {
            lights[i].setBrightness(0.f);
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "interactionMode", json_integer((int)interactionMode));
        json_object_set_new(rootJ, "loopMode", json_integer((int)loopMode));
        json_object_set_new(rootJ, "vintageMode", json_boolean(vintageMode));
        json_object_set_new(rootJ, "dcwKeyTrackEnabled", json_boolean(dcwKeyTrackEnabled));
        json_object_set_new(rootJ, "dcwVelocityEnabled", json_boolean(dcwVelocityEnabled));
        json_object_set_new(rootJ, "chorusEnabled", json_boolean(chorusEnabled));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        if (!rootJ) {
            return;
        }
        json_t* modeJ = json_object_get(rootJ, "interactionMode");
        if (modeJ) {
            interactionMode = (InteractionMode)json_integer_value(modeJ);
            interactionMode = (InteractionMode)rack::math::clamp(
                (int)interactionMode, 0, INTERACTION_MODES_LEN - 1);
        }
        json_t* loopJ = json_object_get(rootJ, "loopMode");
        if (loopJ) {
            loopMode = (LoopMode)json_integer_value(loopJ);
            loopMode = (LoopMode)rack::math::clamp(
                (int)loopMode, 0, LOOP_MODES_LEN - 1);
        }
        json_t* vintageJ = json_object_get(rootJ, "vintageMode");
        if (vintageJ) {
            vintageMode = json_is_true(vintageJ);
        }
        json_t* trackJ = json_object_get(rootJ, "dcwKeyTrackEnabled");
        if (trackJ) {
            dcwKeyTrackEnabled = json_is_true(trackJ);
        }
        json_t* velocityJ = json_object_get(rootJ, "dcwVelocityEnabled");
        if (velocityJ) {
            dcwVelocityEnabled = json_is_true(velocityJ);
        }
        json_t* chorusJ = json_object_get(rootJ, "chorusEnabled");
        if (chorusJ) {
            chorusEnabled = json_is_true(chorusJ);
        }
        resetChorusState();
    }

    void process(const ProcessArgs& args) override {
        int channels = polyProcessor.updateChannels(
            {inputs[VOCT_INPUT], inputs[TORSION_CV_INPUT], inputs[SYMMETRY_CV_INPUT], inputs[STAGE_TRIG_INPUT], inputs[GATE_INPUT]},
            {outputs[MAIN_OUTPUT], outputs[EDGE_OUTPUT]});

        float coarse = params[COARSE_PARAM].getValue();
        float detuneCents = params[DETUNE_PARAM].getValue();
        float detuneOct = detuneCents / 1200.f;

        float torsionBase = params[TORSION_PARAM].getValue();
        float symmetryBase = params[SYMMETRY_PARAM].getValue();

        float rate = params[STAGE_RATE_PARAM].getValue();
        float stageTimeScale = params[STAGE_TIME_PARAM].getValue();
        float stageLevels[kNumStages] = {
            params[STAGE1_PARAM].getValue(),
            params[STAGE2_PARAM].getValue(),
            params[STAGE3_PARAM].getValue(),
            params[STAGE4_PARAM].getValue(),
            params[STAGE5_PARAM].getValue(),
            params[STAGE6_PARAM].getValue()
        };
        float stageCurves[kNumStages] = {
            params[CURVE1_PARAM].getValue(),
            params[CURVE2_PARAM].getValue(),
            params[CURVE3_PARAM].getValue(),
            params[CURVE4_PARAM].getValue(),
            params[CURVE5_PARAM].getValue(),
            params[CURVE6_PARAM].getValue()
        };
        loopMode = (LoopMode)(int)params[LOOP_MODE_PARAM].getValue();
        CZWarpShape warpShape = (CZWarpShape)rack::math::clamp(
            (int)params[WARP_SHAPE_PARAM].getValue(), 0, (int)CZWarpShape::Count - 1);
        bool useSaw = params[SAW_WAVE_PARAM].getValue() > 0.5f;
        bool useTriangle = params[TRIANGLE_WAVE_PARAM].getValue() > 0.5f;
        bool useSquare = params[SQUARE_WAVE_PARAM].getValue() > 0.5f;
        bool dirtyMode = params[DIRTY_MODE_PARAM].getValue() > 0.5f;

        float clockSignal = 0.f;
        if (vintageMode) {
            vintageClockPhase += args.sampleTime * kVintageClockFreq;
            if (vintageClockPhase >= 1.f) {
                vintageClockPhase -= std::floor(vintageClockPhase);
            }
            clockSignal = std::sin(2.f * float(M_PI) * vintageClockPhase) * kVintageClockLevel;
        }

        float polyComp = 1.f;
        if (channels > 1) {
            polyComp = 1.f / std::sqrt((float)channels);
        }

        float chorusPhaseInc = 0.f;
        int chorusBaseSamples = 0;
        int chorusDepthSamples = 0;
        if (chorusEnabled) {
            float sampleRate = args.sampleRate;
            chorusPhaseInc = 2.f * float(M_PI) * kChorusRateHz * args.sampleTime;
            chorusBaseSamples = (int)std::round(kChorusBaseDelayMs * 0.001f * sampleRate);
            chorusDepthSamples = (int)std::round(kChorusDepthMs * 0.001f * sampleRate);
            chorusDepthSamples = std::max(1, chorusDepthSamples);
        }

        for (int ch = 0; ch < channels; ++ch) {
            float drift = 0.f;
            if (vintageMode) {
                float timer = vintageDriftTimer[ch] - args.sampleTime;
                if (timer <= 0.f) {
                    vintageDrift[ch] = rack::random::uniform() * 2.f - 1.f;
                    vintageDrift[ch] *= kVintageDriftRange;
                    float hold = rack::random::uniform();
                    hold = lerp(kVintageDriftHoldMin, kVintageDriftHoldMax, hold);
                    timer = hold;
                }
                vintageDriftTimer[ch] = timer;
                drift = vintageDrift[ch];
            }

            float pitch = coarse;
            pitch += drift;
            if (inputs[VOCT_INPUT].isConnected()) {
                pitch += inputs[VOCT_INPUT].getPolyVoltage(ch);
            }

            float freqA = rack::dsp::FREQ_C4 *
                          std::pow(2.f, rack::math::clamp(pitch, -8.f, 8.f));
            float freqB = rack::dsp::FREQ_C4 *
                          std::pow(2.f, rack::math::clamp(pitch + detuneOct, -8.f, 8.f));

            float phaseA = primaryPhase[ch] + freqA * args.sampleTime;
            bool wrappedA = phaseA >= 1.f;
            if (wrappedA) {
                phaseA -= std::floor(phaseA);
            }

            float phaseB = secondaryPhase[ch] + freqB * args.sampleTime;
            if (interactionMode == INTERACTION_RESET_SYNC && wrappedA) {
                phaseB = phaseA;
            }
            if (phaseB >= 1.f) {
                phaseB -= std::floor(phaseB);
            }

            primaryPhase[ch] = phaseA;
            secondaryPhase[ch] = phaseB;

            // Sub-oscillator at -1 octave with optional sync
            float freqSub = freqA * 0.5f;
            float phaseSub = subPhase[ch] + freqSub * args.sampleTime;
            bool subHardSync = params[SUB_SYNC_PARAM].getValue() > 0.5f;
            if (wrappedA && subHardSync) {
                phaseSub = 0.f;  // Hard sync to primary oscillator
            }
            if (phaseSub >= 1.f) {
                phaseSub -= std::floor(phaseSub);
            }
            subPhase[ch] = phaseSub;

            bool gateConnected = inputs[GATE_INPUT].isConnected();
            bool trigConnected = inputs[STAGE_TRIG_INPUT].isConnected() && !gateConnected;
            float stagePos = stagePositions[ch];
            int dir = loopDirection[ch];

            if (gateConnected) {
                float gateVolt = inputs[GATE_INPUT].getPolyVoltage(ch);
                bool gateHigh = gateVolt >= 1.f;
                bool prevGate = gateHeld[ch];

                if (gateHigh) {
                    if (!prevGate) {
                        stagePos = 0.f;
                        dir = 1;
                        velocityHold[ch] = rack::math::clamp(gateVolt / 10.f, 0.f, 1.f);
                    }
                    float effectiveRate = rate * (1.f + stageTimeScale);
                    stagePos += dir * effectiveRate * args.sampleTime;

                    if (stagePos >= (float)kNumStages) {
                        if (loopMode == LOOP_PINGPONG) {
                            stagePos = 2.f * kNumStages - stagePos;
                            dir = -1;
                        } else if (loopMode == LOOP_RANDOM) {
                            stagePos = rack::random::uniform() * kNumStages;
                        } else {
                            stagePos = (float)kNumStages - 0.01f;
                            dir = 1;
                        }
                    } else if (stagePos < 0.f) {
                        if (loopMode == LOOP_PINGPONG) {
                            stagePos = -stagePos;
                            dir = 1;
                        } else {
                            stagePos = 0.f;
                            dir = 1;
                        }
                    }
                    stageActive[ch] = true;
                } else {
                    if (prevGate) {
                        stageEnvelope[ch] = 0.f;
                    }
                    gateHeld[ch] = false;
                    stageActive[ch] = false;
                    stagePositions[ch] = 0.f;
                    loopDirection[ch] = 1;
                    outputs[MAIN_OUTPUT].setVoltage(0.f, ch);
                    outputs[EDGE_OUTPUT].setVoltage(0.f, ch);
                    clickSuppressor[ch] = 1.0f;
                    continue;
                }
                gateHeld[ch] = gateHigh;
            } else if (trigConnected) {
                float trigVolt = inputs[STAGE_TRIG_INPUT].getPolyVoltage(ch);
                if (stageTriggers[ch].process(trigVolt)) {
                    stagePos = 0.f;
                    dir = 1;
                    stageActive[ch] = true;
                    velocityHold[ch] = rack::math::clamp(std::fabs(trigVolt) / 10.f, 0.f, 1.f);
                }

                if (stageActive[ch]) {
                    float effectiveRate = rate * (1.f + stageTimeScale);
                    stagePos += dir * effectiveRate * args.sampleTime;

                    // Handle looping at boundaries
                    if (stagePos >= (float)kNumStages) {
                        if (loopMode == LOOP_PINGPONG) {
                            stagePos = 2.f * kNumStages - stagePos;
                            dir = -1;
                        } else if (loopMode == LOOP_RANDOM) {
                            stagePos = rack::random::uniform() * kNumStages;
                        } else {
                            stagePos = 0.f;
                            stageActive[ch] = false;
                        }
                    } else if (stagePos < 0.f) {
                        if (loopMode == LOOP_PINGPONG) {
                            stagePos = -stagePos;
                            dir = 1;
                        } else {
                            stagePos = 0.f;
                            stageActive[ch] = false;
                        }
                    }
                }
            } else {
                // Free-running mode (no trigger connected)
                // Behavior depends on loop mode:
                // - Forward: cycle continuously
                // - Others: loop/pingpong as configured

                // Only advance if we haven't reached the end in forward mode
                bool shouldAdvance = true;
                if (loopMode == LOOP_FORWARD) {
                    // In forward mode without trigger, play once then hold at end
                    if (stagePos >= (float)kNumStages - 0.01f && dir > 0) {
                        stagePos = (float)kNumStages - 0.01f;  // Hold at last stage
                        shouldAdvance = false;
                    }
                }

                if (shouldAdvance) {
                    float effectiveRate = rate * (1.f + stageTimeScale);
                    stagePos += dir * effectiveRate * args.sampleTime;

                    if (stagePos >= (float)kNumStages) {
                        if (loopMode == LOOP_FORWARD) {
                            // Hold at end (shouldn't reach here, but just in case)
                            stagePos = (float)kNumStages - 0.01f;
                            dir = 1;
                        } else if (loopMode == LOOP_REVERSE) {
                            // Reverse mode: flip direction at end
                            stagePos = kNumStages - (stagePos - kNumStages);
                            dir = -1;
                        } else if (loopMode == LOOP_PINGPONG) {
                            // Ping-pong: bounce at end
                            stagePos = 2.f * kNumStages - stagePos;
                            dir = -1;
                        } else if (loopMode == LOOP_RANDOM) {
                            // Random: jump to random stage
                            stagePos = rack::random::uniform() * kNumStages;
                            dir = 1;
                        }
                    } else if (stagePos < 0.f) {
                        if (loopMode == LOOP_PINGPONG || loopMode == LOOP_REVERSE) {
                            // Bounce back forward when hitting start
                            stagePos = -stagePos;
                            dir = 1;
                        } else {
                            // Wrap to end
                            stagePos += kNumStages;
                            dir = 1;
                        }
                    }
                }
                stageActive[ch] = true;
            }

            if (!gateConnected) {
                gateHeld[ch] = false;
                if (!trigConnected) {
                    velocityHold[ch] = 1.f;
                }
            }

            if (!std::isfinite(stagePos)) {
                stagePos = 0.f;
            } else {
                stagePos = rack::math::clamp(stagePos, 0.f, (float)kNumStages);
            }

            stagePositions[ch] = stagePos;
            loopDirection[ch] = dir;

            float torsionA = torsionBase;
            if (inputs[TORSION_CV_INPUT].isConnected()) {
                torsionA += inputs[TORSION_CV_INPUT].getPolyVoltage(ch) *
                            params[TORSION_ATTEN_PARAM].getValue() * 0.1f;
            }
            float keyFactor = 1.f;
            if (dcwKeyTrackEnabled) {
                float offset = rack::math::clamp(pitch, -3.f, 3.f);
                keyFactor = rack::math::clamp(1.f + offset * 0.18f, 0.25f, 1.75f);
            }
            float velocityFactor = 1.f;
            if (dcwVelocityEnabled) {
                float heldVelocity = rack::math::clamp(velocityHold[ch], 0.f, 1.f);
                velocityFactor = lerp(0.35f, 1.f, heldVelocity);
            }
            torsionA = rack::math::clamp(torsionA * keyFactor * velocityFactor, 0.f, 1.f);

            float symmetry = symmetryBase;
            if (inputs[SYMMETRY_CV_INPUT].isConnected()) {
                symmetry += inputs[SYMMETRY_CV_INPUT].getPolyVoltage(ch) *
                            params[SYMMETRY_ATTEN_PARAM].getValue() * 0.1f;
            }
            symmetry = rack::math::clamp(symmetry, 0.f, 1.f);

            float stagePosition = stagePos;
            if (ch == 0) {
                bool active = stageActive[ch] || !trigConnected;
                float lightSlew = args.sampleTime * 8.f;
                for (int i = 0; i < kNumStages; ++i) {
                    float distance = std::fabs(stagePosition - (float)i);
                    float brightness = active ? rack::math::clamp(1.f - distance, 0.f, 1.f) : 0.f;
                    lights[STAGE_LIGHT_1 + i].setSmoothBrightness(brightness, lightSlew);
                }
                lights[LOOP_FORWARD_LIGHT].setSmoothBrightness(dir >= 0 ? 1.f : 0.f, lightSlew);
                lights[LOOP_REVERSE_LIGHT].setSmoothBrightness(dir < 0 ? 1.f : 0.f, lightSlew);
            }
            float targetStageValue = 0.f;
            if (stageActive[ch] || !trigConnected) {
                int stageIndex = rack::math::clamp((int)stagePosition, 0, kNumStages - 1);
                // Don't wrap around to stage 1 when at the last stage - hold at final stage value
                int nextStage = rack::math::clamp(stageIndex + 1, 0, kNumStages - 1);
                float stagePhase = stagePosition - stageIndex;

                // Apply curve shaping to the transition
                float curvedPhase = applyCurve(stagePhase, stageCurves[stageIndex]);

                // Crossfade between current and next stage
                // When at last stage, nextStage == stageIndex, so it just holds the value
                targetStageValue = rack::math::crossfade(stageLevels[stageIndex], stageLevels[nextStage], curvedPhase);
            }

            // Envelope smoothing with faster slew for more responsive feel
            // ~6ms time constant for smooth but responsive transitions
            float slewCoeff = std::exp(-args.sampleTime * 160.f);
            float env = stageEnvelope[ch] + (targetStageValue - stageEnvelope[ch]) * (1.f - slewCoeff);
            stageEnvelope[ch] = env;

            // Click suppression system: trigger fade-out when envelope is very low
            // This prevents pops from complex waveforms cutting off abruptly
            const float clickSuppressionThreshold = 0.05f;  // Trigger when envelope drops below 5%
            if (env < clickSuppressionThreshold) {
                // Fast exponential fade-out over ~10ms to ensure smooth zero-crossing
                float suppressorDecay = std::exp(-args.sampleTime * 100.f); // ~10ms fade
                clickSuppressor[ch] *= suppressorDecay;
            } else {
                // Normal operation - suppressor stays at 1.0
                clickSuppressor[ch] = 1.0f;
            }

            // In triggered mode when inactive, let envelope decay naturally instead of hard reset
            if (trigConnected && !stageActive[ch]) {
                // Gently pull toward zero with a slower slew to avoid clicks
                float decayCoeff = std::exp(-args.sampleTime * 40.f); // ~25ms decay tail
                env = stageEnvelope[ch] * decayCoeff;
                stageEnvelope[ch] = env;
            }

            // Only silence output when envelope AND click suppressor are truly negligible
            if (env <= 1e-6f && clickSuppressor[ch] <= 1e-6f) {
                outputs[MAIN_OUTPUT].setVoltage(0.f, ch);
                outputs[EDGE_OUTPUT].setVoltage(0.f, ch);
                stageEnvelope[ch] = 0.f;
                clickSuppressor[ch] = 1.0f;  // Reset for next trigger
                continue;
            }

            float dcwEnv = rack::math::clamp(env * torsionA, 0.f, 1.f);
            float dcwA = dcwEnv;
            float dcwB = dcwEnv;

            if (interactionMode == INTERACTION_DCW_FOLLOW) {
                float influence = std::fabs(std::sin(2.f * M_PI * phaseA));
                dcwB = rack::math::clamp(dcwEnv * influence, 0.f, 1.f);
            }

            // Apply feedback to phase
            float feedbackAmount = params[FEEDBACK_PARAM].getValue();
            float feedbackMod = feedbackSignal[ch] * feedbackAmount * 0.3f;
            float phaseAFinal = phaseA + feedbackMod;
            phaseAFinal = phaseAFinal - std::floor(phaseAFinal);

            float biasA = shapeBias(symmetry, dcwA);
            float biasB = shapeBias(symmetry, dcwB);
            float warpedA = applyCZWarp(phaseAFinal, dcwA, biasA, warpShape);
            float warpedB = applyCZWarp(phaseB, dcwB, biasB, warpShape);

            auto buildWarpedVoice = [&](float warpedPhase, float amount) {
                float theta = 2.f * float(M_PI) * warpedPhase;
                float sin1 = std::sinf(theta);
                float cos1 = std::cosf(theta);

                float voice = 0.f;
                int layers = 0;

                float sin2 = 0.f;
                float cos2 = 0.f;
                float sin3 = 0.f;
                float sin5 = 0.f;

                if (useSaw || useSquare) {
                    sin2 = 2.f * sin1 * cos1;
                    cos2 = cos1 * cos1 - sin1 * sin1;
                }

                if (useSquare) {
                    float sin4 = 2.f * sin2 * cos2;
                    float cos4 = cos2 * cos2 - sin2 * sin2;
                    sin3 = sin2 * cos1 + cos2 * sin1;
                    sin5 = sin4 * cos1 + cos4 * sin1;
                }

                if (useSaw) {
                    float sawHybrid = sin1 + 0.5f * sin2;
                    voice += sawHybrid;
                    layers++;
                }
                if (useTriangle) {
                    float triangleHybrid = 1.f - 4.f * std::fabs(warpedPhase - 0.5f);
                    voice += triangleHybrid;
                    layers++;
                }
                if (useSquare) {
                    float squareHybrid = sin1 + (1.f / 3.f) * sin3 + (1.f / 5.f) * sin5;
                    voice += squareHybrid;
                    layers++;
                }

                if (layers == 0) {
                    voice = sin1;
                } else {
                    voice /= (float)layers;
                }

                float loudness = 1.f + amount * 1.2f;
                return rack::math::clamp(voice * loudness, -3.f, 3.f);
            };

            float baseA = std::sin(2.f * M_PI * phaseAFinal);
            float baseB = std::sin(2.f * M_PI * phaseB);
            float shapedA = buildWarpedVoice(warpedA, dcwA);
            float shapedB = buildWarpedVoice(warpedB, dcwB);

            float interactionGain = 1.f;
            if (interactionMode == INTERACTION_DCW_FOLLOW) {
                shapedB = rack::math::crossfade(shapedB, baseB, 0.25f);
                interactionGain = 1.15f;
            } else if (interactionMode == INTERACTION_RING_MOD) {
                shapedB = shapedA * shapedB;
                interactionGain = 1.7f;
            }

            // Generate sub-oscillator (pure sine wave, -1 octave) with optional DCW warp
            float subLevel = params[SUB_LEVEL_PARAM].getValue();
            float subWarpDepth = rack::math::clamp(env * params[SUB_WARP_PARAM].getValue(), 0.f, 1.f);
            float subBias = shapeBias(symmetry, subWarpDepth);
            float subPhaseWarped = applyCZWarp(phaseSub, subWarpDepth, subBias, warpShape);
            float subLoudness = 1.f + subWarpDepth * 0.8f;
            float subSignal = generateSine(subPhaseWarped) * subLevel * subLoudness;
            float primaryActivity = 0.5f * (std::fabs(shapedA) + std::fabs(shapedB));
            float subTrim = 1.f / (1.f + primaryActivity * 0.9f);
            subSignal *= subTrim;

            // Main output: mix both oscillators with balanced gain staging
            // Envelope modulates torsion, not amplitude directly
            float mainSignal = env * interactionGain * 0.5f * (shapedA + shapedB) + subSignal * env;

            // Edge output: blend between base tone (low torsion) and torsion difference (high torsion)
            float baseSum = baseA + baseB;
            float torsionDifference = (shapedA - baseA) + (shapedB - baseB);
            float edgeContribution = torsionDifference + baseSum * (1.f - dcwEnv);
            float edgeSignal = env * interactionGain * 0.5f * edgeContribution;

            mainSignal *= polyComp;
            edgeSignal *= polyComp;

            if (vintageMode) {
                float hiss = (rack::random::uniform() * 2.f - 1.f) * kVintageHissLevel * polyComp;
                float bleed = clockSignal * polyComp;
                mainSignal += hiss + bleed;
                edgeSignal += hiss + bleed * 0.6f;
            }

            if (!std::isfinite(mainSignal) || !std::isfinite(edgeSignal)) {
                mainSignal = 0.f;
                edgeSignal = 0.f;
            }

            // Optional saturation: dirty mode keeps the original drive, clean mode adds gentle limiting
            float mainOut;
            float edgeOut;
            if (dirtyMode) {
                mainOut = std::tanh(mainSignal * 1.2f) * 0.9f;
                edgeOut = std::tanh(edgeSignal * 1.2f) * 0.9f;
            } else {
                constexpr float cleanDrive = 0.75f;
                constexpr float cleanScale = 1.f / cleanDrive;  // Unity gain around 0 V
                mainOut = std::tanh(mainSignal * cleanDrive) * cleanScale;
                edgeOut = std::tanh(edgeSignal * cleanDrive) * cleanScale;
            }

            // Apply click suppressor to prevent pops at envelope end
            // This creates a smooth fade-out ramp when envelope is very low
            mainOut *= clickSuppressor[ch];
            edgeOut *= clickSuppressor[ch];

            if (vintageMode) {
                float idleHiss = (rack::random::uniform() * 2.f - 1.f) * kVintageIdleHissLevel * polyComp;
                mainOut += idleHiss;
                edgeOut += idleHiss * 0.7f;
            }

            // DC blocking filter to remove DC offset and reduce clicks/pops
            // Uses a 1-pole high-pass filter with very low cutoff (~20Hz at 44.1kHz)
            const float dcBlockCoeff = 0.999f;  // Very low cutoff for sub-bass preservation
            float dcBlockedMain = mainOut - dcBlockerX1[ch] + dcBlockCoeff * dcBlockerY1[ch];
            dcBlockerX1[ch] = mainOut;
            dcBlockerY1[ch] = dcBlockedMain;

            // Store feedback signal for next sample (before DC blocking for stability)
            feedbackSignal[ch] = mainOut;

            float processedMain = dcBlockedMain;
            float processedEdge = edgeOut;
            if (chorusEnabled) {
                ChorusVoiceState& chorusState = chorusVoices[ch];
                chorusState.phase += chorusPhaseInc;
                if (chorusState.phase > 2.f * float(M_PI)) {
                    chorusState.phase -= 2.f * float(M_PI);
                }
                float modA = std::sin(chorusState.phase);
                float modB = std::sin(chorusState.phase + 2.f * float(M_PI) / 3.f);
                int delayA = chorusBaseSamples + (int)std::round(chorusDepthSamples * ((modA + 1.f) * 0.5f));
                int delayB = chorusBaseSamples + (int)std::round(chorusDepthSamples * ((modB + 1.f) * 0.5f));
                delayA = rack::math::clamp(delayA, 0, kChorusMaxDelaySamples - 1);
                delayB = rack::math::clamp(delayB, 0, kChorusMaxDelaySamples - 1);
                float inputL = processedMain + processedEdge * 0.25f;
                float inputR = processedEdge + processedMain * 0.25f;
                float delayOutL = chorusState.delayL.process(inputL, delayA);
                float delayOutR = chorusState.delayR.process(inputR, delayB);
                float dryMix = std::cos(kChorusMix * float(M_PI) * 0.5f);
                float wetMix = std::sin(kChorusMix * float(M_PI) * 0.5f);
                float crossMix = kChorusCrossMix * wetMix;
                processedMain = processedMain * dryMix + delayOutL * wetMix + delayOutR * crossMix;
                processedEdge = processedEdge * dryMix + delayOutR * wetMix + delayOutL * crossMix;
            }

            outputs[MAIN_OUTPUT].setVoltage(processedMain * OUTPUT_SCALE, ch);
            outputs[EDGE_OUTPUT].setVoltage(processedEdge * OUTPUT_SCALE, ch);
        }
    }
};

// Custom slider with LED indicator that follows the handle (like VCV Random module)
struct VintageSliderLED : app::SvgSlider {
    // LED parameters - warm tube glow color
    static constexpr float LED_RADIUS = 4.0f;
    static constexpr float LED_GLOW_RADIUS = 10.0f;
    // Warm orange/amber tube glow color
    NVGcolor ledColor = nvgRGBf(1.0f, 0.6f, 0.2f);

    VintageSliderLED() {
        // Set the background (track) SVG - 8x60px (small compact version)
        setBackgroundSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_track_small.svg"))
        );

        // Set the handle SVG - 12x18px (small compact version)
        setHandleSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_handle_small.svg"))
        );

        // SVG dimensions: track is 8x60px, handle is 12x18px
        // Widget box size matches track width and height
        box.size = Vec(12.f, 60.f);

        // Configure the slider travel range
        maxHandlePos = Vec(-2.f, 0.f);      // Top position (param minimum = 0), offset left 2px to center
        minHandlePos = Vec(-2.f, 42.f);     // Bottom position (param maximum = 1)
    }

    void draw(const DrawArgs& args) override {
        app::SvgSlider::draw(args);

        float value = 0.5f;
        if (auto* pq = getParamQuantity()) {
            value = pq->getScaledValue();
        }

        Vec ledPos = Vec(box.size.x * 0.5f, box.size.y * 0.5f);
        if (handle) {
            ledPos = Vec(handle->box.pos.x + handle->box.size.x * 0.5f,
                         handle->box.pos.y + handle->box.size.y * 0.5f);
        }

        drawLED(args, ledPos, value);
    }

    void drawLED(const DrawArgs& args, Vec pos, float brightness) {
        brightness = rack::math::clamp(brightness, 0.f, 1.f);
        if (brightness <= 0.f) {
            return;  // Slider at minimum, LED fully off
        }

        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        // Outer glow (softer, larger)
        float glowAlpha = brightness * 0.55f;
        NVGcolor glowColor = nvgRGBAf(
            ledColor.r,
            ledColor.g,
            ledColor.b,
            glowAlpha
        );

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, pos.x, pos.y, LED_GLOW_RADIUS);
        NVGpaint glowPaint = nvgRadialGradient(
            args.vg,
            pos.x, pos.y,
            0.f,
            LED_GLOW_RADIUS,
            glowColor,
            nvgRGBAf(ledColor.r, ledColor.g, ledColor.b, 0.f)
        );
        nvgFillPaint(args.vg, glowPaint);
        nvgFill(args.vg);

        // Inner LED core (brighter center)
        float coreAlpha = brightness * 0.95f;
        NVGcolor coreColor = nvgRGBAf(
            rack::math::clamp(ledColor.r * 1.1f, 0.f, 1.f),
            rack::math::clamp(ledColor.g * 1.1f, 0.f, 1.f),
            rack::math::clamp(ledColor.b * 0.95f, 0.f, 1.f),
            coreAlpha
        );

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, pos.x, pos.y, LED_RADIUS);
        NVGpaint corePaint = nvgRadialGradient(
            args.vg,
            pos.x, pos.y - LED_RADIUS * 0.3f,
            0.f,
            LED_RADIUS,
            nvgRGBAf(1.f, 0.92f, 0.6f, coreAlpha),  // Warm white center highlight
            coreColor
        );
        nvgFillPaint(args.vg, corePaint);
        nvgFill(args.vg);

        nvgRestore(args.vg);
    }
};

// Vintage four-position rotary switch sized to match a small knob footprint
struct VintageFourWaySwitch : app::Knob {
    VintageFourWaySwitch() {
        box.size = rack::mm2px(Vec(16.f, 16.f));
        minAngle = -0.75f * M_PI;
        maxAngle = 0.75f * M_PI;
        speed = 0.8f;
        smooth = false;
    }

    void onDragMove(const DragMoveEvent& e) override {
        // Snap to four discrete positions while dragging
        app::Knob::onDragMove(e);
        ParamQuantity* pq = getParamQuantity();
        if (pq) {
            pq->setValue(std::round(pq->getValue()));
        }
    }

    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT)) {
            ParamQuantity* pq = getParamQuantity();
            if (pq) {
                int current = (int)std::round(pq->getValue());
                int direction = (e.button == GLFW_MOUSE_BUTTON_LEFT) ? 1 : -1;
                int minValue = (int)std::round(pq->getMinValue());
                int maxValue = (int)std::round(pq->getMaxValue());
                int next = current + direction;
                if (next > maxValue) {
                    next = minValue;
                } else if (next < minValue) {
                    next = maxValue;
                }
                pq->setValue((float)rack::math::clamp(next, minValue, maxValue));
            }
            e.consume(this);
        }
        app::Knob::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        Vec center = box.size.div(2.f);
        float radius = std::min(box.size.x, box.size.y) * 0.5f - 1.f;

        // Base
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius);
        NVGcolor baseOuter = nvgRGBA(46, 40, 38, 255);
        NVGcolor baseInner = nvgRGBA(87, 74, 66, 255);
        nvgFillPaint(vg, nvgRadialGradient(vg, center.x, center.y, radius * 0.2f, radius, baseInner, baseOuter));
        nvgFill(vg);

        // Brass ring
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius - 2.f);
        nvgStrokeWidth(vg, 2.f);
        nvgStrokeColor(vg, nvgRGBA(170, 139, 87, 255));
        nvgStroke(vg);

        // Tick marks for each position
        ParamQuantity* pq = getParamQuantity();
        float minValue = pq ? pq->getMinValue() : 0.f;
        float maxValue = pq ? pq->getMaxValue() : 3.f;
        for (int i = 0; i <= (int)(maxValue - minValue); ++i) {
            float angle = rack::math::rescale((float)i, minValue, maxValue, minAngle, maxAngle);
            Vec dir(std::cos(angle), std::sin(angle));
            Vec inner = dir.mult(radius - 4.f).plus(center);
            Vec outer = dir.mult(radius - 1.f).plus(center);
            nvgBeginPath(vg);
            nvgMoveTo(vg, inner.x, inner.y);
            nvgLineTo(vg, outer.x, outer.y);
            nvgStrokeWidth(vg, 1.2f);
            nvgStrokeColor(vg, nvgRGBA(230, 214, 176, 160));
            nvgStroke(vg);
        }

        // Pointer
        float value = pq ? pq->getValue() : 0.f;
        float pointerAngle = rack::math::rescale(value, minValue, maxValue, minAngle, maxAngle);
        float pointerLength = radius - 4.f;
        Vec pointerDir(std::cos(pointerAngle), std::sin(pointerAngle));
        pointerDir = pointerDir.normalize();
        Vec tip = pointerDir.mult(pointerLength).plus(center);
        Vec leftWing(std::cos(pointerAngle + 0.9f * M_PI_2), std::sin(pointerAngle + 0.9f * M_PI_2));
        Vec rightWing(std::cos(pointerAngle - 0.9f * M_PI_2), std::sin(pointerAngle - 0.9f * M_PI_2));
        leftWing = leftWing.mult(2.2f).plus(center);
        rightWing = rightWing.mult(2.2f).plus(center);

        nvgBeginPath(vg);
        nvgMoveTo(vg, leftWing.x, leftWing.y);
        nvgLineTo(vg, tip.x, tip.y);
        nvgLineTo(vg, rightWing.x, rightWing.y);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(238, 220, 170, 255));
        nvgFill(vg);

        // Center cap
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, 3.2f);
        nvgFillColor(vg, nvgRGBA(78, 62, 49, 255));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, 3.2f);
        nvgStrokeWidth(vg, 1.f);
        nvgStrokeColor(vg, nvgRGBA(205, 183, 148, 255));
        nvgStroke(vg);
    }
};

struct TorsionWidget : ModuleWidget {
    TorsionWidget(Torsion* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Torsion.svg")));

        float moduleWidth = shapetaker::ui::LayoutHelper::getModuleWidth(
            shapetaker::ui::LayoutHelper::ModuleWidth::WIDTH_18HP);
        shapetaker::ui::LayoutHelper::ScrewPositions::addStandardScrews<ScrewBlack>(this, moduleWidth);

        shapetaker::ui::LayoutHelper::PanelSVGParser parser(
            asset::plugin(pluginInstance, "res/panels/Torsion.svg"));

        auto centerPx = [&](const std::string& id, float defX, float defY) -> Vec {
            return parser.centerPx(id, defX, defY);
        };

        // === LEFT COLUMN: Oscillator controls ===
        float leftCol = 15.f;
        float centerCol = 45.f;

        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("coarse_knob", leftCol, 20.f), module, Torsion::COARSE_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("detune_knob", leftCol, 35.f), module, Torsion::DETUNE_PARAM));

        addParam(createParamCentered<CKSS>(
            centerPx("saw_wave_switch", leftCol - 8.f, 48.f), module, Torsion::SAW_WAVE_PARAM));
        addParam(createParamCentered<CKSS>(
            centerPx("tri_wave_switch", leftCol, 48.f), module, Torsion::TRIANGLE_WAVE_PARAM));
        addParam(createParamCentered<CKSS>(
            centerPx("square_wave_switch", leftCol + 8.f, 48.f), module, Torsion::SQUARE_WAVE_PARAM));
        addParam(createParamCentered<CKSS>(
            centerPx("dirty_mode_switch", leftCol - 8.f, 60.f), module, Torsion::DIRTY_MODE_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("sub_level_knob", leftCol, 56.f), module, Torsion::SUB_LEVEL_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("dcw_depth_knob", leftCol + 10.5f, 56.f), module, Torsion::SUB_WARP_PARAM));
        addParam(createParamCentered<CKSS>(
            centerPx("sub_sync_switch", leftCol - 10.5f, 56.f), module, Torsion::SUB_SYNC_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("feedback_knob", leftCol, 68.f), module, Torsion::FEEDBACK_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("warp_shape_knob", leftCol + 11.f, 76.f), module, Torsion::WARP_SHAPE_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("torsion_knob", leftCol, 88.f), module, Torsion::TORSION_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("torsion_atten", leftCol - 9.f, 88.f), module, Torsion::TORSION_ATTEN_PARAM));

        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("symmetry_knob", leftCol, 106.f), module, Torsion::SYMMETRY_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("symmetry_atten", leftCol - 9.f, 106.f), module, Torsion::SYMMETRY_ATTEN_PARAM));

        // === CENTER COLUMN: DCW Envelope controls ===
        addParam(createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("stage_rate_knob", centerCol, 20.f), module, Torsion::STAGE_RATE_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("stage_time_atten", centerCol, 32.f), module, Torsion::STAGE_TIME_PARAM));

        addParam(createParamCentered<VintageFourWaySwitch>(
            centerPx("loop_mode_swtich", centerCol, 42.f), module, Torsion::LOOP_MODE_PARAM));

        const char* stageSliderIds[Torsion::kNumStages] = {
            "stage_1_slider",
            "stage_2_slider",
            "stage_3_slider",
            "stage_4_slider",
            "stage_5_slider",
            "stage_6_slider"
        };
        constexpr float stageSliderFallbackX[Torsion::kNumStages] = {
            22.14f, 29.54f, 36.94f, 44.34f, 51.74f, 59.14f
        };
        constexpr float stageSliderFallbackY = 74.768f;
        for (int i = 0; i < Torsion::kNumStages; ++i) {
            addParam(createParamCentered<VintageSliderLED>(
                centerPx(stageSliderIds[i], stageSliderFallbackX[i], stageSliderFallbackY),
                module, Torsion::STAGE1_PARAM + i));
        }

        const char* curveSliderIds[Torsion::kNumStages] = {
            "curve_1_slider",
            "curve_2_slider",
            "curve_3_slider",
            "curve_4_slider",
            "curve_5_slider",
            "curve_6_slider"
        };
        constexpr float curveSliderFallbackY = 97.526f;
        for (int i = 0; i < Torsion::kNumStages; ++i) {
            addParam(createParamCentered<VintageSliderLED>(
                centerPx(curveSliderIds[i], stageSliderFallbackX[i], curveSliderFallbackY),
                module, Torsion::CURVE1_PARAM + i));
        }

        constexpr float stageLightOffsetX = 0.3297757f;
        constexpr float stageLightOffsetY = 16.412222f;
        for (int i = 0; i < Torsion::kNumStages; ++i) {
            Vec sliderCenter = centerPx(stageSliderIds[i], stageSliderFallbackX[i], stageSliderFallbackY);
            Vec lightCenter = sliderCenter.plus(Vec(stageLightOffsetX, stageLightOffsetY));
            addChild(createLightCentered<SmallLight<GreenLight>>(lightCenter, module, Torsion::STAGE_LIGHT_1 + i));
        }
        addChild(createLightCentered<SmallLight<GreenLight>>(
            centerPx("loop_forward_light", centerCol + 13.f, 42.f),
            module,
            Torsion::LOOP_FORWARD_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(
            centerPx("loop_reverse_light", centerCol - 13.f, 42.f),
            module,
            Torsion::LOOP_REVERSE_LIGHT));

        // === I/O Section (Bottom) ===
        float ioY = 118.f;
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("voct_cv", 10.f, ioY), module, Torsion::VOCT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("torsion_cv", 20.f, ioY), module, Torsion::TORSION_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("symmetry_cv", 30.f, ioY), module, Torsion::SYMMETRY_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("stage_trig_cv", 40.f, ioY), module, Torsion::STAGE_TRIG_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("gate_input", 24.477f, 113.280f), module, Torsion::GATE_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_output", 55.f, ioY), module, Torsion::MAIN_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("edge_output", 65.f, ioY), module, Torsion::EDGE_OUTPUT));
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> panelTexture = APP->window->loadImage(
            asset::plugin(pluginInstance, "res/panels/vcv-panel-background.png"));

        if (panelTexture) {
            NVGpaint paint = nvgImagePattern(
                args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, panelTexture->handle, 1.f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        } else {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGB(0xf0, 0xeb, 0xe4));
            nvgFill(args.vg);
        }
        ModuleWidget::draw(args);
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);

        Torsion* module = dynamic_cast<Torsion*>(this->module);
        if (!module) {
            return;
        }

        menu->addChild(new ui::MenuSeparator());

        struct InteractionItem : ui::MenuItem {
            Torsion* module;
            Torsion::InteractionMode mode;
            void onAction(const event::Action& e) override {
                module->interactionMode = mode;
            }
            void step() override {
                rightText = (module->interactionMode == mode) ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* heading = new ui::MenuLabel;
        heading->text = "Oscillator interaction";
        menu->addChild(heading);

        const char* labels[] = {
            "Independent",
            "Sync B to A resets",
            "B DCW follows A",
            "Ring mod mix"
        };

        for (int i = 0; i < Torsion::INTERACTION_MODES_LEN; ++i) {
            auto* item = new InteractionItem;
            item->module = module;
            item->mode = (Torsion::InteractionMode)i;
            item->text = labels[i];
            menu->addChild(item);
        }

        // Loop mode menu section
        menu->addChild(new ui::MenuSeparator());

        struct LoopModeItem : ui::MenuItem {
            Torsion* module;
            Torsion::LoopMode mode;
            void onAction(const event::Action& e) override {
                module->params[Torsion::LOOP_MODE_PARAM].setValue((float)mode);
            }
            void step() override {
                int currentMode = (int)module->params[Torsion::LOOP_MODE_PARAM].getValue();
                rightText = (currentMode == mode) ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* loopHeading = new ui::MenuLabel;
        loopHeading->text = "DCW Envelope loop mode";
        menu->addChild(loopHeading);

            const char* loopLabels[] = {
                "Forward",
                "Reverse",
                "Ping-Pong",
                "Random"
            };

            for (int i = 0; i < Torsion::LOOP_MODES_LEN; ++i) {
                auto* item = new LoopModeItem;
                item->module = module;
                item->mode = (Torsion::LoopMode)i;
                item->text = loopLabels[i];
                menu->addChild(item);
            }

        menu->addChild(new ui::MenuSeparator());

        auto* dcwHeading = new ui::MenuLabel;
        dcwHeading->text = "DCW enhancements";
        menu->addChild(dcwHeading);

        struct KeyTrackItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->dcwKeyTrackEnabled = !module->dcwKeyTrackEnabled;
            }
            void step() override {
                rightText = module->dcwKeyTrackEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        struct VelocityItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->dcwVelocityEnabled = !module->dcwVelocityEnabled;
            }
            void step() override {
                rightText = module->dcwVelocityEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* keyTrackItem = new KeyTrackItem;
        keyTrackItem->module = module;
        keyTrackItem->text = "Key tracking (DCW depth)";
        menu->addChild(keyTrackItem);

        auto* velocityItem = new VelocityItem;
        velocityItem->module = module;
        velocityItem->text = "Velocity sensitivity (DCW depth)";
        menu->addChild(velocityItem);

        menu->addChild(new ui::MenuSeparator());

        struct ChorusItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->chorusEnabled = !module->chorusEnabled;
                module->resetChorusState();
            }
            void step() override {
                rightText = module->chorusEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        struct VintageModeItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->vintageMode = !module->vintageMode;
            }
            void step() override {
                rightText = module->vintageMode ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* chorusItem = new ChorusItem;
        chorusItem->module = module;
        chorusItem->text = "Chorus (stereo)";
        menu->addChild(chorusItem);

        auto* vintageItem = new VintageModeItem;
        vintageItem->module = module;
        vintageItem->text = "Vintage mode (hiss/bleed/drift)";
        menu->addChild(vintageItem);
        }
    };

    Model* modelTorsion = createModel<Torsion, TorsionWidget>("Torsion");
