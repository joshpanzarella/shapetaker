#include "plugin.hpp"
#include <vector>
#include <algorithm>

// Forward declaration
struct Evocation;

// Custom Touch Strip Widget - Declaration only
struct TouchStripWidget : Widget {
    Evocation* module;
    
    // Visual properties
    Vec stripSize = Vec(68, 188);
    Vec currentTouchPos = Vec(0, 0);
    bool isDragging = false;
    bool showTouch = false;
    
    // Animation and visual effects
    float glowIntensity = 0.0f;
    std::vector<Vec> sparkles;
    float sparkleTimer = 0.0f;
    
    TouchStripWidget(Evocation* module);
    
    // Method declarations only - implementations after Evocation class
    void onButton(const event::Button& e) override;
    void onDragStart(const event::DragStart& e) override;
    void onDragMove(const event::DragMove& e) override;
    void onDragEnd(const event::DragEnd& e) override;
    void addEnvelopePoint(Vec pos);
    void createSparkle(Vec pos);
    void step() override;
    void drawLayer(const DrawArgs& args, int layer) override;
    void drawTouchStrip(const DrawArgs& args);
    void drawBackground(const DrawArgs& args);
    void drawEnvelope(const DrawArgs& args);
    void drawCurrentTouch(const DrawArgs& args);
    void drawSparkles(const DrawArgs& args);
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
    
    Evocation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(RECORD_PARAM, 0.f, 1.f, 0.f, "Record");
        configParam(TRIGGER_PARAM, 0.f, 1.f, 0.f, "Manual Trigger");
        configParam(CLEAR_PARAM, 0.f, 1.f, 0.f, "Clear Buffer");
        configParam(SPEED_1_PARAM, 0.1f, 8.0f, 1.0f, "Speed 1", "×");
        configParam(SPEED_2_PARAM, 0.1f, 8.0f, 2.0f, "Speed 2", "×");
        configParam(SPEED_3_PARAM, 0.1f, 8.0f, 4.0f, "Speed 3", "×");
        configParam(SPEED_4_PARAM, 0.1f, 8.0f, 8.0f, "Speed 4", "×");
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
        // Handle triggers
        bool triggerPressed = triggerTrigger.process(params[TRIGGER_PARAM].getValue() + inputs[TRIGGER_INPUT].getVoltage());
        bool clearPressed = clearTrigger.process(params[CLEAR_PARAM].getValue() + inputs[CLEAR_INPUT].getVoltage());
        bool recordPressed = recordTrigger.process(params[RECORD_PARAM].getValue());
        
        // Handle individual loop button toggles
        for (int i = 0; i < 4; i++) {
            if (loopTriggers[i].process(params[LOOP_1_PARAM + i].getValue())) {
                loopStates[i] = !loopStates[i];
            }
        }
        
        // Handle invert button toggles
        for (int i = 0; i < 4; i++) {
            if (invertTriggers[i].process(params[INVERT_1_PARAM + i].getValue())) {
                invertStates[i] = !invertStates[i];
            }
        }
        
        // Handle clear
        if (clearPressed) {
            clearBuffer();
        }
        
        // Handle recording
        if (recordPressed) {
            if (!isRecording && !bufferHasData) {
                startRecording();
            } else if (isRecording) {
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
        if (bufferHasData) return; // Don't start if buffer already has data
        
        isRecording = true;
        envelope.clear();
        recordingTime = 0.0f;
        bufferHasData = false;
    }
    
    void stopRecording() {
        if (isRecording && !envelope.empty()) {
            isRecording = false;
            normalizeEnvelopeTiming();
            bufferHasData = true;
        }
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
    
    void addEnvelopePointFromWidget(float x, float y) {
        if (!isRecording) return;
        
        recordingTime += 0.016f; // Approximate 60fps update
        float normalizedTime = recordingTime / maxRecordingTime;
        
        if (normalizedTime <= 1.0f) {
            addEnvelopePoint(x, y, normalizedTime);
        } else {
            stopRecording();
        }
    }
    
    void normalizeEnvelopeTiming() {
        if (envelope.size() < 2) return;
        
        // Ensure time values are properly distributed 0-1
        for (size_t i = 0; i < envelope.size(); i++) {
            envelope[i].time = (float)i / (envelope.size() - 1);
        }
    }
    
    void clearBuffer() {
        envelope.clear();
        bufferHasData = false;
        isRecording = false;
        stopAllPlayback();
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
    
    float getEnvelopeDuration() {
        return 2.0f; // 2 second base duration
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
        
        return rootJ;
    }
    
    void dataFromJson(json_t* rootJ) override {
        json_t* bufferHasDataJ = json_object_get(rootJ, "bufferHasData");
        if (bufferHasDataJ) bufferHasData = json_boolean_value(bufferHasDataJ);
        
        // Load individual loop states
        json_t* loopStatesJ = json_object_get(rootJ, "loopStates");
        if (loopStatesJ) {
            for (int i = 0; i < 4 && i < json_array_size(loopStatesJ); i++) {
                json_t* loopJ = json_array_get(loopStatesJ, i);
                if (loopJ) loopStates[i] = json_boolean_value(loopJ);
            }
        }
        
        // Load invert states
        json_t* invertStatesJ = json_object_get(rootJ, "invertStates");
        if (invertStatesJ) {
            for (int i = 0; i < 4 && i < json_array_size(invertStatesJ); i++) {
                json_t* invertJ = json_array_get(invertStatesJ, i);
                if (invertJ) invertStates[i] = json_boolean_value(invertJ);
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
    }
};

// TouchStripWidget Method Implementations (after Evocation is fully defined)
TouchStripWidget::TouchStripWidget(Evocation* module) {
    this->module = module;
    box.size = stripSize;
}

void TouchStripWidget::onButton(const event::Button& e) {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
        if (!module) return;
        
        // Start recording
        isDragging = true;
        showTouch = true;
        currentTouchPos = e.pos;
        
        // Clamp to widget bounds
        currentTouchPos.x = clamp(currentTouchPos.x, 0.0f, box.size.x);
        currentTouchPos.y = clamp(currentTouchPos.y, 0.0f, box.size.y);
        
        // Start recording in module
        module->startRecording();
        
        // Add first point
        addEnvelopePoint(currentTouchPos);
        
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
    
    currentTouchPos = currentTouchPos.plus(e.mouseDelta);
    
    // Clamp to widget bounds
    currentTouchPos.x = clamp(currentTouchPos.x, 0.0f, box.size.x);
    currentTouchPos.y = clamp(currentTouchPos.y, 0.0f, box.size.y);
    
    // Add envelope point
    addEnvelopePoint(currentTouchPos);
    
    // Create sparkle effect
    createSparkle(currentTouchPos);
}

void TouchStripWidget::onDragEnd(const event::DragEnd& e) {
    if (!module) return;
    
    isDragging = false;
    showTouch = false;
    glowIntensity = 0.0f;
    
    // Stop recording in module
    module->stopRecording();
}

void TouchStripWidget::addEnvelopePoint(Vec pos) {
    if (!module) return;
    
    // Convert widget coordinates to normalized 0-1 values
    float normalizedX = pos.x / box.size.x;
    float normalizedY = 1.0f - (pos.y / box.size.y); // Invert Y so top = 1
    
    // Add point to module's envelope
    module->addEnvelopePointFromWidget(normalizedX, normalizedY);
}

void TouchStripWidget::createSparkle(Vec pos) {
    sparkles.push_back(pos);
    // Limit sparkle count
    if (sparkles.size() > 20) {
        sparkles.erase(sparkles.begin());
    }
}

void TouchStripWidget::step() {
    Widget::step();
    
    // Update sparkle timer
    sparkleTimer += APP->engine->getSampleTime();
    
    // Fade sparkles
    if (sparkleTimer > 0.1f) { // Update every 100ms
        sparkleTimer = 0.0f;
        if (!sparkles.empty()) {
            sparkles.erase(sparkles.begin());
        }
    }
    
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
    
    // Draw sparkles
    drawSparkles(args);
    
    // Draw border and recording glow
    drawBorder(args);
    
    // Draw instructions if no data
    if (!module || (!module->bufferHasData && !module->isRecording)) {
        drawInstructions(args);
    }
    
    nvgRestore(args.vg);
}

void TouchStripWidget::drawBackground(const DrawArgs& args) {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8);
    
    // Gradient background
    NVGpaint gradient = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y,
        nvgRGBA(20, 20, 40, 180), nvgRGBA(40, 20, 60, 180));
    nvgFillPaint(args.vg, gradient);
    nvgFill(args.vg);
    
    // Grid lines for reference
    nvgStrokeColor(args.vg, nvgRGBA(100, 100, 150, 30));
    nvgStrokeWidth(args.vg, 1.0f);
    
    // Horizontal lines
    for (int i = 1; i < 4; i++) {
        float y = (box.size.y / 4.0f) * i;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, y);
        nvgLineTo(args.vg, box.size.x, y);
        nvgStroke(args.vg);
    }
    
    // Vertical lines
    for (int i = 1; i < 4; i++) {
        float x = (box.size.x / 4.0f) * i;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x, 0);
        nvgLineTo(args.vg, x, box.size.y);
        nvgStroke(args.vg);
    }
}

void TouchStripWidget::drawEnvelope(const DrawArgs& args) {
    if (!module || module->envelope.empty()) return;
    
    nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, 200));
    nvgStrokeWidth(args.vg, 3.0f);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);
    
    // Draw glow effect
    nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
    nvgStrokeWidth(args.vg, 6.0f);
    nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, 60));
    
    nvgBeginPath(args.vg);
    bool first = true;
    for (const auto& point : module->envelope) {
        float x = point.x * box.size.x;
        float y = (1.0f - point.y) * box.size.y;
        
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
    nvgStrokeWidth(args.vg, 3.0f);
    nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, 255));
    
    nvgBeginPath(args.vg);
    first = true;
    for (const auto& point : module->envelope) {
        float x = point.x * box.size.x;
        float y = (1.0f - point.y) * box.size.y;
        
        if (first) {
            nvgMoveTo(args.vg, x, y);
            first = false;
        } else {
            nvgLineTo(args.vg, x, y);
        }
    }
    nvgStroke(args.vg);
    
    // Draw envelope points as dots
    nvgFillColor(args.vg, nvgRGBA(0, 255, 170, 255));
    for (const auto& point : module->envelope) {
        float x = point.x * box.size.x;
        float y = (1.0f - point.y) * box.size.y;
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, x, y, 2.5f);
        nvgFill(args.vg);
    }
}

void TouchStripWidget::drawCurrentTouch(const DrawArgs& args) {
    // Main touch indicator
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, currentTouchPos.x, currentTouchPos.y, 8);
    
    NVGpaint touchGradient = nvgRadialGradient(args.vg, 
        currentTouchPos.x, currentTouchPos.y, 0, 15,
        nvgRGBA(0, 255, 170, 200), nvgRGBA(0, 255, 170, 0));
    nvgFillPaint(args.vg, touchGradient);
    nvgFill(args.vg);
    
    // Inner bright circle
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, currentTouchPos.x, currentTouchPos.y, 4);
    nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 255));
    nvgFill(args.vg);
    
    // Pulse ring - using system time for animation
    float pulseRadius = 12 + sin(system::getTime() * 8) * 4;
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, currentTouchPos.x, currentTouchPos.y, pulseRadius);
    nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, 100));
    nvgStrokeWidth(args.vg, 2.0f);
    nvgStroke(args.vg);
}

void TouchStripWidget::drawSparkles(const DrawArgs& args) {
    for (size_t i = 0; i < sparkles.size(); i++) {
        float age = (float)i / sparkles.size();
        float alpha = (1.0f - age) * 255;
        float size = (1.0f - age) * 3 + 1;
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, sparkles[i].x, sparkles[i].y, size);
        nvgFillColor(args.vg, nvgRGBA(100, 200, 255, alpha));
        nvgFill(args.vg);
    }
}

void TouchStripWidget::drawBorder(const DrawArgs& args) {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2, 8);
    
    if (module && module->isRecording) {
        // Recording glow
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, 255));
        nvgStrokeWidth(args.vg, 3.0f);
        
        // Animated glow - using system time
        float glow = 0.5f + 0.5f * sin(system::getTime() * 6);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, glow * 100));
        nvgStrokeWidth(args.vg, 8.0f);
        nvgStroke(args.vg);
        
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 170, 255));
        nvgStrokeWidth(args.vg, 2.0f);
    } else {
        // Normal border
        nvgStrokeColor(args.vg, nvgRGBA(100, 150, 200, 100));
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

// Main Widget
struct EvocationWidget : ModuleWidget {
    TouchStripWidget* touchStrip;
    
    EvocationWidget(Evocation* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Evocation.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Touch strip widget - updated for new panel size
        touchStrip = new TouchStripWidget(module);
        touchStrip->box.pos = Vec(mm2px(8), mm2px(15)); // Position matches new panel
        addChild(touchStrip);

        // Main control buttons - spread out horizontally
        addParam(createParamCentered<VCVButton>(mm2px(Vec(42, 22)), module, Evocation::RECORD_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(50, 22)), module, Evocation::TRIGGER_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(58, 22)), module, Evocation::CLEAR_PARAM));

        // Main inputs - right side
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(50, 47)), module, Evocation::TRIGGER_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(70, 47)), module, Evocation::CLEAR_INPUT));

        // Envelope Output 1 (top left) - much more space
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(40, 72)), module, Evocation::SPEED_1_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40, 80)), module, Evocation::SPEED_1_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(40, 88)), module, Evocation::ENV_1_OUTPUT));
        addParam(createParamCentered<CKSS>(mm2px(Vec(35, 72)), module, Evocation::LOOP_1_PARAM));
        addParam(createParamCentered<VCVBezel>(mm2px(Vec(47, 72)), module, Evocation::INVERT_1_PARAM));
        
        // Envelope Output 2 (top right)
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(80, 72)), module, Evocation::SPEED_2_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(80, 80)), module, Evocation::SPEED_2_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(80, 88)), module, Evocation::ENV_2_OUTPUT));
        addParam(createParamCentered<CKSS>(mm2px(Vec(75, 72)), module, Evocation::LOOP_2_PARAM));
        addParam(createParamCentered<VCVBezel>(mm2px(Vec(87, 72)), module, Evocation::INVERT_2_PARAM));
        
        // Envelope Output 3 (bottom left) 
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(40, 105)), module, Evocation::SPEED_3_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40, 113)), module, Evocation::SPEED_3_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(40, 121)), module, Evocation::ENV_3_OUTPUT));
        addParam(createParamCentered<CKSS>(mm2px(Vec(35, 105)), module, Evocation::LOOP_3_PARAM));
        addParam(createParamCentered<VCVBezel>(mm2px(Vec(47, 105)), module, Evocation::INVERT_3_PARAM));
        
        // Envelope Output 4 (bottom right)
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(80, 105)), module, Evocation::SPEED_4_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(80, 113)), module, Evocation::SPEED_4_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(80, 121)), module, Evocation::ENV_4_OUTPUT));
        addParam(createParamCentered<CKSS>(mm2px(Vec(75, 105)), module, Evocation::LOOP_4_PARAM));
        addParam(createParamCentered<VCVBezel>(mm2px(Vec(87, 105)), module, Evocation::INVERT_4_PARAM));

        // Gate output - centered
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(60, 110)), module, Evocation::GATE_OUTPUT));

        // Status lights - updated positions for new layout
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(37, 19)), module, Evocation::RECORDING_LIGHT));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(63, 19)), module, Evocation::TRIGGER_LIGHT));
        
        // Individual loop lights - positioned with toggle switches
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(35, 72)), module, Evocation::LOOP_1_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(75, 72)), module, Evocation::LOOP_2_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(35, 105)), module, Evocation::LOOP_3_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(75, 105)), module, Evocation::LOOP_4_LIGHT));
        
        // Invert lights - positioned with invert buttons  
        addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(47, 72)), module, Evocation::INVERT_1_LIGHT));
        addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(87, 72)), module, Evocation::INVERT_2_LIGHT));
        addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(47, 105)), module, Evocation::INVERT_3_LIGHT));
        addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(87, 105)), module, Evocation::INVERT_4_LIGHT));
    }
};

// Model registration
Model* modelEvocation = createModel<Evocation, EvocationWidget>("Evocation");