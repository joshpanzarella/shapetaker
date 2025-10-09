#include "plugin.hpp"
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
    static constexpr float MIN_SAMPLE_INTERVAL = 1.f / 120.f;

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
};

// Main Evocation Module
struct Evocation : Module {
    enum ParamId {
        RECORD_PARAM,
        TRIGGER_PARAM,
        CLEAR_PARAM,
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
        RECORD_INPUT,
        TRIGGER_INPUT,
        CLEAR_INPUT,
        GATE_INPUT,
        SPEED_1_INPUT,
        SPEED_2_INPUT,
        SPEED_3_INPUT,
        SPEED_4_INPUT,
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

    int currentEnvelopeIndex = 0;
    int currentParameterIndex = 0;

    std::vector<EnvelopePoint> envelope;
    bool isRecording = false;
    bool bufferHasData = false;
    bool debugTouchLogging = false;
    
    // Individual loop states for each envelope player
    bool loopStates[4] = {false, false, false, false};
    
    // Invert states for each speed output
    bool invertStates[4] = {false, false, false, false};
    
    // Playback state for each output
    struct PlaybackState {
        bool active = false;
        float phase = 0.0f;
        dsp::PulseGenerator eocPulse;
    };

    PlaybackState playback[4]; // Four independent envelope players

    // Triggers
    dsp::SchmittTrigger triggerTrigger;
    dsp::SchmittTrigger gateTrigger;
    dsp::SchmittTrigger clearTrigger;
    dsp::SchmittTrigger recordTrigger;
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
        
        configParam(RECORD_PARAM, 0.f, 1.f, 0.f, "Record");
        configParam(TRIGGER_PARAM, 0.f, 1.f, 0.f, "Manual Trigger");
        configParam(CLEAR_PARAM, 0.f, 1.f, 0.f, "Clear Buffer");
        configParam(SPEED_1_PARAM, 0.1f, 16.0f, 1.0f, "Speed 1", "×");
        configParam(SPEED_2_PARAM, 0.1f, 16.0f, 2.0f, "Speed 2", "×");
        configParam(SPEED_3_PARAM, 0.1f, 16.0f, 4.0f, "Speed 3", "×");
        configParam(SPEED_4_PARAM, 0.1f, 16.0f, 8.0f, "Speed 4", "×");
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
        configParam(ENV_SPEED_PARAM, 0.1f, 16.0f, 1.0f, "Selected Envelope Speed", "×");
        configParam(ENV_PHASE_PARAM, 0.f, 1.f, 0.f, "Selected Envelope Phase");
        configButton(ENV_SELECT_1_PARAM, "Select Envelope 1");
        configButton(ENV_SELECT_2_PARAM, "Select Envelope 2");
        configButton(ENV_SELECT_3_PARAM, "Select Envelope 3");
        configButton(ENV_SELECT_4_PARAM, "Select Envelope 4");
        configInput(RECORD_INPUT, "Record CV");
        configInput(TRIGGER_INPUT, "External Trigger");
        configInput(CLEAR_INPUT, "Clear Trigger");
        configInput(GATE_INPUT, "Gate Input");
        configInput(SPEED_1_INPUT, "Speed 1 CV");
        configInput(SPEED_2_INPUT, "Speed 2 CV");
        configInput(SPEED_3_INPUT, "Speed 3 CV");
        configInput(SPEED_4_INPUT, "Speed 4 CV");
        
        configOutput(ENV_1_OUTPUT, "Envelope 1");
        configOutput(ENV_2_OUTPUT, "Envelope 2");
        configOutput(ENV_3_OUTPUT, "Envelope 3");  
        configOutput(ENV_4_OUTPUT, "Envelope 4");
        configOutput(ENV_1_GATE_OUTPUT, "Envelope 1 Gate");
        configOutput(ENV_2_GATE_OUTPUT, "Envelope 2 Gate");
        configOutput(ENV_3_GATE_OUTPUT, "Envelope 3 Gate");
        configOutput(ENV_4_GATE_OUTPUT, "Envelope 4 Gate");
    }
    
    void process(const ProcessArgs& args) override {
        // Handle triggers using shared helpers
        bool triggerPressed = shapetaker::TriggerHelper::processTrigger(
            triggerTrigger,
            params[TRIGGER_PARAM].getValue(),
            inputs[TRIGGER_INPUT],
            1.f
        );
        bool clearPressed = shapetaker::TriggerHelper::processTrigger(
            clearTrigger,
            params[CLEAR_PARAM].getValue(),
            inputs[CLEAR_INPUT],
            1.f
        );
        bool recordPressed = recordTrigger.process(params[RECORD_PARAM].getValue());
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
            float speedControl = params[ENV_SPEED_PARAM].getValue();
            if (std::fabs(speedControl - envSpeedControlCache) > 1e-6f) {
                envSpeedControlCache = speedControl;
                params[SPEED_1_PARAM + currentEnvelopeIndex].setValue(envSpeedControlCache);
                // Update OLED display
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
                // Update OLED display
                float phaseDeg = envPhaseControlCache * 360.f;
                updateLastTouched(string::f("ENV %d PHASE", currentEnvelopeIndex + 1), string::f("%.2f°", phaseDeg));
            } else {
                float actualPhase = phaseOffsets[currentEnvelopeIndex];
                if (std::fabs(actualPhase - envPhaseControlCache) > 1e-6f) {
                    envPhaseControlCache = actualPhase;
                    params[ENV_PHASE_PARAM].setValue(actualPhase);
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
        if (clearPressed) {
            clearBuffer();
            updateLastTouched("CLEAR", "BUFFER CLEARED");
        }

        // Handle recording
        if (recordPressed) {
            if (!isRecording) {
                startRecording();
                updateLastTouched("RECORD", "STARTED");
            } else {
                stopRecording();
                updateLastTouched("RECORD", "STOPPED");
            }
        }

        // Handle trigger
        // (removed OLED display update for trigger)

        // Update recording
        if (isRecording) {
            updateRecording(args.sampleTime);
        }

        // Handle triggers for playback
        // Trigger takes precedence over gate
        if (triggerPressed && bufferHasData) {
            triggerAllEnvelopes();
        } else if (inputs[GATE_INPUT].isConnected() && bufferHasData) {
            // Gate input - sustain playback while gate is high
            bool gateHigh = inputs[GATE_INPUT].getVoltage() >= 1.0f;
            if (gateHigh) {
                // Start all envelopes if gate just went high
                if (gateTrigger.process(inputs[GATE_INPUT].getVoltage())) {
                    triggerAllEnvelopes();
                }
            }
        }

        // Process each output
        for (int i = 0; i < 4; i++) {
            processPlayback(i, args.sampleTime);
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

        if (debugTouchLogging) {
            INFO("Evocation::normalizeEnvelopeTiming start=%.4f end=%.4f range=%.4f", startTime, endTime, range);
        }
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
    
    void triggerAllEnvelopes() {
        if (!bufferHasData) return;
        
        for (int i = 0; i < 4; i++) {
            playback[i].active = true;
            playback[i].phase = 0.0f;
        }

    }
    
    void processPlayback(int outputIndex, float sampleTime) {
        PlaybackState& pb = playback[outputIndex];

        // Process EOC pulse
        bool eocPulse = pb.eocPulse.process(sampleTime);
        outputs[ENV_1_EOC_OUTPUT + outputIndex].setVoltage(eocPulse ? 10.0f : 0.0f);

        if (!pb.active || !bufferHasData) {
            outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f);
            outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(0.0f);
            return;
        }

        // Get speed from knob and CV
        float speed = params[SPEED_1_PARAM + outputIndex].getValue();
        if (inputs[SPEED_1_INPUT + outputIndex].isConnected()) {
            speed += inputs[SPEED_1_INPUT + outputIndex].getVoltage(); // 1V/oct style
        }
        speed = clamp(speed, 0.1f, 16.0f); // Reasonable speed limits

        // Advance phase
        float phaseIncrement = speed * sampleTime / getEnvelopeDuration();
        pb.phase += phaseIncrement;

        // Check if envelope is complete
        if (pb.phase >= 1.0f) {
            // Trigger EOC pulse
            pb.eocPulse.trigger(1e-3f); // 1ms pulse

            if (loopStates[outputIndex]) {
                pb.phase -= 1.0f; // Wrap around for looping
            } else {
                pb.active = false;
                outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f);
                outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(0.0f);
                return;
            }
        }

        // Interpolate envelope value at current phase
        float samplePhase = pb.phase + phaseOffsets[outputIndex];
        samplePhase -= std::floor(samplePhase);
        if (samplePhase < 0.f) samplePhase += 1.f;

        float envelopeValue = interpolateEnvelope(samplePhase);

        if (invertStates[outputIndex]) {
            envelopeValue = 1.0f - envelopeValue;
        }

        outputs[ENV_1_OUTPUT + outputIndex].setVoltage(envelopeValue * 10.0f); // 0-10V output
        outputs[ENV_1_GATE_OUTPUT + outputIndex].setVoltage(pb.active ? 10.0f : 0.0f);
    }
    
    float interpolateEnvelope(float phase) {
        if (envelope.empty()) return 0.0f;
        if (envelope.size() == 1) return envelope[0].y;
        
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

    float getPlaybackPhase(int index) const {
        if (index < 0 || index >= 4)
            return 0.0f;
        return clamp(playback[index].phase, 0.0f, 1.0f);
    }

    bool isPlaybackActive(int index) const {
        if (index < 0 || index >= 4)
            return false;
        return playback[index].active;
    }

    float getEnvelopeDuration() {
        return getRecordedDuration();
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
        float speed = params[SPEED_1_PARAM + currentEnvelopeIndex].getValue();
        params[ENV_SPEED_PARAM].setValue(speed);
        envSpeedControlCache = speed;
        float phase = phaseOffsets[currentEnvelopeIndex];
        params[ENV_PHASE_PARAM].setValue(phase);
        envPhaseControlCache = phase;

        // Update loop and invert switch states to reflect current envelope
        params[LOOP_1_PARAM].setValue(loopStates[currentEnvelopeIndex] ? 1.0f : 0.0f);
        params[INVERT_1_PARAM].setValue(invertStates[currentEnvelopeIndex] ? 1.0f : 0.0f);

        if (flash)
            selectionFlashTimer = 0.75f;
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
        for (int i = 0; i < 4; i++) {
            if (playback[i].active) return true;
        }
        return false;
    }
    
    void stopAllPlayback() {
        for (int i = 0; i < 4; i++) {
            playback[i].active = false;
            playback[i].phase = 0.0f;
        }
        if (touchStripWidget) {
            touchStripWidget->clearPulses();
        }
    }
    
    // Save/Load state
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        
        json_object_set_new(rootJ, "bufferHasData", json_boolean(bufferHasData));
        
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

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* bufferHasDataJ = json_object_get(rootJ, "bufferHasData");
        if (bufferHasDataJ) bufferHasData = json_boolean_value(bufferHasDataJ);
        
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

        json_t* debugTouchJ = json_object_get(rootJ, "debugTouchLogging");
        if (debugTouchJ) {
            debugTouchLogging = json_boolean_value(debugTouchJ);
        }

        json_t* durationJ = json_object_get(rootJ, "recordedDuration");
        if (durationJ) {
            recordedDuration = clamp((float)json_real_value(durationJ), 1e-3f, maxRecordingTime);
        }

        json_t* currentEnvelopeIndexJ = json_object_get(rootJ, "currentEnvelopeIndex");
        if (currentEnvelopeIndexJ) {
            setCurrentEnvelopeIndex((int)json_integer_value(currentEnvelopeIndexJ));
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

// TouchStripWidget Method Implementations (after Evocation is fully defined)
TouchStripWidget::TouchStripWidget(Evocation* module) {
    this->module = module;
    // Size will be set by the widget constructor based on SVG rect
    box.size = Vec(0, 0);
}

void TouchStripWidget::onButton(const event::Button& e) {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
        if (!module) return;
        
        // Start recording
        isDragging = true;
        showTouch = true;
        currentTouchPos = clampToBounds(resolveMouseLocal(e.pos));

        // Start recording in module
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

    // Stop recording in module
    currentTouchPos = clampToBounds(resolveMouseLocal(currentTouchPos));
    recordSample("release", true);
    if (module->isRecording) {
        module->stopRecording();
    }

    if (module && module->debugTouchLogging) {
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

void TouchStripWidget::step() {
    Widget::step();

    if (module) {
        if (module->isRecording) {
            if (isDragging) {
                currentTouchPos = clampToBounds(resolveMouseLocal(currentTouchPos));
                recordSample("frame");
            }
        } else {
            if (isDragging) {
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
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.7f, "Hold RECORD button", NULL);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.8f, "then draw envelope", NULL);
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

        int envIndex = module->getCurrentEnvelopeIndex();
        envIndex = clamp(envIndex, 0, Evocation::NUM_ENVELOPES - 1);

        bool flash = module->isSelectionFlashActive();
        if (flash) {
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

                // Check if envelope is inverted and active
                bool inverted = module->invertStates[envIndex];
                bool isActive = module->isPlaybackActive(envIndex);

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

                // Draw thin scanline when envelope is active
                if (isActive) {
                    float phase = module->getPlaybackPhase(envIndex);
                    float playheadX = graphX + phase * graphWidth;

                    // Draw thin bright magenta scanline
                    nvgBeginPath(args.vg);
                    nvgMoveTo(args.vg, playheadX, graphY);
                    nvgLineTo(args.vg, playheadX, graphY + graphHeight);
                    nvgStrokeColor(args.vg, nvgRGBA(255, 100, 255, 255));
                    nvgStrokeWidth(args.vg, 0.25f);
                    nvgStroke(args.vg);
                }

                // Draw speed in top left corner (cyan)
                nvgFontFaceId(args.vg, font->handle);
                nvgFontSize(args.vg, 7.0f);
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 200));
                float speed = module->params[Evocation::SPEED_1_PARAM + envIndex].getValue();
                std::string speedText = string::f("%.2fx", speed);
                nvgText(args.vg, sidePadding, 3.0f, speedText.c_str(), nullptr);

                // Draw recorded time at top center (bright magenta)
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGBA(255, 100, 220, 220));
                float duration = module->getEnvelopeDuration();
                std::string timeText = string::f("%.2fs", duration);
                nvgText(args.vg, box.size.x * 0.5f, 3.0f, timeText.c_str(), nullptr);

                // Draw phase in top right corner (cyan)
                nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGBA(0, 255, 220, 200));
                float phase = module->phaseOffsets[envIndex];
                float phaseDeg = phase * 360.0f;
                std::string phaseText = string::f("%.0f°", phaseDeg);
                nvgText(args.vg, box.size.x - sidePadding, 3.0f, phaseText.c_str(), nullptr);

                // Draw envelope label at bottom center (magenta glow)
                nvgFontSize(args.vg, 10.0f);
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
                nvgFillColor(args.vg, nvgRGBA(255, 100, 220, 240));
                std::string text = string::f("ENV %d", envIndex + 1);
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
                nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, "[ENV EMPTY]", nullptr);
            }
        }

        nvgRestore(args.vg);
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

        constexpr float panelWidthMm = 91.44f;
        constexpr float panelHeightMm = 128.5f;

        addChild(createWidget<ScrewBlack>(mm(6.5f, 6.5f)));
        addChild(createWidget<ScrewBlack>(mm(panelWidthMm - 6.5f, 6.5f)));
        addChild(createWidget<ScrewBlack>(mm(6.5f, panelHeightMm - 6.5f)));
        addChild(createWidget<ScrewBlack>(mm(panelWidthMm - 6.5f, panelHeightMm - 6.5f)));

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

        // Record / trigger / clear buttons (positioned by SVG elements)
        Vec recordBtn = parser.centerPx("record-btn", 49.159584f, 18.659674f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(recordBtn, module, Evocation::RECORD_PARAM));

        Vec triggerBtn = parser.centerPx("trigger-btn-0", 63.618366f, 18.659674f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(triggerBtn, module, Evocation::TRIGGER_PARAM));

        Vec clearBtn = parser.centerPx("clear-buffer-btn", 78.077148f, 18.659674f);
        addParam(createParamCentered<ShapetakerVintageMomentary>(clearBtn, module, Evocation::CLEAR_PARAM));

        // CV inputs for record/trigger/clear
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("record-cv-in", 49.159584f, 29.776815f), module, Evocation::RECORD_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("trigger-cv-in", 63.618366f, 29.776815f), module, Evocation::TRIGGER_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("clear-cv-in", 78.077148f, 29.776815f), module, Evocation::CLEAR_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("gate-cv-in", 63.618366f, 40.893959f), module, Evocation::GATE_INPUT));

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
