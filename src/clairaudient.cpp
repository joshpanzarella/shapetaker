#include "plugin.hpp"
#include "transmutation/ui.hpp" // for PanelPatinaOverlay (shared vintage overlay)
#include <cmath>
#include <atomic>

// (Removed: decorative HexagonWidget overlays)

// Shadow behavior now lives in the base Shapetaker knob classes (plugin.hpp)


struct ClairaudientModule : Module, IOscilloscopeSource {
    
    // Quantize voltage to discrete octave steps for oscillator V
    float quantizeToOctave(float voltage) {
        // Clamp to -2 to +2 octaves, then round to nearest octave
        float clamped = clamp(voltage, -2.0f, 2.0f);
        return std::round(clamped);
    }

    // Quantize to semitones within 4 octave range for oscillator Z
    float quantizeToSemitone(float semitones) {
        // Clamp to -24 to +24 semitones range (4 octaves)
        float clamped = clamp(semitones, -24.0f, 24.0f);
        // Round to nearest semitone and convert to voltage (octaves)
        float quantizedSemitones = std::round(clamped);
        return quantizedSemitones / 12.0f;
    }
    enum ParamId {
        FREQ1_PARAM,
        FREQ2_PARAM,
        FINE1_PARAM,
        FINE2_PARAM,
        FINE1_ATTEN_PARAM,
        FINE2_ATTEN_PARAM,
        SHAPE1_PARAM,
        SHAPE2_PARAM,
        SHAPE1_ATTEN_PARAM,
        SHAPE2_ATTEN_PARAM,
        XFADE_PARAM,
        XFADE_ATTEN_PARAM,
        SYNC1_PARAM,
        SYNC2_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        VOCT1_INPUT,
        VOCT2_INPUT,
        FINE1_CV_INPUT,
        FINE2_CV_INPUT,
        SHAPE1_CV_INPUT,
        SHAPE2_CV_INPUT,
        XFADE_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        LEFT_OUTPUT,
        RIGHT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    // Polyphonic oscillator state (up to 6 voices)
    static const int MAX_POLY_VOICES = 6;
    float phase1A[MAX_POLY_VOICES] = {};  // Independent phase for osc 1A per voice
    float phase1B[MAX_POLY_VOICES] = {};  // Independent phase for osc 1B per voice
    float phase2A[MAX_POLY_VOICES] = {};  // Independent phase for osc 2A per voice
    float phase2B[MAX_POLY_VOICES] = {};  // Independent phase for osc 2B per voice
    
    // Organic variation state per voice
    float drift1A[MAX_POLY_VOICES] = {};
    float drift1B[MAX_POLY_VOICES] = {};
    float drift2A[MAX_POLY_VOICES] = {};
    float drift2B[MAX_POLY_VOICES] = {};
    float noise1A[MAX_POLY_VOICES] = {};
    float noise1B[MAX_POLY_VOICES] = {};
    float noise2A[MAX_POLY_VOICES] = {};
    float noise2B[MAX_POLY_VOICES] = {};
    
    // Simple one-pole low-pass filter for anti-aliasing
    // --- Oscilloscope Buffering ---
    static const int OSCILLOSCOPE_BUFFER_SIZE = 1024;
    Vec oscilloscopeBuffer[OSCILLOSCOPE_BUFFER_SIZE] = {};
    std::atomic<int> oscilloscopeBufferIndex = {0};
    int oscilloscopeFrameCounter = 0;

    struct OnePoleFilter {
        float z1 = 0.f;
        
        float process(float input, float cutoff, float sampleRate) {
            float dt = 1.f / sampleRate;
            float RC = 1.f / (2.f * M_PI * cutoff);
            float alpha = dt / (RC + dt);
            z1 = z1 + alpha * (input - z1);
            return z1;
        }
    };
    
    // Anti-aliasing filters per voice
    OnePoleFilter antiAliasFilterLeft[MAX_POLY_VOICES];
    OnePoleFilter antiAliasFilterRight[MAX_POLY_VOICES];

    // Quantization mode settings
    bool quantizeOscV = true;  // V oscillator quantized to octaves by default
    bool quantizeOscZ = true;  // Z oscillator quantized to semitones by default

    // Update parameter snapping based on quantization modes
    void updateParameterSnapping() {
        // V Oscillator snapping
        getParamQuantity(FREQ1_PARAM)->snapEnabled = quantizeOscV;
        getParamQuantity(FREQ1_PARAM)->smoothEnabled = !quantizeOscV;

        // Z Oscillator snapping
        getParamQuantity(FREQ2_PARAM)->snapEnabled = quantizeOscZ;
        getParamQuantity(FREQ2_PARAM)->smoothEnabled = !quantizeOscZ;
    }

    ClairaudientModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Frequency controls
        // V oscillator snaps to whole octaves (5 total values: -2, -1, 0, +1, +2)
        configParam(FREQ1_PARAM, -2.f, 2.f, 0.f, "V Oscillator Octave", " oct");

        // Z oscillator snaps to semitones (49 total values: -24 to +24 semitones)
        configParam(FREQ2_PARAM, -24.f, 24.f, 0.f, "Z Oscillator Semitone", " st");

        // Initialize parameter snapping based on default quantization modes
        updateParameterSnapping();

        // FREQ1: Quantized to discrete octave steps (-2, -1, 0, +1, +2)
        // FREQ2: Quantized to semitones within 4-octave range for musical intervals
        
        // Fine tune controls (±20 cents, centered at 0 for no detune)
        configParam(FINE1_PARAM, -0.2f, 0.2f, 0.f, "V Fine Tune", "cents", 0.f, 0.f, 100.f);
        configParam(FINE2_PARAM, -0.2f, 0.2f, 0.f, "Z Fine Tune", "cents", 0.f, 0.f, 100.f);
        
        // Fine tune CV attenuverters
        configParam(FINE1_ATTEN_PARAM, -1.f, 1.f, 0.f, "V Fine Tune CV Amount", "%", 0.f, 100.f);
        configParam(FINE2_ATTEN_PARAM, -1.f, 1.f, 0.f, "Z Fine Tune CV Amount", "%", 0.f, 100.f);
        
        // Shape morphing controls (default to 50% for proper sigmoid)
        configParam(SHAPE1_PARAM, 0.f, 1.f, 0.5f, "V Shape", "%", 0.f, 100.f);
        configParam(SHAPE2_PARAM, 0.f, 1.f, 0.5f, "Z Shape", "%", 0.f, 100.f);
        
        // Shape CV attenuverters
        configParam(SHAPE1_ATTEN_PARAM, -1.f, 1.f, 0.f, "V Shape CV Amount", "%", 0.f, 100.f);
        configParam(SHAPE2_ATTEN_PARAM, -1.f, 1.f, 0.f, "Z Shape CV Amount", "%", 0.f, 100.f);
        
        // Crossfade control (centered at 0.5)
        configParam(XFADE_PARAM, 0.f, 1.f, 0.5f, "Crossfade", "%", 0.f, 100.f);
        
        // Crossfade CV attenuverter
        configParam(XFADE_ATTEN_PARAM, -1.f, 1.f, 0.f, "Crossfade CV Amount", "%", 0.f, 100.f);
        
        // Sync switches (default off for independent beating)
        configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "V Sync", {"Independent", "Synced"});
        configSwitch(SYNC2_PARAM, 0.f, 1.f, 0.f, "Z Sync", {"Independent", "Synced"});
        
        // Inputs
        configInput(VOCT1_INPUT, "V Oscillator V/Oct");
        configInput(VOCT2_INPUT, "Z Oscillator V/Oct");
        configInput(FINE1_CV_INPUT, "V Fine Tune CV");
        configInput(FINE2_CV_INPUT, "Z Fine Tune CV");
        configInput(SHAPE1_CV_INPUT, "V Shape CV");
        configInput(SHAPE2_CV_INPUT, "Z Shape CV");
        configInput(XFADE_CV_INPUT, "Crossfade CV");
        
        // Outputs
        configOutput(LEFT_OUTPUT, "Left");
        configOutput(RIGHT_OUTPUT, "Right");
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "quantizeOscV", json_boolean(quantizeOscV));
        json_object_set_new(rootJ, "quantizeOscZ", json_boolean(quantizeOscZ));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* quantizeVJ = json_object_get(rootJ, "quantizeOscV");
        if (quantizeVJ)
            quantizeOscV = json_boolean_value(quantizeVJ);

        json_t* quantizeZJ = json_object_get(rootJ, "quantizeOscZ");
        if (quantizeZJ)
            quantizeOscZ = json_boolean_value(quantizeZJ);

        // Update parameter snapping after loading settings
        updateParameterSnapping();
    }

    void process(const ProcessArgs& args) override {
        // Determine number of polyphonic voices (max 6)
        int channels = std::min(std::max(inputs[VOCT1_INPUT].getChannels(), 1), MAX_POLY_VOICES);
        
        // Set output channels to match
        outputs[LEFT_OUTPUT].setChannels(channels);
        outputs[RIGHT_OUTPUT].setChannels(channels);
        
        // 2x oversampling for smoother sound
        const int oversample = 2;
        float oversampleRate = args.sampleRate * oversample;

        // Process each voice
        for (int ch = 0; ch < channels; ch++) {
            float finalLeft = 0.f;
            float finalRight = 0.f;

            // --- Pre-calculate parameters for this voice ---
            // Get V/Oct inputs with fallback logic
            float voct1 = inputs[VOCT1_INPUT].getVoltage(ch);
            float voct2 = inputs[VOCT2_INPUT].isConnected() ? 
                            inputs[VOCT2_INPUT].getPolyVoltage(ch) : voct1;
            
            // Get parameters for this voice
            // V Oscillator: Apply quantization if enabled
            float rawPitch1 = params[FREQ1_PARAM].getValue() + voct1;
            float pitch1 = quantizeOscV ? quantizeToOctave(rawPitch1) : rawPitch1;

            // Z Oscillator: Apply quantization if enabled
            float semitonesZ = params[FREQ2_PARAM].getValue(); // Already in semitone units
            float rawPitch2 = semitonesZ / 12.0f + voct2; // Convert semitones to octaves and add V/Oct
            float pitch2 = quantizeOscZ ? quantizeToSemitone(semitonesZ + voct2 * 12.0f) : rawPitch2;
            
            float fineTune1 = params[FINE1_PARAM].getValue();
            if (inputs[FINE1_CV_INPUT].isConnected()) {
                float cvAmount = params[FINE1_ATTEN_PARAM].getValue();
                fineTune1 = clamp(fineTune1 + inputs[FINE1_CV_INPUT].getPolyVoltage(ch) * cvAmount / 50.f, -0.2f, 0.2f);
            }
            
            float fineTune2 = params[FINE2_PARAM].getValue();
            if (inputs[FINE2_CV_INPUT].isConnected()) {
                float cvAmount = params[FINE2_ATTEN_PARAM].getValue();
                fineTune2 = clamp(fineTune2 + inputs[FINE2_CV_INPUT].getPolyVoltage(ch) * cvAmount / 50.f, -0.2f, 0.2f);
            }

            // Convert cents to octaves
            fineTune1 /= 12.f;
            fineTune2 /= 12.f;

            // Get shape parameters with attenuverters
            float shape1 = params[SHAPE1_PARAM].getValue();
            if (inputs[SHAPE1_CV_INPUT].isConnected()) {
                float cvAmount = params[SHAPE1_ATTEN_PARAM].getValue();
                shape1 = clamp(shape1 + inputs[SHAPE1_CV_INPUT].getPolyVoltage(ch) * cvAmount / 10.f, 0.f, 1.f);
            }
            
            float shape2 = params[SHAPE2_PARAM].getValue();
            if (inputs[SHAPE2_CV_INPUT].isConnected()) {
                float cvAmount = params[SHAPE2_ATTEN_PARAM].getValue();
                shape2 = clamp(shape2 + inputs[SHAPE2_CV_INPUT].getPolyVoltage(ch) * cvAmount / 10.f, 0.f, 1.f);
            }

            // Get crossfade parameter with attenuverter
            float xfade = params[XFADE_PARAM].getValue();
            if (inputs[XFADE_CV_INPUT].isConnected()) {
                float cvAmount = params[XFADE_ATTEN_PARAM].getValue();
                xfade = clamp(xfade + inputs[XFADE_CV_INPUT].getPolyVoltage(ch) * cvAmount / 10.f, 0.f, 1.f);
            }
            
            for (int os = 0; os < oversample; os++) {
                // Add organic frequency drift (very subtle) for this voice
                updateOrganicDrift(ch, args.sampleTime * oversample);
                
                float freq1A = 261.626f * std::pow(2.f, pitch1 + drift1A[ch]);
                float freq1B = 261.626f * std::pow(2.f, pitch1 + fineTune1 + drift1B[ch]);
                float freq2A = 261.626f * std::pow(2.f, pitch2 + drift2A[ch]);
                float freq2B = 261.626f * std::pow(2.f, pitch2 + fineTune2 + drift2B[ch]);
                
                // Check sync switches
                bool sync1 = params[SYNC1_PARAM].getValue() > 0.5f;
                bool sync2 = params[SYNC2_PARAM].getValue() > 0.5f;
                
                // Update phases (with sync logic) for this voice
                float deltaPhase1A = freq1A / oversampleRate; // Master oscillator increment
                float deltaPhase1B = freq1B / oversampleRate; // Slave runs at its own frequency
                float deltaPhase2A = freq2A / oversampleRate; // Master oscillator increment
                float deltaPhase2B = freq2B / oversampleRate; // Slave runs at its own frequency
                
                // Add subtle phase noise for organic character
                phase1A[ch] += deltaPhase1A + noise1A[ch] * 0.00001f;
                phase1B[ch] += deltaPhase1B + noise1B[ch] * 0.00001f;
                phase2A[ch] += deltaPhase2A + noise2A[ch] * 0.00001f;
                phase2B[ch] += deltaPhase2B + noise2B[ch] * 0.00001f;
                
                if (phase1A[ch] >= 1.f) phase1A[ch] -= 1.f;
                if (phase1B[ch] >= 1.f) phase1B[ch] -= 1.f;
                if (phase2A[ch] >= 1.f) phase2A[ch] -= 1.f;
                if (phase2B[ch] >= 1.f) phase2B[ch] -= 1.f;
                
                // Apply sync - reset B oscillator to match A when synced
                if (sync1 && phase1A[ch] < deltaPhase1A) {
                    phase1B[ch] = phase1A[ch];
                }
                if (sync2 && phase2A[ch] < deltaPhase2A) {
                    phase2B[ch] = phase2A[ch];
                }
                
                // Generate sigmoid-morphed oscillators with anti-aliasing
                float osc1A = generateOrganicOsc(phase1A[ch], shape1, freq1A, oversampleRate);
                float osc1B = generateOrganicOsc(phase1B[ch], shape1, freq1B, oversampleRate);
                float osc2A = generateOrganicOsc(phase2A[ch], shape2, freq2A, oversampleRate);
                float osc2B = generateOrganicOsc(phase2B[ch], shape2, freq2B, oversampleRate);
                
                // Crossfade between pairs for each channel (A for Left, B for Right)
                float leftOutput = osc1A * (1.f - xfade) + osc2A * xfade;
                float rightOutput = osc1B * (1.f - xfade) + osc2B * xfade;

                // Apply anti-aliasing filter to each channel separately for true stereo
                float filteredLeft = antiAliasFilterLeft[ch].process(leftOutput, args.sampleRate * 0.45f, oversampleRate);
                float filteredRight = antiAliasFilterRight[ch].process(rightOutput, args.sampleRate * 0.45f, oversampleRate);

                finalLeft += filteredLeft;
                finalRight += filteredRight;
            }
            
            // Average the oversampled result for this voice
            float outL = std::tanh(finalLeft / oversample) * 5.f;
            float outR = std::tanh(finalRight / oversample) * 5.f;

            outputs[LEFT_OUTPUT].setVoltage(outL, ch);
            outputs[RIGHT_OUTPUT].setVoltage(outR, ch);
            
            // Use first voice for oscilloscope display
            if (ch == 0) {
                // --- Adaptive Oscilloscope Timescale ---
                // Determine the dominant frequency based on the crossfader position
                float baseFreq1 = 261.626f * std::pow(2.f, pitch1);
                float baseFreq2 = 261.626f * std::pow(2.f, pitch2);
                float dominantFreq = (xfade < 0.5f) ? baseFreq1 : baseFreq2;
                dominantFreq = std::max(dominantFreq, 1.f); // Prevent division by zero or very small numbers

                const float targetCyclesInDisplay = 2.f; // Aim to show this many waveform cycles
                int downsampleFactor = (int)roundf((targetCyclesInDisplay * args.sampleRate) / (OSCILLOSCOPE_BUFFER_SIZE * dominantFreq));
                downsampleFactor = clamp(downsampleFactor, 1, 256); // Clamp to a reasonable range
                
                // --- Oscilloscope Buffering Logic ---
                // Downsample the audio rate to fill the buffer at a reasonable speed for the UI
                oscilloscopeFrameCounter++;
                if (oscilloscopeFrameCounter >= downsampleFactor) {
                    oscilloscopeFrameCounter = 0;
                    
                    int currentIndex = oscilloscopeBufferIndex.load();
                    // Store the current output voltages in the circular buffer
                    oscilloscopeBuffer[currentIndex] = Vec(outL, outR);
                    oscilloscopeBufferIndex.store((currentIndex + 1) % OSCILLOSCOPE_BUFFER_SIZE);
                }
            }
        }
    }

    // --- IOscilloscopeSource Implementation ---
    const Vec* getOscilloscopeBuffer() const override { return oscilloscopeBuffer; }
    int getOscilloscopeBufferIndex() const override { return oscilloscopeBufferIndex.load(); }
    int getOscilloscopeBufferSize() const override { return OSCILLOSCOPE_BUFFER_SIZE; }

private:
    // Update organic drift and noise for more natural sound (per voice)
    void updateOrganicDrift(int voice, float sampleTime) {
        // Very slow random walk for frequency drift (like analog oscillator aging)
        static float driftSpeed = 0.00002f;
        
        drift1A[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift1B[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift2A[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift2B[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        
        // Limit drift to very small amounts (±0.1 cents)
        drift1A[voice] = clamp(drift1A[voice], -0.001f, 0.001f);
        drift1B[voice] = clamp(drift1B[voice], -0.001f, 0.001f);
        drift2A[voice] = clamp(drift2A[voice], -0.001f, 0.001f);
        drift2B[voice] = clamp(drift2B[voice], -0.001f, 0.001f);
        
        // Generate subtle phase noise
        noise1A[voice] = (rack::random::uniform() - 0.5f) * 2.f;
        noise1B[voice] = (rack::random::uniform() - 0.5f) * 2.f;
        noise2A[voice] = (rack::random::uniform() - 0.5f) * 2.f;
        noise2B[voice] = (rack::random::uniform() - 0.5f) * 2.f;
    }
    
    // Generate organic-sounding sigmoid oscillator with subtle imperfections
    float generateOrganicOsc(float phase, float shape, float freq, float sampleRate) {
        // Linear sawtooth (when shape = 0)
        float linearSaw = 2.f * phase - 1.f;
        
        if (shape < 0.001f) {
            // Add tiny amount of saturation even to linear saw
            return std::tanh(linearSaw * 1.02f) * 0.98f;
        }
        
        // Sigmoid curve with subtle harmonic variations
        float range = 3.f + shape * 5.f; // Variable steepness
        float sigmoidInput = (phase - 0.5f) * range * 2.f;
        
        // Add subtle harmonics based on frequency (like analog oscillator nonlinearities)
        float harmonicBias = std::sin(phase * 2.f * M_PI * 3.f) * 0.02f * shape;
        sigmoidInput += harmonicBias;
        
        float sigmoidOutput = std::tanh(sigmoidInput);
        
        // Blend between linear and sigmoid with subtle nonlinear crossfade
        float blend = shape + std::sin(phase * 2.f * M_PI) * 0.01f * shape;
        blend = clamp(blend, 0.f, 1.f);
        
        float result = linearSaw * (1.f - blend) + sigmoidOutput * blend;
        
        // Add very subtle high-frequency content for "air"
        float nyquist = sampleRate * 0.5f;
        if (freq < nyquist * 0.3f) { // Only add if we're not near Nyquist
            float air = std::sin(phase * 2.f * M_PI * 7.f) * 0.005f * shape;
            result += air;
        }
        
        // Subtle saturation for warmth
        return std::tanh(result * 1.05f) * 0.95f;
    }
};

struct ClairaudientWidget : ModuleWidget {
    // Draw panel background texture to match Transmutation
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/vcv-panel-background.png"));
        if (bg) {
            NVGpaint paint = nvgImagePattern(args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, bg->handle, 1.0f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        }
        ModuleWidget::draw(args);
    }

    ClairaudientWidget(ClairaudientModule* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Clairaudient.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Read positions from panel SVG by id, like Transmutation
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Clairaudient.svg");
        std::string svg;
        {
            std::ifstream f(svgPath);
            if (f) { std::stringstream ss; ss << f.rdbuf(); svg = ss.str(); }
        }
        auto findTagForId = [&](const std::string& id) -> std::string {
            if (svg.empty()) return "";
            std::string needle = "id=\"" + id + "\"";
            size_t pos = svg.find(needle);
            if (pos == std::string::npos) return "";
            size_t start = svg.rfind('<', pos);
            size_t end = svg.find('>', pos);
            if (start == std::string::npos || end == std::string::npos || end <= start) return "";
            return svg.substr(start, end - start + 1);
        };
        auto getAttr = [&](const std::string& tag, const std::string& key, float defVal) -> float {
            if (tag.empty()) return defVal;
            std::string k = key + "=\"";
            size_t p = tag.find(k);
            if (p == std::string::npos) return defVal;
            p += k.size();
            size_t q = tag.find('"', p);
            if (q == std::string::npos) return defVal;
            try { return std::stof(tag.substr(p, q - p)); } catch (...) { return defVal; }
        };
        auto centerFromId = [&](const std::string& id, float defx, float defy) -> Vec {
            std::string tag = findTagForId(id);
            if (tag.find("<rect") != std::string::npos) {
                float x = getAttr(tag, "x", defx);
                float y = getAttr(tag, "y", defy);
                float w = getAttr(tag, "width", 0.f);
                float h = getAttr(tag, "height", 0.f);
                return Vec(x + w * 0.5f, y + h * 0.5f);
            }
            float cx = getAttr(tag, "cx", defx);
            float cy = getAttr(tag, "cy", defy);
            return Vec(cx, cy);
        };

        // V/Z oscillator large frequency knobs
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(centerFromId("freq_v", 13.422475f, 25.464647f)), module, ClairaudientModule::FREQ1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(centerFromId("freq_z", 68.319061f, 25.695415f)), module, ClairaudientModule::FREQ2_PARAM));

        // V/Z sync switches (vintage toggle)
        {
            Vec pos1 = mm2px(centerFromId("sync_v", 26.023623f, 66.637276f));
            auto* sw1 = createParamCentered<ShapetakerVintageToggleSwitch>(pos1, module, ClairaudientModule::SYNC1_PARAM);
            addParam(sw1);

            Vec pos2 = mm2px(centerFromId("sync_z", 55.676144f, 66.637276f));
            auto* sw2 = createParamCentered<ShapetakerVintageToggleSwitch>(pos2, module, ClairaudientModule::SYNC2_PARAM);
            addParam(sw2);
        }

        // V/Z fine tune controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm2px(centerFromId("fine_v", 19.023623f, 45.841431f)), module, ClairaudientModule::FINE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm2px(centerFromId("fine_z", 62.717918f, 45.883205f)), module, ClairaudientModule::FINE2_PARAM));

        // V/Z fine tune attenuverters
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(centerFromId("fine_atten_v", 12.023623f, 61.744068f)), module, ClairaudientModule::FINE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(centerFromId("fine_atten_z", 69.621849f, 61.744068f)), module, ClairaudientModule::FINE2_ATTEN_PARAM));
        
        // (Removed decorative teal hexagon indicators for fine attenuverters)
        
        
        // Crossfade control (center)
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(centerFromId("x_fade_knob", 40.87077f, 57.091526f)), module, ClairaudientModule::XFADE_PARAM));

        // Crossfade attenuverter (center)
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(centerFromId("x_fade_atten", 40.639999f, 75.910126f)), module, ClairaudientModule::XFADE_ATTEN_PARAM));
        
        // (Removed decorative teal hexagon indicator for crossfade attenuverters)
        
        
        // V/Z shape controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm2px(centerFromId("sh_knob_v", 13.422475f, 79.825134f)), module, ClairaudientModule::SHAPE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm2px(centerFromId("sh_knob_z", 68.319061f, 79.825134f)), module, ClairaudientModule::SHAPE2_PARAM));
        
        // V/Z shape attenuverters
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(centerFromId("sh_cv_v", 22.421556f, 93.003937f)), module, ClairaudientModule::SHAPE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(centerFromId("sh_cv_z", 58.858444f, 93.003937f)), module, ClairaudientModule::SHAPE2_ATTEN_PARAM));

        // Vintage oscilloscope display showing real-time waveform (circular)
        // The module itself is the source for the oscilloscope data
        if (module) {
            VintageOscilloscopeWidget* oscope = new VintageOscilloscopeWidget(module);
            Vec scr = centerFromId("oscope_screen", 40.87077f, 29.04454f);
            // Increase oscilloscope screen size for better visibility
            constexpr float OSCOPE_SIZE_MM = 32.f; // was 34mm previously
            Vec sizeMM = Vec(OSCOPE_SIZE_MM, OSCOPE_SIZE_MM);
            Vec topLeft = mm2px(scr).minus(mm2px(sizeMM).div(2.f));
            oscope->box.pos = topLeft;
            oscope->box.size = mm2px(sizeMM);
            addChild(oscope);
        }

        // Input row 1: V oscillator V/OCT and CV inputs - BNC connectors
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("v_oct_v", 19.023623f, 105.77721f)), module, ClairaudientModule::VOCT1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("fine_cv_v", 33.648426f, 105.77721f)), module, ClairaudientModule::FINE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("shape_cv_v", 48.139999f, 105.77721f)), module, ClairaudientModule::SHAPE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("x_fade_cv", 40.639999f, 90.126892f)), module, ClairaudientModule::XFADE_CV_INPUT));

        // Input row 2: Z oscillator and outputs - BNC connectors  
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("v_out_z", 19.023623f, 119.04238f)), module, ClairaudientModule::VOCT2_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("fine_cv_z", 33.648426f, 119.04238f)), module, ClairaudientModule::FINE2_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(centerFromId("shape_cv_z", 48.139999f, 119.04238f)), module, ClairaudientModule::SHAPE2_CV_INPUT));
        
        // Stereo outputs - BNC connectors for consistent vintage look
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(centerFromId("output_l", 62.631577f, 105.77721f)), module, ClairaudientModule::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(centerFromId("output_r", 62.631577f, 119.04238f)), module, ClairaudientModule::RIGHT_OUTPUT));

        // Subtle patina overlay to match Transmutation
        auto overlay = new PanelPatinaOverlay();
        overlay->box = Rect(Vec(0, 0), box.size);
        addChild(overlay);
    }

    void appendContextMenu(Menu* menu) override {
        ClairaudientModule* module = dynamic_cast<ClairaudientModule*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Settings"));

        // V Oscillator quantization setting
        struct VQuantizeItem : MenuItem {
            ClairaudientModule* module;
            void onAction(const event::Action& e) override {
                module->quantizeOscV = !module->quantizeOscV;
                module->updateParameterSnapping();
            }
        };
        VQuantizeItem* vQuantizeItem = createMenuItem<VQuantizeItem>("V Oscillator Quantized");
        vQuantizeItem->rightText = module->quantizeOscV ? "✓" : "";
        vQuantizeItem->module = module;
        menu->addChild(vQuantizeItem);

        // Z Oscillator quantization setting
        struct ZQuantizeItem : MenuItem {
            ClairaudientModule* module;
            void onAction(const event::Action& e) override {
                module->quantizeOscZ = !module->quantizeOscZ;
                module->updateParameterSnapping();
            }
        };
        ZQuantizeItem* zQuantizeItem = createMenuItem<ZQuantizeItem>("Z Oscillator Quantized");
        zQuantizeItem->rightText = module->quantizeOscZ ? "✓" : "";
        zQuantizeItem->module = module;
        menu->addChild(zQuantizeItem);
    }
};

Model* modelClairaudient = createModel<ClairaudientModule, ClairaudientWidget>("Clairaudient");
