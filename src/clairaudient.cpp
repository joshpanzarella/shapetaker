#include "plugin.hpp"
#include <cmath>
#include <atomic>

// Small teal hexagon widget for attenuverter visual distinction
struct HexagonWidget : Widget {
    NVGcolor color = nvgRGBA(64, 224, 208, 255);
    
    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        
        // Draw hexagon centered in the widget box
        float cx = box.size.x / 2.0f;
        float cy = box.size.y / 2.0f;
        float radius = std::min(box.size.x, box.size.y) / 2.0f * 0.8f;
        
        for (int i = 0; i < 6; i++) {
            float angle = i * M_PI / 3.0f;
            float x = cx + radius * cosf(angle);
            float y = cy + radius * sinf(angle);
            
            if (i == 0) {
                nvgMoveTo(args.vg, x, y);
            } else {
                nvgLineTo(args.vg, x, y);
            }
        }
        nvgClosePath(args.vg);
        
        nvgFillColor(args.vg, color);
        nvgFill(args.vg);
        
        // Optional stroke for better visibility
        nvgStrokeColor(args.vg, nvgRGBA(32, 112, 104, 255)); // Darker teal
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStroke(args.vg);
    }
};


struct ClairaudientModule : Module, IOscilloscopeSource {
    
    // Quantize voltage to exact 12-tone musical notes
    float quantizeToNote(float voltage) {
        // Round to nearest semitone
        float semitone = std::round(voltage * 12.0f) / 12.0f;
        return semitone;
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

    ClairaudientModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Frequency controls
        configParam(FREQ1_PARAM, -2.f, 2.f, 0.f, "Pair 1 Frequency", "Hz", 2.f, 261.626f);
        configParam(FREQ2_PARAM, -2.f, 2.f, 0.f, "Pair 2 Frequency", "Hz", 2.f, 261.626f);
        
        // FREQ1: No quantization (smooth octave range)
        // FREQ2: Uses custom musical note quantization in process() function
        
        // Fine tune controls (±20 cents, centered at 0 for no detune)
        configParam(FINE1_PARAM, -0.2f, 0.2f, 0.f, "Pair 1 Fine Tune", "cents", 0.f, 0.f, 100.f);
        configParam(FINE2_PARAM, -0.2f, 0.2f, 0.f, "Pair 2 Fine Tune", "cents", 0.f, 0.f, 100.f);
        
        // Fine tune CV attenuverters
        configParam(FINE1_ATTEN_PARAM, -1.f, 1.f, 0.f, "Pair 1 Fine Tune CV Amount", "%", 0.f, 100.f);
        configParam(FINE2_ATTEN_PARAM, -1.f, 1.f, 0.f, "Pair 2 Fine Tune CV Amount", "%", 0.f, 100.f);
        
        // Shape morphing controls (default to 50% for proper sigmoid)
        configParam(SHAPE1_PARAM, 0.f, 1.f, 0.5f, "Pair 1 Shape", "%", 0.f, 100.f);
        configParam(SHAPE2_PARAM, 0.f, 1.f, 0.5f, "Pair 2 Shape", "%", 0.f, 100.f);
        
        // Shape CV attenuverters
        configParam(SHAPE1_ATTEN_PARAM, -1.f, 1.f, 0.f, "Pair 1 Shape CV Amount", "%", 0.f, 100.f);
        configParam(SHAPE2_ATTEN_PARAM, -1.f, 1.f, 0.f, "Pair 2 Shape CV Amount", "%", 0.f, 100.f);
        
        // Crossfade control (centered at 0.5)
        configParam(XFADE_PARAM, 0.f, 1.f, 0.5f, "Crossfade", "%", 0.f, 100.f);
        
        // Crossfade CV attenuverter
        configParam(XFADE_ATTEN_PARAM, -1.f, 1.f, 0.f, "Crossfade CV Amount", "%", 0.f, 100.f);
        
        // Sync switches (default off for independent beating)
        configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "Pair 1 Sync", {"Independent", "Synced"});
        configSwitch(SYNC2_PARAM, 0.f, 1.f, 0.f, "Pair 2 Sync", {"Independent", "Synced"});
        
        // Inputs
        configInput(VOCT1_INPUT, "Pair 1 V/Oct");
        configInput(VOCT2_INPUT, "Pair 2 V/Oct");
        configInput(FINE1_CV_INPUT, "Pair 1 Fine Tune CV");
        configInput(FINE2_CV_INPUT, "Pair 2 Fine Tune CV");
        configInput(SHAPE1_CV_INPUT, "Pair 1 Shape CV");
        configInput(SHAPE2_CV_INPUT, "Pair 2 Shape CV");
        configInput(XFADE_CV_INPUT, "Crossfade CV");
        
        // Outputs
        configOutput(LEFT_OUTPUT, "Left");
        configOutput(RIGHT_OUTPUT, "Right");
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
            float pitch1 = params[FREQ1_PARAM].getValue() + voct1;
            
            // Quantize FREQ2 to musical notes (semitones)
            float rawPitch2 = params[FREQ2_PARAM].getValue() + voct2;
            float pitch2 = quantizeToNote(rawPitch2);
            
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
    ClairaudientWidget(ClairaudientModule* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Clairaudient.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // OSC X (left side) - Frequency controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(10.075688, 31.045906)), module, ClairaudientModule::FREQ1_PARAM));
        
        // OSC Y (right side) - Frequency controls  
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(50.588146, 31.045906)), module, ClairaudientModule::FREQ2_PARAM));
        
        // Sync switches
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(mm2px(Vec(26.167959, 47.738434)), module, ClairaudientModule::SYNC1_PARAM));
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(mm2px(Vec(35.155243, 47.738434)), module, ClairaudientModule::SYNC2_PARAM));
        
        // Fine tune controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(15.0233, 45.038658)), module, ClairaudientModule::FINE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(45.0233, 45.038658)), module, ClairaudientModule::FINE2_PARAM));
        
        // Fine tune attenuverters
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(15.034473, 58.820183)), module, ClairaudientModule::FINE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(45.034473, 58.820183)), module, ClairaudientModule::FINE2_ATTEN_PARAM));
        
        // Teal hexagon indicators for fine tune attenuverters
        {
            NVGcolor tealColor = nvgRGBA(64, 224, 208, 255); // Teal color
            HexagonWidget* hex1 = new HexagonWidget();
            hex1->box.pos = mm2px(Vec(15.034473 - 1.5, 58.820183 - 1.5));
            hex1->box.size = mm2px(Vec(3, 3));
            hex1->color = tealColor;
            addChild(hex1);
            
            HexagonWidget* hex2 = new HexagonWidget();
            hex2->box.pos = mm2px(Vec(45.034473 - 1.5, 58.820183 - 1.5));
            hex2->box.size = mm2px(Vec(3, 3));
            hex2->color = tealColor;
            addChild(hex2);
        }
        
        
        // Crossfade control (center)
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(30.48, 64.107727)), module, ClairaudientModule::XFADE_PARAM));
        
        // Crossfade attenuverter (center)
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(30.48, 83.021637)), module, ClairaudientModule::XFADE_ATTEN_PARAM));
        
        // Teal hexagon indicator for crossfade attenuverter
        {
            NVGcolor tealColor = nvgRGBA(64, 224, 208, 255); // Teal color
            HexagonWidget* hex = new HexagonWidget();
            hex->box.pos = mm2px(Vec(30.48 - 1.5, 83.021637 - 1.5));
            hex->box.size = mm2px(Vec(3, 3));
            hex->color = tealColor;
            addChild(hex);
        }
        
        
        // Shape controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(10.240995, 74.654305)), module, ClairaudientModule::SHAPE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(50.719231, 74.654305)), module, ClairaudientModule::SHAPE2_PARAM));
        
        // Shape attenuverters
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(15.034473, 86.433929)), module, ClairaudientModule::SHAPE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(44.581718, 86.433929)), module, ClairaudientModule::SHAPE2_ATTEN_PARAM));
        
        // Teal hexagon indicators for shape attenuverters
        {
            NVGcolor tealColor = nvgRGBA(64, 224, 208, 255); // Teal color
            HexagonWidget* hex1 = new HexagonWidget();
            hex1->box.pos = mm2px(Vec(15.034473 - 1.5, 86.433929 - 1.5));
            hex1->box.size = mm2px(Vec(3, 3));
            hex1->color = tealColor;
            addChild(hex1);
            
            HexagonWidget* hex2 = new HexagonWidget();
            hex2->box.pos = mm2px(Vec(44.581718 - 1.5, 86.433929 - 1.5));
            hex2->box.size = mm2px(Vec(3, 3));
            hex2->color = tealColor;
            addChild(hex2);
        }
        

        // Vintage oscilloscope display showing real-time waveform (circular)
        // The module itself is the source for the oscilloscope data
        if (module) {
            VintageOscilloscopeWidget* oscope = new VintageOscilloscopeWidget(module);
            // Position based on the yellow circle in the SVG
            oscope->box.pos = mm2px(Vec(30.563007 - 13, 28.92709 - 13)); // Center on oscilloscope screen position
            oscope->box.size = mm2px(Vec(26, 26)); // Larger square aspect ratio for circular appearance
            addChild(oscope);
        }

        // Input row 1: V/OCT and CV inputs - BNC connectors based on SVG positions
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(8.721756, 99.862808)), module, ClairaudientModule::VOCT1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(21.777, 99.862808)), module, ClairaudientModule::FINE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(34.832245, 99.862808)), module, ClairaudientModule::SHAPE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(47.887489, 99.862808)), module, ClairaudientModule::XFADE_CV_INPUT));

        // Input row 2: Second oscillator and output - BNC connectors  
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(8.721756, 113.38142)), module, ClairaudientModule::VOCT2_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(21.112274, 113.38142)), module, ClairaudientModule::FINE2_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(33.502792, 113.38142)), module, ClairaudientModule::SHAPE2_CV_INPUT));
        
        // Outputs - BNC connectors for consistent vintage look
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(Vec(45.893311, 113.38142)), module, ClairaudientModule::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(Vec(53.724667, 113.38142)), module, ClairaudientModule::RIGHT_OUTPUT));
    }
};

Model* modelClairaudient = createModel<ClairaudientModule, ClairaudientWidget>("Clairaudient");