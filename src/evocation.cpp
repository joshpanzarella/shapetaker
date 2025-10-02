#include "plugin.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

// Forward declaration
struct Evocation;

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
        PARAMS_LEN
    };
    enum InputId {
        TRIGGER_INPUT,
        CLEAR_INPUT,
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
        GATE_OUTPUT,
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
        dsp::PulseGenerator gateGen;
    };
    
    PlaybackState playback[4]; // Four independent envelope players
    
    // Triggers
    dsp::SchmittTrigger triggerTrigger;
    dsp::SchmittTrigger clearTrigger;
    dsp::SchmittTrigger recordTrigger;
    dsp::SchmittTrigger loopTriggers[4];
    dsp::SchmittTrigger invertTriggers[4];
    
    // Recording timing
    float recordingTime = 0.0f;
    float maxRecordingTime = 10.0f; // 10 seconds max
    float firstSampleTime = -1.0f;
    float recordedDuration = 2.0f;

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
        
        configInput(TRIGGER_INPUT, "External Trigger");
        configInput(CLEAR_INPUT, "Clear Trigger");
        configInput(SPEED_1_INPUT, "Speed 1 CV");
        configInput(SPEED_2_INPUT, "Speed 2 CV");
        configInput(SPEED_3_INPUT, "Speed 3 CV");
        configInput(SPEED_4_INPUT, "Speed 4 CV");
        
        configOutput(ENV_1_OUTPUT, "Envelope 1");
        configOutput(ENV_2_OUTPUT, "Envelope 2");
        configOutput(ENV_3_OUTPUT, "Envelope 3");  
        configOutput(ENV_4_OUTPUT, "Envelope 4");
        configOutput(GATE_OUTPUT, "Gate");
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
        
        // Handle individual loop button toggles
        for (int i = 0; i < 4; i++) {
            shapetaker::TriggerHelper::processToggle(loopTriggers[i], params[LOOP_1_PARAM + i].getValue(), loopStates[i]);
        }
        
        // Handle invert button toggles
        for (int i = 0; i < 4; i++) {
            shapetaker::TriggerHelper::processToggle(invertTriggers[i], params[INVERT_1_PARAM + i].getValue(), invertStates[i]);
        }
        
        // Handle clear
        if (clearPressed) {
            clearBuffer();
        }
        
        // Handle recording
        if (recordPressed) {
            if (!isRecording) {
                startRecording();
            } else {
                stopRecording();
            }
        }
        
        // Update recording
        if (isRecording) {
            updateRecording(args.sampleTime);
        }
        
        // Handle triggers for playback
        if (triggerPressed && bufferHasData) {
            triggerAllEnvelopes();
        }
        
        // Process each output
        for (int i = 0; i < 4; i++) {
            processPlayback(i, args.sampleTime);
        }

        // Update lights
        lights[RECORDING_LIGHT].setBrightness(isRecording ? 1.0f : 0.0f);
        lights[TRIGGER_LIGHT].setBrightness(isAnyPlaybackActive() ? 1.0f : 0.0f);
        
        // Update individual loop lights
        for (int i = 0; i < 4; i++) {
            lights[LOOP_1_LIGHT + i].setBrightness(loopStates[i] ? 1.0f : 0.0f);
        }
        
        // Update invert lights
        for (int i = 0; i < 4; i++) {
            lights[INVERT_1_LIGHT + i].setBrightness(invertStates[i] ? 1.0f : 0.0f);
        }
        
        // Output gate signal when any envelope is playing
        outputs[GATE_OUTPUT].setVoltage(isAnyPlaybackActive() ? 10.0f : 0.0f);
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
            playback[i].gateGen.trigger(1e-3f);
        }

    }
    
    void processPlayback(int outputIndex, float sampleTime) {
        PlaybackState& pb = playback[outputIndex];
        
        if (!pb.active || !bufferHasData) {
            outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f);
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
            if (loopStates[outputIndex]) {
                pb.phase -= 1.0f; // Wrap around for looping
            } else {
                pb.active = false;
                outputs[ENV_1_OUTPUT + outputIndex].setVoltage(0.0f);
                return;
            }
        }

        // Interpolate envelope value at current phase
        float envelopeValue = interpolateEnvelope(pb.phase);

        // Apply inversion if enabled for this output
        if (invertStates[outputIndex]) {
            envelopeValue = 1.0f - envelopeValue;
        }

        outputs[ENV_1_OUTPUT + outputIndex].setVoltage(envelopeValue * 10.0f); // 0-10V output
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

        json_object_set_new(rootJ, "debugTouchLogging", json_boolean(debugTouchLogging));
        json_object_set_new(rootJ, "recordedDuration", json_real(recordedDuration));

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
    }
};

// TouchStripWidget Method Implementations (after Evocation is fully defined)
TouchStripWidget::TouchStripWidget(Evocation* module) {
    this->module = module;
    box.size = shapetaker::ui::LayoutHelper::mm2px(Vec(28.0f, 86.0f));
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
    const float topPad = std::min(height * 0.04f, 12.0f);      // keep headroom near 10V
    const float bottomPad = std::min(height * 0.12f, 20.0f);   // widen the 0V landing zone

    float y = clamp(currentTouchPos.y, 0.0f, height);
    if (y >= height - bottomPad)
        return 0.0f;
    if (y <= topPad)
        return 1.0f;

    float usable = height - topPad - bottomPad;
    if (usable <= 1e-6f)
        return (y <= height * 0.5f) ? 1.0f : 0.0f;

    float normalized = 1.0f - ((y - topPad) / usable);
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
    
    // Draw recorded envelope
    if (module && module->bufferHasData) {
        drawEnvelope(args);
    }
    
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
struct EvocationWidget : ModuleWidget {
    TouchStripWidget* touchStrip;

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

        constexpr float panelWidthMm = 81.28f;
        constexpr float panelHeightMm = 128.5f;

        addChild(createWidget<ScrewSilver>(mm(6.5f, 6.5f)));
        addChild(createWidget<ScrewSilver>(mm(panelWidthMm - 6.5f, 6.5f)));
        addChild(createWidget<ScrewSilver>(mm(6.5f, panelHeightMm - 6.5f)));
        addChild(createWidget<ScrewSilver>(mm(panelWidthMm - 6.5f, panelHeightMm - 6.5f)));

        // Touch strip (positioned by SVG rectangle)
        touchStrip = new TouchStripWidget(module);
        Rect touchStripRect = parser.rectMm("touch-strip", 9.0f, 22.0f, 28.0f, 86.0f);
        touchStrip->box.pos = mm(touchStripRect.pos.x, touchStripRect.pos.y);
        touchStrip->box.size = mm(touchStripRect.size.x, touchStripRect.size.y);
        addChild(touchStrip);

        // Store reference in module for clearing pulse trail
        if (module) {
            module->touchStripWidget = touchStrip;
        }

        // Record / trigger / clear (using SVG positioning)
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("record-btn", 58.0f, 24.0f), module, Evocation::RECORD_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(centerPx("record-btn", 58.0f, 24.0f) + Vec(-15, 0), module, Evocation::RECORDING_LIGHT));

        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("trigger-btn", 66.0f, 24.0f), module, Evocation::TRIGGER_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(centerPx("trigger-btn", 66.0f, 24.0f) + Vec(15, 0), module, Evocation::TRIGGER_LIGHT));

        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("clear-btn", 74.0f, 24.0f), module, Evocation::CLEAR_PARAM));

        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("trigger-cv", 58.0f, 34.0f), module, Evocation::TRIGGER_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("clear-cv", 74.0f, 34.0f), module, Evocation::CLEAR_INPUT));

        // Envelope rows (using SVG positioning)
        // Envelope 1 (FAST)
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env1-loop", 32.0f, 50.0f), module, Evocation::LOOP_1_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>(centerPx("env1-loop", 32.0f, 50.0f), module, Evocation::LOOP_1_LIGHT));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("env1-speed", 52.0f, 50.0f), module, Evocation::SPEED_1_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("env1-speed-cv", 52.0f, 58.0f), module, Evocation::SPEED_1_INPUT));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env1-invert", 62.0f, 50.0f), module, Evocation::INVERT_1_PARAM));
        addChild(createLightCentered<MediumLight<YellowLight>>(centerPx("env1-invert", 62.0f, 50.0f), module, Evocation::INVERT_1_LIGHT));
        Vec env1OutCenter = centerPx("env1-out", 72.0f, 50.0f);
        auto* env1Port = createOutputCentered<ShapetakerBNCPort>(env1OutCenter, module, Evocation::ENV_1_OUTPUT);
        if (module) {
            auto* indicator = new OutputProgressIndicator(module, 0);
            Vec pad = Vec(4.0f, 4.0f);
            indicator->box.size = Vec(env1Port->box.size.x + pad.x * 2.0f, env1Port->box.size.y + pad.y * 2.0f);
            indicator->box.pos = Vec(env1Port->box.pos.x - pad.x, env1Port->box.pos.y - pad.y);
            addChild(indicator);
        }
        addOutput(env1Port);

        // Envelope 2 (MEDIUM)
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env2-loop", 32.0f, 72.0f), module, Evocation::LOOP_2_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>(centerPx("env2-loop", 32.0f, 72.0f), module, Evocation::LOOP_2_LIGHT));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("env2-speed", 52.0f, 72.0f), module, Evocation::SPEED_2_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("env2-speed-cv", 52.0f, 80.0f), module, Evocation::SPEED_2_INPUT));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env2-invert", 62.0f, 72.0f), module, Evocation::INVERT_2_PARAM));
        addChild(createLightCentered<MediumLight<YellowLight>>(centerPx("env2-invert", 62.0f, 72.0f), module, Evocation::INVERT_2_LIGHT));
        Vec env2OutCenter = centerPx("env2-out", 72.0f, 72.0f);
        auto* env2Port = createOutputCentered<ShapetakerBNCPort>(env2OutCenter, module, Evocation::ENV_2_OUTPUT);
        if (module) {
            auto* indicator = new OutputProgressIndicator(module, 1);
            Vec pad = Vec(4.0f, 4.0f);
            indicator->box.size = Vec(env2Port->box.size.x + pad.x * 2.0f, env2Port->box.size.y + pad.y * 2.0f);
            indicator->box.pos = Vec(env2Port->box.pos.x - pad.x, env2Port->box.pos.y - pad.y);
            addChild(indicator);
        }
        addOutput(env2Port);

        // Envelope 3 (SLOW)
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env3-loop", 32.0f, 94.0f), module, Evocation::LOOP_3_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>(centerPx("env3-loop", 32.0f, 94.0f), module, Evocation::LOOP_3_LIGHT));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("env3-speed", 52.0f, 94.0f), module, Evocation::SPEED_3_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("env3-speed-cv", 52.0f, 102.0f), module, Evocation::SPEED_3_INPUT));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env3-invert", 62.0f, 94.0f), module, Evocation::INVERT_3_PARAM));
        addChild(createLightCentered<MediumLight<YellowLight>>(centerPx("env3-invert", 62.0f, 94.0f), module, Evocation::INVERT_3_LIGHT));
        Vec env3OutCenter = centerPx("env3-out", 72.0f, 94.0f);
        auto* env3Port = createOutputCentered<ShapetakerBNCPort>(env3OutCenter, module, Evocation::ENV_3_OUTPUT);
        if (module) {
            auto* indicator = new OutputProgressIndicator(module, 2);
            Vec pad = Vec(4.0f, 4.0f);
            indicator->box.size = Vec(env3Port->box.size.x + pad.x * 2.0f, env3Port->box.size.y + pad.y * 2.0f);
            indicator->box.pos = Vec(env3Port->box.pos.x - pad.x, env3Port->box.pos.y - pad.y);
            addChild(indicator);
        }
        addOutput(env3Port);

        // Envelope 4 (PAD)
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env4-loop", 32.0f, 116.0f), module, Evocation::LOOP_4_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>(centerPx("env4-loop", 32.0f, 116.0f), module, Evocation::LOOP_4_LIGHT));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("env4-speed", 52.0f, 116.0f), module, Evocation::SPEED_4_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("env4-speed-cv", 52.0f, 124.0f), module, Evocation::SPEED_4_INPUT));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("env4-invert", 62.0f, 116.0f), module, Evocation::INVERT_4_PARAM));
        addChild(createLightCentered<MediumLight<YellowLight>>(centerPx("env4-invert", 62.0f, 116.0f), module, Evocation::INVERT_4_LIGHT));
        Vec env4OutCenter = centerPx("env4-out", 72.0f, 116.0f);
        auto* env4Port = createOutputCentered<ShapetakerBNCPort>(env4OutCenter, module, Evocation::ENV_4_OUTPUT);
        if (module) {
            auto* indicator = new OutputProgressIndicator(module, 3);
            Vec pad = Vec(4.0f, 4.0f);
            indicator->box.size = Vec(env4Port->box.size.x + pad.x * 2.0f, env4Port->box.size.y + pad.y * 2.0f);
            indicator->box.pos = Vec(env4Port->box.pos.x - pad.x, env4Port->box.pos.y - pad.y);
            addChild(indicator);
        }
        addOutput(env4Port);

        // Gate output
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("gate-out", 65.0f, 118.0f), module, Evocation::GATE_OUTPUT));
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
