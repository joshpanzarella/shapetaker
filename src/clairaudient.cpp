#include "plugin.hpp"
#include <cmath>
#include <atomic>

struct ClairaudientModule : Module, IOscilloscopeSource {
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

    // Oscillator state
    float phase1A = 0.f;  // Independent phase for osc 1A
    float phase1B = 0.f;  // Independent phase for osc 1B
    float phase2A = 0.f;  // Independent phase for osc 2A  
    float phase2B = 0.f;  // Independent phase for osc 2B
    
    // Organic variation state
    float drift1A = 0.f, drift1B = 0.f, drift2A = 0.f, drift2B = 0.f;
    float noise1A = 0.f, noise1B = 0.f, noise2A = 0.f, noise2B = 0.f;
    
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
    } antiAliasFilter, antiAliasFilterRight;

    ClairaudientModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Frequency controls
        configParam(FREQ1_PARAM, -4.f, 4.f, 0.f, "Pair 1 Frequency", "Hz", 2.f, 261.626f);
        configParam(FREQ2_PARAM, -4.f, 4.f, 0.f, "Pair 2 Frequency", "Hz", 2.f, 261.626f);
        
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
        // 2x oversampling for smoother sound
        const int oversample = 2;
        float oversampleRate = args.sampleRate * oversample;
        
        float finalLeft = 0.f;
        float finalRight = 0.f;

        // --- Pre-calculate parameters outside the loop ---
        // Get V/Oct inputs with fallback logic
        float voct1 = inputs[VOCT1_INPUT].getVoltage();
        float voct2 = inputs[VOCT2_INPUT].isConnected() ? 
                        inputs[VOCT2_INPUT].getVoltage() : voct1;
        
        // Get parameters
        float pitch1 = params[FREQ1_PARAM].getValue() + voct1;
        float pitch2 = params[FREQ2_PARAM].getValue() + voct2;
        
        float fineTune1 = params[FINE1_PARAM].getValue();
        if (inputs[FINE1_CV_INPUT].isConnected()) {
            float cvAmount = params[FINE1_ATTEN_PARAM].getValue();
            fineTune1 = clamp(fineTune1 + inputs[FINE1_CV_INPUT].getVoltage() * cvAmount / 50.f, -0.2f, 0.2f);
        }
        
        float fineTune2 = params[FINE2_PARAM].getValue();
        if (inputs[FINE2_CV_INPUT].isConnected()) {
            float cvAmount = params[FINE2_ATTEN_PARAM].getValue();
            fineTune2 = clamp(fineTune2 + inputs[FINE2_CV_INPUT].getVoltage() * cvAmount / 50.f, -0.2f, 0.2f);
        }

        // Convert cents to octaves
        fineTune1 /= 12.f;
        fineTune2 /= 12.f;

        // Get shape parameters with attenuverters
        float shape1 = params[SHAPE1_PARAM].getValue();
        if (inputs[SHAPE1_CV_INPUT].isConnected()) {
            float cvAmount = params[SHAPE1_ATTEN_PARAM].getValue();
            shape1 = clamp(shape1 + inputs[SHAPE1_CV_INPUT].getVoltage() * cvAmount / 10.f, 0.f, 1.f);
        }
        
        float shape2 = params[SHAPE2_PARAM].getValue();
        if (inputs[SHAPE2_CV_INPUT].isConnected()) {
            float cvAmount = params[SHAPE2_ATTEN_PARAM].getValue();
            shape2 = clamp(shape2 + inputs[SHAPE2_CV_INPUT].getVoltage() * cvAmount / 10.f, 0.f, 1.f);
        }

        // Get crossfade parameter with attenuverter
        float xfade = params[XFADE_PARAM].getValue();
        if (inputs[XFADE_CV_INPUT].isConnected()) {
            float cvAmount = params[XFADE_ATTEN_PARAM].getValue();
            xfade = clamp(xfade + inputs[XFADE_CV_INPUT].getVoltage() * cvAmount / 10.f, 0.f, 1.f);
        }
        
        // --- Adaptive Oscilloscope Timescale ---
        // Determine the dominant frequency based on the crossfader position
        float baseFreq1 = 261.626f * std::pow(2.f, pitch1);
        float baseFreq2 = 261.626f * std::pow(2.f, pitch2);
        float dominantFreq = (xfade < 0.5f) ? baseFreq1 : baseFreq2;
        dominantFreq = std::max(dominantFreq, 1.f); // Prevent division by zero or very small numbers

        const float targetCyclesInDisplay = 2.f; // Aim to show this many waveform cycles
        int downsampleFactor = (int)roundf((targetCyclesInDisplay * args.sampleRate) / (OSCILLOSCOPE_BUFFER_SIZE * dominantFreq));
        downsampleFactor = clamp(downsampleFactor, 1, 256); // Clamp to a reasonable range

        for (int os = 0; os < oversample; os++) {
            // Add organic frequency drift (very subtle)
            updateOrganicDrift(args.sampleTime * oversample);
            
            float freq1A = 261.626f * std::pow(2.f, pitch1 + drift1A);
            float freq1B = 261.626f * std::pow(2.f, pitch1 + fineTune1 + drift1B);
            float freq2A = 261.626f * std::pow(2.f, pitch2 + drift2A);
            float freq2B = 261.626f * std::pow(2.f, pitch2 + fineTune2 + drift2B);
            
            // Check sync switches
            bool sync1 = params[SYNC1_PARAM].getValue() > 0.5f;
            bool sync2 = params[SYNC2_PARAM].getValue() > 0.5f;
            
            // Update phases (with sync logic)
            float deltaPhase1A = freq1A / oversampleRate; // Master oscillator increment
            float deltaPhase1B = freq1B / oversampleRate; // Slave runs at its own frequency
            float deltaPhase2A = freq2A / oversampleRate; // Master oscillator increment
            float deltaPhase2B = freq2B / oversampleRate; // Slave runs at its own frequency
            
            // Add subtle phase noise for organic character
            phase1A += deltaPhase1A + noise1A * 0.00001f;
            phase1B += deltaPhase1B + noise1B * 0.00001f;
            phase2A += deltaPhase2A + noise2A * 0.00001f;
            phase2B += deltaPhase2B + noise2B * 0.00001f;
            
            if (phase1A >= 1.f) phase1A -= 1.f;
            if (phase1B >= 1.f) phase1B -= 1.f;
            if (phase2A >= 1.f) phase2A -= 1.f;
            if (phase2B >= 1.f) phase2B -= 1.f;
            
            // Apply sync - reset B oscillator to match A when synced
            if (sync1 && phase1A < deltaPhase1A) {
                phase1B = phase1A;
            }
            if (sync2 && phase2A < deltaPhase2A) {
                phase2B = phase2A;
            }
            
            // Generate sigmoid-morphed oscillators with anti-aliasing
            float osc1A = generateOrganicOsc(phase1A, shape1, freq1A, oversampleRate);
            float osc1B = generateOrganicOsc(phase1B, shape1, freq1B, oversampleRate);
            float osc2A = generateOrganicOsc(phase2A, shape2, freq2A, oversampleRate);
            float osc2B = generateOrganicOsc(phase2B, shape2, freq2B, oversampleRate);
            
            // Crossfade between pairs for each channel (A for Left, B for Right)
            float leftOutput = osc1A * (1.f - xfade) + osc2A * xfade;
            float rightOutput = osc1B * (1.f - xfade) + osc2B * xfade;

            // Apply anti-aliasing filter to each channel separately for true stereo
            float filteredLeft = antiAliasFilter.process(leftOutput, args.sampleRate * 0.45f, oversampleRate);
            float filteredRight = antiAliasFilterRight.process(rightOutput, args.sampleRate * 0.45f, oversampleRate);

            finalLeft += filteredLeft;
            finalRight += filteredRight;
        }
        
        // Average the oversampled result
        float outL = std::tanh(finalLeft / oversample) * 5.f;
        float outR = std::tanh(finalRight / oversample) * 5.f;

        outputs[LEFT_OUTPUT].setVoltage(outL);
        outputs[RIGHT_OUTPUT].setVoltage(outR);

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

    // --- IOscilloscopeSource Implementation ---
    const Vec* getOscilloscopeBuffer() const override { return oscilloscopeBuffer; }
    int getOscilloscopeBufferIndex() const override { return oscilloscopeBufferIndex.load(); }
    int getOscilloscopeBufferSize() const override { return OSCILLOSCOPE_BUFFER_SIZE; }

private:
    // Update organic drift and noise for more natural sound
    void updateOrganicDrift(float sampleTime) {
        // Very slow random walk for frequency drift (like analog oscillator aging)
        static float driftSpeed = 0.00002f;
        
        drift1A += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift1B += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift2A += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift2B += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        
        // Limit drift to very small amounts (±0.1 cents)
        drift1A = clamp(drift1A, -0.001f, 0.001f);
        drift1B = clamp(drift1B, -0.001f, 0.001f);
        drift2A = clamp(drift2A, -0.001f, 0.001f);
        drift2B = clamp(drift2B, -0.001f, 0.001f);
        
        // Generate subtle phase noise
        noise1A = (rack::random::uniform() - 0.5f) * 2.f;
        noise1B = (rack::random::uniform() - 0.5f) * 2.f;
        noise2A = (rack::random::uniform() - 0.5f) * 2.f;
        noise2B = (rack::random::uniform() - 0.5f) * 2.f;
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

        // Row 1: Frequency controls - oscilloscope style
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(mm2px(Vec(15, 32)), module, ClairaudientModule::FREQ1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(mm2px(Vec(45, 32)), module, ClairaudientModule::FREQ2_PARAM));
        
        // Row 2: Fine tune and sync controls - oscilloscope style
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(15, 45)), module, ClairaudientModule::FINE1_PARAM));
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(mm2px(Vec(25, 35)), module, ClairaudientModule::SYNC1_PARAM));
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(mm2px(Vec(35, 35)), module, ClairaudientModule::SYNC2_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(45, 45)), module, ClairaudientModule::FINE2_PARAM));
        
        // Row 3: Fine tune attenuverters - oscilloscope style
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(15, 55)), module, ClairaudientModule::FINE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(45, 55)), module, ClairaudientModule::FINE2_ATTEN_PARAM));
        
        // Row 4: Shape controls - oscilloscope style
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(15, 68)), module, ClairaudientModule::SHAPE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(45, 68)), module, ClairaudientModule::SHAPE2_PARAM));
        
        // Row 5: Shape attenuverters - oscilloscope style
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(15, 78)), module, ClairaudientModule::SHAPE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(45, 78)), module, ClairaudientModule::SHAPE2_ATTEN_PARAM));
        
        // Row 6: Crossfade control - oscilloscope style
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm2px(Vec(30.48, 54)), module, ClairaudientModule::XFADE_PARAM));
        
        // Row 7: Crossfade attenuverter - oscilloscope style
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(30.48, 65)), module, ClairaudientModule::XFADE_ATTEN_PARAM));

        // Vintage oscilloscope display showing real-time waveform (circular)
        // The module itself is the source for the oscilloscope data
        if (module) {
            VintageOscilloscopeWidget* oscope = new VintageOscilloscopeWidget(module);
            // Revert to original size and position
            oscope->box.pos = mm2px(Vec(30.48 - 10, 15)); // Center horizontally between frequency knobs
            oscope->box.size = mm2px(Vec(20, 20)); // Square aspect ratio for circular appearance
            addChild(oscope);
        }

        // Input row 1: V/OCT and Fine CV - BNC connectors for vintage oscilloscope look
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(10, 99.5)), module, ClairaudientModule::VOCT1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(22, 99.5)), module, ClairaudientModule::FINE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(38, 99.5)), module, ClairaudientModule::FINE2_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(50, 99.5)), module, ClairaudientModule::VOCT2_INPUT));

        // Input row 2: Shape CV and Crossfade CV - BNC connectors
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(10, 112)), module, ClairaudientModule::SHAPE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(22, 112)), module, ClairaudientModule::XFADE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(38, 112)), module, ClairaudientModule::SHAPE2_CV_INPUT));
        
        // Outputs - BNC connectors for consistent vintage look
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(Vec(50, 110)), module, ClairaudientModule::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(Vec(50, 119)), module, ClairaudientModule::RIGHT_OUTPUT));
    }
};

Model* modelClairaudient = createModel<ClairaudientModule, ClairaudientWidget>("Clairaudient");