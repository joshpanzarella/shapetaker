#include "plugin.hpp"
#include "transmutation/ui.hpp" // for PanelPatinaOverlay (shared vintage overlay)
#include "ui/menu_helpers.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

struct ClairaudientModule : Module, IOscilloscopeSource {

    // DSP Constants
    static constexpr float MIDDLE_C_HZ = 261.626f;
    static constexpr float CV_FINE_SCALE = 1.f / 50.f;
    static constexpr float CV_SHAPE_SCALE = 1.f / 5.f;
    static constexpr float CV_XFADE_SCALE = 1.f / 10.f;
    static constexpr float OUTPUT_GAIN = 5.f;
    static constexpr float NOISE_V_PEAK = 0.45f;
    static constexpr float HIGH_CUT_HZ = 14500.f;
    static constexpr float ANTI_ALIAS_CUTOFF = 0.45f;

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

    enum WaveformMode {
        WAVEFORM_SIGMOID_SAW = 0,
        WAVEFORM_PWM = 1
    };

    // Polyphonic oscillator state (up to 8 voices for Clairaudient)
    static constexpr int MAX_POLY_VOICES = 8;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> phase1A;  // Independent phase for osc 1A per voice
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> phase1B;  // Independent phase for osc 1B per voice
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> phase2A;  // Independent phase for osc 2A per voice
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> phase2B;  // Independent phase for osc 2B per voice

    // Phase direction for Z oscillators (used by reverse sync: +1 forward, -1 reverse)
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> phaseDir2A;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> phaseDir2B;

    // Organic variation state per voice
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> drift1A;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> drift1B;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> drift2A;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> drift2B;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> noise1A;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> noise1B;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> noise2A;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> noise2B;

    // User-adjustable oscillator noise amount (0..1), exposed via context menu slider.
    // Defaults to 0.0 (off). Controls both subtle phase jitter and added noise floor.
    std::atomic<float> oscNoiseAmount = {0.0f};
    
    // --- Oscilloscope Buffering ---
    static constexpr int OSCILLOSCOPE_BUFFER_SIZE = 1024;
    std::array<std::atomic<uint64_t>, OSCILLOSCOPE_BUFFER_SIZE> oscilloscopeBufferPacked = {};
    mutable Vec oscilloscopeBuffer[OSCILLOSCOPE_BUFFER_SIZE] = {};
    std::atomic<int> oscilloscopeBufferIndex = {0};
    int oscilloscopeFrameCounter = 0;

    // Anti-aliasing filters per voice (8 voices)
    shapetaker::dsp::VoiceArray<shapetaker::dsp::OnePoleLowpass, MAX_POLY_VOICES> antiAliasFilterLeft;
    shapetaker::dsp::VoiceArray<shapetaker::dsp::OnePoleLowpass, MAX_POLY_VOICES> antiAliasFilterRight;
    shapetaker::dsp::VoiceArray<shapetaker::dsp::OnePoleLowpass, MAX_POLY_VOICES> antiAliasFilterLeftStage2;
    shapetaker::dsp::VoiceArray<shapetaker::dsp::OnePoleLowpass, MAX_POLY_VOICES> antiAliasFilterRightStage2;
    shapetaker::dsp::VoiceArray<shapetaker::dsp::OnePoleLowpass, MAX_POLY_VOICES> highCutFilterLeft;
    shapetaker::dsp::VoiceArray<shapetaker::dsp::OnePoleLowpass, MAX_POLY_VOICES> highCutFilterRight;

    // DC blocking filter state per voice (left and right channels)
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> dcLastInputL;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> dcLastOutputL;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> dcLastInputR;
    shapetaker::dsp::VoiceArray<float, MAX_POLY_VOICES> dcLastOutputR;

    shapetaker::PolyphonicProcessor polyProcessor;

    // Quantization mode settings
    std::atomic<bool> quantizeOscV = {true};  // V oscillator quantized to octaves by default
    std::atomic<bool> quantizeOscZ = {true};  // Z oscillator quantized to semitones by default
    std::atomic<int> crossfadeMode = {CROSSFADE_EQUAL_POWER};
    std::atomic<int> waveformMode = {WAVEFORM_SIGMOID_SAW};
    std::atomic<int> oversampleFactor = {2};
    std::atomic<bool> highCutEnabled = {false};
    std::atomic<float> driftAmount = {0.0f};
    std::atomic<int> oscilloscopeTheme = {shapetaker::ui::ThemeManager::DisplayTheme::PHOSPHOR};
    std::atomic<bool> pendingFilterReset = {false};

    // Parameter decimation for performance (update every N samples instead of every sample)
    static constexpr int kParamDecimation = 32;  // ~0.7ms at 44.1kHz - imperceptible latency
    int paramDecimationCounter = 0;
    static constexpr int kDriftDecimation = 64;  // Drift is extremely slow; update less often
    int driftDecimationCounter = 0;

    // Cached parameter values (updated every kParamDecimation samples)
    float cachedBasePitch1 = 0.f;
    float cachedBaseSemitoneZ = 0.f;
    float cachedFineTune1 = 0.f;
    float cachedFineTune2 = 0.f;
    float cachedShape1 = 0.5f;
    float cachedShape2 = 0.5f;
    float cachedXfade = 0.5f;
    float cachedFine1Atten = 0.f;
    float cachedFine2Atten = 0.f;
    float cachedShape1Atten = 0.f;
    float cachedShape2Atten = 0.f;
    float cachedXfadeAtten = 0.f;
    bool cachedSync1 = false;
    bool cachedSync2 = false;

    // Cached input connection states (updated every kParamDecimation samples)
    bool cachedVoct2Connected = false;
    bool cachedFine1CVConnected = false;
    bool cachedFine2CVConnected = false;
    bool cachedShape1CVConnected = false;
    bool cachedShape2CVConnected = false;
    bool cachedXfadeCVConnected = false;

    // Cached noise shaping
    float cachedOscNoiseAmount = -1.f;
    float cachedShapedNoise = 0.f;

    mutable int oscilloscopeReadIndex = 0;

    // Cached filter coefficients to avoid recompute every sample
    float cachedAntiAliasAlpha = 0.f;
    float cachedHighCutAlpha = 0.f;
    float cachedSampleRate = 0.f;
    int cachedOversample = 0;
    bool cachedHighCutEnabled = false;

    // Per-voice PRNG state for fast drift/noise updates
    shapetaker::dsp::VoiceArray<uint32_t, MAX_POLY_VOICES> rngState;

    // Update parameter snapping based on quantization modes
    void updateParameterSnapping() {
        // V Oscillator snapping
        bool quantizeV = quantizeOscV.load(std::memory_order_relaxed);
        getParamQuantity(FREQ1_PARAM)->snapEnabled = quantizeV;
        getParamQuantity(FREQ1_PARAM)->smoothEnabled = !quantizeV;

        // Z Oscillator snapping
        bool quantizeZ = quantizeOscZ.load(std::memory_order_relaxed);
        getParamQuantity(FREQ2_PARAM)->snapEnabled = quantizeZ;
        getParamQuantity(FREQ2_PARAM)->smoothEnabled = !quantizeZ;
    }

    void resetFilters() {
        antiAliasFilterLeft.reset();
        antiAliasFilterRight.reset();
        antiAliasFilterLeftStage2.reset();
        antiAliasFilterRightStage2.reset();
        highCutFilterLeft.reset();
        highCutFilterRight.reset();
    }

    void updateFilterCoefficients(float sampleRate, int oversample, bool highCut) {
        cachedSampleRate = sampleRate;
        cachedOversample = oversample;
        cachedHighCutEnabled = highCut;

        if (oversample > 1) {
            float oversampleRate = sampleRate * oversample;
            float antiAliasCutoffHz = sampleRate * ANTI_ALIAS_CUTOFF;
            cachedAntiAliasAlpha = shapetaker::dsp::OnePoleLowpass::computeAlpha(antiAliasCutoffHz, oversampleRate);
        } else {
            cachedAntiAliasAlpha = 0.f;
        }

        cachedHighCutAlpha = (highCut ? shapetaker::dsp::OnePoleLowpass::computeAlpha(HIGH_CUT_HZ, sampleRate) : 0.f);
    }

    static uint64_t packVec(float x, float y) {
        uint32_t xi = 0;
        uint32_t yi = 0;
        std::memcpy(&xi, &x, sizeof(float));
        std::memcpy(&yi, &y, sizeof(float));
        return (static_cast<uint64_t>(yi) << 32) | xi;
    }

    static Vec unpackVec(uint64_t packed) {
        uint32_t xi = static_cast<uint32_t>(packed & 0xFFFFFFFFu);
        uint32_t yi = static_cast<uint32_t>(packed >> 32);
        float x = 0.f;
        float y = 0.f;
        std::memcpy(&x, &xi, sizeof(float));
        std::memcpy(&y, &yi, sizeof(float));
        return Vec(x, y);
    }

    static inline void wrapPhase(float& phase) {
        if (phase >= 1.f) {
            phase -= 1.f;
            if (phase >= 1.f) {
                phase -= std::floor(phase);
            }
        }
    }

    // Bidirectional phase wrap for reverse sync (handles negative phase values)
    static inline void wrapPhaseBidirectional(float& phase) {
        if (phase >= 1.f) {
            phase -= 1.f;
            if (phase >= 1.f) phase -= std::floor(phase);
        } else if (phase < 0.f) {
            phase += 1.f;
            if (phase < 0.f) phase -= std::floor(phase);
        }
    }

    static inline uint32_t xorshift32(uint32_t& state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    static inline float fastUniform(uint32_t& state) {
        return xorshift32(state) * (1.0f / 4294967296.0f);
    }

    static inline float fastUniformSigned(uint32_t& state) {
        return fastUniform(state) * 2.f - 1.f;
    }

    ClairaudientModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        using ParameterHelper = shapetaker::ParameterHelper;

        // Frequency controls
        // V oscillator snaps to whole octaves (5 total values: -2, -1, 0, +1, +2)
        configParam(FREQ1_PARAM, -2.f, 2.f, 0.f, "v osc octave", " oct");

        // Z oscillator snaps to semitones (49 total values: -24 to +24 semitones)
        configParam(FREQ2_PARAM, -24.f, 24.f, 0.f, "z osc semitone", " st");

        // Initialize parameter snapping based on default quantization modes
        updateParameterSnapping();

        // FREQ1: Quantized to discrete octave steps (-2, -1, 0, +1, +2)
        // FREQ2: Quantized to semitones within 4-octave range for musical intervals
        
        // Fine tune controls (±20 cents, centered at 0 for no detune)
        configParam(FINE1_PARAM, -0.2f, 0.2f, 0.f, "v fine", " cents", 0.f, 100.f);
        configParam(FINE2_PARAM, -0.2f, 0.2f, 0.f, "z fine", " cents", 0.f, 100.f);
        
        // Fine tune CV attenuverters
        ParameterHelper::configAttenuverter(this, FINE1_ATTEN_PARAM, "v fine tune cv");
        ParameterHelper::configAttenuverter(this, FINE2_ATTEN_PARAM, "z fine tune cv");
        
        // Shape morphing controls (default to 50% for proper sigmoid)
        ParameterHelper::configGain(this, SHAPE1_PARAM, "v shape", 0.5f);
        ParameterHelper::configGain(this, SHAPE2_PARAM, "z shape", 0.5f);
        
        // Shape CV attenuverters
        ParameterHelper::configAttenuverter(this, SHAPE1_ATTEN_PARAM, "v shape cv");
        ParameterHelper::configAttenuverter(this, SHAPE2_ATTEN_PARAM, "z shape cv");
        
        // Crossfade control (centered at 0.5)
        ParameterHelper::configMix(this, XFADE_PARAM, "crossfade", 0.5f);
        
        // Crossfade CV attenuverter
        ParameterHelper::configAttenuverter(this, XFADE_ATTEN_PARAM, "crossfade cv");
        
        // Sync switches: cross-sync (V resets Z) and reverse sync (V reverses Z direction)
        configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "cross sync", {"off", "on"});
        configSwitch(SYNC2_PARAM, 0.f, 1.f, 0.f, "reverse sync", {"off", "on"});
        
        // Inputs
        ParameterHelper::configCVInput(this, VOCT1_INPUT, "v osc v/oct");
        ParameterHelper::configCVInput(this, VOCT2_INPUT, "z osc v/oct");
        ParameterHelper::configCVInput(this, FINE1_CV_INPUT, "v fine tune cv");
        ParameterHelper::configCVInput(this, FINE2_CV_INPUT, "z fine tune cv");
        ParameterHelper::configCVInput(this, SHAPE1_CV_INPUT, "v shape cv");
        ParameterHelper::configCVInput(this, SHAPE2_CV_INPUT, "z shape cv");
        ParameterHelper::configCVInput(this, XFADE_CV_INPUT, "crossfade cv");
        
        // Outputs
        ParameterHelper::configAudioOutput(this, LEFT_OUTPUT, "L");
        ParameterHelper::configAudioOutput(this, RIGHT_OUTPUT, "R");

        // Initialize phase directions to forward
        for (int i = 0; i < MAX_POLY_VOICES; ++i) {
            phaseDir2A[i] = 1.f;
            phaseDir2B[i] = 1.f;
        }

        // Seed per-voice RNG state (avoid zero)
        for (int i = 0; i < MAX_POLY_VOICES; ++i) {
            uint32_t seed = rack::random::u32();
            rngState[i] = (seed == 0u) ? 0x6d2b79f5u : seed;
        }

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "quantizeOscV", json_boolean(quantizeOscV.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "quantizeOscZ", json_boolean(quantizeOscZ.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "oscNoiseAmount", json_real(oscNoiseAmount.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "crossfadeMode", json_integer(crossfadeMode.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "waveformMode", json_integer(waveformMode.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "oversampleFactor", json_integer(oversampleFactor.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "highCutEnabled", json_boolean(highCutEnabled.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "driftAmount", json_real(driftAmount.load(std::memory_order_relaxed)));
        json_object_set_new(rootJ, "oscopeTheme", json_integer(oscilloscopeTheme.load(std::memory_order_relaxed)));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        const int prevOversample = oversampleFactor.load(std::memory_order_relaxed);
        const bool prevHighCut = highCutEnabled.load(std::memory_order_relaxed);

        json_t* quantizeVJ = json_object_get(rootJ, "quantizeOscV");
        if (quantizeVJ)
            quantizeOscV.store(json_boolean_value(quantizeVJ), std::memory_order_relaxed);

        json_t* quantizeZJ = json_object_get(rootJ, "quantizeOscZ");
        if (quantizeZJ)
            quantizeOscZ.store(json_boolean_value(quantizeZJ), std::memory_order_relaxed);

        json_t* noiseJ = json_object_get(rootJ, "oscNoiseAmount");
        if (noiseJ)
            oscNoiseAmount.store(clamp((float)json_number_value(noiseJ), 0.f, 1.f), std::memory_order_relaxed);

        json_t* xfadeModeJ = json_object_get(rootJ, "crossfadeMode");
        if (xfadeModeJ)
            crossfadeMode.store(clamp((int)json_integer_value(xfadeModeJ), CROSSFADE_EQUAL_POWER, CROSSFADE_STEREO_SWAP), std::memory_order_relaxed);

        json_t* waveformModeJ = json_object_get(rootJ, "waveformMode");
        if (waveformModeJ)
            waveformMode.store(clamp((int)json_integer_value(waveformModeJ), WAVEFORM_SIGMOID_SAW, WAVEFORM_PWM), std::memory_order_relaxed);

        json_t* oversampleJ = json_object_get(rootJ, "oversampleFactor");
        if (oversampleJ) {
            int newOversample = clamp((int)json_integer_value(oversampleJ), 1, 8);
            oversampleFactor.store(newOversample, std::memory_order_relaxed);
            if (newOversample != prevOversample)
                pendingFilterReset.store(true, std::memory_order_relaxed);
        }

        json_t* highCutJ = json_object_get(rootJ, "highCutEnabled");
        if (highCutJ) {
            bool newHighCut = json_boolean_value(highCutJ);
            highCutEnabled.store(newHighCut, std::memory_order_relaxed);
            if (newHighCut != prevHighCut)
                pendingFilterReset.store(true, std::memory_order_relaxed);
        }

        json_t* driftJ = json_object_get(rootJ, "driftAmount");
        if (driftJ)
            driftAmount.store(clamp((float)json_number_value(driftJ), 0.f, 1.f), std::memory_order_relaxed);

        json_t* oscopeThemeJ = json_object_get(rootJ, "oscopeTheme");
        if (oscopeThemeJ)
            oscilloscopeTheme.store(clamp((int)json_integer_value(oscopeThemeJ), 0, shapetaker::ui::ThemeManager::DisplayTheme::THEME_COUNT - 1), std::memory_order_relaxed);

        // Update parameter snapping after loading settings
        updateParameterSnapping();
    }

    void process(const ProcessArgs& args) override {
        if (pendingFilterReset.exchange(false, std::memory_order_acq_rel)) {
            resetFilters();
        }

        // Determine number of polyphonic voices (max 8 for Clairaudient)
        int channels = std::min(
            polyProcessor.updateChannels(
                {inputs[VOCT1_INPUT], inputs[VOCT2_INPUT]},
                {outputs[LEFT_OUTPUT], outputs[RIGHT_OUTPUT]}),
            MAX_POLY_VOICES);

        // Apply the configured oversampling factor (1×, 2×, 4×, or 8×, default 2×)
        const int oversample = std::max(1, oversampleFactor.load(std::memory_order_relaxed));
        const bool highCutEnabledLocal = highCutEnabled.load(std::memory_order_relaxed);
        const int crossfadeModeLocal = crossfadeMode.load(std::memory_order_relaxed);
        const int waveformModeLocal = waveformMode.load(std::memory_order_relaxed);
        const float driftAmountLocal = driftAmount.load(std::memory_order_relaxed);
        float oversampleRate = args.sampleRate * oversample;

        // Pre-calculate constants that are the same for all voices and oversample iterations
        const float oscNoise = oscNoiseAmount.load(std::memory_order_relaxed);
        if (oscNoise != cachedOscNoiseAmount) {
            cachedOscNoiseAmount = oscNoise;
            cachedShapedNoise = std::pow(clamp(oscNoise, 0.f, 1.f), 0.65f);
        }
        float shapedNoise = cachedShapedNoise;
        float invOversampleRate = 1.f / oversampleRate; // Pre-compute reciprocal for faster multiplication
        bool doAntiAlias = oversample > 1;

        if (args.sampleRate != cachedSampleRate || oversample != cachedOversample || highCutEnabledLocal != cachedHighCutEnabled) {
            updateFilterCoefficients(args.sampleRate, oversample, highCutEnabledLocal);
        }
        float antiAliasAlpha = cachedAntiAliasAlpha;
        float highCutAlpha = cachedHighCutAlpha;

        // Parameter decimation: only read parameters every N samples for performance
        // ~0.7ms latency at 44.1kHz is imperceptible but saves ~15-20% CPU
        if (paramDecimationCounter == 0) {
            // Cache base parameter values (before CV modulation)
            cachedBasePitch1 = params[FREQ1_PARAM].getValue();
            if (quantizeOscV.load(std::memory_order_relaxed))
                cachedBasePitch1 = shapetaker::dsp::PitchHelper::quantizeToOctave(cachedBasePitch1);

            cachedBaseSemitoneZ = params[FREQ2_PARAM].getValue();
            if (quantizeOscZ.load(std::memory_order_relaxed))
                cachedBaseSemitoneZ = shapetaker::dsp::PitchHelper::quantizeToSemitone(cachedBaseSemitoneZ, 24.f);

            cachedFineTune1 = params[FINE1_PARAM].getValue();
            cachedFineTune2 = params[FINE2_PARAM].getValue();
            cachedShape1 = params[SHAPE1_PARAM].getValue();
            cachedShape2 = params[SHAPE2_PARAM].getValue();
            cachedXfade = params[XFADE_PARAM].getValue();
            cachedFine1Atten = params[FINE1_ATTEN_PARAM].getValue();
            cachedFine2Atten = params[FINE2_ATTEN_PARAM].getValue();
            cachedShape1Atten = params[SHAPE1_ATTEN_PARAM].getValue();
            cachedShape2Atten = params[SHAPE2_ATTEN_PARAM].getValue();
            cachedXfadeAtten = params[XFADE_ATTEN_PARAM].getValue();
            cachedSync1 = params[SYNC1_PARAM].getValue() > 0.5f;
            cachedSync2 = params[SYNC2_PARAM].getValue() > 0.5f;

            // Cache input connection states
            cachedVoct2Connected = inputs[VOCT2_INPUT].isConnected();
            cachedFine1CVConnected = inputs[FINE1_CV_INPUT].isConnected();
            cachedFine2CVConnected = inputs[FINE2_CV_INPUT].isConnected();
            cachedShape1CVConnected = inputs[SHAPE1_CV_INPUT].isConnected();
            cachedShape2CVConnected = inputs[SHAPE2_CV_INPUT].isConnected();
            cachedXfadeCVConnected = inputs[XFADE_CV_INPUT].isConnected();
        }
        paramDecimationCounter = (paramDecimationCounter + 1) % kParamDecimation;

        // Drift updates can be decimated without audible impact (extremely slow movement).
        bool updateDrift = (driftDecimationCounter == 0);
        float driftSampleTime = updateDrift ? args.sampleTime * kDriftDecimation : args.sampleTime;
        driftDecimationCounter = (driftDecimationCounter + 1) % kDriftDecimation;

        // Pre-calculate crossfade coefficients for the common (no CV) case
        float xfadeClampedGlobal = clamp(cachedXfade, 0.f, 1.f);
        float xfadeAngleGlobal = xfadeClampedGlobal * (float)M_PI_2;
        float xfadeCosGlobal = std::cos(xfadeAngleGlobal);
        float xfadeSinGlobal = std::sin(xfadeAngleGlobal);
        float widthBlendGlobal = std::sin(xfadeClampedGlobal * (float)M_PI);

        // Process each voice
        for (int ch = 0; ch < channels; ch++) {
            float finalLeft = 0.f;
            float finalRight = 0.f;

            // --- Pre-calculate parameters for this voice ---
            // Get V/Oct inputs with fallback logic (use cached connection state)
            float voct1 = inputs[VOCT1_INPUT].getPolyVoltage(ch);
            float voct2 = cachedVoct2Connected ?
                            inputs[VOCT2_INPUT].getPolyVoltage(ch) : voct1;

            // Get parameters for this voice (use cached base values)
            // V Oscillator: use pre-quantized cached value, then add CV
            float pitch1 = cachedBasePitch1 + voct1;

            // Z Oscillator: use pre-quantized cached value, then add CV
            float pitch2 = cachedBaseSemitoneZ / 12.0f + voct2;

            float fineTune1 = cachedFineTune1;
            if (cachedFine1CVConnected) {
                float cvAmount = cachedFine1Atten;
                fineTune1 = clamp(fineTune1 + inputs[FINE1_CV_INPUT].getPolyVoltage(ch) * cvAmount * CV_FINE_SCALE, -0.2f, 0.2f);
            }

            // Fine 2 CV is independent (no normalization)
            float fineTune2 = cachedFineTune2;
            if (cachedFine2CVConnected) {
                float cvAmount = cachedFine2Atten;
                fineTune2 = clamp(fineTune2 + inputs[FINE2_CV_INPUT].getPolyVoltage(ch) * cvAmount * CV_FINE_SCALE, -0.2f, 0.2f);
            }

            // Convert semitone offsets to octaves
            fineTune1 /= 12.f;
            fineTune2 /= 12.f;

            // Get shape parameters with attenuverters (use cached base values)
            float shape1 = cachedShape1;
            if (cachedShape1CVConnected) {
                float cvAmount = cachedShape1Atten;
                shape1 = clamp(shape1 + inputs[SHAPE1_CV_INPUT].getPolyVoltage(ch) * cvAmount * CV_SHAPE_SCALE, 0.f, 1.f);
            }

            // Shape 2 CV is independent (no normalization)
            float shape2 = cachedShape2;
            if (cachedShape2CVConnected) {
                float cvAmount = cachedShape2Atten;
                shape2 = clamp(shape2 + inputs[SHAPE2_CV_INPUT].getPolyVoltage(ch) * cvAmount * CV_SHAPE_SCALE, 0.f, 1.f);
            }

            // Get crossfade parameter with attenuverter (use cached base value)
            float xfade = cachedXfade;
            if (cachedXfadeCVConnected) {
                float cvAmount = cachedXfadeAtten;
                xfade = clamp(xfade + inputs[XFADE_CV_INPUT].getPolyVoltage(ch) * cvAmount * CV_XFADE_SCALE, 0.f, 1.f);
            }
            float xfadeClamped = cachedXfadeCVConnected ? clamp(xfade, 0.f, 1.f) : xfadeClampedGlobal;

            // Add organic frequency drift (very subtle) for this voice - once per process() call
            updateOrganicDrift(ch, driftSampleTime, driftAmountLocal, updateDrift);

            // Pre-calculate frequencies outside oversample loop (major optimization)
            // Use exp2f() instead of std::pow(2.f, x) for ~2-3x faster computation
            // Symmetric detune: A goes flat by half, B goes sharp by half — keeps center pitch stable
            float halfFine1 = fineTune1 * 0.5f;
            float halfFine2 = fineTune2 * 0.5f;
            float freq1A = MIDDLE_C_HZ * exp2f(pitch1 - halfFine1 + drift1A[ch]);
            float freq1B = MIDDLE_C_HZ * exp2f(pitch1 + halfFine1 + drift1B[ch]);
            float freq2A = MIDDLE_C_HZ * exp2f(pitch2 - halfFine2 + drift2A[ch]);
            float freq2B = MIDDLE_C_HZ * exp2f(pitch2 + halfFine2 + drift2B[ch]);

            // Use cached sync switch states (doesn't change during oversampling)
            bool sync1 = cachedSync1;
            bool sync2 = cachedSync2;

            // Pre-calculate phase deltas using multiplication instead of division (faster)
            float deltaPhase1A = freq1A * invOversampleRate;
            float deltaPhase1B = freq1B * invOversampleRate;
            float deltaPhase2A = freq2A * invOversampleRate;
            float deltaPhase2B = freq2B * invOversampleRate;

            // Pre-calculate crossfade coefficients outside loop to avoid repeated sin/cos
            float xfadeAngle = xfadeClamped * (float)M_PI_2;
            float xfadeCos = cachedXfadeCVConnected ? std::cos(xfadeAngle) : xfadeCosGlobal;
            float xfadeSin = cachedXfadeCVConnected ? std::sin(xfadeAngle) : xfadeSinGlobal;
            bool stereoSwap = (crossfadeModeLocal == CROSSFADE_STEREO_SWAP);
            // Width accent for swap: crossfeed with opposite polarity peaks at mid fade
            float widthBlend = cachedXfadeCVConnected ? std::sin(xfadeClamped * (float)M_PI) : widthBlendGlobal;
            float widthGain = 0.35f * widthBlend;

            const float noiseScale = 0.00005f * shapedNoise;
            if (waveformModeLocal == WAVEFORM_PWM) {
                for (int os = 0; os < oversample; os++) {

                    // Add subtle phase noise for organic character (scaled by shaped user amount)
                    phase1A[ch] += deltaPhase1A + noise1A[ch] * noiseScale;
                    phase1B[ch] += deltaPhase1B + noise1B[ch] * noiseScale;
                    phase2A[ch] += deltaPhase2A * phaseDir2A[ch] + noise2A[ch] * noiseScale;
                    phase2B[ch] += deltaPhase2B * phaseDir2B[ch] + noise2B[ch] * noiseScale;

                    wrapPhase(phase1A[ch]);
                    wrapPhase(phase1B[ch]);
                    wrapPhaseBidirectional(phase2A[ch]);
                    wrapPhaseBidirectional(phase2B[ch]);

                    // Detect V master (1A) cycle completion
                    bool vCycleComplete = phase1A[ch] < deltaPhase1A;

                    // Cross-sync: V master resets Z slave phases
                    if (sync1 && vCycleComplete) {
                        phase2A[ch] = phase1A[ch];
                        phase2B[ch] = phase1A[ch];
                        phaseDir2A[ch] = 1.f;
                        phaseDir2B[ch] = 1.f;
                    }

                    // Reverse sync: V master reverses Z slave direction
                    if (sync2 && !sync1 && vCycleComplete) {
                        phaseDir2A[ch] = -phaseDir2A[ch];
                        phaseDir2B[ch] = -phaseDir2B[ch];
                    }

                    // Reset direction when neither sync is active
                    if (!sync1 && !sync2) {
                        phaseDir2A[ch] = 1.f;
                        phaseDir2B[ch] = 1.f;
                    }

                    // PWM mode - shape parameter controls pulse width
                    float osc1A = shapetaker::dsp::OscillatorHelper::pwmWithPolyBLEP(phase1A[ch], shape1, freq1A, oversampleRate);
                    float osc1B = shapetaker::dsp::OscillatorHelper::pwmWithPolyBLEP(phase1B[ch], shape1, freq1B, oversampleRate);
                    float osc2A = shapetaker::dsp::OscillatorHelper::pwmWithPolyBLEP(phase2A[ch], shape2, freq2A, oversampleRate);
                    float osc2B = shapetaker::dsp::OscillatorHelper::pwmWithPolyBLEP(phase2B[ch], shape2, freq2B, oversampleRate);

                    float leftOutput;
                    float rightOutput;

                    // Use pre-calculated trig values to avoid sin/cos in hot loop
                    if (!stereoSwap) {
                        leftOutput = osc1A * xfadeCos + osc2A * xfadeSin;
                        rightOutput = osc1B * xfadeCos + osc2B * xfadeSin;
                    } else {
                        float baseLeft = osc1A * xfadeCos + osc2B * xfadeSin;
                        float baseRight = osc1B * xfadeCos + osc2A * xfadeSin;
                        // Out-of-phase crossfeed widens and makes swap distinct from equal-power
                        float leftCross = -(osc1B * (1.f - xfadeClamped) + osc2A * xfadeClamped);
                        float rightCross = -(osc1A * (1.f - xfadeClamped) + osc2B * xfadeClamped);
                        leftOutput = baseLeft + widthGain * leftCross;
                        rightOutput = baseRight + widthGain * rightCross;
                    }

                    // Apply anti-aliasing filter to each channel separately for true stereo
                    float filteredLeft = doAntiAlias
                        ? antiAliasFilterLeftStage2[ch].processWithAlpha(
                            antiAliasFilterLeft[ch].processWithAlpha(leftOutput, antiAliasAlpha),
                            antiAliasAlpha)
                        : leftOutput;
                    float filteredRight = doAntiAlias
                        ? antiAliasFilterRightStage2[ch].processWithAlpha(
                            antiAliasFilterRight[ch].processWithAlpha(rightOutput, antiAliasAlpha),
                            antiAliasAlpha)
                        : rightOutput;

                    finalLeft += filteredLeft;
                    finalRight += filteredRight;
                }
            } else {
                for (int os = 0; os < oversample; os++) {

                    // Add subtle phase noise for organic character (scaled by shaped user amount)
                    phase1A[ch] += deltaPhase1A + noise1A[ch] * noiseScale;
                    phase1B[ch] += deltaPhase1B + noise1B[ch] * noiseScale;
                    phase2A[ch] += deltaPhase2A * phaseDir2A[ch] + noise2A[ch] * noiseScale;
                    phase2B[ch] += deltaPhase2B * phaseDir2B[ch] + noise2B[ch] * noiseScale;

                    wrapPhase(phase1A[ch]);
                    wrapPhase(phase1B[ch]);
                    wrapPhaseBidirectional(phase2A[ch]);
                    wrapPhaseBidirectional(phase2B[ch]);

                    // Detect V master (1A) cycle completion
                    bool vCycleComplete = phase1A[ch] < deltaPhase1A;

                    // Cross-sync: V master resets Z slave phases
                    if (sync1 && vCycleComplete) {
                        phase2A[ch] = phase1A[ch];
                        phase2B[ch] = phase1A[ch];
                        phaseDir2A[ch] = 1.f;
                        phaseDir2B[ch] = 1.f;
                    }

                    // Reverse sync: V master reverses Z slave direction
                    if (sync2 && !sync1 && vCycleComplete) {
                        phaseDir2A[ch] = -phaseDir2A[ch];
                        phaseDir2B[ch] = -phaseDir2B[ch];
                    }

                    // Reset direction when neither sync is active
                    if (!sync1 && !sync2) {
                        phaseDir2A[ch] = 1.f;
                        phaseDir2B[ch] = 1.f;
                    }

                    // Sigmoid saw mode (default)
                    float osc1A = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase1A[ch], shape1, freq1A, oversampleRate);
                    float osc1B = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase1B[ch], shape1, freq1B, oversampleRate);
                    float osc2A = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase2A[ch], shape2, freq2A, oversampleRate);
                    float osc2B = shapetaker::dsp::OscillatorHelper::organicSigmoidSaw(phase2B[ch], shape2, freq2B, oversampleRate);

                    float leftOutput;
                    float rightOutput;

                    // Use pre-calculated trig values to avoid sin/cos in hot loop
                    if (!stereoSwap) {
                        leftOutput = osc1A * xfadeCos + osc2A * xfadeSin;
                        rightOutput = osc1B * xfadeCos + osc2B * xfadeSin;
                    } else {
                        float baseLeft = osc1A * xfadeCos + osc2B * xfadeSin;
                        float baseRight = osc1B * xfadeCos + osc2A * xfadeSin;
                        // Out-of-phase crossfeed widens and makes swap distinct from equal-power
                        float leftCross = -(osc1B * (1.f - xfadeClamped) + osc2A * xfadeClamped);
                        float rightCross = -(osc1A * (1.f - xfadeClamped) + osc2B * xfadeClamped);
                        leftOutput = baseLeft + widthGain * leftCross;
                        rightOutput = baseRight + widthGain * rightCross;
                    }

                    // Apply anti-aliasing filter to each channel separately for true stereo
                    float filteredLeft = doAntiAlias
                        ? antiAliasFilterLeftStage2[ch].processWithAlpha(
                            antiAliasFilterLeft[ch].processWithAlpha(leftOutput, antiAliasAlpha),
                            antiAliasAlpha)
                        : leftOutput;
                    float filteredRight = doAntiAlias
                        ? antiAliasFilterRightStage2[ch].processWithAlpha(
                            antiAliasFilterRight[ch].processWithAlpha(rightOutput, antiAliasAlpha),
                            antiAliasAlpha)
                        : rightOutput;

                    finalLeft += filteredLeft;
                    finalRight += filteredRight;
                }
            }
            
            // Average the oversampled result for this voice
            float outL = std::tanh(finalLeft / oversample) * OUTPUT_GAIN;
            float outR = std::tanh(finalRight / oversample) * OUTPUT_GAIN;

            // DC blocking (~10 Hz high-pass) removes offset from asymmetric waveshaping
            outL = shapetaker::dsp::AudioProcessor::processDCBlock(outL, dcLastInputL[ch], dcLastOutputL[ch]);
            outR = shapetaker::dsp::AudioProcessor::processDCBlock(outR, dcLastInputR[ch], dcLastOutputR[ch]);

            // Add audible white noise floor scaled by user amount (post waveshaping, in volts)
            if (shapedNoise > 0.f) {
                float nL = (rack::random::uniform() - 0.5f) * 2.f * NOISE_V_PEAK * shapedNoise;
                float nR = (rack::random::uniform() - 0.5f) * 2.f * NOISE_V_PEAK * shapedNoise;
                outL += nL;
                outR += nR;
            }

            if (highCutEnabledLocal && highCutAlpha > 0.f) {
                outL = highCutFilterLeft[ch].processWithAlpha(outL, highCutAlpha);
                outR = highCutFilterRight[ch].processWithAlpha(outR, highCutAlpha);
            }

            outputs[LEFT_OUTPUT].setVoltage(outL, ch);
            outputs[RIGHT_OUTPUT].setVoltage(outR, ch);
            
            // Use first voice for oscilloscope display
            if (ch == 0) {
                // --- Adaptive Oscilloscope Timescale ---
                // Determine the dominant frequency based on the crossfader position
                float baseFreq1 = MIDDLE_C_HZ * exp2f(pitch1);
                float baseFreq2 = MIDDLE_C_HZ * exp2f(pitch2);
                float dominantFreq = (xfade < 0.5f) ? baseFreq1 : baseFreq2;
                dominantFreq = std::max(dominantFreq, 1.f); // Prevent division by zero or very small numbers

                const float targetCyclesInDisplay = 1.5f; // Aim to show fewer cycles for snappier updates
                int downsampleFactor = (int)roundf((targetCyclesInDisplay * args.sampleRate) / (OSCILLOSCOPE_BUFFER_SIZE * dominantFreq));
                downsampleFactor = clamp(downsampleFactor, 1, 128); // Clamp to a reasonable range
                
                // --- Oscilloscope Buffering Logic ---
                // Downsample the audio rate to fill the buffer at a reasonable speed for the UI
                oscilloscopeFrameCounter++;
                if (oscilloscopeFrameCounter >= downsampleFactor) {
                    oscilloscopeFrameCounter = 0;
                    
                    int currentIndex = oscilloscopeBufferIndex.load(std::memory_order_relaxed);
                    // Store the current output voltages in the circular buffer
                    oscilloscopeBufferPacked[currentIndex].store(packVec(outL, outR), std::memory_order_relaxed);
                    oscilloscopeBufferIndex.store((currentIndex + 1) % OSCILLOSCOPE_BUFFER_SIZE, std::memory_order_release);
                }
            }
        }

    }

    // --- IOscilloscopeSource Implementation ---
    const Vec* getOscilloscopeBuffer() const override {
        int snapshot = oscilloscopeBufferIndex.load(std::memory_order_acquire);
        oscilloscopeReadIndex = snapshot;
        for (int i = 0; i < OSCILLOSCOPE_BUFFER_SIZE; ++i) {
            uint64_t packed = oscilloscopeBufferPacked[i].load(std::memory_order_relaxed);
            oscilloscopeBuffer[i] = unpackVec(packed);
        }
        return oscilloscopeBuffer;
    }
    int getOscilloscopeBufferIndex() const override { return oscilloscopeReadIndex; }
    int getOscilloscopeBufferSize() const override { return OSCILLOSCOPE_BUFFER_SIZE; }
    int getOscilloscopeTheme() const override { return oscilloscopeTheme.load(std::memory_order_relaxed); }

private:
    // Update organic drift and noise for more natural sound (per voice)
    void updateOrganicDrift(int voice, float sampleTime, float amount, bool updateDrift) {
        amount = clamp(amount, 0.f, 1.f);
        if (amount <= 0.f) {
            drift1A[voice] = drift1B[voice] = drift2A[voice] = drift2B[voice] = 0.f;
            noise1A[voice] = noise1B[voice] = noise2A[voice] = noise2B[voice] = 0.f;
            return;
        }
        uint32_t& rng = rngState[voice];

        if (updateDrift) {
            // Very slow random walk for frequency drift (like analog oscillator aging)
            const float baseDriftSpeed = 0.00002f;
            float driftSpeed = baseDriftSpeed * amount;

            drift1A[voice] += fastUniformSigned(rng) * driftSpeed * sampleTime;
            drift1B[voice] += fastUniformSigned(rng) * driftSpeed * sampleTime;
            drift2A[voice] += fastUniformSigned(rng) * driftSpeed * sampleTime;
            drift2B[voice] += fastUniformSigned(rng) * driftSpeed * sampleTime;

            // Limit drift to very small amounts (about ±1.2 cents at full amount)
            const float driftLimit = 0.001f * amount;
            drift1A[voice] = clamp(drift1A[voice], -driftLimit, driftLimit);
            drift1B[voice] = clamp(drift1B[voice], -driftLimit, driftLimit);
            drift2A[voice] = clamp(drift2A[voice], -driftLimit, driftLimit);
            drift2B[voice] = clamp(drift2B[voice], -driftLimit, driftLimit);
        }

        // Generate subtle phase noise (keep per-sample updates to avoid dulling)
        float noiseScale = amount;
        noise1A[voice] = fastUniformSigned(rng) * noiseScale;
        noise1B[voice] = fastUniformSigned(rng) * noiseScale;
        noise2A[voice] = fastUniformSigned(rng) * noiseScale;
        noise2B[voice] = fastUniformSigned(rng) * noiseScale;
    }
};

// KnobShadowWidget is now defined in plugin.hpp and shared across all modules

struct ClairaudientWidget : ModuleWidget {
    // Match the uniform Clairaudient/Tessellation/Transmutation/Torsion leather treatment
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            // Keep leather grain density consistent across panel widths via fixed-height tiling.
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;

            nvgSave(args.vg);

            // Base tile pass
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            // Offset low-opacity pass to soften seam visibility
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            // Slight darkening to match existing module tone
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
            nvgFill(args.vg);

            nvgRestore(args.vg);
        }
        ModuleWidget::draw(args);

        // Draw a black inner frame to fully mask any edge tinting
        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }

    ClairaudientWidget(ClairaudientModule* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Clairaudient.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;

        // Keep patina/scratches beneath controls by adding the overlay early.
        auto overlay = new PanelPatinaOverlay();
        overlay->box = Rect(Vec(0, 0), box.size);
        addChild(overlay);

        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        // Use shared panel parser utilities for control placement
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Clairaudient.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        // Use global shadow helper from plugin.hpp
        auto addKnobWithShadow = [this](app::ParamWidget* knob) {
            ::addKnobWithShadow(this, knob);
        };
        
        // V/Z oscillator frequency knobs — vintage knob with background + shadow
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageMedium>(centerPx("freq_v", 13.422475f, 25.464647f), module, ClairaudientModule::FREQ1_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageMedium>(centerPx("freq_z", 68.319061f, 25.695415f), module, ClairaudientModule::FREQ2_PARAM));

        // V/Z sync switches — ShapetakerDarkToggle (9.5 x 10.7mm, black body, grey lever)
        addParam(createParamCentered<ShapetakerDarkToggle>(centerPx("sync_v", 26.023623f, 66.637276f), module, ClairaudientModule::SYNC1_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggle>(centerPx("sync_z", 55.676144f, 66.637276f), module, ClairaudientModule::SYNC2_PARAM));

        // V/Z fine tune controls — Vintage small-medium (15mm) + shadow
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("fine_v", 19.023623f, 45.841431f), module, ClairaudientModule::FINE1_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("fine_z", 62.717918f, 45.883205f), module, ClairaudientModule::FINE2_PARAM));

        // V/Z fine tune attenuverters — ShapetakerAttenuverterOscilloscope (10mm) + shadow
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("fine_atten_v", 12.023623f, 61.744068f), module, ClairaudientModule::FINE1_ATTEN_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("fine_atten_z", 69.621849f, 61.744068f), module, ClairaudientModule::FINE2_ATTEN_PARAM));

        // Crossfade control — Vintage medium (18mm) + shadow
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageMedium>(centerPx("x_fade_knob", 40.87077f, 57.091526f), module, ClairaudientModule::XFADE_PARAM));

        // Crossfade attenuverter — ShapetakerAttenuverterOscilloscope (10mm) + shadow
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("x_fade_atten", 40.639999f, 75.910126f), module, ClairaudientModule::XFADE_ATTEN_PARAM));

        // V/Z shape controls — Vintage small-medium (15mm) + shadow
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("sh_knob_v", 13.422475f, 79.825134f), module, ClairaudientModule::SHAPE1_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("sh_knob_z", 68.319061f, 79.825134f), module, ClairaudientModule::SHAPE2_PARAM));

        // V/Z shape attenuverters — ShapetakerAttenuverterOscilloscope (10mm) + shadow
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("sh_cv_v", 22.421556f, 93.003937f), module, ClairaudientModule::SHAPE1_ATTEN_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("sh_cv_z", 58.858444f, 93.003937f), module, ClairaudientModule::SHAPE2_ATTEN_PARAM));

        // Vintage oscilloscope display (draw even in module browser previews)
        {
            VintageOscilloscopeWidget* oscope = new VintageOscilloscopeWidget(module);
            Vec scrPx = centerPx("oscope_screen", 40.87077f, 29.04454f);
            constexpr float OSCOPE_SIZE_MM = 36.3f; // 10% larger
            Vec sizePx = LayoutHelper::mm2px(Vec(OSCOPE_SIZE_MM, OSCOPE_SIZE_MM));
            Vec topLeft = scrPx.minus(sizePx.div(2.f));
            oscope->box.pos = topLeft;
            oscope->box.size = sizePx;
            addChild(oscope);
        }

        // Input row 1: V oscillator — ShapetakerBNCPort (8mm)
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("v_oct_v", 23.762346f, 105.77721f), module, ClairaudientModule::VOCT1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("fine_cv_v", 38.386749f, 105.77721f), module, ClairaudientModule::FINE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("shape_cv_v", 52.878323f, 105.77721f), module, ClairaudientModule::SHAPE1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("x_fade_cv", 40.639999f, 90.126892f), module, ClairaudientModule::XFADE_CV_INPUT));

        // Input row 2: Z oscillator
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("v_out_z", 23.76195f, 118.09399f), module, ClairaudientModule::VOCT2_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("fine_cv_z", 38.386749f, 118.09399f), module, ClairaudientModule::FINE2_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("shape_cv_z", 52.878323f, 118.09399f), module, ClairaudientModule::SHAPE2_CV_INPUT));

        // Stereo outputs — ShapetakerBNCPort (8mm)
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("output_l", 67.369896f, 105.77721f), module, ClairaudientModule::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("output_r", 67.369896f, 117.72548f), module, ClairaudientModule::RIGHT_OUTPUT));

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
                bool newValue = !module->quantizeOscV.load(std::memory_order_relaxed);
                module->quantizeOscV.store(newValue, std::memory_order_relaxed);
                module->updateParameterSnapping();
            }
        };
        VQuantizeItem* vQuantizeItem = createMenuItem<VQuantizeItem>("V Oscillator Quantized");
        vQuantizeItem->rightText = module->quantizeOscV.load(std::memory_order_relaxed) ? "✓" : "";
        vQuantizeItem->module = module;
        menu->addChild(vQuantizeItem);

        // Z Oscillator quantization setting
        struct ZQuantizeItem : MenuItem {
            ClairaudientModule* module;
            void onAction(const event::Action& e) override {
                bool newValue = !module->quantizeOscZ.load(std::memory_order_relaxed);
                module->quantizeOscZ.store(newValue, std::memory_order_relaxed);
                module->updateParameterSnapping();
            }
        };
        ZQuantizeItem* zQuantizeItem = createMenuItem<ZQuantizeItem>("Z Oscillator Quantized");
        zQuantizeItem->rightText = module->quantizeOscZ.load(std::memory_order_relaxed) ? "✓" : "";
        zQuantizeItem->module = module;
        menu->addChild(zQuantizeItem);

        // Oscilloscope theme submenu - using centralized DisplayTheme system
        menu->addChild(createSubmenuItem("Oscilloscope Theme", "", [=](Menu* subMenu) {
            auto addThemeItem = [&](int theme, const char* name) {
                subMenu->addChild(createCheckMenuItem(
                    name,
                    "",
                    [=] { return module->oscilloscopeTheme.load(std::memory_order_relaxed) == theme; },
                    [=] { module->oscilloscopeTheme.store(theme, std::memory_order_relaxed); }
                ));
            };

            addThemeItem(shapetaker::ui::ThemeManager::DisplayTheme::PHOSPHOR, "Phosphor");
            addThemeItem(shapetaker::ui::ThemeManager::DisplayTheme::ICE, "Ice");
            addThemeItem(shapetaker::ui::ThemeManager::DisplayTheme::SOLAR, "Solar");
            addThemeItem(shapetaker::ui::ThemeManager::DisplayTheme::AMBER, "Amber");
        }));

        // Oscillator noise amount slider (0..100%)
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Oscillator Noise"));
        menu->addChild(shapetaker::ui::createPercentageSlider(
            module,
            [](ClairaudientModule* m, float v) { m->oscNoiseAmount.store(v, std::memory_order_relaxed); },
            [](ClairaudientModule* m) { return m->oscNoiseAmount.load(std::memory_order_relaxed); },
            "Noise"
        ));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Organic Drift"));
        menu->addChild(shapetaker::ui::createPercentageSlider(
            module,
            [](ClairaudientModule* m, float v) { m->driftAmount.store(v, std::memory_order_relaxed); },
            [](ClairaudientModule* m) { return m->driftAmount.load(std::memory_order_relaxed); },
            "Drift"
        ));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Tone Options"));

        menu->addChild(createCheckMenuItem("High Cut Enabled", "", [=] { return module->highCutEnabled.load(std::memory_order_relaxed); }, [=] {
            bool newValue = !module->highCutEnabled.load(std::memory_order_relaxed);
            module->highCutEnabled.store(newValue, std::memory_order_relaxed);
            module->pendingFilterReset.store(true, std::memory_order_relaxed);
        }));

        menu->addChild(createSubmenuItem("Oversampling", "", [=](Menu* subMenu) {
            auto addOversampleItem = [&](const std::string& label, int factor) {
                subMenu->addChild(createCheckMenuItem(label, "", [=] { return module->oversampleFactor.load(std::memory_order_relaxed) == factor; }, [=] {
                    module->oversampleFactor.store(factor, std::memory_order_relaxed);
                    module->pendingFilterReset.store(true, std::memory_order_relaxed);
                }));
            };

            addOversampleItem("1× (Off)", 1);
            addOversampleItem("2×", 2);
            addOversampleItem("4×", 4);
            addOversampleItem("8×", 8);
        }));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Waveform Mode"));

        auto addWaveformModeItem = [&](const std::string& label, int mode) {
            menu->addChild(createCheckMenuItem(label, "", [=] { return module->waveformMode.load(std::memory_order_relaxed) == mode; }, [=] {
                module->waveformMode.store(mode, std::memory_order_relaxed);
            }));
        };

        addWaveformModeItem("Sigmoid Saw", ClairaudientModule::WAVEFORM_SIGMOID_SAW);
        addWaveformModeItem("PWM", ClairaudientModule::WAVEFORM_PWM);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Crossfade Curve"));

        auto addCrossfadeModeItem = [&](const std::string& label, int mode) {
            menu->addChild(createCheckMenuItem(label, "", [=] { return module->crossfadeMode.load(std::memory_order_relaxed) == mode; }, [=] {
                module->crossfadeMode.store(mode, std::memory_order_relaxed);
            }));
        };

        addCrossfadeModeItem("Equal-Power", ClairaudientModule::CROSSFADE_EQUAL_POWER);
        addCrossfadeModeItem("Stereo Swap", ClairaudientModule::CROSSFADE_STEREO_SWAP);
    }
};

Model* modelClairaudient = createModel<ClairaudientModule, ClairaudientWidget>("Clairaudient");
