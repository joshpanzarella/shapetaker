#include "plugin.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>
#include <cmath>
#include <array>

struct Chiaroscuro : Module {
    enum ParamIds {
        VCA_PARAM,
        TYPE_PARAM,
        DIST_PARAM,        // New: Distortion knob (dist-knob)
        DIST_ATT_PARAM,    // New: Distortion attenuverter
        DRIVE_PARAM,
        DRIVE_ATT_PARAM,   // New: Drive attenuverter
        MIX_PARAM,
        MIX_ATT_PARAM,     // New: Mix attenuverter
        LINK_PARAM,
        RESPONSE_PARAM,    // Linear/Exponential response switch
        WIDTH_PARAM,       // Stereo width control
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_L_INPUT,
        AUDIO_R_INPUT,
        VCA_CV_INPUT,
        SIDECHAIN_INPUT,
        TYPE_CV_INPUT,
        DIST_CV_INPUT,     // CV input for distortion amount
        DRIVE_CV_INPUT,
        MIX_CV_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_L_OUTPUT,
        AUDIO_R_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        DIST_LED_R,
        DIST_LED_G,
        DIST_LED_B,
        SIDECHAIN_LED,     // Visual feedback for sidechain activity
        GAIN_LED_R,
        GAIN_LED_G,
        GAIN_LED_B,
        NUM_LIGHTS
    };

    // DSP constants (Clairaudient-style: keep process() free of unnamed literals)
    static constexpr float UNIT_MIN = 0.f;
    static constexpr float UNIT_MAX = 1.f;
    static constexpr float BIPOLAR_MIN = -1.f;
    static constexpr float BIPOLAR_MAX = 1.f;
    static constexpr float SWITCH_ON_THRESHOLD = 0.5f;
    static constexpr float DEFAULT_SAMPLE_RATE = 44100.f;
    static constexpr float NOMINAL_LEVEL = 5.f;          // Reference voltage used for distortion normalization
    static constexpr float CV_TO_UNIT_SCALE = 0.1f;      // 10V -> 1.0
    static constexpr float SIDECHAIN_MIN_WET_MIX = 0.8f;
    static constexpr float SIDECHAIN_DUCK_DRIVE = 0.7f;
    static constexpr float RATE_THRESHOLD_HZ10 = 6.28f;  // 2*pi*10Hz
    static constexpr float LED_INTENSITY_PEAK_HINT = 0.1f;
    static constexpr float LED_BRIGHTNESS_GAMMA = 0.55f;
    static constexpr float LED_BRIGHTNESS_BOOST = 1.8f;
    static constexpr float VCA_GAIN_MAX = 2.f;
    static constexpr float VCA_OPEN_START = 0.9f;
    static constexpr float VCA_OPEN_RANGE = 0.4f;
    static constexpr float HOT_SIGNAL_START_V = 6.f;
    static constexpr float HOT_SIGNAL_RANGE_V = 4.f;
    static constexpr float AGGRESSION_GAIN_SCALE = 0.18f;
    static constexpr float PRE_DRIVE_BOOST_SCALE = 0.35f;
    static constexpr float MAKEUP_GAIN_MIN = 0.25f;
    static constexpr float MAKEUP_GAIN_MAX = 4.f;
    static constexpr float MIX_MAKEUP_SCALE = 0.19f;     // ~+1.5dB at 100% wet
    static constexpr float OUTPUT_HEADROOM = 9.5f;
    static constexpr float GAIN_LED_BLUE_SCALE = 0.7f;
    static constexpr float DIST_SLEW_HZ = 1000.f;
    static constexpr float SIDECHAIN_ATTACK_MS = 10.f;
    static constexpr float SIDECHAIN_RELEASE_MS = 200.f;
    static constexpr float DISPLAY_SMOOTH_FAST_HZ = 15.f;
    static constexpr float DISPLAY_SMOOTH_SLOW_HZ = 2.f;
    static constexpr float CONTROL_SMOOTH_HZ = 60.f;
    static constexpr float TYPE_SMOOTH_HZ = 50.f;
    static constexpr float MAKEUP_SMOOTH_HZ = 6.f;
    static constexpr float MAKEUP_MIN_LEVEL = 1e-4f;
    static constexpr int MIN_OVERSAMPLE_FACTOR = 1;
    static constexpr int MAX_OVERSAMPLE_FACTOR = 8;
    static constexpr int DEFAULT_OVERSAMPLE_FACTOR = 4;
    static constexpr float OVERSAMPLE_LP1_CUTOFF_RATIO = 0.38f;
    static constexpr float OVERSAMPLE_POLE_RATIO = 0.55f;
    static constexpr int   DISTORTION_TYPE_COUNT    = 6; // hard clip, tube sat, wave fold, bit crush, destroy, ring mod
    static constexpr float TYPE_CV_SPAN = static_cast<float>(DISTORTION_TYPE_COUNT);
    static constexpr int MAX_DISTORTION_TYPE_INDEX = DISTORTION_TYPE_COUNT - 1;

    enum SidechainMode {
        SIDECHAIN_ENHANCEMENT = 0,
        SIDECHAIN_DUCKING = 1,
        SIDECHAIN_DIRECT = 2
    };

    struct OversampleState {
        int factor = 1;
        bool bypass = true;
        float prevInput = 0.0f;
        float lp1 = 0.0f;
        float lp2 = 0.0f;
        float lp3 = 0.0f;
        float a1 = 0.0f;
        float b1 = 0.0f;
        float a2 = 0.0f;
        float b2 = 0.0f;
        float a3 = 0.0f;
        float b3 = 0.0f;

        void configure(float baseSampleRate, int newFactor) {
            factor = rack::math::clamp(newFactor, Chiaroscuro::MIN_OVERSAMPLE_FACTOR, Chiaroscuro::MAX_OVERSAMPLE_FACTOR);
            bypass = (factor <= Chiaroscuro::MIN_OVERSAMPLE_FACTOR);
            if (bypass) {
                a1 = b1 = a2 = b2 = a3 = b3 = 0.0f;
                return;
            }

            float oversampleRate = baseSampleRate * factor;
            float cutoff = baseSampleRate * Chiaroscuro::OVERSAMPLE_LP1_CUTOFF_RATIO; // Keep below Nyquist with more attenuation
            float alpha1 = expf(-2.0f * M_PI * cutoff / oversampleRate);
            a1 = 1.0f - alpha1;
            b1 = alpha1;

            float cutoff2 = cutoff * Chiaroscuro::OVERSAMPLE_POLE_RATIO; // Slightly lower for second pole
            float alpha2 = expf(-2.0f * M_PI * cutoff2 / oversampleRate);
            a2 = 1.0f - alpha2;
            b2 = alpha2;

            float cutoff3 = cutoff2 * Chiaroscuro::OVERSAMPLE_POLE_RATIO; // Third pole for extra rejection
            float alpha3 = expf(-2.0f * M_PI * cutoff3 / oversampleRate);
            a3 = 1.0f - alpha3;
            b3 = alpha3;
        }

        void reset() {
            prevInput = 0.0f;
            lp1 = 0.0f;
            lp2 = 0.0f;
            lp3 = 0.0f;
        }

        float filter(float input) {
            if (bypass) {
                return input;
            }
            lp1 = a1 * input + b1 * lp1;
            lp2 = a2 * lp1 + b2 * lp2;
            lp3 = a3 * lp2 + b3 * lp3;
            return lp3;
        }
    };

    shapetaker::PolyphonicProcessor polyProcessor;
    shapetaker::SidechainDetector detector;
    shapetaker::VoiceArray<shapetaker::DistortionEngine> distortion_l, distortion_r;
    dsp::ExponentialSlewLimiter distortion_slew;

    // Processed parameter values (including CV modulation) for UI display
    float processed_distortion = 0.0f;
    float processed_drive = 0.0f;
    float processed_mix = 0.0f;

    // Smoothed values for LED display (prevents audio-rate flickering)
    float smoothed_distortion_display = 0.0f;
    float smoothed_drive_display = 0.0f;
    float smoothed_mix_display = 0.0f;
    int currentDistortionType = 0;
    // Smoothed distortion value for LED color calculation (same value LEDs use)
    float smoothed_distortion_for_leds = 0.0f;

    // Audio-rate smoothing for control CVs
    float smoothed_drive = 0.0f;
    float smoothed_mix = 0.0f;
    float smoothed_type = 0.0f;
    bool smoothersInitialized = false;

    // Wet/dry level tracking for auto-compensation
    shapetaker::FloatVoices cleanLevelL;
    shapetaker::FloatVoices cleanLevelR;
    shapetaker::FloatVoices wetLevelL;
    shapetaker::FloatVoices wetLevelR;
    shapetaker::FloatVoices makeupGainL;
    shapetaker::FloatVoices makeupGainR;

    std::array<OversampleState, shapetaker::PolyphonicProcessor::MAX_VOICES> oversampleStateL{};
    std::array<OversampleState, shapetaker::PolyphonicProcessor::MAX_VOICES> oversampleStateR{};

    // Rate-of-change tracking for adaptive smoothing
    float prev_distortion = 0.0f;
    float prev_drive = 0.0f;
    float prev_mix = 0.0f;

    // Cached smoothing coefficients (updated on sample-rate changes)
    float displaySmoothFast = 0.0f;
    float displaySmoothSlow = 0.0f;
    float controlSmoothCoeff = 0.0f;
    float typeSmoothCoeff = 0.0f;
    float levelSmoothCoeff = 0.0f;
    float makeupSmoothCoeff = 0.0f;

    // Sidechain mode (context menu): enhancement, ducking, direct
    int sidechainMode = SIDECHAIN_ENHANCEMENT;
    int oversampleFactor = DEFAULT_OVERSAMPLE_FACTOR;
    float currentSampleRate = DEFAULT_SAMPLE_RATE;

    Chiaroscuro() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        shapetaker::ParameterHelper::configGain(this, VCA_PARAM, "vca gain");
        shapetaker::ParameterHelper::configSwitch(this, TYPE_PARAM, "dist type",
            {"hard clip", "tube sat", "wave fold", "bit crush", "destroy", "ring mod"}, 0);
        shapetaker::ParameterHelper::configGain(this, DIST_PARAM, "dist %");
        shapetaker::ParameterHelper::configAttenuverter(this, DIST_ATT_PARAM, "dist cv");
        shapetaker::ParameterHelper::configDrive(this, DRIVE_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, DRIVE_ATT_PARAM, "drive cv");
        shapetaker::ParameterHelper::configMix(this, MIX_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, MIX_ATT_PARAM, "mix cv");
        shapetaker::ParameterHelper::configToggle(this, LINK_PARAM, "link L/R channels");
        shapetaker::ParameterHelper::configToggle(this, RESPONSE_PARAM, "vca resp: lin/exp");
        configParam(WIDTH_PARAM, -1.0f, 1.0f, 0.0f, "stereo width", "%", 0.f, 100.f);
        paramQuantities[WIDTH_PARAM]->description = "stereo field adj: -100% = mono, 0% = normal, +100% = wide";
        
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_L_INPUT, "L");
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_R_INPUT, "R");
        shapetaker::ParameterHelper::configCVInput(this, VCA_CV_INPUT, "vca cv");
        shapetaker::ParameterHelper::configAudioInput(this, SIDECHAIN_INPUT, "sidechain detect");
        shapetaker::ParameterHelper::configCVInput(this, TYPE_CV_INPUT, "dist type");
        shapetaker::ParameterHelper::configCVInput(this, DIST_CV_INPUT, "dist amt");
        shapetaker::ParameterHelper::configCVInput(this, DRIVE_CV_INPUT, "drive amt");
        shapetaker::ParameterHelper::configCVInput(this, MIX_CV_INPUT, "mix control");
        
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_L_OUTPUT, "L");
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_R_OUTPUT, "R");
        // Initialize distortion smoothing - fast enough to be responsive but slow enough to avoid clicks
        distortion_slew.setRiseFall(DIST_SLEW_HZ, DIST_SLEW_HZ);

        currentSampleRate = APP->engine->getSampleRate();
        detector.setTiming(SIDECHAIN_ATTACK_MS, SIDECHAIN_RELEASE_MS, currentSampleRate);
        updateSmoothingCoeffs();
        configureOversampling();
        resetLevelTracking();
        resetSmoothers();

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }
    
    void onSampleRateChange() override {
        currentSampleRate = APP->engine->getSampleRate();
        detector.setTiming(SIDECHAIN_ATTACK_MS, SIDECHAIN_RELEASE_MS, currentSampleRate);
        updateSmoothingCoeffs();
        configureOversampling();
        resetLevelTracking();
        resetSmoothers();
    }

    void resetLevelTracking() {
        cleanLevelL.reset();
        cleanLevelR.reset();
        wetLevelL.reset();
        wetLevelR.reset();
        makeupGainL.forEach([](float& g) { g = 1.0f; });
        makeupGainR.forEach([](float& g) { g = 1.0f; });
        for (auto& state : oversampleStateL) {
            state.reset();
        }
        for (auto& state : oversampleStateR) {
            state.reset();
        }
    }

    void resetSmoothers() {
        smoothersInitialized = false;
        smoothed_drive = 0.0f;
        smoothed_mix = 0.0f;
        smoothed_type = 0.0f;
    }

    void updateSmoothingCoeffs() {
        float sampleTime = 1.0f / std::max(currentSampleRate, 1.0f);
        auto coeffForHz = [sampleTime](float hz) {
            return 1.0f - expf(-2.0f * M_PI * hz * sampleTime);
        };
        displaySmoothFast = coeffForHz(DISPLAY_SMOOTH_FAST_HZ);
        displaySmoothSlow = coeffForHz(DISPLAY_SMOOTH_SLOW_HZ);
        controlSmoothCoeff = coeffForHz(CONTROL_SMOOTH_HZ);
        typeSmoothCoeff = coeffForHz(TYPE_SMOOTH_HZ);
        levelSmoothCoeff = displaySmoothFast;
        makeupSmoothCoeff = coeffForHz(MAKEUP_SMOOTH_HZ);
    }

    void configureOversampling() {
        float oversampleRate = currentSampleRate * oversampleFactor;
        distortion_l.forEach([oversampleRate](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(oversampleRate);
        });
        distortion_r.forEach([oversampleRate](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(oversampleRate);
        });
        for (auto& state : oversampleStateL) {
            state.configure(currentSampleRate, oversampleFactor);
        }
        for (auto& state : oversampleStateR) {
            state.configure(currentSampleRate, oversampleFactor);
        }
    }

    void setOversampleFactor(int factor) {
        factor = rack::math::clamp(factor, MIN_OVERSAMPLE_FACTOR, MAX_OVERSAMPLE_FACTOR);
        if (factor == oversampleFactor)
            return;
        oversampleFactor = factor;
        configureOversampling();
        resetLevelTracking();
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "sidechainMode", json_integer(sidechainMode));
        json_object_set_new(rootJ, "oversampleFactor", json_integer(oversampleFactor));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* sidechainModeJ = json_object_get(rootJ, "sidechainMode");
        if (sidechainModeJ)
            sidechainMode = rack::math::clamp((int)json_integer_value(sidechainModeJ), SIDECHAIN_ENHANCEMENT, SIDECHAIN_DIRECT);
        json_t* oversampleJ = json_object_get(rootJ, "oversampleFactor");
        if (oversampleJ) {
            int factor = json_integer_value(oversampleJ);
            setOversampleFactor(factor);
        }
    }

    static inline float clampUnit(float v) {
        return clamp(v, UNIT_MIN, UNIT_MAX);
    }

    static inline float normalizeCV10V(float cv) {
        return cv * CV_TO_UNIT_SCALE;
    }

    float getModulatedUnitParam(int paramId, int cvInputId, int attenuverterParamId) {
        float value = params[paramId].getValue();
        if (inputs[cvInputId].isConnected()) {
            value += normalizeCV10V(inputs[cvInputId].getVoltage()) * params[attenuverterParamId].getValue();
        }
        return clampUnit(value);
    }

    void process(const ProcessArgs& args) override {
        // Update polyphonic channel count and set outputs (use max of L/R for full stereo poly)
        int channels = polyProcessor.updateChannels(
            {inputs[AUDIO_L_INPUT], inputs[AUDIO_R_INPUT]},
            {outputs[AUDIO_L_OUTPUT], outputs[AUDIO_R_OUTPUT]});
        
        // Link switch state
        bool linked = params[LINK_PARAM].getValue() > SWITCH_ON_THRESHOLD;
        
        // Sidechain processing (shared across all voices)
        float sidechain = inputs[SIDECHAIN_INPUT].isConnected() ?
            inputs[SIDECHAIN_INPUT].getVoltage() : 0.0f;
        sidechain = clampUnit(fabsf(sidechain) * CV_TO_UNIT_SCALE);
        
        float sc_env = detector.process(sidechain);
        lights[SIDECHAIN_LED].setBrightness(sc_env);
        
        // Global parameters (shared across all voices)
        // Drive parameter with CV (smoothed to avoid zippering)
        float driveTarget = getModulatedUnitParam(DRIVE_PARAM, DRIVE_CV_INPUT, DRIVE_ATT_PARAM);

        // Mix parameter with CV (smoothed to avoid zippering)
        float mixTarget = getModulatedUnitParam(MIX_PARAM, MIX_CV_INPUT, MIX_ATT_PARAM);
        
        // Distortion type with CV (light smoothing to prevent chatter)
        float typeTarget = params[TYPE_PARAM].getValue();
        if (inputs[TYPE_CV_INPUT].isConnected()) {
            float cv = normalizeCV10V(inputs[TYPE_CV_INPUT].getVoltage());
            typeTarget = params[TYPE_PARAM].getValue() + cv * TYPE_CV_SPAN;
        }
        typeTarget = clamp(typeTarget, UNIT_MIN, static_cast<float>(MAX_DISTORTION_TYPE_INDEX));

        if (!smoothersInitialized) {
            smoothed_drive = driveTarget;
            smoothed_mix = mixTarget;
            smoothed_type = typeTarget;
            smoothersInitialized = true;
        } else {
            smoothed_drive += controlSmoothCoeff * (driveTarget - smoothed_drive);
            smoothed_mix += controlSmoothCoeff * (mixTarget - smoothed_mix);
            smoothed_type += typeSmoothCoeff * (typeTarget - smoothed_type);
        }

        float drive = clampUnit(smoothed_drive);
        float mix = clampUnit(smoothed_mix);
        int distortion_type = clamp((int)std::round(smoothed_type), 0, MAX_DISTORTION_TYPE_INDEX);
        currentDistortionType = distortion_type;
        
        // Distortion amount parameter with CV
        float dist_amount = getModulatedUnitParam(DIST_PARAM, DIST_CV_INPUT, DIST_ATT_PARAM);

        // Enhanced sidechain behavior with three modes
        float combined_distortion, effective_drive, effective_mix;

        if (inputs[SIDECHAIN_INPUT].isConnected()) {
            float sidechain_intensity = sc_env; // 0.0 to 1.0 based on sidechain input

            switch (sidechainMode) {
                case SIDECHAIN_ENHANCEMENT:
                    // Both distortion and drive scale together with sidechain
                    // Knob positions set the maximum values that sidechain can reach
                    combined_distortion = dist_amount * sidechain_intensity;
                    effective_drive = drive * sidechain_intensity;
                    effective_mix = fmaxf(mix, SIDECHAIN_MIN_WET_MIX);
                    break;

                case SIDECHAIN_DUCKING:
                    // Reduce distortion when sidechain is hot
                    combined_distortion = dist_amount * (1.0f - sidechain_intensity);
                    effective_drive = drive * (1.0f - sidechain_intensity * SIDECHAIN_DUCK_DRIVE);
                    effective_mix = mix;
                    break;

                case SIDECHAIN_DIRECT:
                    // Sidechain directly controls distortion amount
                    combined_distortion = sidechain_intensity;
                    effective_drive = drive;
                    effective_mix = mix;
                    break;

                default:
                    combined_distortion = dist_amount;
                    effective_drive = drive;
                    effective_mix = mix;
                    break;
            }
        } else {
            // Normal mode: knobs control directly
            combined_distortion = dist_amount;
            effective_drive = drive;
            effective_mix = mix;
        }

        // Clamp final values
        combined_distortion = clampUnit(combined_distortion);
        
        
        // Apply smoothing to the combined distortion to prevent clicks
        float smoothed_distortion = distortion_slew.process(args.sampleTime, combined_distortion);
        // Store for LED color matching in dot matrix
        smoothed_distortion_for_leds = smoothed_distortion;
        processed_distortion = smoothed_distortion;
        processed_drive = drive;
        processed_mix = mix;

        // Apply adaptive smoothing to display values to prevent audio-rate flickering
        // Calculate rate of change for each parameter
        float dist_rate = fabsf(processed_distortion - prev_distortion) / args.sampleTime;
        float drive_rate = fabsf(processed_drive - prev_drive) / args.sampleTime;
        float mix_rate = fabsf(processed_mix - prev_mix) / args.sampleTime;

        // Update previous values
        prev_distortion = processed_distortion;
        prev_drive = processed_drive;
        prev_mix = processed_mix;

        // Adaptive cutoff frequency: good response up to 10Hz, then averaging above
        // Rate threshold of ~6.28 corresponds to 10Hz sine wave at full amplitude
        float dist_smooth_factor = (dist_rate > RATE_THRESHOLD_HZ10) ? displaySmoothSlow : displaySmoothFast;
        float drive_smooth_factor = (drive_rate > RATE_THRESHOLD_HZ10) ? displaySmoothSlow : displaySmoothFast;
        float mix_smooth_factor = (mix_rate > RATE_THRESHOLD_HZ10) ? displaySmoothSlow : displaySmoothFast;

        smoothed_distortion_display += (processed_distortion - smoothed_distortion_display) * dist_smooth_factor;
        smoothed_drive_display += (processed_drive - smoothed_drive_display) * drive_smooth_factor;
        smoothed_mix_display += (processed_mix - smoothed_mix_display) * mix_smooth_factor;

        // The actual distortion amount used in processing - use effective drive for sidechain mode
        float distortion_amount = smoothed_distortion * effective_drive;

        // Distortion LED: product-based (reflects actual distortion heard) plus
        // a small peak hint so any single knob at ~half shows a subtle glow
        float product = smoothed_distortion_display * smoothed_drive_display * smoothed_mix_display;
        float peak = std::max(smoothed_distortion_display, std::max(smoothed_drive_display, smoothed_mix_display));
        float dist_intensity = clampUnit(product + peak * peak * LED_INTENSITY_PEAK_HINT);
        // Boost brightness while keeping darker base hues; allow >1.0 pre-clamp for stronger glow.
        float dist_led_brightness = std::pow(dist_intensity, LED_BRIGHTNESS_GAMMA) * LED_BRIGHTNESS_BOOST;

        // Color palette for each distortion type (normalized RGB)
        float dist_r = 0.26f;
        float dist_g = 0.34f;
        float dist_b = 0.46f;
        getDistortionTypeColor(distortion_type, dist_r, dist_g, dist_b);

        // Apply brightness to the type color, clamping per channel for LED intensity
        shapetaker::RGBColor currentLEDColor(
            clampUnit(dist_r * dist_led_brightness),
            clampUnit(dist_g * dist_led_brightness),
            clampUnit(dist_b * dist_led_brightness));

        lights[DIST_LED_R].setBrightness(currentLEDColor.r);
        lights[DIST_LED_G].setBrightness(currentLEDColor.g);
        lights[DIST_LED_B].setBrightness(currentLEDColor.b);
        
        // VCA gain calculation (polyphonic CV support)
        float base_vca_gain = params[VCA_PARAM].getValue();
        bool exponential_response = params[RESPONSE_PARAM].getValue() > SWITCH_ON_THRESHOLD;
        
        const float normalizationVoltage = NOMINAL_LEVEL;
        const float invNormalization = 1.0f / normalizationVoltage;
        float gain_led_peak = 0.0f;

        // Process each voice
        for (int ch = 0; ch < channels; ch++) {
            // Per-voice VCA gain calculation
            float vca_gain = base_vca_gain;
            
            if (inputs[VCA_CV_INPUT].isConnected()) {
                float cv = normalizeCV10V(inputs[VCA_CV_INPUT].getPolyVoltage(ch));
                cv = clamp(cv, BIPOLAR_MIN, BIPOLAR_MAX);
                vca_gain += cv; // Direct CV control without attenuverter
            }
            
            vca_gain = clamp(vca_gain, UNIT_MIN, VCA_GAIN_MAX);

            // Apply response curve
            if (exponential_response) {
                vca_gain = vca_gain * vca_gain; // Square for exponential
            }

            // No automatic polyphonic normalization here; leave gain as-is
            
            // Get audio inputs for this voice
            float input_l = inputs[AUDIO_L_INPUT].getPolyVoltage(ch);
            float input_r = linked ? input_l : 
                           (inputs[AUDIO_R_INPUT].isConnected() ? inputs[AUDIO_R_INPUT].getPolyVoltage(ch) : input_l);
            
            float base_vca_l = input_l * vca_gain;
            float base_vca_r = input_r * vca_gain;

            float openFactor = clamp((vca_gain - VCA_OPEN_START) / VCA_OPEN_RANGE, UNIT_MIN, UNIT_MAX);
            float signalPeak = fmaxf(fabsf(base_vca_l), fabsf(base_vca_r));
            float hotSignal = clamp((signalPeak - HOT_SIGNAL_START_V) / HOT_SIGNAL_RANGE_V, UNIT_MIN, UNIT_MAX);
            float aggression = openFactor * hotSignal;
            float aggression_gain = 1.0f + aggression * AGGRESSION_GAIN_SCALE;
            float pre_drive_boost = 1.0f + aggression * PRE_DRIVE_BOOST_SCALE;

            float vca_l = base_vca_l * aggression_gain;
            float vca_r = base_vca_r * aggression_gain;
            
            // Process distortion for this voice
            float normalized_l = (vca_l * pre_drive_boost) * invNormalization;
            float normalized_r = (vca_r * pre_drive_boost) * invNormalization;

            float wetNormL = 0.0f;
            float wetNormR = 0.0f;

            if (oversampleFactor <= MIN_OVERSAMPLE_FACTOR) {
                wetNormL = distortion_l[ch].process(normalized_l, distortion_amount,
                                                   (shapetaker::DistortionEngine::Type)distortion_type);
                wetNormR = distortion_r[ch].process(normalized_r, distortion_amount,
                                                   (shapetaker::DistortionEngine::Type)distortion_type);
                oversampleStateL[ch].prevInput = normalized_l;
                oversampleStateR[ch].prevInput = normalized_r;
            } else {
                auto& osL = oversampleStateL[ch];
                auto& osR = oversampleStateR[ch];
                float prevL = osL.prevInput;
                float prevR = osR.prevInput;
                float accumL = 0.0f;
                float accumR = 0.0f;
                const int factor = oversampleFactor;
                const float step = 1.0f / static_cast<float>(factor);

                for (int os = 0; os < factor; ++os) {
                    float t = (os + 1) * step;
                    float interpL = rack::math::crossfade(prevL, normalized_l, t);
                    float interpR = rack::math::crossfade(prevR, normalized_r, t);

                    float distortedL = distortion_l[ch].process(
                        interpL, distortion_amount,
                        (shapetaker::DistortionEngine::Type)distortion_type);
                    float distortedR = distortion_r[ch].process(
                        interpR, distortion_amount,
                        (shapetaker::DistortionEngine::Type)distortion_type);

                    accumL += osL.filter(distortedL);
                    accumR += osR.filter(distortedR);
                }

                osL.prevInput = normalized_l;
                osR.prevInput = normalized_r;
                wetNormL = accumL * step;
                wetNormR = accumR * step;
            }

            float wet_l = wetNormL * normalizationVoltage;
            float wet_r = wetNormR * normalizationVoltage;

            // Track RMS-like envelopes for wet/dry signals
            float cleanAbsL = fabsf(vca_l);
            float wetAbsL = fabsf(wet_l);
            cleanLevelL[ch] += (cleanAbsL - cleanLevelL[ch]) * levelSmoothCoeff;
            wetLevelL[ch] += (wetAbsL - wetLevelL[ch]) * levelSmoothCoeff;

            float cleanAbsR = fabsf(vca_r);
            float wetAbsR = fabsf(wet_r);
            cleanLevelR[ch] += (cleanAbsR - cleanLevelR[ch]) * levelSmoothCoeff;
            wetLevelR[ch] += (wetAbsR - wetLevelR[ch]) * levelSmoothCoeff;
            gain_led_peak = fmaxf(gain_led_peak, fmaxf(cleanLevelL[ch], cleanLevelR[ch]));

            float desiredGainL = 1.0f;
            float desiredGainR = 1.0f;

            if (wetLevelL[ch] > MAKEUP_MIN_LEVEL) {
                desiredGainL = (cleanLevelL[ch] > MAKEUP_MIN_LEVEL) ? cleanLevelL[ch] / wetLevelL[ch] : 1.0f;
            }
            desiredGainL = clamp(desiredGainL, MAKEUP_GAIN_MIN, MAKEUP_GAIN_MAX);
            makeupGainL[ch] += (desiredGainL - makeupGainL[ch]) * makeupSmoothCoeff;

            if (wetLevelR[ch] > MAKEUP_MIN_LEVEL) {
                desiredGainR = (cleanLevelR[ch] > MAKEUP_MIN_LEVEL) ? cleanLevelR[ch] / wetLevelR[ch] : 1.0f;
            }
            desiredGainR = clamp(desiredGainR, MAKEUP_GAIN_MIN, MAKEUP_GAIN_MAX);
            makeupGainR[ch] += (desiredGainR - makeupGainR[ch]) * makeupSmoothCoeff;

            float compensated_l = wet_l * makeupGainL[ch];
            float compensated_r = wet_r * makeupGainR[ch];

            // Mix between clean and distorted signals - use effective mix for sidechain mode
            // Simple linear crossfade with slight boost to compensate for wet/dry balance
            float output_l = vca_l * (1.0f - effective_mix) + compensated_l * effective_mix;
            float output_r = vca_r * (1.0f - effective_mix) + compensated_r * effective_mix;

            // Apply gentle makeup gain at higher mix values to compensate for level drop
            // +1.5dB at 100% mix to maintain consistent loudness
            float mix_makeup = 1.0f + effective_mix * MIX_MAKEUP_SCALE;
            output_l *= mix_makeup;
            output_r *= mix_makeup;

            // Safety check for NaN/infinity
            if (!std::isfinite(output_l)) output_l = 0.0f;
            if (!std::isfinite(output_r)) output_r = 0.0f;

            // Stereo width processing (mid/side)
            float width = params[WIDTH_PARAM].getValue();
            if (width != 0.0f) {
                // Convert to mid/side
                float mid = (output_l + output_r) * 0.5f;
                float side = (output_l - output_r) * 0.5f;

                // Apply width adjustment
                // width = -1.0: full mono (side = 0)
                // width =  0.0: normal stereo (no change)
                // width = +1.0: wide stereo (side * 2)
                float width_scale = 1.0f + width;
                side *= width_scale;

                // Convert back to L/R
                output_l = mid + side;
                output_r = mid - side;
            }

            output_l = OUTPUT_HEADROOM * std::tanh(output_l / OUTPUT_HEADROOM);
            output_r = OUTPUT_HEADROOM * std::tanh(output_r / OUTPUT_HEADROOM);

            outputs[AUDIO_L_OUTPUT].setVoltage(output_l, ch);
            outputs[AUDIO_R_OUTPUT].setVoltage(output_r, ch);
        }

        // Gain LED: Show effective VCA gain (knob + CV) with teal color
        float gain_cv_level = 0.0f;
        if (inputs[VCA_CV_INPUT].isConnected()) {
            // Use maximum effective gain across all channels
            for (int ch = 0; ch < channels; ch++) {
                float cv = normalizeCV10V(inputs[VCA_CV_INPUT].getPolyVoltage(ch));
                cv = clamp(cv, BIPOLAR_MIN, BIPOLAR_MAX);
                float effective_gain = clamp(base_vca_gain + cv, UNIT_MIN, UNIT_MAX);
                gain_cv_level = fmaxf(gain_cv_level, effective_gain);
            }
        } else {
            // No CV connected - show knob position (0-1 range maps directly)
            gain_cv_level = clamp(base_vca_gain, UNIT_MIN, UNIT_MAX);
        }

        // Teal color for gain LED (matches Channel A theme)
        // Use sqrt for better low-end visibility
        float gain_brightness = clamp(std::sqrt(gain_cv_level), UNIT_MIN, UNIT_MAX);
        lights[GAIN_LED_R].setSmoothBrightness(0.0f, args.sampleTime);
        lights[GAIN_LED_G].setSmoothBrightness(gain_brightness, args.sampleTime);
        lights[GAIN_LED_B].setSmoothBrightness(gain_brightness * GAIN_LED_BLUE_SCALE, args.sampleTime);
    }

private:
    static void getDistortionTypeColor(int type, float& r, float& g, float& b) {
        switch (type) {
            case 0: r = 0.09f; g = 0.45f; b = 0.38f; break; // Hard Clip  — teal
            case 1: r = 0.20f; g = 0.50f; b = 0.50f; break; // Tube Sat   — aqua
            case 2: r = 0.18f; g = 0.38f; b = 0.58f; break; // Wave Fold  — cyan blue
            case 3: r = 0.13f; g = 0.25f; b = 0.58f; break; // Bit Crush  — deep blue
            case 4: r = 0.28f; g = 0.20f; b = 0.58f; break; // Destroy    — violet
            case 5: r = 0.38f; g = 0.17f; b = 0.58f; break; // Ring Mod   — magenta purple
            default: r = 0.26f; g = 0.34f; b = 0.46f; break;
        }
    }
};

// Module widget
struct ChiaroscuroWidget : ShapetakerModuleWidget {

    void appendContextMenu(Menu* menu) override {
        Chiaroscuro* module = dynamic_cast<Chiaroscuro*>(this->module);
        if (!module)
            return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Oversampling"));
        menu->addChild(createCheckMenuItem("1x", "", [=]{ return module->oversampleFactor == 1; }, [=]{ module->setOversampleFactor(1); }));
        menu->addChild(createCheckMenuItem("2x", "", [=]{ return module->oversampleFactor == 2; }, [=]{ module->setOversampleFactor(2); }));
        menu->addChild(createCheckMenuItem("4x", "", [=]{ return module->oversampleFactor == 4; }, [=]{ module->setOversampleFactor(4); }));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Sidechain Mode"));
        menu->addChild(createCheckMenuItem("Enhancement (Trigger)", "", [=]{ return module->sidechainMode == Chiaroscuro::SIDECHAIN_ENHANCEMENT; }, [=]{ module->sidechainMode = Chiaroscuro::SIDECHAIN_ENHANCEMENT; }));
        menu->addChild(createCheckMenuItem("Ducking (Inverse)",     "", [=]{ return module->sidechainMode == Chiaroscuro::SIDECHAIN_DUCKING;     }, [=]{ module->sidechainMode = Chiaroscuro::SIDECHAIN_DUCKING;     }));
        menu->addChild(createCheckMenuItem("Direct Control",        "", [=]{ return module->sidechainMode == Chiaroscuro::SIDECHAIN_DIRECT;      }, [=]{ module->sidechainMode = Chiaroscuro::SIDECHAIN_DIRECT;      }));
    }

    ChiaroscuroWidget(Chiaroscuro* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;

        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        auto svgPath = asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        auto addKnobWithShadow = [this](app::ParamWidget* knob) {
            ::addKnobWithShadow(this, knob);
        };

        // Jewel LEDs
        addChild(createLightCentered<JewelLEDLarge>(
            centerPx("gain_led", 9.3945856f, 19.047686f), module, Chiaroscuro::GAIN_LED_R));
        addChild(createLightCentered<JewelLEDLarge>(
            centerPx("dist_amt_led", 52.005203f, 19.047686f), module, Chiaroscuro::DIST_LED_R));

        // Main VCA knob — Vintage XLarge (27mm)
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageXLarge>(centerPx("vca-knob", 30.699905f, 20.462957f), module, Chiaroscuro::VCA_PARAM));

        // Distortion type selector: analog blade switch with 6 positions
        auto* selector = createParamCentered<ShapetakerBladeDistortionSelector>(centerPx("dist-type-select", 30.699905f, 39.287804f), module, Chiaroscuro::TYPE_PARAM);
        selector->drawDetents = true;
        addParam(selector);

        // Toggle switches
        addParam(createParamCentered<ShapetakerDarkToggle>(centerPx("lin-lr-switch",  9.3945856f, 35.89994f), module, Chiaroscuro::LINK_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggle>(centerPx("lin-exp-switch", 52.005203f, 35.89994f), module, Chiaroscuro::RESPONSE_PARAM));

        // Section knobs — Vintage SmallMedium (15mm)
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("dist-knob",  10.691628f, 52.975525f), module, Chiaroscuro::DIST_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("drive-knob", 30.699905f, 52.975525f), module, Chiaroscuro::DRIVE_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("mix-knob",   50.708183f, 52.975525f), module, Chiaroscuro::MIX_PARAM));

        // Attenuverters
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("dist-atten",  10.691628f, 70.16468f), module, Chiaroscuro::DIST_ATT_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("drive-atten", 30.699905f, 70.16468f), module, Chiaroscuro::DRIVE_ATT_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("mix-atten",   50.708183f, 70.16468f), module, Chiaroscuro::MIX_ATT_PARAM));

        // CV inputs
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("vca-cv",             10.691628f, 101.572f),    module, Chiaroscuro::VCA_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("dist-cv",            10.691628f, 88.656288f),  module, Chiaroscuro::DIST_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("drive-cv",           30.699905f, 88.656288f),  module, Chiaroscuro::DRIVE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("dist-type-cv",       30.699905f, 101.572f),    module, Chiaroscuro::TYPE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("mix-cv",             50.708183f, 88.656288f),  module, Chiaroscuro::MIX_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("sidechain-detect-cv",50.708183f, 101.572f),    module, Chiaroscuro::SIDECHAIN_INPUT));

        // Audio I/O
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio-in-l",  10.691628f, 114.48771f), module, Chiaroscuro::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio-in-r",  24.030479f, 114.45824f), module, Chiaroscuro::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio-out-l", 37.369331f, 114.45824f), module, Chiaroscuro::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio-out-r", 50.708183f, 114.48771f), module, Chiaroscuro::AUDIO_R_OUTPUT));
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
