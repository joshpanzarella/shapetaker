#include "plugin.hpp"
#include "dsp/envelopes.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <cctype>

// Forward declaration
struct Evocation;
struct EvocationOLEDDisplay;

// Custom ParamQuantity for ADSR stage selection buttons
// Implementation will be after Evocation class definition
struct ADSRStageButtonQuantity : ParamQuantity {
    int stageIndex; // 0=Attack, 1=Decay, 2=Sustain, 3=Release
    std::string getLabel() override; // Implemented later
};

// Custom ParamQuantity for ENV_SPEED_PARAM knob
struct ADSRSpeedParamQuantity : ParamQuantity {
    std::string getLabel() override; // Implemented later
};

// Custom Touch Strip Widget - Declaration only
struct TouchStripWidget : Widget {
    Evocation* module;

    // Visual properties
    Vec currentTouchPos = Vec(0, 0);
    bool isDragging = false;
    bool showTouch = false;
    
    // Animation and visual effects
    float glowIntensity = 0.0f;
    struct LightPulse {
        Vec pos;
        float intensity;
    };
    std::vector<LightPulse> lightPulses;
    float lastSampleTime = -1.0f;
    // Capture gesture samples at ~480 Hz for higher resolution playback
    static constexpr float MIN_SAMPLE_INTERVAL = 1.f / 480.f;
    float lastADSRSustainLevel = -1.f;
    float lastADSRReleaseTime = -1.f;
    float lastADSRReleaseContour = -1.f;

    TouchStripWidget(Evocation* module);

    // Method declarations only - implementations after Evocation class
    void onButton(const event::Button& e) override;
    void onDragStart(const event::DragStart& e) override;
    void onDragMove(const event::DragMove& e) override;
    void onDragEnd(const event::DragEnd& e) override;
    void recordSample(const char* stage, bool force = false);
    float computeNormalizedVoltage() const;
    void createPulse(Vec pos);
    void clearPulses();
    Vec clampToBounds(Vec pos) const;
    Vec resolveMouseLocal(const Vec& fallback);
    void resetForNewRecording();
    void logTouchDebug(const char* stage, const Vec& localPos, float normalizedTime, float normalizedVoltage);
    void step() override;
    void drawLayer(const DrawArgs& args, int layer) override;
    void drawTouchStrip(const DrawArgs& args);
    void drawBackground(const DrawArgs& args);
    void drawEnvelope(const DrawArgs& args);
    void drawEnvelopeStandard(const DrawArgs& args);
    void drawEnvelopeVoltageTime(const DrawArgs& args);
    void drawCurrentTouch(const DrawArgs& args);
    void drawPulses(const DrawArgs& args);
    void drawBorder(const DrawArgs& args);
    void drawInstructions(const DrawArgs& args);
    void applyADSRTouch(bool initial);
    float computeNormalizedHorizontal() const;
};

// Main Evocation Module
struct Evocation : Module {
    enum ParamId {
        TRIGGER_PARAM,
        CLEAR_PARAM,
        TRIM_LEAD_PARAM,
        TRIM_TAIL_PARAM,
        SPEED_1_PARAM,
        SPEED_2_PARAM,
        SPEED_3_PARAM,
        SPEED_4_PARAM,
        LOOP_1_PARAM,
        LOOP_2_PARAM,
        LOOP_3_PARAM,
        LOOP_4_PARAM,
        INVERT_1_PARAM,
        INVERT_2_PARAM,
        INVERT_3_PARAM,
        INVERT_4_PARAM,
        ENVELOPE_ADVANCE_PARAM,
        PARAM_ADVANCE_PARAM,
        ENV_SPEED_PARAM,
        ENV_PHASE_PARAM,
        ENV_SELECT_1_PARAM,
        ENV_SELECT_2_PARAM,
        ENV_SELECT_3_PARAM,
        ENV_SELECT_4_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        TRIGGER_INPUT,
        CLEAR_INPUT,
        GATE_INPUT,
        SPEED_1_INPUT,
        SPEED_2_INPUT,
        SPEED_3_INPUT,
        SPEED_4_INPUT,
        PHASE_1_INPUT,
        PHASE_2_INPUT,
        PHASE_3_INPUT,
        PHASE_4_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        ENV_1_OUTPUT,
        ENV_2_OUTPUT,
        ENV_3_OUTPUT,
        ENV_4_OUTPUT,
        ENV_1_GATE_OUTPUT,
        ENV_2_GATE_OUTPUT,
        ENV_3_GATE_OUTPUT,
        ENV_4_GATE_OUTPUT,
        ENV_1_EOC_OUTPUT,
        ENV_2_EOC_OUTPUT,
        ENV_3_EOC_OUTPUT,
        ENV_4_EOC_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        RECORDING_LIGHT,
        TRIGGER_LIGHT,
        LOOP_1_LIGHT,
        LOOP_2_LIGHT,
        LOOP_3_LIGHT,
        LOOP_4_LIGHT,
        INVERT_1_LIGHT,
        INVERT_2_LIGHT,
        INVERT_3_LIGHT,
        INVERT_4_LIGHT,
        LIGHTS_LEN
    };

    struct EnvelopePoint {
        float x;        // normalized position 0-1
        float y;        // normalized amplitude 0-1
        float time;     // normalized time 0-1
    };

    enum class EditableParam : int {
        Speed = 0,
        Loop,
        Invert,
        Phase,
        Count
    };

    static constexpr int NUM_ENVELOPES = 4;
    static constexpr int NUM_EDIT_PARAMS = static_cast<int>(EditableParam::Count);

    enum class EnvelopeMode {
        GESTURE = 0,
        ADSR = 1
    };

    EnvelopeMode mode = EnvelopeMode::GESTURE;

    // ADSR parameters (times in seconds, contour controls stored 0-1 with 0.5 = linear)
    float adsrAttackTime = 0.01f;    // Fast attack (10ms)
    float adsrDecayTime = 0.5f;      // Medium decay (500ms)
    float adsrSustainLevel = 0.5f;   // Mid-level sustain
    float adsrReleaseTime = 2.0f;    // 2 second release
    float adsrAttackContour = 0.5f;  // mapped from ENV_PHASE_PARAM when ENV1 selected
    float adsrDecayContour = 0.5f;   // mapped from ENV_PHASE_PARAM when ENV2 selected
    float adsrSustainContour = 0.5f; // mapped from ENV_PHASE_PARAM when ENV3 selected
    float adsrReleaseContour = 0.5f; // mapped from ENV_PHASE_PARAM when ENV4 selected

    int currentEnvelopeIndex = 0;
    int currentParameterIndex = 0;

    std::vector<EnvelopePoint> envelope;
    std::vector<EnvelopePoint> gestureEnvelopeBackup;
    float gestureDurationBackup = 2.0f;
    bool gestureBufferHasDataBackup = false;
    bool isRecording = false;
    bool bufferHasData = false;
    bool debugTouchLogging = false;

    // Polyphony configuration
    static const int MAX_POLY_CHANNELS = 8;

    // Individual loop states for each envelope player
    bool loopStates[4] = {false, false, false, false};

    // Invert states for each speed output
    bool invertStates[4] = {false, false, false, false};

    // Playback state for each output - now supports up to 6 voices per output
    struct PlaybackState {
        bool active[MAX_POLY_CHANNELS] = {false};
        float phase[MAX_POLY_CHANNELS] = {0.0f};
        dsp::PulseGenerator eocPulse[MAX_POLY_CHANNELS];
        float smoothedVoltage[MAX_POLY_CHANNELS] = {0.0f};
        bool releaseActive[MAX_POLY_CHANNELS] = {false};
        float releaseValue[MAX_POLY_CHANNELS] = {0.0f};
    };

    PlaybackState playback[4]; // Four independent envelope players (each with 6 voices)
    bool adsrSurfaceGate = false;
    bool previousGateHigh[MAX_POLY_CHANNELS] = {false}; // Track gate state per voice
    bool adsrGateHeld[MAX_POLY_CHANNELS] = {false};      // Sustain hold per voice

    static constexpr float ADSR_TRIGGER_PULSE_TIME = 1e-3f;

    struct ADSRVoiceState {
        shapetaker::dsp::EnvelopeGenerator env;
        shapetaker::dsp::EnvelopeGenerator::Stage prevStage = shapetaker::dsp::EnvelopeGenerator::IDLE;
    };
    ADSRVoiceState adsrVoices[MAX_POLY_CHANNELS];
    dsp::PulseGenerator adsrTriggerPulses[MAX_POLY_CHANNELS];
    bool adsrGateSignals[MAX_POLY_CHANNELS] = {false};
    float adsrValues[MAX_POLY_CHANNELS] = {0.0f};
    bool adsrCompleted[MAX_POLY_CHANNELS] = {false};
    float adsrReleaseStartLevel[MAX_POLY_CHANNELS] = {0.0f};
    float adsrPhaseNormalized[MAX_POLY_CHANNELS] = {0.0f};

    // Track current input channel counts for output channel management
    int currentTriggerChannels = 0;
    int currentGateChannels = 0;

    // Voice allocation for monophonic inputs
    int nextVoiceIndex = 0; // Round-robin voice allocation

    // Triggers
    dsp::SchmittTrigger triggerTrigger; // For button only
    dsp::SchmittTrigger triggerInputTriggers[MAX_POLY_CHANNELS]; // Per-channel trigger input
    dsp::SchmittTrigger gateTrigger;
    dsp::SchmittTrigger clearTrigger;
    dsp::SchmittTrigger trimLeadButtonTrigger;
    dsp::SchmittTrigger trimTailButtonTrigger;
    dsp::SchmittTrigger envSelectTriggers[4];
    bool envelopeAdvanceButtonLatch = false;
    bool parameterAdvanceButtonLatch = false;
    
    // Recording timing
    float recordingTime = 0.0f;
    float maxRecordingTime = 5.0f; // 5 seconds max
    float firstSampleTime = -1.0f;
    float phaseOffsets[4] = {0.f, 0.f, 0.f, 0.f};
    float envSpeedControlCache = 1.0f;
    float envPhaseControlCache = 0.0f;
    float selectionFlashTimer = 0.0f;

    float recordedDuration = 2.0f;

    // Track last touched parameter for OLED display
    struct LastTouchedParam {
        std::string name = "";
        std::string value = "";
        float timer = 0.0f;
        bool hasParam = false;
    } lastTouched;

    // Reference to the touch strip widget for clearing gesture lights
    TouchStripWidget* touchStripWidget = nullptr;

    ~Evocation() override {
        if (debugTouchLogging) {
            INFO("Evocation::~Evocation envelopeSize=%zu bufferHasData=%d", envelope.size(), bufferHasData);
        }
    }
    
    Evocation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(TRIGGER_PARAM, 0.f, 1.f, 0.f, "Manual Trigger");
        configParam(CLEAR_PARAM, 0.f, 1.f, 0.f, "Clear Buffer");
        configButton(TRIM_LEAD_PARAM, "Trim Gesture Lead");
        configButton(TRIM_TAIL_PARAM, "Trim Gesture Tail");
        configParam(SPEED_1_PARAM, 0.0f, 16.0f, 1.0f, "Speed 1", "×");
        configParam(SPEED_2_PARAM, 0.0f, 16.0f, 2.0f, "Speed 2", "×");
        configParam(SPEED_3_PARAM, 0.0f, 16.0f, 4.0f, "Speed 3", "×");
        configParam(SPEED_4_PARAM, 0.0f, 16.0f, 8.0f, "Speed 4", "×");
        configParam(LOOP_1_PARAM, 0.f, 1.f, 0.f, "Loop Output 1");
        configParam(LOOP_2_PARAM, 0.f, 1.f, 0.f, "Loop Output 2");
        configParam(LOOP_3_PARAM, 0.f, 1.f, 0.f, "Loop Output 3");
        configParam(LOOP_4_PARAM, 0.f, 1.f, 0.f, "Loop Output 4");
        configParam(INVERT_1_PARAM, 0.f, 1.f, 0.f, "Invert Output 1");
        configParam(INVERT_2_PARAM, 0.f, 1.f, 0.f, "Invert Output 2");
        configParam(INVERT_3_PARAM, 0.f, 1.f, 0.f, "Invert Output 3");
        configParam(INVERT_4_PARAM, 0.f, 1.f, 0.f, "Invert Output 4");
        configButton(ENVELOPE_ADVANCE_PARAM, "Next Envelope");
        configButton(PARAM_ADVANCE_PARAM, "Next Parameter");

        // Use custom ParamQuantity for dynamic speed knob label
        ADSRSpeedParamQuantity* speedQ = new ADSRSpeedParamQuantity();
        speedQ->module = this;
        speedQ->paramId = ENV_SPEED_PARAM;
        speedQ->name = "Selected Envelope Speed";
        speedQ->minValue = 0.0f;  // 0-16 range to support both modes
        speedQ->maxValue = 16.0f;
        speedQ->defaultValue = 1.0f;
        speedQ->unit = "×";
        paramQuantities[ENV_SPEED_PARAM] = speedQ;

        configParam(ENV_PHASE_PARAM, 0.f, 1.f, 0.f, "Selected Envelope Phase");
        // Use custom ParamQuantity for dynamic ADSR/Gesture mode labels
        ADSRStageButtonQuantity* adsrQ1 = new ADSRStageButtonQuantity();
        adsrQ1->stageIndex = 0;
        adsrQ1->module = this;
        adsrQ1->paramId = ENV_SELECT_1_PARAM;
        adsrQ1->name = "Select Envelope 1";
        adsrQ1->minValue = 0.f;
        adsrQ1->maxValue = 1.f;
        adsrQ1->defaultValue = 0.f;
        paramQuantities[ENV_SELECT_1_PARAM] = adsrQ1;

        ADSRStageButtonQuantity* adsrQ2 = new ADSRStageButtonQuantity();
        adsrQ2->stageIndex = 1;
        adsrQ2->module = this;
        adsrQ2->paramId = ENV_SELECT_2_PARAM;
        adsrQ2->name = "Select Envelope 2";
        adsrQ2->minValue = 0.f;
        adsrQ2->maxValue = 1.f;
        adsrQ2->defaultValue = 0.f;
        paramQuantities[ENV_SELECT_2_PARAM] = adsrQ2;

        ADSRStageButtonQuantity* adsrQ3 = new ADSRStageButtonQuantity();
        adsrQ3->stageIndex = 2;
        adsrQ3->module = this;
        adsrQ3->paramId = ENV_SELECT_3_PARAM;
        adsrQ3->name = "Select Envelope 3";
        adsrQ3->minValue = 0.f;
        adsrQ3->maxValue = 1.f;
        adsrQ3->defaultValue = 0.f;
        paramQuantities[ENV_SELECT_3_PARAM] = adsrQ3;

        ADSRStageButtonQuantity* adsrQ4 = new ADSRStageButtonQuantity();
        adsrQ4->stageIndex = 3;
        adsrQ4->module = this;
        adsrQ4->paramId = ENV_SELECT_4_PARAM;
        adsrQ4->name = "Select Envelope 4";
        adsrQ4->minValue = 0.f;
        adsrQ4->maxValue = 1.f;
        adsrQ4->defaultValue = 0.f;
        paramQuantities[ENV_SELECT_4_PARAM] = adsrQ4;
        configInput(TRIGGER_INPUT, "External Trigger");
        configInput(CLEAR_INPUT, "Clear Trigger");
        configInput(GATE_INPUT, "Gate Input");
        configInput(SPEED_1_INPUT, "Speed 1 CV");
        configInput(SPEED_2_INPUT, "Speed 2 CV");
        configInput(SPEED_3_INPUT, "Speed 3 CV");
        configInput(SPEED_4_INPUT, "Speed 4 CV");
        configInput(PHASE_1_INPUT, "Phase 1 CV");
        configInput(PHASE_2_INPUT, "Phase 2 CV");
        configInput(PHASE_3_INPUT, "Phase 3 CV");
        configInput(PHASE_4_INPUT, "Phase 4 CV");

        configOutput(ENV_1_OUTPUT, "Envelope 1");
        configOutput(ENV_2_OUTPUT, "Envelope 2");
        configOutput(ENV_3_OUTPUT, "Envelope 3");
        configOutput(ENV_4_OUTPUT, "Envelope 4");
        configOutput(ENV_1_EOC_OUTPUT, "Envelope 1 EOC");
        configOutput(ENV_2_EOC_OUTPUT, "Envelope 2 EOC");
        configOutput(ENV_3_EOC_OUTPUT, "Envelope 3 EOC");
        configOutput(ENV_4_EOC_OUTPUT, "Envelope 4 EOC");
        configOutput(ENV_1_GATE_OUTPUT, "Envelope 1 Gate");
        configOutput(ENV_2_GATE_OUTPUT, "Envelope 2 Gate");
        configOutput(ENV_3_GATE_OUTPUT, "Envelope 3 Gate");
        configOutput(ENV_4_GATE_OUTPUT, "Envelope 4 Gate");

        resetADSREngine();
    }
    
    void process(const ProcessArgs& args) override {
        // Handle triggers using shared helpers
        bool triggerButtonPressed = triggerTrigger.process(params[TRIGGER_PARAM].getValue());
        bool clearPressed = shapetaker::TriggerHelper::processTrigger(
            clearTrigger,
            params[CLEAR_PARAM].getValue(),
            inputs[CLEAR_INPUT],
            1.f
        );
        bool trimLeadPressed = trimLeadButtonTrigger.process(params[TRIM_LEAD_PARAM].getValue());
        bool trimTailPressed = trimTailButtonTrigger.process(params[TRIM_TAIL_PARAM].getValue());
        for (int i = 0; i < 4; ++i) {
            if (envSelectTriggers[i].process(params[ENV_SELECT_1_PARAM + i].getValue())) {
                selectEnvelope(i);
            }
        }
        bool envelopeButtonPressed = params[ENVELOPE_ADVANCE_PARAM].getValue() > 0.5f;
        bool paramButtonPressed = params[PARAM_ADVANCE_PARAM].getValue() > 0.5f;

        if (envelopeButtonPressed && !envelopeAdvanceButtonLatch) {
            advanceEnvelopeSelection();
        }
        if (paramButtonPressed && !parameterAdvanceButtonLatch) {
            advanceParameterSelection();
        }
        envelopeAdvanceButtonLatch = envelopeButtonPressed;
        parameterAdvanceButtonLatch = paramButtonPressed;
        
        if (selectionFlashTimer > 0.f) {
            selectionFlashTimer = std::max(0.f, selectionFlashTimer - args.sampleTime);
        }

        // Decay last touched timer
        if (lastTouched.timer > 0.f) {
            lastTouched.timer = std::max(0.f, lastTouched.timer - args.sampleTime);
            if (lastTouched.timer <= 0.f) {
                lastTouched.hasParam = false;
            }
        }

        if (currentEnvelopeIndex >= 0 && currentEnvelopeIndex < NUM_ENVELOPES) {
            if (mode == EnvelopeMode::GESTURE) {
                // Gesture mode: speed and phase controls
                float speedControl = params[ENV_SPEED_PARAM].getValue();
                if (std::fabs(speedControl - envSpeedControlCache) > 1e-6f) {
                    envSpeedControlCache = speedControl;
                    params[SPEED_1_PARAM + currentEnvelopeIndex].setValue(envSpeedControlCache);
                    std::string speedStr = string::f("%.2fx", envSpeedControlCache);
                    updateLastTouched(string::f("ENV %d SPEED", currentEnvelopeIndex + 1), speedStr);
                } else {
                    float actualSpeed = params[SPEED_1_PARAM + currentEnvelopeIndex].getValue();
                    if (std::fabs(actualSpeed - envSpeedControlCache) > 1e-6f) {
                        envSpeedControlCache = actualSpeed;
                        params[ENV_SPEED_PARAM].setValue(actualSpeed);
                    }
                }

                float phaseControl = params[ENV_PHASE_PARAM].getValue();
                if (std::fabs(phaseControl - envPhaseControlCache) > 1e-6f) {
                    envPhaseControlCache = phaseControl;
                    phaseOffsets[currentEnvelopeIndex] = envPhaseControlCache;
                    float phaseDeg = envPhaseControlCache * 360.f;
                    updateLastTouched(string::f("ENV %d PHASE", currentEnvelopeIndex + 1), string::f("%.2f°", phaseDeg));
                } else {
                    float actualPhase = phaseOffsets[currentEnvelopeIndex];
                    if (std::fabs(actualPhase - envPhaseControlCache) > 1e-6f) {
                        envPhaseControlCache = actualPhase;
                        params[ENV_PHASE_PARAM].setValue(actualPhase);
                    }
                }
            } else {
                // ADSR mode: ENV_SPEED_PARAM controls current stage time/level
                float speedControl = params[ENV_SPEED_PARAM].getValue();
                // Normalize 0-16 range to 0-1 for ADSR mode
                speedControl = clamp(speedControl / 16.0f, 0.0f, 1.0f);

                // Map 0-1 knob range to 0.01-5 seconds for times, 0-1 for sustain level
                float targetValue;
                if (currentEnvelopeIndex == 2) {
                    // Sustain level: 0-1
                    targetValue = speedControl;
                } else {
                    // Attack/Decay/Release: 0.01-5 seconds
                    targetValue = 0.01f + speedControl * 4.99f;
                }

                bool changed = false;
                std::string paramName = "";
                std::string paramUnit = "";

                switch (currentEnvelopeIndex) {
                    case 0: // Attack
                        if (std::fabs(targetValue - adsrAttackTime) > 1e-6f) {
                            adsrAttackTime = targetValue;
                            changed = true;
                        } else {
                            // Sync knob to current value (inverse: 0.01-5s -> 0-1 -> 0-16)
                            float normalized = (adsrAttackTime - 0.01f) / 4.99f;
                            float currentKnobValue = normalized * 16.0f;
                            float actualKnobValue = params[ENV_SPEED_PARAM].getValue();
                            if (std::fabs(currentKnobValue - actualKnobValue) > 0.01f) {
                                params[ENV_SPEED_PARAM].setValue(currentKnobValue);
                            }
                        }
                        break;
                    case 1: // Decay
                        if (std::fabs(targetValue - adsrDecayTime) > 1e-6f) {
                            adsrDecayTime = targetValue;
                            changed = true;
                        } else {
                            float normalized = (adsrDecayTime - 0.01f) / 4.99f;
                            float currentKnobValue = normalized * 16.0f;
                            float actualKnobValue = params[ENV_SPEED_PARAM].getValue();
                            if (std::fabs(currentKnobValue - actualKnobValue) > 0.01f) {
                                params[ENV_SPEED_PARAM].setValue(currentKnobValue);
                            }
                        }
                        break;
                    case 2: // Sustain
                        if (std::fabs(targetValue - adsrSustainLevel) > 1e-6f) {
                            adsrSustainLevel = clamp(targetValue, 0.0f, 1.0f);
                            changed = true;
                        } else {
                            float currentKnobValue = adsrSustainLevel * 16.0f;
                            float actualKnobValue = params[ENV_SPEED_PARAM].getValue();
                            if (std::fabs(currentKnobValue - actualKnobValue) > 0.01f) {
                                params[ENV_SPEED_PARAM].setValue(currentKnobValue);
                            }
                        }
                        break;
                    case 3: // Release
                        if (std::fabs(targetValue - adsrReleaseTime) > 1e-6f) {
                            adsrReleaseTime = targetValue;
                            changed = true;
                        } else {
                            float normalized = (adsrReleaseTime - 0.01f) / 4.99f;
                            float currentKnobValue = normalized * 16.0f;
                            float actualKnobValue = params[ENV_SPEED_PARAM].getValue();
                            if (std::fabs(currentKnobValue - actualKnobValue) > 0.01f) {
                                params[ENV_SPEED_PARAM].setValue(currentKnobValue);
                            }
                        }
                        break;
                }

                // Handle contour control
                float contourControl = params[ENV_PHASE_PARAM].getValue();
                if (std::fabs(contourControl - envPhaseControlCache) > 1e-6f) {
                    envPhaseControlCache = contourControl;
                    switch (currentEnvelopeIndex) {
                        case 0: adsrAttackContour = contourControl; break;
                        case 1: adsrDecayContour = contourControl; break;
                        case 2: adsrSustainContour = contourControl; break;
                        case 3: adsrReleaseContour = contourControl; break;
                    }
                    changed = true;
                }

                if (changed) {
                    generateADSREnvelope();
                }
            }
        }

        // Handle loop switch for current envelope (latching switch - read param value directly)
        if (currentEnvelopeIndex >= 0 && currentEnvelopeIndex < NUM_ENVELOPES) {
            bool newLoopState = params[LOOP_1_PARAM].getValue() > 0.5f;
            if (loopStates[currentEnvelopeIndex] != newLoopState) {
                loopStates[currentEnvelopeIndex] = newLoopState;
                updateLastTouched(string::f("ENV %d LOOP", currentEnvelopeIndex + 1), loopStates[currentEnvelopeIndex] ? "ON" : "OFF");
            }
        }

        // Handle invert switch for current envelope (latching switch - read param value directly)
        if (currentEnvelopeIndex >= 0 && currentEnvelopeIndex < NUM_ENVELOPES) {
            bool newInvertState = params[INVERT_1_PARAM].getValue() > 0.5f;
            if (invertStates[currentEnvelopeIndex] != newInvertState) {
                invertStates[currentEnvelopeIndex] = newInvertState;
                updateLastTouched(string::f("ENV %d INVERT", currentEnvelopeIndex + 1), invertStates[currentEnvelopeIndex] ? "ON" : "OFF");
            }
        }
        
        // Handle clear
        if (clearPressed && mode == EnvelopeMode::GESTURE) {
            clearBuffer();
            updateLastTouched("CLEAR", "BUFFER CLEARED");
        }

        if (trimLeadPressed) {
            if (!trimGestureLeadingSilence()) {
                updateLastTouched("", "NO TRIM");
            }
        }
        if (trimTailPressed) {
            if (!trimGestureTrailingSilence()) {
                updateLastTouched("", "NO TRIM");
            }
        }

        // Update recording during gesture capture
        if (mode == EnvelopeMode::GESTURE && isRecording) {
            updateRecording(args.sampleTime);
        }

        // Handle triggers for playback
        // Track connected poly channel counts each process cycle
        int detectedTriggerChannels = inputs[TRIGGER_INPUT].isConnected()
            ? std::min(inputs[TRIGGER_INPUT].getChannels(), MAX_POLY_CHANNELS)
            : 0;
        int detectedGateChannels = inputs[GATE_INPUT].isConnected()
            ? std::min(inputs[GATE_INPUT].getChannels(), MAX_POLY_CHANNELS)
            : 0;

        if (mode == EnvelopeMode::ADSR) {
            processADSRTriggers(triggerButtonPressed, detectedTriggerChannels, detectedGateChannels);
            processADSROutputs(args);
        } else {
            currentTriggerChannels = detectedTriggerChannels;
            currentGateChannels = detectedGateChannels;

            // Manually pressed trigger button fires all voices
            if (triggerButtonPressed && bufferHasData) {
                triggerAllEnvelopes();
            }

            if (detectedTriggerChannels > 0 && bufferHasData) {
                // Handle polyphonic trigger input
                // Process triggers for each input channel
                for (int c = 0; c < detectedTriggerChannels; c++) {
                    if (triggerInputTriggers[c].process(inputs[TRIGGER_INPUT].getPolyVoltage(c))) {
                        bool forceRestart = (mode == EnvelopeMode::GESTURE);
                        triggerEnvelope(c, forceRestart);
                    }
                }

                // Stop any voices beyond the current channel count
                for (int c = detectedTriggerChannels; c < MAX_POLY_CHANNELS; c++) {
                    triggerInputTriggers[c].reset();
                    stopEnvelope(c);
                    adsrGateHeld[c] = false;
                    previousGateHigh[c] = false;
                }
            } else if (detectedGateChannels > 0 && bufferHasData) {
                // Handle polyphonic gate input (for gate mode)
                // Process each polyphonic channel
                for (int c = 0; c < detectedGateChannels; c++) {
                    bool gateHigh = inputs[GATE_INPUT].getPolyVoltage(c) >= 1.0f;

                    // Start playback on gate rising edge
                    if (gateHigh && !previousGateHigh[c] && bufferHasData) {
                        bool forceRestart = (mode == EnvelopeMode::GESTURE);
                        triggerEnvelope(c, forceRestart);
                    }
                    // Handle gate release
                    if (!gateHigh && previousGateHigh[c]) {
                        if (mode == EnvelopeMode::ADSR) {
                            adsrGateHeld[c] = false;
                        } else {
                            startGestureRelease(c);
                        }
                    } else if (gateHigh && mode == EnvelopeMode::ADSR) {
                        adsrGateHeld[c] = true;
                    }

                    previousGateHigh[c] = gateHigh;
                }

                // Stop any voices beyond the current channel count
                for (int c = detectedGateChannels; c < MAX_POLY_CHANNELS; c++) {
                    previousGateHigh[c] = false;
                    adsrGateHeld[c] = false;
                    stopEnvelope(c);
                }
            } else {
                // No inputs connected, reset gate state
                for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
                    previousGateHigh[c] = false;
                    adsrGateHeld[c] = false;
                    triggerInputTriggers[c].reset();
                }
            }

            // Process each output
            for (int i = 0; i < 4; i++) {
                processPlayback(i, args.sampleTime);
            }
        }

        // Update lights
        lights[RECORDING_LIGHT].setBrightness(isRecording ? 1.0f : 0.0f);
        lights[TRIGGER_LIGHT].setBrightness(isAnyPlaybackActive() ? 1.0f : 0.0f);

        // Update loop and invert lights for currently selected envelope
        if (currentEnvelopeIndex >= 0 && currentEnvelopeIndex < NUM_ENVELOPES) {
            lights[LOOP_1_LIGHT].setBrightness(loopStates[currentEnvelopeIndex] ? 1.0f : 0.0f);
            lights[INVERT_1_LIGHT].setBrightness(invertStates[currentEnvelopeIndex] ? 1.0f : 0.0f);
        }
    }
    
    void startRecording() {
        if (isRecording)
            return;

        isRecording = true;
        recordingTime = 0.0f;
        bufferHasData = false;
        envelope.clear();
        stopAllPlayback();
        firstSampleTime = -1.0f;
        if (touchStripWidget) {
            touchStripWidget->clearPulses();
        }
        recordedDuration = 2.0f;

        if (debugTouchLogging) {
            INFO("Evocation::startRecording");
        }

        if (touchStripWidget) {
        touchStripWidget->resetForNewRecording();
        }
    }

    void stopRecording() {
        if (!isRecording)
            return;

        isRecording = false;

        if (!envelope.empty()) {
            normalizeEnvelopeTiming();
            bufferHasData = true;

            float effectiveDuration = recordingTime;
            if (firstSampleTime >= 0.0f) {
                effectiveDuration -= firstSampleTime;
            }
            effectiveDuration = clamp(effectiveDuration, 1e-3f, maxRecordingTime);
            recordedDuration = effectiveDuration;

            if (debugTouchLogging) {
                INFO("Evocation::stopRecording normalized points=%zu duration=%.3f", envelope.size(), recordedDuration);
            }
        } else {
            bufferHasData = false;
            recordedDuration = 2.0f;
        }

        firstSampleTime = -1.0f;
    }
    
    void updateRecording(float sampleTime) {
        recordingTime += sampleTime;
        
        // Stop recording if max time reached
        if (recordingTime >= maxRecordingTime) {
            stopRecording();
        }
    }
    
    void addEnvelopePoint(float x, float y, float time) {
        EnvelopePoint point;
        point.x = clamp(x, 0.0f, 1.0f);
        point.y = clamp(y, 0.0f, 1.0f);
        point.time = clamp(time, 0.0f, 1.0f);
        envelope.push_back(point);
    }
    
    void addEnvelopeSample(float normalizedVoltage) {
        if (!isRecording) {
            return;
        }

        if (firstSampleTime < 0.0f) {
            firstSampleTime = recordingTime;
        }

        float effectiveTime = recordingTime - firstSampleTime;
        if (!std::isfinite(effectiveTime) || effectiveTime < 0.f) {
            effectiveTime = 0.f;
        }

        float normalizedTime = maxRecordingTime <= 0.f ? 0.f : clamp(effectiveTime / maxRecordingTime, 0.f, 1.f);

        if (!envelope.empty()) {
            float lastTime = envelope.back().time;
            if (normalizedTime <= lastTime + 1e-5f) {
                envelope.back().y = clamp(normalizedVoltage, 0.0f, 1.0f);
                envelope.back().x = envelope.back().time;
                return;
            }
        }

        addEnvelopePoint(normalizedTime, normalizedVoltage, normalizedTime);

        if (debugTouchLogging) {
            INFO("Evocation::addEnvelopeSample voltage=%.4f time=%.4f rawTime=%.4f", normalizedVoltage, normalizedTime, effectiveTime);
        }
    }

    void normalizeEnvelopeTiming() {
        if (envelope.size() < 2) return;

        // Remove consecutive duplicate Y values to eliminate "flat" sections
        // Keep first and last points always
        std::vector<EnvelopePoint> filtered;
        filtered.reserve(envelope.size());
        filtered.push_back(envelope[0]);

        const float minYDelta = 0.005f; // 0.5% threshold
        for (size_t i = 1; i < envelope.size() - 1; i++) {
            float prevY = filtered.back().y;
            float currY = envelope[i].y;
            float nextY = envelope[i + 1].y;

            // Keep point if Y value is changing (not flat)
            if (std::abs(currY - prevY) > minYDelta || std::abs(nextY - currY) > minYDelta) {
                filtered.push_back(envelope[i]);
            }
        }
        filtered.push_back(envelope.back());
        envelope = filtered;

        if (envelope.size() < 2) return;

        float startTime = envelope.front().time;
        float endTime = envelope.back().time;
        float range = std::max(endTime - startTime, 1e-3f);
        float lastValue = 0.f;
        for (size_t i = 0; i < envelope.size(); i++) {
            float normalized = (envelope[i].time - startTime) / range;
            normalized = clamp(normalized, 0.f, 1.f);
            if (i > 0) {
                normalized = fmaxf(normalized, lastValue);
            }
            envelope[i].time = normalized;
            lastValue = normalized;
        }

        // Ensure first point is exactly at 0.0 to avoid any floating point drift
        if (!envelope.empty()) {
            envelope[0].time = 0.0f;
        }

        if (debugTouchLogging) {
            INFO("Evocation::normalizeEnvelopeTiming start=%.4f end=%.4f range=%.4f filtered=%zu->%zu",
                 startTime, endTime, range, filtered.size(), envelope.size());
        }
    }

    bool trimGestureLeadingSilence(float threshold = 0.01f) {
        if (mode != EnvelopeMode::GESTURE) {
            return false;
        }
        if (!bufferHasData || envelope.size() < 2) {
            return false;
        }

        size_t firstIdx = 0;
        while (firstIdx < envelope.size() && envelope[firstIdx].y <= threshold) {
            firstIdx++;
        }

        if (firstIdx == 0 || firstIdx >= envelope.size()) {
            return false;
        }

        float firstTime = envelope[firstIdx].time;
        if (!(firstTime > 0.f && firstTime < 1.f)) {
            return false;
        }

        float remaining = 1.0f - firstTime;
        if (remaining <= 1e-5f) {
            return false;
        }

        std::vector<EnvelopePoint> trimmed;
        trimmed.reserve(envelope.size() - firstIdx + 2);
        trimmed.push_back({0.0f, 0.0f, 0.0f});

        for (size_t i = firstIdx; i < envelope.size(); ++i) {
            EnvelopePoint point = envelope[i];
            float shifted = (point.time - firstTime) / remaining;
            shifted = clamp(shifted, 0.0f, 1.0f);
            if (trimmed.size() == 1) {
                shifted = std::max(shifted, 1e-4f);
            }
            point.time = shifted;
            point.x = shifted;
            point.y = clamp(point.y, 0.0f, 1.0f);
            trimmed.push_back(point);
        }

        if (trimmed.size() < 2) {
            return false;
        }

        envelope = std::move(trimmed);
        normalizeEnvelopeTiming();

        recordedDuration = std::max(recordedDuration * remaining, 1e-3f);
        gestureEnvelopeBackup = envelope;
        gestureDurationBackup = recordedDuration;
        gestureBufferHasDataBackup = bufferHasData;

        updateLastTouched("", "TRIMMED");
        return true;
    }

    bool trimGestureTrailingSilence(float threshold = 0.01f) {
        if (mode != EnvelopeMode::GESTURE) {
            return false;
        }
        if (!bufferHasData || envelope.size() < 2) {
            return false;
        }

        ssize_t lastIdx = (ssize_t)envelope.size() - 1;
        while (lastIdx >= 0 && envelope[(size_t)lastIdx].y <= threshold) {
            --lastIdx;
        }

        if (lastIdx < 1) {
            return false;
        }

        float lastTime = envelope[(size_t)lastIdx].time;
        lastTime = clamp(lastTime, 1e-4f, 1.0f);
        if (!(lastTime > 0.f && lastTime <= 1.f)) {
            return false;
        }

        std::vector<EnvelopePoint> trimmed;
        trimmed.reserve((size_t)lastIdx + 2);

        for (size_t i = 0; i <= (size_t)lastIdx; ++i) {
            EnvelopePoint point = envelope[i];
            float scaled = lastTime <= 1e-6f ? 0.f : point.time / lastTime;
            scaled = clamp(scaled, 0.0f, 1.0f);
            if (i == (size_t)lastIdx) {
                scaled = 1.0f;
            }
            point.time = scaled;
            point.x = scaled;
            point.y = clamp(point.y, 0.0f, 1.0f);
            trimmed.push_back(point);
        }

        if (!trimmed.empty()) {
            if (trimmed.back().y > threshold) {
                trimmed.push_back({1.0f, 0.0f, 1.0f});
            } else {
                trimmed.back().y = 0.0f;
            }
        }

        envelope = std::move(trimmed);
        normalizeEnvelopeTiming();

        recordedDuration = std::max(recordedDuration * lastTime, 1e-3f);
        gestureEnvelopeBackup = envelope;
        gestureDurationBackup = recordedDuration;
        gestureBufferHasDataBackup = bufferHasData;

        updateLastTouched("", "TRIMMED");
        return true;
    }
    
    void clearBuffer() {
        envelope.clear();
        bufferHasData = false;
        isRecording = false;
        stopAllPlayback();
        firstSampleTime = -1.0f;
        recordedDuration = 2.0f;

        if (debugTouchLogging) {
            INFO("Evocation::clearBuffer");
        }

        // Clear light pulses from the touch strip widget
        if (touchStripWidget) {
            touchStripWidget->resetForNewRecording();
        }
    }
    
    void resetADSREngine() {
        nextVoiceIndex = 0;
        for (int voice = 0; voice < MAX_POLY_CHANNELS; ++voice) {
            adsrVoices[voice].env.reset();
            adsrVoices[voice].prevStage = shapetaker::dsp::EnvelopeGenerator::IDLE;
            adsrTriggerPulses[voice] = dsp::PulseGenerator();
            adsrGateSignals[voice] = false;
            adsrValues[voice] = 0.f;
            adsrCompleted[voice] = false;
            adsrReleaseStartLevel[voice] = 0.f;
            adsrPhaseNormalized[voice] = 0.f;
        }
    }
    
    void triggerAllEnvelopes() {
        if (!bufferHasData) return;

        if (mode == EnvelopeMode::ADSR) {
            for (int voice = 0; voice < MAX_POLY_CHANNELS; ++voice) {
                adsrTriggerPulses[voice].trigger(ADSR_TRIGGER_PULSE_TIME);
            }
            return;
        }

        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
                playback[i].active[c] = true;
                playback[i].phase[c] = 0.0f;
                playback[i].eocPulse[c] = dsp::PulseGenerator();
                playback[i].smoothedVoltage[c] = 0.0f;
                playback[i].releaseActive[c] = false;
                playback[i].releaseValue[c] = 0.0f;
                if (i == 0) {
                    adsrGateHeld[c] = false;
                    previousGateHigh[c] = false;
                }
            }
        }
    }

    int allocateTriggerVoice(int inputChannel, int totalChannels) {
        if (totalChannels <= 1) {
            int voice = nextVoiceIndex;
            nextVoiceIndex = (nextVoiceIndex + 1) % MAX_POLY_CHANNELS;
            return voice;
        }
        return clamp(inputChannel, 0, MAX_POLY_CHANNELS - 1);
    }

    void triggerEnvelope(int channel, bool forceRestart = false) {
        if (!bufferHasData || channel < 0 || channel >= MAX_POLY_CHANNELS) return;

        // Get current output voltage to find smooth retrigger point
        float currentVoltage = 0.0f;
        bool wasActive = false;

        for (int i = 0; i < 4; i++) {
            if (playback[i].active[channel]) {
                wasActive = true;
                // Get the current envelope value from output 0
                if (i == 0) {
                    float phase = playback[i].phase[channel];
                    if (phase >= 0.0f && phase < 1.0f) {
                        currentVoltage = interpolateEnvelope(phase);
                        if (invertStates[i]) {
                            currentVoltage = 1.0f - currentVoltage;
                        }
                    }
                }
            }
        }

        // If retriggering, find the closest phase point to current voltage to avoid clicks
        float startPhase = 0.0f;
        if (!forceRestart && wasActive && currentVoltage > 0.01f) {
            // Find the earliest phase in the envelope that matches current voltage
            startPhase = findPhaseForVoltage(currentVoltage);
        }

        for (int i = 0; i < 4; i++) {
            playback[i].active[channel] = true;
            playback[i].phase[channel] = startPhase;
            playback[i].eocPulse[channel] = dsp::PulseGenerator();
            playback[i].releaseActive[channel] = false;
            playback[i].releaseValue[channel] = 0.0f;
            playback[i].smoothedVoltage[channel] = 0.0f;
        }
    }

    // Find the phase in the envelope that best matches the target voltage
    // This prevents clicks when retriggering
    float findPhaseForVoltage(float targetVoltage) {
        if (envelope.empty()) return 0.0f;

        // Search for the earliest point in the envelope close to target voltage
        float bestPhase = 0.0f;
        float bestDiff = std::abs(envelope[0].y - targetVoltage);

        for (size_t i = 0; i < envelope.size(); i++) {
            float diff = std::abs(envelope[i].y - targetVoltage);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestPhase = envelope[i].time;
            }
            // Stop searching after we pass the target (prefer early phases)
            if (envelope[i].y < targetVoltage && i > 0) {
                break;
            }
        }

        return bestPhase;
    }

    void stopEnvelope(int channel) {
        if (channel < 0 || channel >= MAX_POLY_CHANNELS) return;

        for (int i = 0; i < 4; i++) {
            playback[i].active[channel] = false;
            playback[i].phase[channel] = 0.0f;
            playback[i].eocPulse[channel] = dsp::PulseGenerator();
            playback[i].smoothedVoltage[channel] = 0.0f;
            playback[i].releaseActive[channel] = false;
            playback[i].releaseValue[channel] = 0.0f;
        }
        adsrGateHeld[channel] = false;
        previousGateHigh[channel] = false;
    }

    void startGestureRelease(int channel) {
        if (channel < 0 || channel >= MAX_POLY_CHANNELS)
            return;
        for (int i = 0; i < 4; i++) {
            PlaybackState& pb = playback[i];
            if (!pb.active[channel])
                continue;
            pb.releaseActive[channel] = true;
            pb.releaseValue[channel] = pb.smoothedVoltage[channel];
            pb.releaseValue[channel] = clamp(pb.releaseValue[channel], -10.0f, 10.0f);
            pb.phase[channel] = clamp(pb.phase[channel], 0.0f, 1.0f);
        }
    }

    void processADSRTriggers(bool manualTrigger, int detectedTriggerChannels, int detectedGateChannels) {
        if (!bufferHasData)
            return;

        currentTriggerChannels = 0;
        currentGateChannels = detectedGateChannels;

        // Manual trigger acts as a one-shot gate
        if (manualTrigger) {
            int voice = allocateTriggerVoice(0, 1);
            adsrTriggerPulses[voice].trigger(ADSR_TRIGGER_PULSE_TIME);
            currentTriggerChannels = std::max(currentTriggerChannels, voice + 1);
        }

        // Process trigger inputs
        for (int c = 0; c < detectedTriggerChannels; c++) {
            if (triggerInputTriggers[c].process(inputs[TRIGGER_INPUT].getPolyVoltage(c))) {
                int voice = allocateTriggerVoice(c, detectedTriggerChannels);
                adsrTriggerPulses[voice].trigger(ADSR_TRIGGER_PULSE_TIME);
                currentTriggerChannels = std::max(currentTriggerChannels, voice + 1);
            }
        }

        for (int c = detectedTriggerChannels; c < MAX_POLY_CHANNELS; c++) {
            triggerInputTriggers[c].reset();
        }

        // Process gate inputs
        for (int c = 0; c < detectedGateChannels; c++) {
            bool gateHigh = inputs[GATE_INPUT].getPolyVoltage(c) >= 1.0f;
            int voice = (detectedGateChannels <= 1) ? 0 : clamp(c, 0, MAX_POLY_CHANNELS - 1);

            if (gateHigh && !previousGateHigh[c]) {
                adsrGateSignals[voice] = true;
                adsrTriggerPulses[voice].trigger(ADSR_TRIGGER_PULSE_TIME);
                currentGateChannels = std::max(currentGateChannels, voice + 1);
            } else if (!gateHigh && previousGateHigh[c]) {
                adsrGateSignals[voice] = false;
            }

            previousGateHigh[c] = gateHigh;
        }

        for (int c = detectedGateChannels; c < MAX_POLY_CHANNELS; c++) {
            previousGateHigh[c] = false;
            adsrGateSignals[c] = false;
        }

        if (detectedGateChannels == 0) {
            for (int voice = 0; voice < MAX_POLY_CHANNELS; ++voice) {
                adsrGateSignals[voice] = false;
            }
        }
    }

    void processADSROutputs(const ProcessArgs& args) {
        float sampleTime = args.sampleTime;
        float sampleRate = args.sampleRate > 0.f ? args.sampleRate : (sampleTime > 0.f ? 1.f / sampleTime : 44100.f);

        bool anyLoopEnabled = false;
        for (int i = 0; i < NUM_ENVELOPES; ++i) {
            if (loopStates[i]) {
                anyLoopEnabled = true;
                break;
            }
        }

        for (int voice = 0; voice < MAX_POLY_CHANNELS; ++voice) {
            bool pulseHigh = adsrTriggerPulses[voice].process(sampleTime);
            bool gateSignal = adsrGateSignals[voice] || pulseHigh || adsrSurfaceGate;

            shapetaker::dsp::EnvelopeGenerator& env = adsrVoices[voice].env;
            env.setAttack(adsrAttackTime, sampleRate);
            env.setDecay(adsrDecayTime, sampleRate);
            env.setSustain(adsrSustainLevel);
            env.setRelease(adsrReleaseTime, sampleRate);
            env.gate(gateSignal);

            float rawValue = env.process();
            auto stage = env.getCurrentStage();

            if (stage == shapetaker::dsp::EnvelopeGenerator::RELEASE && adsrVoices[voice].prevStage != shapetaker::dsp::EnvelopeGenerator::RELEASE) {
                adsrReleaseStartLevel[voice] = std::max(rawValue, 1e-3f);
            }
            if (stage == shapetaker::dsp::EnvelopeGenerator::IDLE) {
                adsrReleaseStartLevel[voice] = 0.f;
            }

            float shapedValue = rawValue;
            float sustain = clamp(adsrSustainLevel, 0.f, 1.f);
            switch (stage) {
                case shapetaker::dsp::EnvelopeGenerator::ATTACK: {
                    shapedValue = applyContour(rawValue, adsrAttackContour);
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::DECAY: {
                    float denom = std::max(1.f - sustain, 1e-6f);
                    float t = (1.f - rawValue) / denom;
                    t = clamp(t, 0.f, 1.f);
                    float shaped = applyContour(t, adsrDecayContour);
                    shapedValue = 1.f - shaped * (1.f - sustain);
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::SUSTAIN: {
                    shapedValue = sustain;
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::RELEASE: {
                    float startLevel = std::max(adsrReleaseStartLevel[voice], std::max(sustain, 1e-3f));
                    float t = (startLevel - rawValue) / std::max(startLevel, 1e-3f);
                    t = clamp(t, 0.f, 1.f);
                    float shaped = applyContour(t, adsrReleaseContour);
                    shapedValue = startLevel * (1.f - shaped);
                    break;
                }
                default:
                    break;
            }

            shapedValue = clamp(shapedValue, 0.f, 1.f);
            adsrValues[voice] = shapedValue;

            adsrCompleted[voice] = (adsrVoices[voice].prevStage != shapetaker::dsp::EnvelopeGenerator::IDLE && stage == shapetaker::dsp::EnvelopeGenerator::IDLE);
            adsrVoices[voice].prevStage = stage;

            if (anyLoopEnabled && adsrCompleted[voice] && !adsrGateSignals[voice]) {
                adsrTriggerPulses[voice].trigger(ADSR_TRIGGER_PULSE_TIME);
                adsrCompleted[voice] = false;
            }

            float attack = std::max(adsrAttackTime, 0.0f);
            float decay = std::max(adsrDecayTime, 0.0f);
            float releaseTime = std::max(adsrReleaseTime, 0.0f);
            float total = std::max(attack + decay + releaseTime, 1e-6f);
            float attackShare = attack / total;
            float decayShare = decay / total;
            float releaseShare = releaseTime / total;
            float phaseNorm = 0.f;

            switch (stage) {
                case shapetaker::dsp::EnvelopeGenerator::ATTACK: {
                    phaseNorm = attackShare * clamp(rawValue, 0.f, 1.f);
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::DECAY: {
                    float denom = std::max(1.f - sustain, 1e-6f);
                    float t = (1.f - rawValue) / denom;
                    t = clamp(t, 0.f, 1.f);
                    phaseNorm = attackShare + t * decayShare;
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::SUSTAIN: {
                    phaseNorm = attackShare + decayShare;
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::RELEASE: {
                    float startLevel = std::max(adsrReleaseStartLevel[voice], std::max(sustain, 1e-3f));
                    float t = (startLevel > 1e-6f) ? (startLevel - rawValue) / startLevel : 1.f;
                    t = clamp(t, 0.f, 1.f);
                    phaseNorm = attackShare + decayShare + t * releaseShare;
                    break;
                }
                case shapetaker::dsp::EnvelopeGenerator::IDLE:
                default: {
                    phaseNorm = (adsrGateSignals[voice] || adsrValues[voice] > 1e-3f) ? 1.f : 0.f;
                    break;
                }
            }

            adsrPhaseNormalized[voice] = clamp(phaseNorm, 0.f, 1.f);
        }

        int outputChannels = std::max(currentTriggerChannels, currentGateChannels);
        if (outputChannels == 0) {
            for (int voice = MAX_POLY_CHANNELS - 1; voice >= 0; --voice) {
                if (adsrValues[voice] > 1e-4f || adsrGateSignals[voice]) {
                    outputChannels = voice + 1;
                    break;
                }
            }
        }
        if (outputChannels == 0)
            outputChannels = 1;

        for (int outputIndex = 0; outputIndex < 4; ++outputIndex) {
            PlaybackState& pb = playback[outputIndex];

            outputs[ENV_1_OUTPUT + outputIndex].setChannels(outputChannels);
            outputs[ENV_1_EOC_OUTPUT + outputIndex].setChannels(outputChannels);
            outputs[ENV_1_GATE_OUTPUT + outputIndex].setChannels(outputChannels);

            for (int c = 0; c < outputChannels; ++c) {
                float envValue = (c < MAX_POLY_CHANNELS) ? adsrValues[c] : 0.f;
                envValue = clamp(envValue, 0.f, 1.f);

                if (invertStates[outputIndex]) {
                    envValue = 1.f - envValue;
                }

                float outputVoltage = envValue * 10.f;
                outputs[ENV_1_OUTPUT + outputIndex].setVoltage(outputVoltage, c);

                bool gateHigh = (c < MAX_POLY_CHANNELS) ? (adsrGateSignals[c] || adsrValues[c] > 1e-3f) : false;
                outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(gateHigh ? 10.f : 0.f, c);

                bool completed = (c < MAX_POLY_CHANNELS) ? adsrCompleted[c] : false;
                if (completed) {
                    pb.eocPulse[c].trigger(1e-3f);
                }
                float eocVoltage = pb.eocPulse[c].process(sampleTime) ? 10.f : 0.f;
                outputs[ENV_1_EOC_OUTPUT + outputIndex].setVoltage(eocVoltage, c);

                pb.active[c] = gateHigh;
                pb.phase[c] = (c < MAX_POLY_CHANNELS) ? adsrPhaseNormalized[c] : 0.f;
                pb.smoothedVoltage[c] = outputVoltage;
            }
        }

        for (int voice = 0; voice < MAX_POLY_CHANNELS; ++voice) {
            adsrCompleted[voice] = false;
        }
    }
    
    void processPlayback(int outputIndex, float sampleTime) {
        PlaybackState& pb = playback[outputIndex];

        if (!bufferHasData) {
            outputs[ENV_1_OUTPUT + outputIndex].setChannels(0);
            outputs[ENV_1_EOC_OUTPUT + outputIndex].setChannels(0);
            outputs[ENV_1_GATE_OUTPUT + outputIndex].setChannels(0);
            return;
        }

        // Determine output channel count based on inputs and active voices
        int outputChannels = std::max(currentTriggerChannels, currentGateChannels);
        outputChannels = std::max(outputChannels, getActiveVoiceChannels(outputIndex));
        if (outputChannels == 0) {
            outputChannels = 1; // Always output at least 1 channel when buffer has data
        }

        // Set output channel counts to match input
        outputs[ENV_1_OUTPUT + outputIndex].setChannels(outputChannels);
        outputs[ENV_1_EOC_OUTPUT + outputIndex].setChannels(outputChannels);
        outputs[ENV_1_GATE_OUTPUT + outputIndex].setChannels(outputChannels);

        // Process each polyphonic channel
        for (int c = 0; c < outputChannels; c++) {
            // Process EOC pulse
            bool eocPulse = pb.eocPulse[c].process(sampleTime);
            outputs[ENV_1_EOC_OUTPUT + outputIndex].setVoltage(eocPulse ? 10.0f : 0.0f, c);

            if (!pb.active[c]) {
                pb.smoothedVoltage[c] = 0.0f;
                outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f, c);
                outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(0.0f, c);
                continue;
            }

            bool gestureRelease = (mode == EnvelopeMode::GESTURE) && pb.releaseActive[c];

            if (gestureRelease) {
                const float releaseTau = 0.02f; // 20 ms glide to zero
                float decay = std::exp(-sampleTime / std::max(releaseTau, 1e-6f));
                pb.releaseValue[c] *= decay;
                if (std::fabs(pb.releaseValue[c]) <= 1e-3f) {
                    pb.releaseValue[c] = 0.0f;
                    pb.releaseActive[c] = false;
                    pb.active[c] = false;
                    pb.phase[c] = 0.0f;
                    pb.smoothedVoltage[c] = 0.0f;
                    outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f, c);
                    outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(0.0f, c);
                    continue;
                }

                float targetVoltage = pb.releaseValue[c];
                float smoothingTau = 0.001f;
                float alpha = smoothingTau <= 0.f ? 1.f : sampleTime / (smoothingTau + sampleTime);
                alpha = clamp(alpha, 0.f, 1.f);
                float previousVoltage = pb.smoothedVoltage[c];
                float outputVoltage = previousVoltage + (targetVoltage - previousVoltage) * alpha;
                pb.smoothedVoltage[c] = outputVoltage;
                outputs[ENV_1_OUTPUT + outputIndex].setVoltage(outputVoltage, c);
                outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(0.0f, c);
                continue;
            }

            // Get speed from knob and CV
            float speed;
            if (mode == EnvelopeMode::ADSR) {
                // ADSR mode: all outputs use 1x speed (timing baked into envelope)
                speed = 1.0f;
                // Still allow CV modulation if connected
                if (inputs[SPEED_1_INPUT + outputIndex].isConnected()) {
                    speed += inputs[SPEED_1_INPUT + outputIndex].getPolyVoltage(c);
                    speed = clamp(speed, 0.1f, 16.0f);
                }
            } else {
                // Gesture mode: each output has individual speed
                speed = params[SPEED_1_PARAM + outputIndex].getValue();
                if (inputs[SPEED_1_INPUT + outputIndex].isConnected()) {
                    speed += inputs[SPEED_1_INPUT + outputIndex].getPolyVoltage(c); // 1V/oct style
                }
                speed = clamp(speed, 0.1f, 16.0f); // Reasonable speed limits
            }

            // Advance phase
            float envDuration = getEnvelopeDuration();
            float phaseIncrement = speed * sampleTime / envDuration;
            pb.phase[c] += phaseIncrement;

            if (mode == EnvelopeMode::ADSR) {
                float sustainStart = (adsrAttackTime + adsrDecayTime) / envDuration;
                sustainStart = clamp(sustainStart, 0.0f, 1.0f);
                bool holdAtSustain = adsrSurfaceGate || adsrGateHeld[c];
                if (holdAtSustain && pb.phase[c] >= sustainStart) {
                    pb.phase[c] = sustainStart;
                }
            }

            // Check if envelope is complete
            if (pb.phase[c] >= 1.0f) {
                // Trigger EOC pulse
                pb.eocPulse[c].trigger(1e-3f); // 1ms pulse

                if (loopStates[outputIndex]) {
                    pb.phase[c] -= 1.0f; // Wrap around for looping
                } else {
                    pb.active[c] = false;
                    outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f, c);
                    outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(0.0f, c);
                    continue;
                }
            }

            // Interpolate envelope value at current phase
            float samplePhase;
            float phaseOffset = 0.0f;

            if (mode == EnvelopeMode::ADSR) {
                // ADSR mode: use phase CV for quantized delays
                if (inputs[PHASE_1_INPUT + outputIndex].isConnected()) {
                    // Quantize to 16th notes (16 steps per full envelope)
                    float cv = inputs[PHASE_1_INPUT + outputIndex].getPolyVoltage(c) / 10.0f; // 0-1 range
                    phaseOffset = std::floor(cv * 16.0f) / 16.0f; // Quantize to 1/16th increments
                }
                samplePhase = pb.phase[c] + phaseOffset;
                samplePhase -= std::floor(samplePhase);
            } else {
                // Gesture mode: apply phase offset for each output
                phaseOffset = phaseOffsets[outputIndex];

                // Add CV modulation if connected (0-10V = 0-1 phase)
                if (inputs[PHASE_1_INPUT + outputIndex].isConnected()) {
                    phaseOffset += inputs[PHASE_1_INPUT + outputIndex].getPolyVoltage(c) / 10.0f;
                }

                samplePhase = pb.phase[c] + phaseOffset;
                samplePhase -= std::floor(samplePhase);
                if (samplePhase < 0.f) samplePhase += 1.f;
            }

            float envelopeValue = interpolateEnvelope(samplePhase);

            if (invertStates[outputIndex]) {
                envelopeValue = 1.0f - envelopeValue;
            }

            float targetVoltage = envelopeValue * 10.0f;
            float outputVoltage = targetVoltage;

            if (mode == EnvelopeMode::GESTURE) {
                float speedFactor = std::max(speed, 0.1f);
                float smoothingTau = 0.0002f / std::max(speedFactor, 1.0f); // shorter tau for higher speeds
                smoothingTau = clamp(smoothingTau, 1e-5f, 0.0005f);
                float alpha = smoothingTau <= 0.f ? 1.f : sampleTime / (smoothingTau + sampleTime);
                alpha = clamp(alpha, 0.f, 1.f);
                float previousVoltage = pb.smoothedVoltage[c];
                outputVoltage = previousVoltage + (targetVoltage - previousVoltage) * alpha;
                pb.smoothedVoltage[c] = outputVoltage;
            } else {
                pb.smoothedVoltage[c] = targetVoltage;
            }

            outputs[ENV_1_OUTPUT + outputIndex].setVoltage(outputVoltage, c); // 0-10V output
            float gateVoltage = (pb.active[c] && mode == EnvelopeMode::GESTURE) ? 10.0f : 0.0f;
            if (mode == EnvelopeMode::ADSR) {
                gateVoltage = pb.active[c] ? 10.0f : 0.0f;
            }
            outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(gateVoltage, c);
        }
    }
    
    float interpolateEnvelope(float phase) {
        if (envelope.empty()) return 0.0f;
        if (envelope.size() == 1) return envelope[0].y;

        // If phase is before first point, return first point's value
        if (phase <= envelope[0].time) {
            return envelope[0].y;
        }

        // Find the two points to interpolate between
        for (size_t i = 0; i < envelope.size() - 1; i++) {
            if (phase >= envelope[i].time && phase <= envelope[i + 1].time) {
                float t = (phase - envelope[i].time) / (envelope[i + 1].time - envelope[i].time);
                return envelope[i].y + t * (envelope[i + 1].y - envelope[i].y);
            }
        }

        // Return last point if phase is beyond envelope
        return envelope.back().y;
    }
    
    bool hasRecordedEnvelope() const {
        return bufferHasData && !envelope.empty();
    }

    float getRecordedDuration() const {
        return std::max(recordedDuration, 1e-3f);
    }

    float getPlaybackPhase(int index, int channel = 0) const {
        if (mode == EnvelopeMode::ADSR) {
            int voice = channel;
            if (voice < 0 || voice >= MAX_POLY_CHANNELS) {
                voice = -1;
                for (int v = 0; v < MAX_POLY_CHANNELS; ++v) {
                    if (adsrGateSignals[v] || adsrValues[v] > 1e-3f) {
                        voice = v;
                        break;
                    }
                }
                if (voice < 0)
                    voice = 0;
            }
            return clamp(adsrPhaseNormalized[voice], 0.0f, 1.0f);
        }
        if (index < 0 || index >= 4 || channel < 0 || channel >= MAX_POLY_CHANNELS)
            return 0.0f;
        return clamp(playback[index].phase[channel], 0.0f, 1.0f);
    }

    bool isPlaybackActive(int index, int channel = 0) const {
        if (mode == EnvelopeMode::ADSR) {
            if (channel >= 0 && channel < MAX_POLY_CHANNELS) {
                return adsrGateSignals[channel] || adsrValues[channel] > 1e-3f;
            }
            for (int v = 0; v < MAX_POLY_CHANNELS; ++v) {
                if (adsrGateSignals[v] || adsrValues[v] > 1e-3f)
                    return true;
            }
            return false;
        }
        if (index < 0 || index >= 4 || channel < 0 || channel >= MAX_POLY_CHANNELS)
            return false;
        return playback[index].active[channel];
    }

    int getActiveVoiceChannels(int index) const {
        if (mode == EnvelopeMode::ADSR) {
            int channels = 0;
            for (int v = 0; v < MAX_POLY_CHANNELS; ++v) {
                if (adsrGateSignals[v] || adsrValues[v] > 1e-3f) {
                    channels = v + 1;
                }
            }
            return channels;
        }
        if (index < 0 || index >= NUM_ENVELOPES)
            return 0;
        int channels = 0;
        const PlaybackState& pb = playback[index];
        for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
            if (pb.active[c]) {
                channels = c + 1;
            }
        }
        return channels;
    }

    float getEnvelopeDuration() {
        if (mode == EnvelopeMode::ADSR) {
            return adsrAttackTime + adsrDecayTime + adsrReleaseTime;
        }
        return getRecordedDuration();
    }

    // Map stored 0-1 contour control to a bipolar -1 to 1 range
    static float mapContourControl(float value) {
        return clamp((value - 0.5f) * 2.0f, -1.0f, 1.0f);
    }

    // Apply contour curve to a linear 0-1 value
    // contour 0.0 = logarithmic, 0.5 = linear, 1.0 = exponential
    float applyContour(float linear, float contour) {
        if (std::fabs(contour - 0.5f) < 0.01f) {
            // Near center = linear
            return linear;
        } else if (contour > 0.5f) {
            // Above 0.5 = exponential (steeper start, slower end)
            float amount = (contour - 0.5f) * 2.0f; // 0-1 range
            float curve = 1.0f + amount * 3.0f; // 1 to 4 exponent
            return std::pow(linear, curve);
        } else {
            // Below 0.5 = logarithmic (slower start, steeper end)
            float amount = (0.5f - contour) * 2.0f; // 0-1 range
            float curve = 1.0f + amount * 3.0f; // 1 to 4 exponent
            return 1.0f - std::pow(1.0f - linear, curve);
        }
    }

    // Generate ADSR envelope from current parameters
    void generateADSREnvelope() {
        envelope.clear();

        float totalTime = adsrAttackTime + adsrDecayTime + adsrReleaseTime;
        if (totalTime < 0.001f) totalTime = 0.001f;

        // Attack phase
        int attackPoints = std::max(2, (int)(adsrAttackTime * 20.0f)); // 20 points per second
        float attackContour = mapContourControl(adsrAttackContour);
        for (int i = 0; i < attackPoints; i++) {
            float t = (float)i / (attackPoints - 1);
            float curved = applyContour(t, attackContour);
            float time = (adsrAttackTime * t) / totalTime;
            envelope.push_back({0.0f, curved, time});
        }

        // Decay phase
        float decayStart = adsrAttackTime / totalTime;
        int decayPoints = std::max(2, (int)(adsrDecayTime * 20.0f));
        float decayContour = mapContourControl(adsrDecayContour);
        float clampedSustain = clamp(adsrSustainLevel, 0.0f, 1.0f);
        for (int i = 0; i < decayPoints; i++) {
            float t = (float)i / (decayPoints - 1);
            float curved = applyContour(t, decayContour);
            float level = 1.0f - curved * (1.0f - clampedSustain);
            float time = decayStart + (adsrDecayTime * t) / totalTime;
            envelope.push_back({0.0f, level, time});
        }

        // Sustain point (will hold here during gate)
        float sustainStart = (adsrAttackTime + adsrDecayTime) / totalTime;
        envelope.push_back({0.0f, clampedSustain, sustainStart});

        // Release phase (from sustain level to 0)
        float releaseStart = sustainStart;
        int releasePoints = std::max(2, (int)(adsrReleaseTime * 20.0f));
        float releaseContour = mapContourControl(adsrReleaseContour);
        for (int i = 1; i < releasePoints; i++) {
            float t = (float)i / (releasePoints - 1);
            float curved = applyContour(t, releaseContour);
            float level = clampedSustain * (1.0f - curved);
            float time = releaseStart + (adsrReleaseTime * t) / totalTime;
            envelope.push_back({0.0f, level, time});
        }

        bufferHasData = true;
        recordedDuration = totalTime;
    }

    static int wrapIndex(int current, int delta, int maxCount) {
        if (maxCount <= 0)
            return 0;
        int next = current + delta;
        next %= maxCount;
        if (next < 0) {
            next += maxCount;
        }
        return next;
    }

    void setCurrentEnvelopeIndex(int index) {
        if (NUM_ENVELOPES <= 0) {
            currentEnvelopeIndex = 0;
            return;
        }
        int normalized = index % NUM_ENVELOPES;
        if (normalized < 0) {
            normalized += NUM_ENVELOPES;
        }
        currentEnvelopeIndex = normalized;
    }

    void setCurrentParameterIndex(int index) {
        if (NUM_EDIT_PARAMS <= 0) {
            currentParameterIndex = 0;
            return;
        }
        int normalized = index % NUM_EDIT_PARAMS;
        if (normalized < 0) {
            normalized += NUM_EDIT_PARAMS;
        }
        currentParameterIndex = normalized;
    }

    void advanceEnvelopeSelection(int delta = 1) {
        currentEnvelopeIndex = wrapIndex(currentEnvelopeIndex, delta, NUM_ENVELOPES);
        onEnvelopeSelectionChanged();
    }

    void advanceParameterSelection(int delta = 1) {
        currentParameterIndex = wrapIndex(currentParameterIndex, delta, NUM_EDIT_PARAMS);
    }

    void onEnvelopeSelectionChanged(bool flash = true) {
        currentEnvelopeIndex = clamp(currentEnvelopeIndex, 0, NUM_ENVELOPES - 1);

        if (mode == EnvelopeMode::ADSR) {
            // ADSR mode: sync knob to ADSR parameter values
            float normalized = 0.0f;
            switch (currentEnvelopeIndex) {
                case 0: normalized = (adsrAttackTime - 0.01f) / 4.99f; break;
                case 1: normalized = (adsrDecayTime - 0.01f) / 4.99f; break;
                case 2: normalized = adsrSustainLevel; break;
                case 3: normalized = (adsrReleaseTime - 0.01f) / 4.99f; break;
            }
            normalized = clamp(normalized, 0.0f, 1.0f);
            float knobValue = normalized * 16.0f;
            params[ENV_SPEED_PARAM].setValue(knobValue);
            envSpeedControlCache = knobValue;

            // Sync phase knob to ADSR contour
            float contour = 0.0f;
            switch (currentEnvelopeIndex) {
                case 0: contour = adsrAttackContour; break;
                case 1: contour = adsrDecayContour; break;
                case 2: contour = adsrSustainContour; break;
                case 3: contour = adsrReleaseContour; break;
            }
            params[ENV_PHASE_PARAM].setValue(contour);
            envPhaseControlCache = contour;
        } else {
            // Gesture mode: sync from SPEED_1-4 params
            float speed = params[SPEED_1_PARAM + currentEnvelopeIndex].getValue();
            params[ENV_SPEED_PARAM].setValue(speed);
            envSpeedControlCache = speed;
            float phase = phaseOffsets[currentEnvelopeIndex];
            params[ENV_PHASE_PARAM].setValue(phase);
            envPhaseControlCache = phase;
        }

        // Update loop and invert switch states to reflect current envelope
        params[LOOP_1_PARAM].setValue(loopStates[currentEnvelopeIndex] ? 1.0f : 0.0f);
        params[INVERT_1_PARAM].setValue(invertStates[currentEnvelopeIndex] ? 1.0f : 0.0f);

        if (flash)
            selectionFlashTimer = 0.75f;
    }

    void switchToGestureMode() {
        if (mode == EnvelopeMode::GESTURE)
            return;
        if (isRecording) {
            stopRecording();
        }
        adsrSurfaceGate = false;
        mode = EnvelopeMode::GESTURE;
        for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
            adsrGateHeld[c] = false;
            previousGateHigh[c] = false;
        }
        resetADSREngine();
        if (gestureBufferHasDataBackup && !gestureEnvelopeBackup.empty()) {
            envelope = gestureEnvelopeBackup;
            recordedDuration = gestureDurationBackup;
            bufferHasData = true;
        } else {
            bufferHasData = false;
            envelope.clear();
            recordedDuration = 2.0f;
        }
        onEnvelopeSelectionChanged(false);
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
                playback[i].smoothedVoltage[c] = 0.0f;
                playback[i].releaseActive[c] = false;
                playback[i].releaseValue[c] = 0.0f;
            }
        }
    }

    void switchToADSRMode() {
        if (mode == EnvelopeMode::ADSR)
            return;
        if (isRecording) {
            stopRecording();
        }
        gestureEnvelopeBackup = envelope;
        gestureDurationBackup = recordedDuration;
        gestureBufferHasDataBackup = bufferHasData;
        adsrSurfaceGate = false;
        mode = EnvelopeMode::ADSR;
        for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
            adsrGateHeld[c] = false;
            previousGateHigh[c] = false;
        }
        generateADSREnvelope();
        onEnvelopeSelectionChanged(false);
        resetADSREngine();
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
                playback[i].smoothedVoltage[c] = 0.0f;
                playback[i].releaseActive[c] = false;
                playback[i].releaseValue[c] = 0.0f;
            }
        }
    }

    void regenerateADSR() {
        if (mode == EnvelopeMode::ADSR) {
            generateADSREnvelope();
        }
    }

    void setADSRTouchActive(bool active) {
        if (mode != EnvelopeMode::ADSR) {
            adsrSurfaceGate = false;
            return;
        }
        adsrSurfaceGate = active;
    }

    void selectEnvelope(int index, bool flash = true) {
        setCurrentEnvelopeIndex(index);
        onEnvelopeSelectionChanged(flash);
    }

    bool isSelectionFlashActive() const {
        return selectionFlashTimer > 0.f;
    }

    void updateLastTouched(const std::string& paramName, const std::string& paramValue) {
        lastTouched.name = paramName;
        lastTouched.value = paramValue;
        lastTouched.timer = 0.35f; // Display for 0.35 seconds
        lastTouched.hasParam = true;
        selectionFlashTimer = 0.f; // Clear envelope selection flash immediately
    }

    EditableParam getCurrentEditableParam() const {
        int clamped = clamp(currentParameterIndex, 0, NUM_EDIT_PARAMS - 1);
        return static_cast<EditableParam>(clamped);
    }

    static const char* editableParamLabel(EditableParam param) {
        switch (param) {
            case EditableParam::Speed:
                return "Speed";
            case EditableParam::Loop:
                return "Loop";
            case EditableParam::Invert:
                return "Invert";
            case EditableParam::Phase:
                return "Phase";
            default:
                return "";
        }
    }

    const char* getCurrentEditableParamLabel() const {
        return editableParamLabel(getCurrentEditableParam());
    }

    char getCurrentEnvelopeCode() const {
        return static_cast<char>('1' + clamp(currentEnvelopeIndex, 0, NUM_ENVELOPES - 1));
    }

    char getCurrentParameterCode() const {
        switch (getCurrentEditableParam()) {
            case EditableParam::Speed:
                return 'S';
            case EditableParam::Loop:
                return 'L';
            case EditableParam::Invert:
                return 'I';
            case EditableParam::Phase:
                return 'P';
            default:
                return '-';
        }
    }

    int getCurrentParameterOrdinal() const {
        return clamp(currentParameterIndex, 0, NUM_EDIT_PARAMS - 1) + 1;
    }

    int getCurrentEnvelopeIndex() const {
        return clamp(currentEnvelopeIndex, 0, NUM_ENVELOPES - 1);
    }

    int getCurrentParameterIndex() const {
        return clamp(currentParameterIndex, 0, NUM_EDIT_PARAMS - 1);
    }
    
    bool isAnyPlaybackActive() {
        if (mode == EnvelopeMode::ADSR) {
            for (int v = 0; v < MAX_POLY_CHANNELS; ++v) {
                if (adsrGateSignals[v] || adsrValues[v] > 1e-3f)
                    return true;
            }
            return false;
        }
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
                if (playback[i].active[c]) return true;
            }
        }
        return false;
    }

    void stopAllPlayback() {
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
                playback[i].active[c] = false;
                playback[i].phase[c] = 0.0f;
                playback[i].eocPulse[c] = dsp::PulseGenerator();
                playback[i].smoothedVoltage[c] = 0.0f;
                playback[i].releaseActive[c] = false;
                playback[i].releaseValue[c] = 0.0f;
            }
        }
        for (int c = 0; c < MAX_POLY_CHANNELS; c++) {
            adsrGateHeld[c] = false;
            previousGateHigh[c] = false;
        }
        adsrSurfaceGate = false;
        if (touchStripWidget) {
            touchStripWidget->clearPulses();
        }
        nextVoiceIndex = 0;
        resetADSREngine();
    }
    
    // Save/Load state
    json_t* dataToJson() override {
        json_t* rootJ = json_object();

        json_object_set_new(rootJ, "bufferHasData", json_boolean(bufferHasData));
        json_object_set_new(rootJ, "mode", json_integer((int)mode));

        // Save ADSR parameters
        json_object_set_new(rootJ, "adsrAttackTime", json_real(adsrAttackTime));
        json_object_set_new(rootJ, "adsrDecayTime", json_real(adsrDecayTime));
        json_object_set_new(rootJ, "adsrSustainLevel", json_real(adsrSustainLevel));
        json_object_set_new(rootJ, "adsrReleaseTime", json_real(adsrReleaseTime));
        json_object_set_new(rootJ, "adsrAttackContour", json_real(adsrAttackContour));
        json_object_set_new(rootJ, "adsrDecayContour", json_real(adsrDecayContour));
        json_object_set_new(rootJ, "adsrSustainContour", json_real(adsrSustainContour));
        json_object_set_new(rootJ, "adsrReleaseContour", json_real(adsrReleaseContour));

        // Save individual loop states
        json_t* loopStatesJ = json_array();
        for (int i = 0; i < 4; i++) {
            json_array_append_new(loopStatesJ, json_boolean(loopStates[i]));
        }
        json_object_set_new(rootJ, "loopStates", loopStatesJ);

        // Save invert states
        json_t* invertStatesJ = json_array();
        for (int i = 0; i < 4; i++) {
            json_array_append_new(invertStatesJ, json_boolean(invertStates[i]));
        }
        json_object_set_new(rootJ, "invertStates", invertStatesJ);

        // Save envelope data
        if (bufferHasData && !envelope.empty()) {
            json_t* envelopeJ = json_array();
            for (const auto& point : envelope) {
                json_t* pointJ = json_object();
                json_object_set_new(pointJ, "x", json_real(point.x));
                json_object_set_new(pointJ, "y", json_real(point.y));
                json_object_set_new(pointJ, "time", json_real(point.time));
                json_array_append_new(envelopeJ, pointJ);
            }
        json_object_set_new(rootJ, "envelope", envelopeJ);
        }

        json_t* phaseOffsetsJ = json_array();
        for (int i = 0; i < 4; i++) {
            json_array_append_new(phaseOffsetsJ, json_real(phaseOffsets[i]));
        }
        json_object_set_new(rootJ, "phaseOffsets", phaseOffsetsJ);

        json_object_set_new(rootJ, "debugTouchLogging", json_boolean(debugTouchLogging));
        json_object_set_new(rootJ, "recordedDuration", json_real(recordedDuration));
        json_object_set_new(rootJ, "currentEnvelopeIndex", json_integer(currentEnvelopeIndex));
        json_object_set_new(rootJ, "currentParameterIndex", json_integer(currentParameterIndex));
        json_object_set_new(rootJ, "gestureBufferHasDataBackup", json_boolean(gestureBufferHasDataBackup));
        json_object_set_new(rootJ, "gestureDurationBackup", json_real(gestureDurationBackup));
        if (gestureBufferHasDataBackup && !gestureEnvelopeBackup.empty()) {
            json_t* gestureBackupJ = json_array();
            for (const auto& point : gestureEnvelopeBackup) {
                json_t* pointJ = json_object();
                json_object_set_new(pointJ, "x", json_real(point.x));
                json_object_set_new(pointJ, "y", json_real(point.y));
                json_object_set_new(pointJ, "time", json_real(point.time));
                json_array_append_new(gestureBackupJ, pointJ);
            }
            json_object_set_new(rootJ, "gestureEnvelopeBackup", gestureBackupJ);
        }

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* bufferHasDataJ = json_object_get(rootJ, "bufferHasData");
        if (bufferHasDataJ) bufferHasData = json_boolean_value(bufferHasDataJ);

        json_t* modeJ = json_object_get(rootJ, "mode");
        if (modeJ) mode = (EnvelopeMode)json_integer_value(modeJ);

        // Load ADSR parameters
        json_t* adsrAttackTimeJ = json_object_get(rootJ, "adsrAttackTime");
        if (adsrAttackTimeJ) adsrAttackTime = json_real_value(adsrAttackTimeJ);

        json_t* adsrDecayTimeJ = json_object_get(rootJ, "adsrDecayTime");
        if (adsrDecayTimeJ) adsrDecayTime = json_real_value(adsrDecayTimeJ);

        json_t* adsrSustainLevelJ = json_object_get(rootJ, "adsrSustainLevel");
        if (adsrSustainLevelJ) adsrSustainLevel = json_real_value(adsrSustainLevelJ);

        json_t* adsrReleaseTimeJ = json_object_get(rootJ, "adsrReleaseTime");
        if (adsrReleaseTimeJ) adsrReleaseTime = json_real_value(adsrReleaseTimeJ);

        adsrAttackTime = clamp(adsrAttackTime, 0.01f, 5.0f);
        adsrDecayTime = clamp(adsrDecayTime, 0.01f, 5.0f);
        adsrReleaseTime = clamp(adsrReleaseTime, 0.01f, 5.0f);
        adsrSustainLevel = clamp(adsrSustainLevel, 0.0f, 1.0f);

        json_t* adsrAttackContourJ = json_object_get(rootJ, "adsrAttackContour");
        if (adsrAttackContourJ) adsrAttackContour = json_real_value(adsrAttackContourJ);

        json_t* adsrDecayContourJ = json_object_get(rootJ, "adsrDecayContour");
        if (adsrDecayContourJ) adsrDecayContour = json_real_value(adsrDecayContourJ);

        json_t* adsrSustainContourJ = json_object_get(rootJ, "adsrSustainContour");
        if (adsrSustainContourJ) adsrSustainContour = json_real_value(adsrSustainContourJ);

        json_t* adsrReleaseContourJ = json_object_get(rootJ, "adsrReleaseContour");
        if (adsrReleaseContourJ) adsrReleaseContour = json_real_value(adsrReleaseContourJ);

        adsrAttackContour = clamp(adsrAttackContour, 0.0f, 1.0f);
        adsrDecayContour = clamp(adsrDecayContour, 0.0f, 1.0f);
        adsrSustainContour = clamp(adsrSustainContour, 0.0f, 1.0f);
        adsrReleaseContour = clamp(adsrReleaseContour, 0.0f, 1.0f);

        // Load individual loop states
        json_t* loopStatesJ = json_object_get(rootJ, "loopStates");
        if (loopStatesJ) {
            size_t n = json_array_size(loopStatesJ);
            n = std::min(n, (size_t)4);
            for (size_t i = 0; i < n; i++) {
                json_t* loopJ = json_array_get(loopStatesJ, i);
                if (loopJ) loopStates[(int)i] = json_boolean_value(loopJ);
            }
        }

        // Load invert states
        json_t* invertStatesJ = json_object_get(rootJ, "invertStates");
        if (invertStatesJ) {
            size_t n = json_array_size(invertStatesJ);
            n = std::min(n, (size_t)4);
            for (size_t i = 0; i < n; i++) {
                json_t* invertJ = json_array_get(invertStatesJ, i);
                if (invertJ) invertStates[(int)i] = json_boolean_value(invertJ);
            }
        }
        json_t* phaseOffsetsJ = json_object_get(rootJ, "phaseOffsets");
        if (phaseOffsetsJ) {
            size_t n = json_array_size(phaseOffsetsJ);
            n = std::min(n, (size_t)4);
            for (size_t i = 0; i < n; i++) {
                json_t* phaseJ = json_array_get(phaseOffsetsJ, i);
                if (phaseJ) phaseOffsets[(int)i] = clamp((float)json_real_value(phaseJ), 0.f, 1.f);
            }
        }
        
        // Load envelope data
        json_t* envelopeJ = json_object_get(rootJ, "envelope");
        if (envelopeJ) {
            envelope.clear();
            size_t i;
            json_t* pointJ;
            json_array_foreach(envelopeJ, i, pointJ) {
                EnvelopePoint point;
                json_t* xJ = json_object_get(pointJ, "x");
                json_t* yJ = json_object_get(pointJ, "y");
                json_t* timeJ = json_object_get(pointJ, "time");
                
                if (xJ) point.x = json_real_value(xJ);
                if (yJ) point.y = json_real_value(yJ);
                if (timeJ) point.time = json_real_value(timeJ);
                
                envelope.push_back(point);
            }
        }

        json_t* gestureBufferHasDataBackupJ = json_object_get(rootJ, "gestureBufferHasDataBackup");
        if (gestureBufferHasDataBackupJ) gestureBufferHasDataBackup = json_boolean_value(gestureBufferHasDataBackupJ);

        json_t* gestureDurationBackupJ = json_object_get(rootJ, "gestureDurationBackup");
        if (gestureDurationBackupJ) gestureDurationBackup = json_real_value(gestureDurationBackupJ);

        json_t* gestureBackupJ = json_object_get(rootJ, "gestureEnvelopeBackup");
        bool gestureBackupLoaded = false;
        if (gestureBackupJ) {
            gestureEnvelopeBackup.clear();
            size_t i;
            json_t* pointJ;
            json_array_foreach(gestureBackupJ, i, pointJ) {
                EnvelopePoint point;
                json_t* xJ = json_object_get(pointJ, "x");
                json_t* yJ = json_object_get(pointJ, "y");
                json_t* timeJ = json_object_get(pointJ, "time");

                if (xJ) point.x = json_real_value(xJ);
                if (yJ) point.y = json_real_value(yJ);
                if (timeJ) point.time = json_real_value(timeJ);

                gestureEnvelopeBackup.push_back(point);
            }
            gestureBackupLoaded = true;
        }

        json_t* debugTouchJ = json_object_get(rootJ, "debugTouchLogging");
        if (debugTouchJ) {
            debugTouchLogging = json_boolean_value(debugTouchJ);
        }

        json_t* durationJ = json_object_get(rootJ, "recordedDuration");
        if (durationJ) {
            recordedDuration = clamp((float)json_real_value(durationJ), 1e-3f, maxRecordingTime);
        }

        if (!gestureBackupLoaded && mode == EnvelopeMode::GESTURE) {
            gestureEnvelopeBackup = envelope;
            gestureDurationBackup = recordedDuration;
            gestureBufferHasDataBackup = bufferHasData;
        }

        json_t* currentEnvelopeIndexJ = json_object_get(rootJ, "currentEnvelopeIndex");
        if (currentEnvelopeIndexJ) {
            setCurrentEnvelopeIndex((int)json_integer_value(currentEnvelopeIndexJ));
        }

        // Regenerate ADSR envelope if in ADSR mode
        if (mode == EnvelopeMode::ADSR) {
            generateADSREnvelope();
        }

        json_t* currentParameterIndexJ = json_object_get(rootJ, "currentParameterIndex");
        if (currentParameterIndexJ) {
            setCurrentParameterIndex((int)json_integer_value(currentParameterIndexJ));
        }

        onEnvelopeSelectionChanged(false);
    }

    void onReset() override {
        Module::onReset();
        for (int i = 0; i < 4; ++i) {
            loopStates[i] = false;
            invertStates[i] = false;
            phaseOffsets[i] = 0.f;
        }
        selectionFlashTimer = 0.f;
        onEnvelopeSelectionChanged(false);
    }

};

// ADSRStageButtonQuantity Implementation
std::string ADSRStageButtonQuantity::getLabel() {
    Evocation* evocation = dynamic_cast<Evocation*>(module);
    if (!evocation) return ParamQuantity::getLabel();

    const char* gestureLabels[] = {"Select Envelope 1", "Select Envelope 2", "Select Envelope 3", "Select Envelope 4"};
    const char* adsrLabels[] = {"Select Attack", "Select Decay", "Select Sustain", "Select Release"};

    if (evocation->mode == Evocation::EnvelopeMode::ADSR) {
        return adsrLabels[stageIndex];
    } else {
        return gestureLabels[stageIndex];
    }
}

// ADSRSpeedParamQuantity Implementation
std::string ADSRSpeedParamQuantity::getLabel() {
    Evocation* evocation = dynamic_cast<Evocation*>(module);
    if (!evocation) return ParamQuantity::getLabel();

    if (evocation->mode == Evocation::EnvelopeMode::ADSR) {
        // Return label based on current envelope selection
        int envIndex = evocation->getCurrentEnvelopeIndex();
        const char* adsrLabels[] = {"Attack Time", "Decay Time", "Sustain Level", "Release Time"};
        if (envIndex >= 0 && envIndex < 4) {
            return adsrLabels[envIndex];
        }
        return "ADSR Parameter";
    } else {
        return "Selected Envelope Speed";
    }
}

// TouchStripWidget Method Implementations (after Evocation is fully defined)
TouchStripWidget::TouchStripWidget(Evocation* module) {
    this->module = module;
    // Size will be set by the widget constructor based on SVG rect
    box.size = Vec(0, 0);
}

void TouchStripWidget::onButton(const event::Button& e) {
    if (!module) {
        Widget::onButton(e);
        return;
    }

    if (module->mode == Evocation::EnvelopeMode::ADSR) {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            isDragging = true;
            showTouch = true;
            currentTouchPos = clampToBounds(resolveMouseLocal(e.pos));
            glowIntensity = 1.0f;
            module->setADSRTouchActive(true);
            if (!module->bufferHasData) {
                module->generateADSREnvelope();
            }
            module->triggerAllEnvelopes();
            applyADSRTouch(true);
            e.consume(this);
        }
        Widget::onButton(e);
        return;
    }

    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
        // Gesture mode recording
        isDragging = true;
        showTouch = true;
        currentTouchPos = clampToBounds(resolveMouseLocal(e.pos));

        module->startRecording();

        lastSampleTime = -1.f;
        recordSample("press", true);

        glowIntensity = 1.0f;
        e.consume(this);
    }

    Widget::onButton(e);
}

void TouchStripWidget::onDragStart(const event::DragStart& e) {
    if (!module) return;
    isDragging = true;
    showTouch = true;
}

void TouchStripWidget::onDragMove(const event::DragMove& e) {
    if (!module || !isDragging) return;
    Vec fallbackPos = currentTouchPos.plus(e.mouseDelta);
    currentTouchPos = clampToBounds(resolveMouseLocal(fallbackPos));

    if (module->mode == Evocation::EnvelopeMode::ADSR) {
        applyADSRTouch(false);
        return;
    }

    recordSample("drag");
}

void TouchStripWidget::onDragEnd(const event::DragEnd& e) {
    if (!module) return;

    isDragging = false;
    showTouch = false;
    glowIntensity = 0.0f;

    // Clear light trail when done drawing
    lightPulses.clear();
    lastSampleTime = -1.f;

    if (module->mode == Evocation::EnvelopeMode::ADSR) {
        module->setADSRTouchActive(false);
        lastADSRSustainLevel = -1.f;
        lastADSRReleaseTime = -1.f;
        lastADSRReleaseContour = -1.f;
        return;
    }

    // Stop recording in module
    currentTouchPos = clampToBounds(resolveMouseLocal(currentTouchPos));
    recordSample("release", true);
    if (module->isRecording) {
        module->stopRecording();
    }

    if (module->debugTouchLogging) {
        INFO("TouchStripWidget::onDragEnd");
    }
}

float TouchStripWidget::computeNormalizedVoltage() const {
    if (box.size.y <= 0.f)
        return 0.f;
    const float height = box.size.y;
    const float deadZone = height * 0.08f; // 8% dead zone at bottom for clean 0V

    float y = clamp(currentTouchPos.y, 0.0f, height);

    // If in the bottom dead zone, return 0V
    if (y >= (height - deadZone)) {
        return 0.0f;
    }

    // Remap the active area (excluding dead zone) to 0-1
    float activeHeight = height - deadZone;
    float normalized = 1.0f - (y / activeHeight);
    return clamp(normalized, 0.0f, 1.0f);
}

float TouchStripWidget::computeNormalizedHorizontal() const {
    if (box.size.x <= 0.f)
        return 0.f;
    return clamp(currentTouchPos.x / box.size.x, 0.0f, 1.0f);
}

void TouchStripWidget::recordSample(const char* stage, bool force) {
    if (!module || !module->isRecording)
        return;

    float currentTime = module->recordingTime;
    if (!force && lastSampleTime >= 0.f && (currentTime - lastSampleTime) < MIN_SAMPLE_INTERVAL)
        return;

    float normalizedVoltage = computeNormalizedVoltage();
    module->addEnvelopeSample(normalizedVoltage);
    lastSampleTime = currentTime;

    float effectiveTime = currentTime;
    if (module->firstSampleTime >= 0.0f) {
        effectiveTime = std::max(currentTime - module->firstSampleTime, 0.0f);
    }

    float normalizedTime = module->maxRecordingTime <= 0.f ? 0.f : clamp(effectiveTime / module->maxRecordingTime, 0.f, 1.f);
    logTouchDebug(stage, currentTouchPos, normalizedTime, normalizedVoltage);

    createPulse(currentTouchPos);
}

void TouchStripWidget::createPulse(Vec pos) {
    LightPulse pulse;
    pulse.pos = pos;
    pulse.intensity = 1.0f;
    lightPulses.push_back(pulse);

    // Keep a manageable trail length so blending stays efficient
    if (lightPulses.size() > 60) {
        lightPulses.erase(lightPulses.begin());
    }
}

void TouchStripWidget::clearPulses() {
    lightPulses.clear();
}

Vec TouchStripWidget::clampToBounds(Vec pos) const {
    pos.x = clamp(pos.x, 0.0f, box.size.x);
    pos.y = clamp(pos.y, 0.0f, box.size.y);
    return pos;
}

Vec TouchStripWidget::resolveMouseLocal(const Vec& fallback) {
    if (!(APP && APP->scene))
        return fallback;

    Vec scenePos = APP->scene->getMousePos();
    Vec widgetOrigin = getAbsoluteOffset(Vec());
    float zoom = getAbsoluteZoom();
    if (zoom <= 0.f) {
        zoom = 1.f;
    }

    Vec local = scenePos.minus(widgetOrigin).div(zoom);
    if (!local.isFinite())
        return fallback;

    return local;
}

void TouchStripWidget::resetForNewRecording() {
    clearPulses();
    isDragging = false;
    showTouch = false;
    glowIntensity = 0.0f;
    lastSampleTime = -1.f;
    lastADSRSustainLevel = -1.f;
    lastADSRReleaseTime = -1.f;
    lastADSRReleaseContour = -1.f;
}

void TouchStripWidget::logTouchDebug(const char* stage, const Vec& localPos, float normalizedTime, float normalizedVoltage) {
    if (!module || !module->debugTouchLogging)
        return;

    math::Vec scenePos;
    if (APP && APP->scene) {
        scenePos = APP->scene->getMousePos();
    }
    Vec widgetOrigin = getAbsoluteOffset(Vec());
    float zoom = getAbsoluteZoom();
    INFO("EvocationTouch[%s] scene=(%.2f, %.2f) origin=(%.2f, %.2f) zoom=%.3f local=(%.2f, %.2f) size=(%.2f, %.2f) norm=(t=%.3f, v=%.3f)",
         stage,
         scenePos.x, scenePos.y,
         widgetOrigin.x, widgetOrigin.y,
         zoom,
         localPos.x, localPos.y,
         box.size.x, box.size.y,
         normalizedTime, normalizedVoltage);
}

void TouchStripWidget::applyADSRTouch(bool initial) {
    if (!module || module->mode != Evocation::EnvelopeMode::ADSR)
        return;

    float sustainLevel = computeNormalizedVoltage();
    float releaseMix = computeNormalizedHorizontal();
    float releaseTime = 0.01f + releaseMix * 4.99f;
    float releaseContour = clamp(sustainLevel, 0.0f, 1.0f);

    bool changed = false;

    if (std::fabs(sustainLevel - module->adsrSustainLevel) > 1e-3f) {
        module->adsrSustainLevel = sustainLevel;
        changed = true;
    }
    if (std::fabs(releaseTime - module->adsrReleaseTime) > 1e-3f) {
        module->adsrReleaseTime = releaseTime;
        changed = true;
    }
    if (std::fabs(releaseContour - module->adsrReleaseContour) > 1e-3f) {
        module->adsrReleaseContour = releaseContour;
        changed = true;
    }

    if (!changed && !initial)
        return;

    int currentStage = module->getCurrentEnvelopeIndex();
    if (currentStage == 2) {
        float knobValue = clamp(sustainLevel * 16.0f, 0.0f, 16.0f);
        module->envSpeedControlCache = knobValue;
        module->params[Evocation::ENV_SPEED_PARAM].setValue(knobValue);
    } else if (currentStage == 3) {
        float normalized = clamp((module->adsrReleaseTime - 0.01f) / 4.99f, 0.0f, 1.0f);
        float knobValue = normalized * 16.0f;
        module->envSpeedControlCache = knobValue;
        module->params[Evocation::ENV_SPEED_PARAM].setValue(knobValue);
        module->envPhaseControlCache = module->adsrReleaseContour;
        module->params[Evocation::ENV_PHASE_PARAM].setValue(module->adsrReleaseContour);
    }

    module->generateADSREnvelope();

    lastADSRSustainLevel = sustainLevel;
    lastADSRReleaseTime = releaseTime;
    lastADSRReleaseContour = releaseContour;
}

void TouchStripWidget::step() {
    Widget::step();

    if (module) {
        if (module->isRecording) {
            if (isDragging) {
                currentTouchPos = clampToBounds(resolveMouseLocal(currentTouchPos));
                recordSample("frame");
            }
        } else {
            if (module->mode == Evocation::EnvelopeMode::GESTURE && isDragging) {
                isDragging = false;
                showTouch = false;
                glowIntensity = 0.0f;
            }
            lastSampleTime = -1.f;
        }
    }

    float sampleTime = (APP && APP->engine) ? APP->engine->getSampleTime() : 1.f / 60.f;
    const float decayPerSecond = 1.8f;

    for (auto& pulse : lightPulses) {
        pulse.intensity -= decayPerSecond * sampleTime;
        pulse.intensity = fmaxf(pulse.intensity, 0.0f);
    }

    lightPulses.erase(std::remove_if(lightPulses.begin(), lightPulses.end(), [](const LightPulse& pulse) {
        return pulse.intensity <= 0.01f;
    }), lightPulses.end());

    // Fade glow
    if (glowIntensity > 0.0f && !isDragging) {
        glowIntensity -= APP->engine->getSampleTime() * 2.0f; // Fade over 0.5 seconds
        glowIntensity = fmaxf(0.0f, glowIntensity);
    }
}

void TouchStripWidget::drawLayer(const DrawArgs& args, int layer) {
    if (layer == 1) {
        drawTouchStrip(args);
    }
    Widget::drawLayer(args, layer);
}

void TouchStripWidget::drawTouchStrip(const DrawArgs& args) {
    nvgSave(args.vg);

    // Clip to widget bounds
    nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

    // Draw background gradient
    drawBackground(args);

    // Draw current touch position
    if (showTouch && isDragging) {
        drawCurrentTouch(args);
    }
    
    // Draw LED pulses following the gesture trail only during capture
    if (module && module->isRecording) {
        drawPulses(args);
    }
    
    // Draw border and recording glow
    drawBorder(args);
    
    // Instructions removed per user request
    
    nvgRestore(args.vg);
}

void TouchStripWidget::drawBackground(const DrawArgs& args) {
    nvgBeginPath(args.vg);
    const float borderRadius = 8.0f;
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, borderRadius);

    // Base brass gradient
    NVGpaint base = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y,
        nvgRGBA(118, 92, 52, 255),
        nvgRGBA(46, 30, 16, 255));
    nvgFillPaint(args.vg, base);
    nvgFill(args.vg);

    // Subtle center glow to simulate polished metal
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2, borderRadius - 1);
    NVGpaint centerGlow = nvgLinearGradient(args.vg,
        0,
        box.size.y * 0.2f,
        0,
        box.size.y * 0.8f,
        nvgRGBA(220, 190, 110, 90),
        nvgRGBA(90, 60, 28, 0));
    nvgFillPaint(args.vg, centerGlow);
    nvgFill(args.vg);

    // Edge sheen so the strip feels inset
    NVGpaint edgeSheen = nvgBoxGradient(args.vg,
        -4.0f,
        -2.0f,
        box.size.x + 8.0f,
        box.size.y + 4.0f,
        10.0f,
        14.0f,
        nvgRGBA(255, 215, 130, 32),
        nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, -2, -2, box.size.x + 4, box.size.y + 4, borderRadius + 2);
    nvgFillPaint(args.vg, edgeSheen);
    nvgFill(args.vg);

    // Brushed metal texture (horizontal strokes)
    nvgSave(args.vg);
    nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeWidth(args.vg, 0.8f);
    nvgStrokeColor(args.vg, nvgRGBA(255, 230, 180, 18));
    const int horizontalStrokes = 22;
    for (int i = 1; i < horizontalStrokes; i++) {
        float y = (box.size.y / horizontalStrokes) * i;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, y);
        nvgLineTo(args.vg, box.size.x, y);
        nvgStroke(args.vg);
    }

    // Subtle segmentation to reference LED ladders
    nvgStrokeWidth(args.vg, 1.2f);
    nvgStrokeColor(args.vg, nvgRGBA(255, 207, 130, 35));
    const int segments = 5;
    for (int i = 1; i < segments; i++) {
        float x = (box.size.x / segments) * i;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x, 4.0f);
        nvgLineTo(args.vg, x, box.size.y - 4.0f);
        nvgStroke(args.vg);
    }
    nvgRestore(args.vg);
}

void TouchStripWidget::drawEnvelope(const DrawArgs& args) {
    if (!module || module->envelope.empty()) return;

    if (!module->isRecording && module->hasRecordedEnvelope()) {
        drawEnvelopeVoltageTime(args);
    } else {
        drawEnvelopeStandard(args);
    }
}

void TouchStripWidget::drawEnvelopeStandard(const DrawArgs& args) {
    nvgStrokeColor(args.vg, nvgRGBA(255, 222, 150, 180));
    nvgStrokeWidth(args.vg, 2.2f);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);

    // Draw glow effect
    nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
    nvgStrokeWidth(args.vg, 4.0f);
    nvgStrokeColor(args.vg, nvgRGBA(255, 210, 110, 60));

    nvgBeginPath(args.vg);
    bool first = true;
    for (const auto& point : module->envelope) {
        float x = point.time * box.size.x;
        float y = (1.f - point.y) * box.size.y;

        if (first) {
            nvgMoveTo(args.vg, x, y);
            first = false;
        } else {
            nvgLineTo(args.vg, x, y);
        }
    }
    nvgStroke(args.vg);

    // Draw main line
    nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
    nvgStrokeWidth(args.vg, 1.8f);
    nvgStrokeColor(args.vg, nvgRGBA(255, 238, 200, 220));

    nvgBeginPath(args.vg);
    first = true;
    for (const auto& point : module->envelope) {
        float x = point.time * box.size.x;
        float y = (1.f - point.y) * box.size.y;

        if (first) {
            nvgMoveTo(args.vg, x, y);
            first = false;
        } else {
            nvgLineTo(args.vg, x, y);
        }
    }
    nvgStroke(args.vg);

    // Draw envelope points as dots
    nvgFillColor(args.vg, nvgRGBA(255, 244, 210, 200));
    for (const auto& point : module->envelope) {
        float x = point.time * box.size.x;
        float y = (1.f - point.y) * box.size.y;

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, x, y, 1.8f);
        nvgFill(args.vg);
    }

}

void TouchStripWidget::drawEnvelopeVoltageTime(const DrawArgs& args) {
    constexpr int samples = 256;
    float width = box.size.x;
    float height = box.size.y;
    float duration = module->getRecordedDuration();

    // Background grid for time vs voltage reference
    nvgSave(args.vg);
    nvgStrokeWidth(args.vg, 1.0f);
    nvgStrokeColor(args.vg, nvgRGBA(180, 140, 90, 40));
    const int timeDivisions = 6;
    for (int i = 1; i < timeDivisions; ++i) {
        float y = (height / timeDivisions) * i;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.0f, y);
        nvgLineTo(args.vg, width, y);
        nvgStroke(args.vg);
    }
    const int voltageDivisions = 5;
    for (int i = 1; i < voltageDivisions; ++i) {
        float x = (width / voltageDivisions) * i;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x, 0.0f);
        nvgLineTo(args.vg, x, height);
        nvgStroke(args.vg);
    }
    nvgRestore(args.vg);

    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);

    auto drawSampledPath = [&](float strokeWidth, NVGcolor color) {
        nvgGlobalCompositeOperation(args.vg, strokeWidth > 3.5f ? NVG_LIGHTER : NVG_SOURCE_OVER);
        nvgStrokeWidth(args.vg, strokeWidth);
        nvgStrokeColor(args.vg, color);

        nvgBeginPath(args.vg);
        for (int i = 0; i < samples; ++i) {
            float phase = (float)i / (float)(samples - 1);
            float value = clamp(module->interpolateEnvelope(phase), 0.0f, 1.0f);
            float x = value * width;
            float y = phase * height;
            if (i == 0) {
                nvgMoveTo(args.vg, x, y);
            } else {
                nvgLineTo(args.vg, x, y);
            }
        }
        nvgStroke(args.vg);
    };

    drawSampledPath(4.0f, nvgRGBA(255, 200, 110, 60));
    drawSampledPath(2.0f, nvgRGBA(255, 238, 200, 220));

    // Draw original samples for reference
    nvgFillColor(args.vg, nvgRGBA(255, 244, 210, 170));
    for (const auto& point : module->envelope) {
        float x = clamp(point.y, 0.0f, 1.0f) * width;
        float y = clamp(point.time, 0.0f, 1.0f) * height;
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, x, y, 1.5f);
        nvgFill(args.vg);
    }

    // Axis labels (simple markers)
    nvgFontSize(args.vg, 11.0f);
    nvgFontFaceId(args.vg, APP->window->uiFont->handle);
    nvgFillColor(args.vg, nvgRGBA(230, 210, 170, 180));
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    char timeLabel[32];
    snprintf(timeLabel, sizeof(timeLabel), "%.2fs", duration);
    nvgText(args.vg, 4.0f, 4.0f, "0V", nullptr);
    nvgText(args.vg, 4.0f, 18.0f, "0s", nullptr);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    nvgText(args.vg, width - 4.0f, 4.0f, "10V", nullptr);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
    nvgText(args.vg, 4.0f, height - 4.0f, timeLabel, nullptr);

}

void TouchStripWidget::drawCurrentTouch(const DrawArgs& args) {
    // Very subtle amber glow anchored to the finger position
    NVGpaint aura = nvgRadialGradient(
        args.vg,
        currentTouchPos.x,
        currentTouchPos.y,
        0.0f,
        3.2f,
        nvgRGBA(255, 196, 106, 40),
        nvgRGBA(120, 78, 30, 0)
    );
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, currentTouchPos.x, currentTouchPos.y, 2.6f);
    nvgFillPaint(args.vg, aura);
    nvgFill(args.vg);

    // Core highlight
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, currentTouchPos.x, currentTouchPos.y, 1.0f);
    nvgFillColor(args.vg, nvgRGBA(255, 230, 180, 110));
    nvgFill(args.vg);
}

void TouchStripWidget::drawPulses(const DrawArgs& args) {
    for (const auto& pulse : lightPulses) {
        if (pulse.intensity <= 0.0f)
            continue;

        float normalized = clamp(pulse.intensity, 0.0f, 1.0f);

        // Establish an elongated footprint so the light feels embedded under glass
        float baseWidth = box.size.x * 0.12f;
        float baseHeight = box.size.y * 0.08f;
        float width = clamp(baseWidth + normalized * baseWidth * 0.5f, 10.0f, box.size.x * 0.28f);
        float height = clamp(baseHeight + normalized * baseHeight * 0.45f, 6.0f, box.size.y * 0.20f);

        float ledX = clamp(pulse.pos.x, width * 0.5f, box.size.x - width * 0.5f);
        float ledY = clamp(pulse.pos.y, height * 0.5f, box.size.y - height * 0.5f);

        NVGcolor inner = nvgRGBA(255, 210, 128, (int)(110 * normalized));
        NVGcolor outer = nvgRGBA(110, 70, 30, 0);

        NVGpaint ledPaint = nvgBoxGradient(
            args.vg,
            ledX - width * 0.5f,
            ledY - height * 0.5f,
            width,
            height,
            height * 0.45f,
            height,
            inner,
            outer
        );

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ledX - width * 0.5f, ledY - height * 0.5f, width, height, height * 0.45f);
        nvgFillPaint(args.vg, ledPaint);
        nvgFill(args.vg);

        // Brighter core highlight to sell the LED diffusion
        float highlightWidth = width * 0.5f;
        float highlightHeight = height * 0.32f;
        NVGpaint highlight = nvgLinearGradient(
            args.vg,
            ledX,
            ledY - highlightHeight * 0.5f,
            ledX,
            ledY + highlightHeight * 0.5f,
            nvgRGBA(255, 230, 188, (int)(110 * normalized)),
            nvgRGBA(255, 190, 100, (int)(45 * normalized))
        );

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ledX - highlightWidth * 0.5f, ledY - highlightHeight * 0.5f, highlightWidth, highlightHeight, highlightHeight * 0.4f);
        nvgFillPaint(args.vg, highlight);
        nvgFill(args.vg);
    }
}

void TouchStripWidget::drawBorder(const DrawArgs& args) {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2, 8);
    
    if (module && module->isRecording) {
        // Recording glow
        nvgStrokeColor(args.vg, nvgRGBA(255, 214, 138, 255));
        nvgStrokeWidth(args.vg, 3.0f);
        
        // Animated glow - using system time
        float glow = 0.5f + 0.5f * sin(system::getTime() * 6);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgStrokeColor(args.vg, nvgRGBA(255, 196, 110, glow * 120));
        nvgStrokeWidth(args.vg, 8.0f);
        nvgStroke(args.vg);
        
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        nvgStrokeColor(args.vg, nvgRGBA(255, 224, 170, 255));
        nvgStrokeWidth(args.vg, 2.0f);
    } else {
        // Normal border
        nvgStrokeColor(args.vg, nvgRGBA(78, 52, 26, 160));
        nvgStrokeWidth(args.vg, 2.0f);
    }
    nvgStroke(args.vg);
}

void TouchStripWidget::drawInstructions(const DrawArgs& args) {
    nvgFontSize(args.vg, 11);
    nvgFontFaceId(args.vg, APP->window->uiFont->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGBA(150, 150, 150, 200));
    
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.4f, "Click and drag", NULL);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, "to cast spell", NULL);
    
    nvgFontSize(args.vg, 9);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.7f, "Tap strip to record", NULL);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.8f, "Drag to sculpt envelope", NULL);
}

struct OutputProgressIndicator : Widget {
    Evocation* module = nullptr;
    int outputIndex = 0;

    OutputProgressIndicator(Evocation* module, int outputIndex) {
        this->module = module;
        this->outputIndex = outputIndex;
    }

    void draw(const DrawArgs& args) override {
        if (!module)
            return;

        bool hasEnvelope = module->hasRecordedEnvelope();
        bool active = hasEnvelope && module->isPlaybackActive(outputIndex);
        float phase = hasEnvelope ? clamp(module->getPlaybackPhase(outputIndex), 0.0f, 1.0f) : 0.0f;

        NVGcontext* vg = args.vg;
        Vec center = box.size.div(2.0f);
        float maxDiameter = std::min(box.size.x, box.size.y);
        float radius = maxDiameter * 0.5f - 4.0f; // keep everything inside the bezel "screen"
        if (radius <= 0.f)
            return;

        // Base bezel / screen border
        NVGcolor bezelColor = hasEnvelope ? nvgRGBA(120, 110, 100, 160) : nvgRGBA(70, 60, 50, 140);
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius + 3.0f);
        nvgStrokeWidth(vg, 1.2f);
        nvgStrokeColor(vg, bezelColor);
        nvgStroke(vg);

        // Dark faceplate area where the light lives
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius + 2.0f);
        nvgFillColor(vg, nvgRGBA(8, 8, 12, 235));
        nvgFill(vg);

        // Inner screen glow (subtle) so the port feels inset
        NVGpaint screenGlow = nvgRadialGradient(
            vg,
            center.x,
            center.y,
            radius * 0.1f,
            radius + 2.0f,
            nvgRGBA(40, 30, 45, 120),
            nvgRGBA(5, 5, 10, 0)
        );
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius + 2.0f);
        nvgFillPaint(vg, screenGlow);
        nvgFill(vg);

        if (!hasEnvelope)
            return;
        if (!active)
            return;

        float angleStart = -M_PI / 2.0f;
        float angleEnd = angleStart + phase * 2.0f * (float)M_PI;

        float arcRadius = radius;

        // Progress arc
        nvgBeginPath(vg);
        nvgArc(vg, center.x, center.y, arcRadius, angleStart, angleEnd, NVG_CW);
        nvgStrokeWidth(vg, 3.0f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStrokeColor(vg, nvgRGBA(255, 214, 130, 200));
        nvgStroke(vg);

        // Leading indicator
        Vec tip = center.plus(Vec(cosf(angleEnd), sinf(angleEnd)).mult(arcRadius));
        nvgBeginPath(vg);
        nvgCircle(vg, tip.x, tip.y, 4.0f);
        nvgFillColor(vg, nvgRGBA(255, 244, 200, 220));
        nvgFill(vg);
    }
};

// Main Widget
// OLED Display Widget
struct EvocationOLEDDisplay : Widget {
    Evocation* module = nullptr;
    widget::SvgWidget* background = nullptr;
    std::shared_ptr<Font> font;

    explicit EvocationOLEDDisplay(Evocation* module) {
        this->module = module;
        background = new widget::SvgWidget();
        background->setSvg(Svg::load(asset::plugin(pluginInstance, "res/ui/feedback_oled.svg")));
        addChild(background);
        box.size = background->box.size;
    }

    void step() override {
        Widget::step();
        if (background) {
            background->box.pos = Vec();
            background->box.size = box.size;
        }
    }

    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        drawContent(args);
    }

  private:
    void ensureFont() {
        if (!font && APP && APP->window) {
            font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        }
    }

    void drawContent(const DrawArgs& args) {
        ensureFont();
        nvgSave(args.vg);
        if (!module) {
            nvgRestore(args.vg);
            return;
        }

        // Define safe display area (inside bezel)
        const float padding = 6.0f;
        const float safeWidth = box.size.x - (padding * 2.0f);
        const float safeHeight = box.size.y - (padding * 2.0f);

        int envIndex = module->getCurrentEnvelopeIndex();
        envIndex = clamp(envIndex, 0, Evocation::NUM_ENVELOPES - 1);

        bool flash = module->isSelectionFlashActive();
        // Only show flash in Gesture mode
        if (flash && module->mode == Evocation::EnvelopeMode::GESTURE) {
            std::string flashText = string::f("ENV %d SELECTED", envIndex + 1);
            if (font) {
                nvgFontFaceId(args.vg, font->handle);

                // Measure text and scale to fit
                float fontSize = 12.0f;
                nvgFontSize(args.vg, fontSize);
                float bounds[4];
                nvgTextBounds(args.vg, 0, 0, flashText.c_str(), nullptr, bounds);
                float textWidth = bounds[2] - bounds[0];

                // Scale down if too wide
                if (textWidth > safeWidth) {
                    fontSize *= safeWidth / textWidth;
                    nvgFontSize(args.vg, fontSize);
                }

                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(args.vg, nvgRGBA(120, 220, 208, 240));
                nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, flashText.c_str(), nullptr);
            }
            nvgRestore(args.vg);
            return;
        }

        // Show recording indicator in Gesture mode
        if (module->isRecording && module->mode == Evocation::EnvelopeMode::GESTURE && font) {
            nvgFontFaceId(args.vg, font->handle);

            // "RECORDING" text at top
            float fontSize = 12.0f;
            nvgFontSize(args.vg, fontSize);
            float bounds[4];
            std::string recordText = "RECORDING";
            nvgTextBounds(args.vg, 0, 0, recordText.c_str(), nullptr, bounds);
            float textWidth = bounds[2] - bounds[0];

            // Scale down if too wide
            if (textWidth > safeWidth) {
                fontSize *= safeWidth / textWidth;
                nvgFontSize(args.vg, fontSize);
            }

            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(args.vg, nvgRGBA(120, 220, 208, 240));
            nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.35f, recordText.c_str(), nullptr);

            // Progress bar
            float progress = clamp(module->recordingTime / module->maxRecordingTime, 0.f, 1.f);
            const float barWidth = safeWidth * 0.8f;
            const float barHeight = 4.0f;
            const float barX = padding + (safeWidth - barWidth) * 0.5f;
            const float barY = box.size.y * 0.6f;

            // Background bar (dim)
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, barX, barY, barWidth, barHeight, 2.0f);
            nvgFillColor(args.vg, nvgRGBA(60, 120, 110, 100));
            nvgFill(args.vg);

            // Progress fill (bright cyan)
            if (progress > 0.001f) {
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, barX, barY, barWidth * progress, barHeight, 2.0f);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 255));
                nvgFill(args.vg);
            }

            nvgRestore(args.vg);
            return;
        }

        // Display last touched parameter if available
        if (module->lastTouched.hasParam && module->lastTouched.timer > 0.f && font) {
            nvgFontFaceId(args.vg, font->handle);

            // Parameter name
            float nameFontSize = 9.0f;
            nvgFontSize(args.vg, nameFontSize);
            float nameBounds[4];
            nvgTextBounds(args.vg, 0, 0, module->lastTouched.name.c_str(), nullptr, nameBounds);
            float nameWidth = nameBounds[2] - nameBounds[0];

            // Scale name if too wide
            if (nameWidth > safeWidth) {
                nameFontSize *= safeWidth / nameWidth;
                nvgFontSize(args.vg, nameFontSize);
            }

            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(args.vg, nvgRGBA(140, 220, 208, 200));
            nvgText(args.vg, box.size.x * 0.5f, padding + 8.0f, module->lastTouched.name.c_str(), nullptr);

            // Parameter value (larger)
            float valueFontSize = 16.0f;
            nvgFontSize(args.vg, valueFontSize);
            float valueBounds[4];
            nvgTextBounds(args.vg, 0, 0, module->lastTouched.value.c_str(), nullptr, valueBounds);
            float valueWidth = valueBounds[2] - valueBounds[0];

            // Scale value if too wide
            if (valueWidth > safeWidth) {
                valueFontSize *= safeWidth / valueWidth;
                nvgFontSize(args.vg, valueFontSize);
            }

            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(args.vg, nvgRGBA(180, 255, 240, 255));
            nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.6f, module->lastTouched.value.c_str(), nullptr);
        } else if (font) {
            // Default display showing current envelope
            bool hasEnv = module->hasRecordedEnvelope();

            if (hasEnv && !module->envelope.empty()) {
                // Draw envelope waveform on OLED
                const float labelHeight = 14.0f;
                const float topPadding = 10.0f;
                const float bottomPadding = 6.0f;
                const float sidePadding = 8.0f;
                const float graphWidth = box.size.x - (sidePadding * 2.0f);
                const float graphHeight = box.size.y - topPadding - bottomPadding - labelHeight;
                const float graphX = sidePadding;
                const float graphY = topPadding;

                // Draw grid lines with synthwave aesthetic
                nvgStrokeColor(args.vg, nvgRGBA(180, 64, 255, 35));
                nvgStrokeWidth(args.vg, 0.5f);
                // Horizontal center line (5V)
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, graphX, graphY + graphHeight * 0.5f);
                nvgLineTo(args.vg, graphX + graphWidth, graphY + graphHeight * 0.5f);
                nvgStroke(args.vg);

                // Draw 10V line (top) and 0V line (bottom) in magenta
                nvgStrokeColor(args.vg, nvgRGBA(255, 0, 180, 70));
                nvgStrokeWidth(args.vg, 0.5f);
                // 10V line at top
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, graphX, graphY);
                nvgLineTo(args.vg, graphX + graphWidth, graphY);
                nvgStroke(args.vg);
                // 0V line at bottom
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, graphX, graphY + graphHeight);
                nvgLineTo(args.vg, graphX + graphWidth, graphY + graphHeight);
                nvgStroke(args.vg);

                // Check if envelope is inverted
                bool inverted = module->invertStates[envIndex];

                // Draw envelope waveform (thin bright cyan)
                nvgStrokeColor(args.vg, nvgRGBA(0, 255, 220, 255));
                nvgStrokeWidth(args.vg, 0.3f);
                nvgLineCap(args.vg, NVG_ROUND);
                nvgLineJoin(args.vg, NVG_ROUND);

                nvgBeginPath(args.vg);
                bool first = true;
                for (const auto& point : module->envelope) {
                    float x = graphX + point.time * graphWidth;
                    float yValue = inverted ? point.y : (1.0f - point.y);
                    float y = graphY + yValue * graphHeight;

                    if (first) {
                        nvgMoveTo(args.vg, x, y);
                        first = false;
                    } else {
                        nvgLineTo(args.vg, x, y);
                    }
                }
                nvgStroke(args.vg);

                // Draw envelope points as tiny bright dots
                nvgFillColor(args.vg, nvgRGBA(180, 255, 255, 255));
                for (const auto& point : module->envelope) {
                    float x = graphX + point.time * graphWidth;
                    float yValue = inverted ? point.y : (1.0f - point.y);
                    float y = graphY + yValue * graphHeight;

                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x, y, 0.4f);
                    nvgFill(args.vg);
                }

                // Draw per-voice playback scanlines (up to 8 shades)
                static const NVGcolor voiceColors[8] = {
                    nvgRGBA(255, 190, 255, 255),
                    nvgRGBA(240, 170, 250, 240),
                    nvgRGBA(225, 150, 245, 225),
                    nvgRGBA(210, 130, 235, 210),
                    nvgRGBA(195, 110, 225, 195),
                    nvgRGBA(180, 95, 215, 180),
                    nvgRGBA(165, 80, 205, 165),
                    nvgRGBA(150, 70, 195, 150)
                };

                std::vector<int> activeVoices;
                const int maxVoiceVisuals = std::min(8, Evocation::MAX_POLY_CHANNELS);
                for (int voice = 0; voice < maxVoiceVisuals; ++voice) {
                    if (module->isPlaybackActive(envIndex, voice)) {
                        activeVoices.push_back(voice);
                    }
                }

                for (size_t idx = 0; idx < activeVoices.size(); ++idx) {
                    int voice = activeVoices[idx];
                    float phase = clamp(module->getPlaybackPhase(envIndex, voice), 0.f, 1.f);
                    float playheadX = graphX + phase * graphWidth;

                    NVGcolor color = voiceColors[std::min(idx, (size_t)7)];
                    nvgBeginPath(args.vg);
                    nvgMoveTo(args.vg, playheadX, graphY);
                    nvgLineTo(args.vg, playheadX, graphY + graphHeight);
                    nvgStrokeColor(args.vg, color);
                    nvgStrokeWidth(args.vg, 0.25f);
                    nvgStroke(args.vg);
                }

                // Draw mode-specific info in top corners
                nvgFontFaceId(args.vg, font->handle);
                nvgFontSize(args.vg, 7.0f);
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 200));

                if (module->mode == Evocation::EnvelopeMode::ADSR) {
                    // ADSR mode: show individual stage duration/level
                    std::string leftText;
                    switch (envIndex) {
                        case 0: // Attack
                            leftText = string::f("%.2fs", module->adsrAttackTime);
                            break;
                        case 1: // Decay
                            leftText = string::f("%.2fs", module->adsrDecayTime);
                            break;
                        case 2: // Sustain (level, not time)
                            leftText = string::f("%.2f", module->adsrSustainLevel);
                            break;
                        case 3: // Release
                            leftText = string::f("%.2fs", module->adsrReleaseTime);
                            break;
                    }
                    nvgText(args.vg, sidePadding, 3.0f, leftText.c_str(), nullptr);
                } else {
                    // Gesture mode: show speed
                    float speed = module->params[Evocation::SPEED_1_PARAM + envIndex].getValue();
                    std::string speedText = string::f("%.2fx", speed);
                    nvgText(args.vg, sidePadding, 3.0f, speedText.c_str(), nullptr);
                }

                // Draw total time at top center (bright magenta)
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGBA(255, 100, 220, 220));
                float duration = module->getEnvelopeDuration();
                std::string timeText = string::f("%.2fs", duration);
                nvgText(args.vg, box.size.x * 0.5f, 3.0f, timeText.c_str(), nullptr);

                // Draw phase/contour in top right corner (cyan)
                nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 200));

                if (module->mode == Evocation::EnvelopeMode::ADSR) {
                    // ADSR mode: show contour type
                    float contour = 0.0f;
                    switch (envIndex) {
                        case 0: contour = module->adsrAttackContour; break;
                        case 1: contour = module->adsrDecayContour; break;
                        case 2: contour = module->adsrSustainContour; break;
                        case 3: contour = module->adsrReleaseContour; break;
                    }
                    float curveAmount = Evocation::mapContourControl(contour);
                    const char* contourLabel = "LIN";
                    if (curveAmount > 0.1f) {
                        contourLabel = "EXP";
                    } else if (curveAmount < -0.1f) {
                        contourLabel = "LOG";
                    }
                    nvgText(args.vg, box.size.x - sidePadding, 3.0f, contourLabel, nullptr);
                } else {
                    // Gesture mode: show phase
                    float phase = module->phaseOffsets[envIndex];
                    float phaseDeg = phase * 360.0f;
                    std::string phaseText = string::f("%.0f°", phaseDeg);
                    nvgText(args.vg, box.size.x - sidePadding, 3.0f, phaseText.c_str(), nullptr);
                }

                // Draw envelope label at bottom center (magenta glow)
                nvgFontSize(args.vg, 10.0f);
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
                nvgFillColor(args.vg, nvgRGBA(255, 100, 220, 240));
                std::string text;
                if (module->mode == Evocation::EnvelopeMode::ADSR) {
                    const char* stages[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
                    text = stages[envIndex];
                } else {
                    text = string::f("ENV %d", envIndex + 1);
                }
                nvgText(args.vg, box.size.x * 0.5f, box.size.y - bottomPadding, text.c_str(), nullptr);

                // Draw invert status at bottom left (cyan)
                nvgFontSize(args.vg, 7.0f);
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 200));
                std::string invertText = inverted ? "INV" : "";
                if (!invertText.empty()) {
                    nvgText(args.vg, sidePadding, box.size.y - bottomPadding, invertText.c_str(), nullptr);
                }

                // Draw loop status at bottom right (cyan)
                nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 200));
                bool looping = module->loopStates[envIndex];
                std::string loopText = looping ? "LOOP" : "";
                if (!loopText.empty()) {
                    nvgText(args.vg, box.size.x - sidePadding, box.size.y - bottomPadding, loopText.c_str(), nullptr);
                }

            } else {
                // No envelope recorded
                nvgFontFaceId(args.vg, font->handle);
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFontSize(args.vg, 10.0f);
                nvgFillColor(args.vg, nvgRGBA(100, 160, 150, 180));
                std::string emptyText;
                if (module->mode == Evocation::EnvelopeMode::ADSR) {
                    emptyText = "[ADSR MODE]";
                } else {
                    emptyText = "[ENV EMPTY]";
                }
                nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, emptyText.c_str(), nullptr);
            }
        }

        nvgRestore(args.vg);
    }
};

struct TrimGestureLeadMenuItem : MenuItem {
    Evocation* module = nullptr;

    void step() override {
        MenuItem::step();
        bool enabled = module && module->mode == Evocation::EnvelopeMode::GESTURE && module->hasRecordedEnvelope();
        disabled = !enabled;
    }

    void onAction(const event::Action& e) override {
        MenuItem::onAction(e);
        if (!module)
            return;
        bool trimmed = module->trimGestureLeadingSilence();
        if (!trimmed) {
            module->updateLastTouched("", "NO TRIM");
        }
    }
};

struct TrimGestureTailMenuItem : MenuItem {
    Evocation* module = nullptr;

    void step() override {
        MenuItem::step();
        bool enabled = module && module->mode == Evocation::EnvelopeMode::GESTURE && module->hasRecordedEnvelope();
        disabled = !enabled;
    }

    void onAction(const event::Action& e) override {
        MenuItem::onAction(e);
        if (!module)
            return;
        bool trimmed = module->trimGestureTrailingSilence();
        if (!trimmed) {
            module->updateLastTouched("", "NO TRIM");
        }
    }
};

struct EvocationWidget : ModuleWidget {
    TouchStripWidget* touchStrip = nullptr;
    EvocationOLEDDisplay* oledDisplay = nullptr;

    void draw(const DrawArgs& args) override {
        // Reapply shared panel background without caching across shutdown
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/vcv-panel-background.png"));
        if (bg) {
            NVGpaint paint = nvgImagePattern(args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, bg->handle, 1.f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        }
        ModuleWidget::draw(args);
    }

    EvocationWidget(Evocation* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Evocation.svg")));

        using shapetaker::ui::LayoutHelper;
        auto mm = [](float x, float y) { return LayoutHelper::mm2px(Vec(x, y)); };

        // Parse SVG panel for precise positioning
        LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/Evocation.svg"));
        auto centerPx = [&](const std::string& id, float defx, float defy) -> Vec {
            return parser.centerPx(id, defx, defy);
        };

        constexpr float panelWidthMm = 101.6f;
        constexpr float panelHeightMm = 128.5f;

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Touch strip (positioned by SVG rectangle)
        touchStrip = new TouchStripWidget(module);
        Rect touchStripRect = parser.rectMm("touch-strip", 6.8731313f, 15.396681f, 30.561571f, 72.217186f);
        touchStrip->box.pos = mm(touchStripRect.pos.x, touchStripRect.pos.y);
        touchStrip->box.size = mm(touchStripRect.size.x, touchStripRect.size.y);
        addChild(touchStrip);

        // Store reference in module for clearing pulse trail
        if (module) {
            module->touchStripWidget = touchStrip;
        }

        // Feedback OLED display
        Rect oledRect = parser.rectMm("feedback-oled", 6.8391566f, 98.025497f, 29.917749f, 22.122351f);
        oledDisplay = new EvocationOLEDDisplay(module);
        oledDisplay->box.pos = mm(oledRect.pos.x, oledRect.pos.y);
        oledDisplay->box.size = mm(oledRect.size.x, oledRect.size.y);
        addChild(oledDisplay);

        // Trigger / clear buttons (positioned by SVG elements)
        Vec triggerBtn = parser.centerPx("trigger-btn-0", 63.618366f, 18.659674f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(triggerBtn, module, Evocation::TRIGGER_PARAM));

        Vec clearBtn = parser.centerPx("clear-buffer-btn", 78.077148f, 18.659674f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(clearBtn, module, Evocation::CLEAR_PARAM));

        Vec trimBtn = parser.centerPx("trim-gesture-btn", 92.535f, 18.659674f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(trimBtn, module, Evocation::TRIM_LEAD_PARAM));
        Vec trimTailBtn = parser.centerPx("trim-gesture-tail-btn", 92.535f, 29.776815f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(trimTailBtn, module, Evocation::TRIM_TAIL_PARAM));

        // CV inputs for trigger/clear
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("trigger-cv-in", 63.618366f, 29.776815f), module, Evocation::TRIGGER_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("clear-cv-in", 78.077148f, 29.776815f), module, Evocation::CLEAR_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("gate-cv-in", 63.618366f, 40.893959f), module, Evocation::GATE_INPUT));

        // Phase CV inputs (right column)
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("phase1-cv-in", 92.5f, 100.0f), module, Evocation::PHASE_1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("phase2-cv-in", 92.5f, 106.5f), module, Evocation::PHASE_2_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("phase3-cv-in", 92.5f, 113.0f), module, Evocation::PHASE_3_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("phase4-cv-in", 92.5f, 119.5f), module, Evocation::PHASE_4_INPUT));

        // Envelope controls
        addParam(createParamCentered<ShapetakerKnobMedium>(centerPx("env-speed", 49.159584f, 47.892654f), module, Evocation::ENV_SPEED_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("env-phase-offset", 78.077148f, 47.892654f), module, Evocation::ENV_PHASE_PARAM));

        // Loop and invert capacitive touch switches with jewel LED indicators
        addParam(createParamCentered<CapacitiveTouchSwitch>(centerPx("loop-sw", 78.077148f, 66.94957f), module, Evocation::LOOP_1_PARAM));
        addChild(createLightCentered<shapetaker::ui::SmallJewelLED>(centerPx("loop-sw", 78.077148f, 66.94957f), module, Evocation::LOOP_1_LIGHT));

        addParam(createParamCentered<CapacitiveTouchSwitch>(centerPx("invert-sw", 49.159584f, 68.657234f), module, Evocation::INVERT_1_PARAM));
        addChild(createLightCentered<shapetaker::ui::SmallJewelLED>(centerPx("invert-sw", 49.159584f, 68.657234f), module, Evocation::INVERT_1_LIGHT));

        // Envelope selection buttons
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("env1-select-btn", 46.216522f, 92.244675f), module, Evocation::ENV_SELECT_1_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("env2-select-btn", 58.543388f, 92.244675f), module, Evocation::ENV_SELECT_2_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("env3-select-btn", 70.870247f, 92.244675f), module, Evocation::ENV_SELECT_3_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("env4-select-btn", 83.197113f, 92.244675f), module, Evocation::ENV_SELECT_4_PARAM));

        // EOC outputs per envelope
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env1-eoc", 46.216522f, 92.957283f), module, Evocation::ENV_1_EOC_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env2-eoc", 58.543388f, 92.957283f), module, Evocation::ENV_2_EOC_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env3-eoc", 70.870247f, 92.957291f), module, Evocation::ENV_3_EOC_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env4-eoc", 83.197113f, 92.957291f), module, Evocation::ENV_4_EOC_OUTPUT));

        // Envelope outputs (using SVG positioning)
        // Envelope 1
        Vec env1OutCenter = centerPx("env1-out", 46.216522f, 104.81236f);
        addOutput(createOutputCentered<ShapetakerBNCPort>(env1OutCenter, module, Evocation::ENV_1_OUTPUT));

        // Envelope 2
        Vec env2OutCenter = centerPx("env2-out", 58.543388f, 104.81236f);
        addOutput(createOutputCentered<ShapetakerBNCPort>(env2OutCenter, module, Evocation::ENV_2_OUTPUT));

        // Envelope 3
        Vec env3OutCenter = centerPx("env3-out", 70.870247f, 104.81237f);
        addOutput(createOutputCentered<ShapetakerBNCPort>(env3OutCenter, module, Evocation::ENV_3_OUTPUT));

        // Envelope 4
        Vec env4OutCenter = centerPx("env4-out", 83.197113f, 104.81237f);
        addOutput(createOutputCentered<ShapetakerBNCPort>(env4OutCenter, module, Evocation::ENV_4_OUTPUT));

        // Gate outputs per envelope (updated positions from SVG)
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env1-gate", 46.216522f, 117.38005f), module, Evocation::ENV_1_GATE_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env2-gate", 58.543388f, 117.38005f), module, Evocation::ENV_2_GATE_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env3-gate", 70.870247f, 117.38005f), module, Evocation::ENV_3_GATE_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("env4-gate", 83.197113f, 117.38005f), module, Evocation::ENV_4_GATE_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* evocation = dynamic_cast<Evocation*>(module);
        if (!evocation)
            return;

        menu->addChild(new MenuSeparator);

        // Mode selection - Gesture
        menu->addChild(createCheckMenuItem("Gesture Mode", "", [=] {
            return evocation->mode == Evocation::EnvelopeMode::GESTURE;
        }, [=] {
            evocation->switchToGestureMode();
        }));

        // Mode selection - ADSR
        menu->addChild(createCheckMenuItem("ADSR Mode", "", [=] {
            return evocation->mode == Evocation::EnvelopeMode::ADSR;
        }, [=] {
            evocation->switchToADSRMode();
        }));

        auto* trimItem = new TrimGestureLeadMenuItem();
        trimItem->module = evocation;
        trimItem->text = "Trim Gesture Lead";
        menu->addChild(trimItem);

        auto* trimTailItem = new TrimGestureTailMenuItem();
        trimTailItem->module = evocation;
        trimTailItem->text = "Trim Gesture Tail";
        menu->addChild(trimTailItem);

        menu->addChild(new MenuSeparator);
        menu->addChild(createCheckMenuItem("Debug Touch Logging", "", [=] {
            return evocation->debugTouchLogging;
        }, [=] {
            evocation->debugTouchLogging = !evocation->debugTouchLogging;
            INFO("Evocation debug logging %s", evocation->debugTouchLogging ? "enabled" : "disabled");
        }));
    }
};

// Model registration
Model* modelEvocation = createModel<Evocation, EvocationWidget>("Evocation");
