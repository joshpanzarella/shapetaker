#include "plugin.hpp"
#include "transmutation/ui.hpp" // for PanelPatinaOverlay (shared vintage overlay)
#include <cmath>
#include <atomic>
#include <functional>

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
        // Round to the nearest semitone step
        return std::round(clamped);
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

    enum CrossfadeMode {
        CROSSFADE_EQUAL_POWER = 0,
        CROSSFADE_STEREO_SWAP = 1
    };

    // Polyphonic oscillator state (up to 6 voices)
    static constexpr int MAX_POLY_VOICES = shapetaker::PolyphonicProcessor::MAX_VOICES;
    shapetaker::FloatVoices phase1A;  // Independent phase for osc 1A per voice
    shapetaker::FloatVoices phase1B;  // Independent phase for osc 1B per voice
    shapetaker::FloatVoices phase2A;  // Independent phase for osc 2A per voice
    shapetaker::FloatVoices phase2B;  // Independent phase for osc 2B per voice
    
    // Organic variation state per voice
    shapetaker::FloatVoices drift1A;
    shapetaker::FloatVoices drift1B;
    shapetaker::FloatVoices drift2A;
    shapetaker::FloatVoices drift2B;
    shapetaker::FloatVoices noise1A;
    shapetaker::FloatVoices noise1B;
    shapetaker::FloatVoices noise2A;
    shapetaker::FloatVoices noise2B;

    // User-adjustable oscillator noise amount (0..1), exposed via context menu slider.
    // Defaults to 0.0 (off). Controls both subtle phase jitter and added noise floor.
    float oscNoiseAmount = 0.0f;
    
    // --- Oscilloscope Buffering ---
    static const int OSCILLOSCOPE_BUFFER_SIZE = 1024;
    Vec oscilloscopeBuffer[OSCILLOSCOPE_BUFFER_SIZE] = {};
    std::atomic<int> oscilloscopeBufferIndex = {0};
    int oscilloscopeFrameCounter = 0;

    // Anti-aliasing filters per voice
    shapetaker::VoiceArray<shapetaker::dsp::OnePoleLowpass> antiAliasFilterLeft;
    shapetaker::VoiceArray<shapetaker::dsp::OnePoleLowpass> antiAliasFilterRight;
    shapetaker::VoiceArray<shapetaker::dsp::OnePoleLowpass> highCutFilterLeft;
    shapetaker::VoiceArray<shapetaker::dsp::OnePoleLowpass> highCutFilterRight;

    shapetaker::PolyphonicProcessor polyProcessor;

    // Quantization mode settings
    bool quantizeOscV = true;  // V oscillator quantized to octaves by default
    bool quantizeOscZ = true;  // Z oscillator quantized to semitones by default
    int crossfadeMode = CROSSFADE_EQUAL_POWER;
    int oversampleFactor = 2;
    bool highCutEnabled = false;
    float driftAmount = 0.0f;

    // Update parameter snapping based on quantization modes
    void updateParameterSnapping() {
        // V Oscillator snapping
        getParamQuantity(FREQ1_PARAM)->snapEnabled = quantizeOscV;
        getParamQuantity(FREQ1_PARAM)->smoothEnabled = !quantizeOscV;

        // Z Oscillator snapping
        getParamQuantity(FREQ2_PARAM)->snapEnabled = quantizeOscZ;
        getParamQuantity(FREQ2_PARAM)->smoothEnabled = !quantizeOscZ;
    }

    void resetFilters() {
        antiAliasFilterLeft.reset();
        antiAliasFilterRight.reset();
        highCutFilterLeft.reset();
        highCutFilterRight.reset();
    }

    ClairaudientModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        using ParameterHelper = shapetaker::ParameterHelper;
        
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
        configParam(FINE1_PARAM, -0.2f, 0.2f, 0.f, "V Fine Tune", "cents", 0.f, 100.f);
        configParam(FINE2_PARAM, -0.2f, 0.2f, 0.f, "Z Fine Tune", "cents", 0.f, 100.f);
        
        // Fine tune CV attenuverters
        ParameterHelper::configAttenuverter(this, FINE1_ATTEN_PARAM, "V Fine Tune CV Amount");
        ParameterHelper::configAttenuverter(this, FINE2_ATTEN_PARAM, "Z Fine Tune CV Amount");
        
        // Shape morphing controls (default to 50% for proper sigmoid)
        ParameterHelper::configGain(this, SHAPE1_PARAM, "V Shape", 0.5f);
        ParameterHelper::configGain(this, SHAPE2_PARAM, "Z Shape", 0.5f);
        
        // Shape CV attenuverters
        ParameterHelper::configAttenuverter(this, SHAPE1_ATTEN_PARAM, "V Shape CV Amount");
        ParameterHelper::configAttenuverter(this, SHAPE2_ATTEN_PARAM, "Z Shape CV Amount");
        
        // Crossfade control (centered at 0.5)
        ParameterHelper::configMix(this, XFADE_PARAM, "Crossfade", 0.5f);
        
        // Crossfade CV attenuverter
        ParameterHelper::configAttenuverter(this, XFADE_ATTEN_PARAM, "Crossfade CV Amount");
        
        // Sync switches (default off for independent beating)
        configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "V Sync", {"Independent", "Synced"});
        configSwitch(SYNC2_PARAM, 0.f, 1.f, 0.f, "Z Sync", {"Independent", "Synced"});
        
        // Inputs
        ParameterHelper::configCVInput(this, VOCT1_INPUT, "V Oscillator V/Oct");
        ParameterHelper::configCVInput(this, VOCT2_INPUT, "Z Oscillator V/Oct");
        ParameterHelper::configCVInput(this, FINE1_CV_INPUT, "V Fine Tune CV");
        ParameterHelper::configCVInput(this, FINE2_CV_INPUT, "Z Fine Tune CV");
        ParameterHelper::configCVInput(this, SHAPE1_CV_INPUT, "V Shape CV");
        ParameterHelper::configCVInput(this, SHAPE2_CV_INPUT, "Z Shape CV");
        ParameterHelper::configCVInput(this, XFADE_CV_INPUT, "Crossfade CV");
        
        // Outputs
        ParameterHelper::configAudioOutput(this, LEFT_OUTPUT, "Left");
        ParameterHelper::configAudioOutput(this, RIGHT_OUTPUT, "Right");
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "quantizeOscV", json_boolean(quantizeOscV));
        json_object_set_new(rootJ, "quantizeOscZ", json_boolean(quantizeOscZ));
        json_object_set_new(rootJ, "oscNoiseAmount", json_real(oscNoiseAmount));
        json_object_set_new(rootJ, "crossfadeMode", json_integer(crossfadeMode));
        json_object_set_new(rootJ, "oversampleFactor", json_integer(oversampleFactor));
        json_object_set_new(rootJ, "highCutEnabled", json_boolean(highCutEnabled));
        json_object_set_new(rootJ, "driftAmount", json_real(driftAmount));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* quantizeVJ = json_object_get(rootJ, "quantizeOscV");
        if (quantizeVJ)
            quantizeOscV = json_boolean_value(quantizeVJ);

        json_t* quantizeZJ = json_object_get(rootJ, "quantizeOscZ");
        if (quantizeZJ)
            quantizeOscZ = json_boolean_value(quantizeZJ);

        json_t* noiseJ = json_object_get(rootJ, "oscNoiseAmount");
        if (noiseJ)
            oscNoiseAmount = clamp((float)json_number_value(noiseJ), 0.f, 1.f);

        json_t* xfadeModeJ = json_object_get(rootJ, "crossfadeMode");
        if (xfadeModeJ)
            crossfadeMode = clamp((int)json_integer_value(xfadeModeJ), CROSSFADE_EQUAL_POWER, CROSSFADE_STEREO_SWAP);

        json_t* oversampleJ = json_object_get(rootJ, "oversampleFactor");
        if (oversampleJ)
            oversampleFactor = clamp((int)json_integer_value(oversampleJ), 1, 8);

        json_t* highCutJ = json_object_get(rootJ, "highCutEnabled");
        if (highCutJ)
            highCutEnabled = json_boolean_value(highCutJ);

        json_t* driftJ = json_object_get(rootJ, "driftAmount");
        if (driftJ)
            driftAmount = clamp((float)json_number_value(driftJ), 0.f, 1.f);

        // Update parameter snapping after loading settings
        updateParameterSnapping();
    }

    void process(const ProcessArgs& args) override {
        // Determine number of polyphonic voices (max 6)
        int channels = polyProcessor.updateChannels(
            {inputs[VOCT1_INPUT], inputs[VOCT2_INPUT]},
            {outputs[LEFT_OUTPUT], outputs[RIGHT_OUTPUT]});
        
        // Apply the configured oversampling factor (default 2×) for smoother sound
        int oversample = std::max(1, oversampleFactor);
        float oversampleRate = args.sampleRate * oversample;

        // Process each voice
        for (int ch = 0; ch < channels; ch++) {
            float finalLeft = 0.f;
            float finalRight = 0.f;

            // --- Pre-calculate parameters for this voice ---
            // Get V/Oct inputs with fallback logic
            float voct1 = inputs[VOCT1_INPUT].getPolyVoltage(ch);
            float voct2 = inputs[VOCT2_INPUT].isConnected() ?
                            inputs[VOCT2_INPUT].getPolyVoltage(ch) : voct1;
            
            // Get parameters for this voice
            // V Oscillator: quantize knob value if enabled, then add CV
            float basePitch1 = params[FREQ1_PARAM].getValue();
            if (quantizeOscV)
                basePitch1 = quantizeToOctave(basePitch1);
            float pitch1 = basePitch1 + voct1;

            // Z Oscillator: quantize knob (in semitones) if enabled, then add CV
            float baseSemitoneZ = params[FREQ2_PARAM].getValue();
            if (quantizeOscZ)
                baseSemitoneZ = quantizeToSemitone(baseSemitoneZ);
            float pitch2 = baseSemitoneZ / 12.0f + voct2;
            
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

            // Convert semitone offsets to octaves
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
            float xfadeClamped = clamp(xfade, 0.f, 1.f);

            // Shape the user noise amount so the audible noise ramps up sooner
            float shapedNoise = std::pow(clamp(oscNoiseAmount, 0.f, 1.f), 0.65f);

            for (int os = 0; os < oversample; os++) {
                // Add organic frequency drift (very subtle) for this voice
                updateOrganicDrift(ch, args.sampleTime * oversample, driftAmount);
                
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
                
                // Add subtle phase noise for organic character (scaled by shaped user amount)
                const float noiseScale = 0.00005f * shapedNoise;
                phase1A[ch] += deltaPhase1A + noise1A[ch] * noiseScale;
                phase1B[ch] += deltaPhase1B + noise1B[ch] * noiseScale;
                phase2A[ch] += deltaPhase2A + noise2A[ch] * noiseScale;
                phase2B[ch] += deltaPhase2B + noise2B[ch] * noiseScale;
                
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
                float osc1A = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase1A[ch], shape1, freq1A, oversampleRate);
                float osc1B = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase1B[ch], shape1, freq1B, oversampleRate);
                float osc2A = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase2A[ch], shape2, freq2A, oversampleRate);
                float osc2B = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase2B[ch], shape2, freq2B, oversampleRate);
                
                float leftOutput;
                float rightOutput;

                if (crossfadeMode == CROSSFADE_EQUAL_POWER) {
                    leftOutput = shapetaker::dsp::OscillatorHelper::equalPowerMix(osc1A, osc2A, xfadeClamped);
                    rightOutput = shapetaker::dsp::OscillatorHelper::equalPowerMix(osc1B, osc2B, xfadeClamped);
                } else {
                    if (xfadeClamped <= 0.5f) {
                        float t = xfadeClamped * 2.f;
                        leftOutput = shapetaker::dsp::OscillatorHelper::equalPowerMix(osc1A, osc2B, t);
                        rightOutput = shapetaker::dsp::OscillatorHelper::equalPowerMix(osc1B, osc1A, t);
                    } else {
                        float t = (xfadeClamped - 0.5f) * 2.f;
                        leftOutput = shapetaker::dsp::OscillatorHelper::equalPowerMix(osc2B, osc2A, t);
                        rightOutput = shapetaker::dsp::OscillatorHelper::equalPowerMix(osc1A, osc2B, t);
                    }
                }

                // Apply anti-aliasing filter to each channel separately for true stereo
                float filteredLeft = antiAliasFilterLeft[ch].process(leftOutput, args.sampleRate * 0.45f, oversampleRate);
                float filteredRight = antiAliasFilterRight[ch].process(rightOutput, args.sampleRate * 0.45f, oversampleRate);

                finalLeft += filteredLeft;
                finalRight += filteredRight;
            }
            
            // Average the oversampled result for this voice
            float outL = std::tanh(finalLeft / oversample) * 5.f;
            float outR = std::tanh(finalRight / oversample) * 5.f;

            // Add audible white noise floor scaled by user amount (post waveshaping, in volts)
            if (shapedNoise > 0.f) {
                const float NOISE_V_PEAK = 0.45f; // ~±0.45 V at 100%
                float nL = (rack::random::uniform() - 0.5f) * 2.f * NOISE_V_PEAK * shapedNoise;
                float nR = (rack::random::uniform() - 0.5f) * 2.f * NOISE_V_PEAK * shapedNoise;
                outL += nL;
                outR += nR;
            }

            if (highCutEnabled) {
                const float HIGH_CUT_HZ = 10000.f;
                outL = highCutFilterLeft[ch].process(outL, HIGH_CUT_HZ, args.sampleRate);
                outR = highCutFilterRight[ch].process(outR, HIGH_CUT_HZ, args.sampleRate);
            }

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
    void updateOrganicDrift(int voice, float sampleTime, float amount) {
        amount = clamp(amount, 0.f, 1.f);
        if (amount <= 0.f) {
            drift1A[voice] = drift1B[voice] = drift2A[voice] = drift2B[voice] = 0.f;
            noise1A[voice] = noise1B[voice] = noise2A[voice] = noise2B[voice] = 0.f;
            return;
        }
        // Very slow random walk for frequency drift (like analog oscillator aging)
        const float baseDriftSpeed = 0.00002f;
        float driftSpeed = baseDriftSpeed * amount;

        drift1A[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift1B[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift2A[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;
        drift2B[voice] += (rack::random::uniform() - 0.5f) * driftSpeed * sampleTime;

        // Limit drift to very small amounts (about ±1.2 cents at full amount)
        const float driftLimit = 0.001f * amount;
        drift1A[voice] = clamp(drift1A[voice], -driftLimit, driftLimit);
        drift1B[voice] = clamp(drift1B[voice], -driftLimit, driftLimit);
        drift2A[voice] = clamp(drift2A[voice], -driftLimit, driftLimit);
        drift2B[voice] = clamp(drift2B[voice], -driftLimit, driftLimit);

        // Generate subtle phase noise
        float noiseScale = amount;
        noise1A[voice] = (rack::random::uniform() - 0.5f) * 2.f * noiseScale;
        noise1B[voice] = (rack::random::uniform() - 0.5f) * 2.f * noiseScale;
        noise2A[voice] = (rack::random::uniform() - 0.5f) * 2.f * noiseScale;
        noise2B[voice] = (rack::random::uniform() - 0.5f) * 2.f * noiseScale;
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

        using LayoutHelper = shapetaker::ui::LayoutHelper;

        LayoutHelper::ScrewPositions::addStandardScrews<ScrewBlack>(this, box.size.x);

        // Use shared panel parser utilities for control placement
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Clairaudient.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = [&](const std::string& id, float defx, float defy) -> Vec {
            return parser.centerPx(id, defx, defy);
        };

        // V/Z oscillator large frequency knobs
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(centerPx("freq_v", 13.422475f, 25.464647f), module, ClairaudientModule::FREQ1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(centerPx("freq_z", 68.319061f, 25.695415f), module, ClairaudientModule::FREQ2_PARAM));

        // V/Z sync switches (vintage toggle)
        {
            Vec pos1 = centerPx("sync_v", 26.023623f, 66.637276f);
            auto* sw1 = createParamCentered<ShapetakerVintageToggleSwitch>(pos1, module, ClairaudientModule::SYNC1_PARAM);
            addParam(sw1);

            Vec pos2 = centerPx("sync_z", 55.676144f, 66.637276f);
            auto* sw2 = createParamCentered<ShapetakerVintageToggleSwitch>(pos2, module, ClairaudientModule::SYNC2_PARAM);
            addParam(sw2);
        }

        // V/Z fine tune controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("fine_v", 19.023623f, 45.841431f), module, ClairaudientModule::FINE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("fine_z", 62.717918f, 45.883205f), module, ClairaudientModule::FINE2_PARAM));

        // V/Z fine tune attenuverters
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("fine_atten_v", 12.023623f, 61.744068f), module, ClairaudientModule::FINE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("fine_atten_z", 69.621849f, 61.744068f), module, ClairaudientModule::FINE2_ATTEN_PARAM));
        
        // (Removed decorative teal hexagon indicators for fine attenuverters)
        
        
        // Crossfade control (center)
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(centerPx("x_fade_knob", 40.87077f, 57.091526f), module, ClairaudientModule::XFADE_PARAM));

        // Crossfade attenuverter (center)
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("x_fade_atten", 40.639999f, 75.910126f), module, ClairaudientModule::XFADE_ATTEN_PARAM));
        
        // (Removed decorative teal hexagon indicator for crossfade attenuverters)
        
        
        // V/Z shape controls
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("sh_knob_v", 13.422475f, 79.825134f), module, ClairaudientModule::SHAPE1_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("sh_knob_z", 68.319061f, 79.825134f), module, ClairaudientModule::SHAPE2_PARAM));
        
        // V/Z shape attenuverters
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("sh_cv_v", 22.421556f, 93.003937f), module, ClairaudientModule::SHAPE1_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("sh_cv_z", 58.858444f, 93.003937f), module, ClairaudientModule::SHAPE2_ATTEN_PARAM));

        // Vintage oscilloscope display showing real-time waveform (circular)
        // The module itself is the source for the oscilloscope data
        if (module) {
            VintageOscilloscopeWidget* oscope = new VintageOscilloscopeWidget(module);
            Vec scrPx = centerPx("oscope_screen", 40.87077f, 29.04454f);
            // Increase oscilloscope screen size slightly (square to preserve CRT effect)
            constexpr float OSCOPE_SIZE_MM = 33.f; // +1mm from previous 32
            Vec sizePx = LayoutHelper::mm2px(Vec(OSCOPE_SIZE_MM, OSCOPE_SIZE_MM));
            Vec topLeft = scrPx.minus(sizePx.div(2.f));
            oscope->box.pos = topLeft;
            oscope->box.size = sizePx;
            addChild(oscope);
        }

        // Input row 1: V oscillator V/OCT and CV inputs - BNC connectors
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("v_oct_v", 23.762346f, 105.77721f), module, ClairaudientModule::VOCT1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("fine_cv_v", 38.386749f, 105.77721f), module, ClairaudientModule::FINE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("shape_cv_v", 52.878323f, 105.77721f), module, ClairaudientModule::SHAPE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("x_fade_cv", 40.639999f, 90.126892f), module, ClairaudientModule::XFADE_CV_INPUT));

        // Input row 2: Z oscillator and outputs - BNC connectors
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("v_out_z", 23.76195f, 118.09399f), module, ClairaudientModule::VOCT2_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("fine_cv_z", 38.386749f, 118.09399f), module, ClairaudientModule::FINE2_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("shape_cv_z", 52.878323f, 118.09399f), module, ClairaudientModule::SHAPE2_CV_INPUT));

        // Stereo outputs - BNC connectors for consistent vintage look
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("output_l", 67.369896f, 105.77721f), module, ClairaudientModule::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("output_r", 67.369896f, 117.72548f), module, ClairaudientModule::RIGHT_OUTPUT));

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

        // Oscillator noise amount slider (0..100%)
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Oscillator Noise"));
        struct NoiseQuantity : Quantity {
            ClairaudientModule* m;
            explicit NoiseQuantity(ClairaudientModule* mod) : m(mod) {}
            void setValue(float v) override { m->oscNoiseAmount = clamp(v, 0.f, 1.f); }
            float getValue() override { return m->oscNoiseAmount; }
            float getMinValue() override { return 0.f; }
            float getMaxValue() override { return 1.f; }
            float getDefaultValue() override { return 0.f; }
            float getDisplayValue() override { return getValue() * 100.f; }
            void setDisplayValue(float v) override { setValue(v / 100.f); }
            std::string getLabel() override { return "Noise"; }
            std::string getUnit() override { return "%"; }
        };
        struct NoiseSlider : ui::Slider {
            explicit NoiseSlider(ClairaudientModule* m) { quantity = new NoiseQuantity(m); }
        };
        auto* ns = new NoiseSlider(module);
        ns->box.size.x = 200.f;
        menu->addChild(ns);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Organic Drift"));
        struct DriftQuantity : Quantity {
            ClairaudientModule* m;
            explicit DriftQuantity(ClairaudientModule* mod) : m(mod) {}
            void setValue(float v) override { m->driftAmount = clamp(v, 0.f, 1.f); }
            float getValue() override { return m->driftAmount; }
            float getMinValue() override { return 0.f; }
            float getMaxValue() override { return 1.f; }
            float getDefaultValue() override { return 0.f; }
            float getDisplayValue() override { return getValue() * 100.f; }
            void setDisplayValue(float v) override { setValue(v / 100.f); }
            std::string getLabel() override { return "Drift"; }
            std::string getUnit() override { return "%"; }
        };
        struct DriftSlider : ui::Slider {
            explicit DriftSlider(ClairaudientModule* m) { quantity = new DriftQuantity(m); }
        };
        auto* ds = new DriftSlider(module);
        ds->box.size.x = 200.f;
        menu->addChild(ds);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Tone Options"));

        menu->addChild(createCheckMenuItem("High Cut Enabled", "", [=] { return module->highCutEnabled; }, [=] {
            module->highCutEnabled = !module->highCutEnabled;
            module->resetFilters();
        }));

        menu->addChild(createSubmenuItem("Oversampling", "", [=](Menu* subMenu) {
            auto addOversampleItem = [&](const std::string& label, int factor) {
                subMenu->addChild(createCheckMenuItem(label, "", [=] { return module->oversampleFactor == factor; }, [=] {
                    module->oversampleFactor = factor;
                    module->resetFilters();
                }));
            };

            addOversampleItem("2×", 2);
            addOversampleItem("4×", 4);
            addOversampleItem("8×", 8);
        }));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Crossfade Curve"));

        auto addCrossfadeModeItem = [&](const std::string& label, int mode) {
            menu->addChild(createCheckMenuItem(label, "", [=] { return module->crossfadeMode == mode; }, [=] {
                module->crossfadeMode = mode;
            }));
        };

        addCrossfadeModeItem("Equal-Power", ClairaudientModule::CROSSFADE_EQUAL_POWER);
        addCrossfadeModeItem("Stereo Swap", ClairaudientModule::CROSSFADE_STEREO_SWAP);
    }
};

Model* modelClairaudient = createModel<ClairaudientModule, ClairaudientWidget>("Clairaudient");
