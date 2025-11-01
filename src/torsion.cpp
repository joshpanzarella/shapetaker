#include "plugin.hpp"
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
    // curve: -1 = exponential, 0 = linear, +1 = logarithmic
    float applyCurve(float t, float curve) {
        t = rack::math::clamp(t, 0.f, 1.f);
        curve = rack::math::clamp(curve, -1.f, 1.f);

        if (curve < -0.01f) {
            // Exponential (fast start, slow end)
            float amount = -curve;
            return std::pow(t, 1.f + amount * 3.f);
        } else if (curve > 0.01f) {
            // Logarithmic (slow start, fast end)
            return 1.f - std::pow(1.f - t, 1.f + curve * 3.f);
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

    // DC blocking filters for clean output (prevents clicks/pops)
    shapetaker::dsp::VoiceArray<float> dcBlockerX1;  // Previous input
    shapetaker::dsp::VoiceArray<float> dcBlockerY1;  // Previous output

    // Click suppression fade-out ramp for smooth envelope endings
    shapetaker::dsp::VoiceArray<float> clickSuppressor;  // 1.0 = normal, 0.0 = fully faded

    InteractionMode interactionMode = INTERACTION_NONE;
    LoopMode loopMode = LOOP_FORWARD;
    static constexpr int kNumStages = 6;

    Torsion() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(COARSE_PARAM, -4.f, 4.f, 0.f, "Octave", " oct");
        if (auto* quantity = paramQuantities[COARSE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        configParam(DETUNE_PARAM, -20.f, 20.f, 0.f, "Detune", " cents");

        shapetaker::ParameterHelper::configGain(this, TORSION_PARAM, "Torsion depth", 1.0f);
        shapetaker::ParameterHelper::configGain(this, SYMMETRY_PARAM, "Symmetry warp", 0.5f);

        shapetaker::ParameterHelper::configAttenuverter(this, TORSION_ATTEN_PARAM, "Torsion CV");
        shapetaker::ParameterHelper::configAttenuverter(this, SYMMETRY_ATTEN_PARAM, "Symmetry CV");

        configSwitch(WARP_SHAPE_PARAM, 0.f, (float)((int)CZWarpShape::Count - 1), 0.f, "Warp shape",
            {"Single sine", "Resonant", "Double sine", "Saw pulse", "Pulse"});
        if (auto* quantity = paramQuantities[WARP_SHAPE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        // Stage envelope controls
        shapetaker::ParameterHelper::configDiscrete(this, STAGE_RATE_PARAM, "DCW cycle rate", 1, 30, 1);

        shapetaker::ParameterHelper::configAttenuverter(this, STAGE_TIME_PARAM, "Stage time scale");

        // Stage levels for DCW envelope - ADSR-like shape by default
        shapetaker::ParameterHelper::configGain(this, STAGE1_PARAM, "Stage 1 level", 0.0f);    // Start at 0
        shapetaker::ParameterHelper::configGain(this, STAGE2_PARAM, "Stage 2 level", 1.0f);    // Attack to full
        shapetaker::ParameterHelper::configGain(this, STAGE3_PARAM, "Stage 3 level", 0.7f);    // Decay to sustain
        shapetaker::ParameterHelper::configGain(this, STAGE4_PARAM, "Stage 4 level", 0.7f);    // Hold sustain
        shapetaker::ParameterHelper::configGain(this, STAGE5_PARAM, "Stage 5 level", 0.6f);    // Slight decay
        shapetaker::ParameterHelper::configGain(this, STAGE6_PARAM, "Stage 6 level", 0.0f);    // Release to 0

        // Curve shapers (-1 = exp, 0 = linear, +1 = log)
        configParam(CURVE1_PARAM, -1.f, 1.f, -0.3f, "Stage 1 curve");  // Exponential attack
        configParam(CURVE2_PARAM, -1.f, 1.f, 0.3f, "Stage 2 curve");   // Logarithmic decay
        configParam(CURVE3_PARAM, -1.f, 1.f, 0.f, "Stage 3 curve");    // Linear sustain
        configParam(CURVE4_PARAM, -1.f, 1.f, 0.f, "Stage 4 curve");    // Linear hold
        configParam(CURVE5_PARAM, -1.f, 1.f, 0.f, "Stage 5 curve");    // Linear decay
        configParam(CURVE6_PARAM, -1.f, 1.f, 0.3f, "Stage 6 curve");   // Logarithmic release

        configSwitch(LOOP_MODE_PARAM, 0.f, LOOP_MODES_LEN - 1.f, 0.f, "Loop mode",
            {"Forward", "Reverse", "Ping-Pong", "Random"});

        shapetaker::ParameterHelper::configGain(this, FEEDBACK_PARAM, "Feedback amount", 0.0f);

        configParam(SAW_WAVE_PARAM, 0.f, 1.f, 0.f, "Sawtooth wave");
        configParam(TRIANGLE_WAVE_PARAM, 0.f, 1.f, 0.f, "Triangle wave");
        configParam(SQUARE_WAVE_PARAM, 0.f, 1.f, 0.f, "Square wave");

        // Sub oscillator with extended range for powerful bass
        configParam(SUB_LEVEL_PARAM, 0.f, 2.0f, 0.0f, "Sub oscillator level", "%", 0.f, 100.f);
        configParam(SUB_WARP_PARAM, 0.f, 1.f, 0.f, "Sub DCW depth");
        configSwitch(SUB_SYNC_PARAM, 0.f, 1.f, 1.f, "Sub sync mode", {"Free-run", "Hard sync"});
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
        interactionMode = INTERACTION_NONE;
        loopMode = LOOP_FORWARD;
        for (int i = 0; i < LIGHTS_LEN; ++i) {
            lights[i].setBrightness(0.f);
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "interactionMode", json_integer((int)interactionMode));
        json_object_set_new(rootJ, "loopMode", json_integer((int)loopMode));
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

        for (int ch = 0; ch < channels; ++ch) {
            float pitch = coarse;
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
            torsionA = rack::math::clamp(torsionA, 0.f, 1.f);

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

            // Main output: mix both oscillators with proper gain staging
            // Using 0.6f instead of 0.5f to boost output amplitude
            // Envelope modulates torsion, not amplitude directly
            float mainOut = env * interactionGain * 0.6f * (shapedA + shapedB) + subSignal * env;

            // Edge output: blend between base tone (low torsion) and torsion difference (high torsion)
            float baseSum = baseA + baseB;
            float torsionDifference = (shapedA - baseA) + (shapedB - baseB);
            float edgeSignal = torsionDifference + baseSum * (1.f - dcwEnv);
            float edgeOut = env * interactionGain * 0.6f * edgeSignal;

            if (!std::isfinite(mainOut) || !std::isfinite(edgeOut)) {
                mainOut = 0.f;
                edgeOut = 0.f;
            }

            // Apply gentle soft clipping (tanh) for analog warmth and prevent harsh clipping
            // Drive slightly before saturation for more character
            mainOut = std::tanh(mainOut * 1.2f) * 0.9f;
            edgeOut = std::tanh(edgeOut * 1.2f) * 0.9f;

            // Apply click suppressor to prevent pops at envelope end
            // This creates a smooth fade-out ramp when envelope is very low
            mainOut *= clickSuppressor[ch];
            edgeOut *= clickSuppressor[ch];

            // DC blocking filter to remove DC offset and reduce clicks/pops
            // Uses a 1-pole high-pass filter with very low cutoff (~20Hz at 44.1kHz)
            const float dcBlockCoeff = 0.999f;  // Very low cutoff for sub-bass preservation
            float dcBlockedMain = mainOut - dcBlockerX1[ch] + dcBlockCoeff * dcBlockerY1[ch];
            dcBlockerX1[ch] = mainOut;
            dcBlockerY1[ch] = dcBlockedMain;

            // Store feedback signal for next sample (before DC blocking for stability)
            feedbackSignal[ch] = mainOut;

            outputs[MAIN_OUTPUT].setVoltage(dcBlockedMain * OUTPUT_SCALE, ch);
            outputs[EDGE_OUTPUT].setVoltage(edgeOut * OUTPUT_SCALE, ch);
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
        // Draw the slider background first
        if (background && background->svg) {
            background->draw(args);
        }

        // Get current parameter value for brightness
        float value = 0.5f;
        ParamQuantity* pq = getParamQuantity();
        if (pq) {
            value = pq->getScaledValue();
        }

        // Calculate LED position - it follows the handle position
        Vec ledPos;
        if (handle) {
            // Position LED at the center of the handle
            ledPos = Vec(handle->box.pos.x + handle->box.size.x * 0.5f,
                        handle->box.pos.y + handle->box.size.y * 0.5f);
        } else {
            // Fallback if handle doesn't exist
            ledPos = Vec(box.size.x * 0.5f, box.size.y * 0.5f);
        }

        // Draw the handle first using the cached SVG
        if (handle && handle->svg) {
            nvgSave(args.vg);
            nvgTranslate(args.vg, handle->box.pos.x, handle->box.pos.y);
            handle->draw(args);
            nvgRestore(args.vg);
        }

        // Draw LED on top so the glow isn't hidden behind the handle
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
    }
};

Model* modelTorsion = createModel<Torsion, TorsionWidget>("Torsion");
