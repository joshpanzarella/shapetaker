#include "plugin.hpp"
#include "componentlibrary.hpp"
#include "dissolution/Voice.hpp"

#include <array>
#include <cmath>

using shapetaker::dissolution::DegradationStyle;
using shapetaker::dissolution::Voice;

struct Dissolution : Module {
    static constexpr int NUM_VOICES = 4;
    static constexpr float MAX_DEGRADATION_TIME = 5.f;   // seconds
    static constexpr float BUFFER_MARGIN = 1.5f;         // extra safety margin
    static constexpr float DEFAULT_ATTACK_SEC = 0.008f;
    static constexpr float DEFAULT_CROSSFADE_SEC = 0.012f;

    enum ParamId {
        DEGRADATION_TIME_PARAM,
        SUSTAIN_TIME_PARAM,
        FADE_TIME_PARAM,
        DEGRADE_MIX_PARAM,
        WOW_FLUTTER_PARAM,
        LOFI_PARAM,
        FILTER_SWEEP_PARAM,
        SATURATION_PARAM,
        NOISE_PARAM,
        MODE_PARAM,
        STYLE_BUTTON_PARAM,
        CLEAR_BUTTON_PARAM,
        FADE_TRIGGER_PARAM,
        VOICE1_LEVEL_PARAM,
        VOICE2_LEVEL_PARAM,
        VOICE3_LEVEL_PARAM,
        VOICE4_LEVEL_PARAM,
        DEGRADATION_TIME_ATTEN_PARAM,
        SUSTAIN_TIME_ATTEN_PARAM,
        DEGRADE_MIX_ATTEN_PARAM,
        FILTER_SWEEP_ATTEN_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,
        GATE_INPUT,
        DEGRADATION_TIME_CV_INPUT,
        SUSTAIN_TIME_CV_INPUT,
        DEGRADE_MIX_CV_INPUT,
        FILTER_SWEEP_CV_INPUT,
        FADE_TRIGGER_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        MAIN_L_OUTPUT,
        MAIN_R_OUTPUT,
        VOICE1_OUTPUT,
        VOICE2_OUTPUT,
        VOICE3_OUTPUT,
        VOICE4_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        VOICE1_LIGHT,
        VOICE2_LIGHT,
        VOICE3_LIGHT,
        VOICE4_LIGHT,
        STYLE_TAPE_LIGHT,
        STYLE_DIGITAL_LIGHT,
        STYLE_AMBIENT_LIGHT,
        STYLE_CHAOS_LIGHT,
        LIGHTS_LEN
    };

    enum class Mode {
        AUTO_FADE = 0,
        HOLD = 1,
        RETRIGGER = 2
    };

    std::array<Voice, NUM_VOICES> voices;
    DegradationStyle currentStyle = DegradationStyle::TAPE;
    Mode currentMode = Mode::AUTO_FADE;

    rack::dsp::SchmittTrigger gateTrigger;
    rack::dsp::SchmittTrigger fadeTrigger;
    rack::dsp::SchmittTrigger styleButtonTrigger;
    rack::dsp::SchmittTrigger clearButtonTrigger;

    float currentTime = 0.f;
    bool buffersAllocated = false;

    Dissolution() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Time & mix controls
        configParam(DEGRADATION_TIME_PARAM, 50.f, 5000.f, 2000.f, "Degradation time", " ms");
        configParam(SUSTAIN_TIME_PARAM, 500.f, 30000.f, 4000.f, "Sustain time", " ms");
        configParam(FADE_TIME_PARAM, 0.01f, 10.f, 1.f, "Fade time", " s");

        // Character controls
        shapetaker::ParameterHelper::configGain(this, DEGRADE_MIX_PARAM, "Degradation mix", 0.65f);
        shapetaker::ParameterHelper::configGain(this, WOW_FLUTTER_PARAM, "Wow/Flutter", 0.35f);
        shapetaker::ParameterHelper::configGain(this, LOFI_PARAM, "Lo-fi amount", 0.45f);
        shapetaker::ParameterHelper::configGain(this, FILTER_SWEEP_PARAM, "Filter sweep", 0.5f);
        shapetaker::ParameterHelper::configGain(this, SATURATION_PARAM, "Saturation", 0.3f);
        shapetaker::ParameterHelper::configGain(this, NOISE_PARAM, "Noise", 0.25f);

        // Voice mixing
        shapetaker::ParameterHelper::configGain(this, VOICE1_LEVEL_PARAM, "Voice 1 level", 1.f);
        shapetaker::ParameterHelper::configGain(this, VOICE2_LEVEL_PARAM, "Voice 2 level", 1.f);
        shapetaker::ParameterHelper::configGain(this, VOICE3_LEVEL_PARAM, "Voice 3 level", 1.f);
        shapetaker::ParameterHelper::configGain(this, VOICE4_LEVEL_PARAM, "Voice 4 level", 1.f);

        shapetaker::ParameterHelper::configSwitch(this, MODE_PARAM, "Voice mode",
            {"Auto fade", "Hold", "Retrigger"}, 0);

        // Attenuverters
        shapetaker::ParameterHelper::configAttenuverter(this, DEGRADATION_TIME_ATTEN_PARAM, "Degradation time CV");
        shapetaker::ParameterHelper::configAttenuverter(this, SUSTAIN_TIME_ATTEN_PARAM, "Sustain time CV");
        shapetaker::ParameterHelper::configAttenuverter(this, DEGRADE_MIX_ATTEN_PARAM, "Degradation mix CV");
        shapetaker::ParameterHelper::configAttenuverter(this, FILTER_SWEEP_ATTEN_PARAM, "Filter sweep CV");

        // Buttons
        configButton(STYLE_BUTTON_PARAM, "Degradation style");
        configButton(CLEAR_BUTTON_PARAM, "Clear all voices");
        configButton(FADE_TRIGGER_PARAM, "Fade trigger");

        // Inputs
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_INPUT, "Audio");
        shapetaker::ParameterHelper::configGateInput(this, GATE_INPUT, "Gate");
        shapetaker::ParameterHelper::configCVInput(this, DEGRADATION_TIME_CV_INPUT, "Degradation time CV");
        shapetaker::ParameterHelper::configCVInput(this, SUSTAIN_TIME_CV_INPUT, "Sustain time CV");
        shapetaker::ParameterHelper::configCVInput(this, DEGRADE_MIX_CV_INPUT, "Degradation mix CV");
        shapetaker::ParameterHelper::configCVInput(this, FILTER_SWEEP_CV_INPUT, "Filter sweep CV");
        shapetaker::ParameterHelper::configGateInput(this, FADE_TRIGGER_INPUT, "Fade trigger");

        // Outputs
        shapetaker::ParameterHelper::configAudioOutput(this, MAIN_L_OUTPUT, "Main left");
        shapetaker::ParameterHelper::configAudioOutput(this, MAIN_R_OUTPUT, "Main right");
        shapetaker::ParameterHelper::configAudioOutput(this, VOICE1_OUTPUT, "Voice 1");
        shapetaker::ParameterHelper::configAudioOutput(this, VOICE2_OUTPUT, "Voice 2");
        shapetaker::ParameterHelper::configAudioOutput(this, VOICE3_OUTPUT, "Voice 3");
        shapetaker::ParameterHelper::configAudioOutput(this, VOICE4_OUTPUT, "Voice 4");

        float sr = 44100.f;
        if (APP && APP->engine) {
            sr = APP->engine->getSampleRate();
        }
        for (auto& voice : voices) {
            voice.setSampleRate(sr);
            voice.setAttackTime(DEFAULT_ATTACK_SEC);
            voice.setCrossfadeTime(DEFAULT_CROSSFADE_SEC);
            voice.setFadeTime(1.f);
            voice.setLevel(1.f);
        }
    }

    void allocateBuffers() {
        if (!APP || !APP->engine) {
            return;
        }
        float sr = APP->engine->getSampleRate();
        size_t bufferSize = static_cast<size_t>(MAX_DEGRADATION_TIME * BUFFER_MARGIN * sr);
        for (auto& voice : voices) {
            voice.allocateBuffer(bufferSize);
        }
        buffersAllocated = true;
    }

    void configureVoiceTiming() {
        for (auto& voice : voices) {
            voice.setAttackTime(DEFAULT_ATTACK_SEC);
            voice.setCrossfadeTime(DEFAULT_CROSSFADE_SEC);
        }
    }

    void onSampleRateChange() override {
        float sr = 44100.f;
        if (APP && APP->engine) {
            sr = APP->engine->getSampleRate();
        }
        for (auto& voice : voices) {
            voice.setSampleRate(sr);
        }
        configureVoiceTiming();
        buffersAllocated = false;
    }

    void onAdd() override {
        buffersAllocated = false;
    }

    void onReset() override {
        for (auto& voice : voices) {
            voice.reset();
        }
        currentTime = 0.f;
        currentStyle = DegradationStyle::TAPE;
        currentMode = Mode::AUTO_FADE;
        buffersAllocated = false;
    }

    void process(const ProcessArgs& args) override {
        if (!buffersAllocated) {
            allocateBuffers();
        }

        currentTime += args.sampleTime;

        if (styleButtonTrigger.process(params[STYLE_BUTTON_PARAM].getValue())) {
            int styleIndex = ((int)currentStyle + 1) % (int)DegradationStyle::COUNT;
            currentStyle = static_cast<DegradationStyle>(styleIndex);
        }

        if (clearButtonTrigger.process(params[CLEAR_BUTTON_PARAM].getValue())) {
            clearAllVoices();
        }

        if (fadeTrigger.process(params[FADE_TRIGGER_PARAM].getValue() +
                                inputs[FADE_TRIGGER_INPUT].getVoltage())) {
            fadeAllVoices();
        }

        if (gateTrigger.process(inputs[GATE_INPUT].getVoltage())) {
            allocateVoice();
        }

        int modeIndex = rack::math::clamp((int)std::round(params[MODE_PARAM].getValue()), 0, 2);
        currentMode = static_cast<Mode>(modeIndex);

        const float MIN_DEG_MS = 50.f;
        const float MAX_DEG_MS = 5000.f;
        float degradationTimeMs = params[DEGRADATION_TIME_PARAM].getValue();
        if (inputs[DEGRADATION_TIME_CV_INPUT].isConnected()) {
            degradationTimeMs += inputs[DEGRADATION_TIME_CV_INPUT].getVoltage() *
                params[DEGRADATION_TIME_ATTEN_PARAM].getValue() * 1000.f;
        }
        degradationTimeMs = rack::math::clamp(degradationTimeMs, MIN_DEG_MS, MAX_DEG_MS);
        float degradationTime = degradationTimeMs * 0.001f;

        const float MIN_SUSTAIN_MS = 500.f;
        const float MAX_SUSTAIN_MS = 30000.f;
        float sustainTimeMs = params[SUSTAIN_TIME_PARAM].getValue();
        if (inputs[SUSTAIN_TIME_CV_INPUT].isConnected()) {
            sustainTimeMs += inputs[SUSTAIN_TIME_CV_INPUT].getVoltage() *
                params[SUSTAIN_TIME_ATTEN_PARAM].getValue() * 1000.f;
        }
        sustainTimeMs = rack::math::clamp(sustainTimeMs, MIN_SUSTAIN_MS, MAX_SUSTAIN_MS);
        float sustainTime = sustainTimeMs * 0.001f;
        if (currentMode == Mode::HOLD) {
            sustainTime = -1.f;
        }

        float fadeTime = rack::math::clamp(params[FADE_TIME_PARAM].getValue(), 0.01f, 10.f);

        float degradeMix = params[DEGRADE_MIX_PARAM].getValue();
        if (inputs[DEGRADE_MIX_CV_INPUT].isConnected()) {
            degradeMix += inputs[DEGRADE_MIX_CV_INPUT].getVoltage() *
                params[DEGRADE_MIX_ATTEN_PARAM].getValue() * 0.1f;
        }
        degradeMix = rack::math::clamp(degradeMix, 0.f, 1.f);

        float wowAmount = rack::math::clamp(params[WOW_FLUTTER_PARAM].getValue(), 0.f, 1.f);
        float lofiAmount = rack::math::clamp(params[LOFI_PARAM].getValue(), 0.f, 1.f);

        float filterSweep = params[FILTER_SWEEP_PARAM].getValue();
        if (inputs[FILTER_SWEEP_CV_INPUT].isConnected()) {
            filterSweep += inputs[FILTER_SWEEP_CV_INPUT].getVoltage() *
                params[FILTER_SWEEP_ATTEN_PARAM].getValue() * 0.1f;
        }
        filterSweep = rack::math::clamp(filterSweep, 0.f, 1.f);

        float saturationAmount = rack::math::clamp(params[SATURATION_PARAM].getValue(), 0.f, 1.f);
        float noiseAmount = rack::math::clamp(params[NOISE_PARAM].getValue(), 0.f, 1.f);

        float audioIn = inputs[AUDIO_INPUT].getVoltage();

        float mainLeft = 0.f;
        float mainRight = 0.f;
        int activeVoices = 0;

        auto panGain = [](float pan, float& gainL, float& gainR) {
            pan = rack::math::clamp(pan, -1.f, 1.f);
            gainL = std::sqrt(0.5f * (1.f - pan));
            gainR = std::sqrt(0.5f * (1.f + pan));
        };
        constexpr float panPositions[NUM_VOICES] = {-0.75f, -0.25f, 0.25f, 0.75f};

        for (int i = 0; i < NUM_VOICES; ++i) {
            auto& voice = voices[i];
            voice.setFadeTime(fadeTime);
            voice.setLevel(params[VOICE1_LEVEL_PARAM + i].getValue());
            voice.setWowFlutter(wowAmount);
            voice.setLoFi(lofiAmount);
            voice.setFilterSweep(filterSweep);
            voice.setSaturation(saturationAmount);
            voice.setNoise(noiseAmount);

            float voiceOut = voice.process(audioIn, currentTime, degradationTime,
                                           sustainTime, currentStyle, degradeMix,
                                           args.sampleTime);

            outputs[VOICE1_OUTPUT + i].setVoltage(voiceOut);

            float gainL = 0.f;
            float gainR = 0.f;
            panGain(panPositions[i], gainL, gainR);
            mainLeft += voiceOut * gainL;
            mainRight += voiceOut * gainR;

            if (voice.isActive()) {
                ++activeVoices;
            }

            float lightValue = 0.f;
            switch (voice.getState()) {
                case Voice::State::RECORDING:
                    lightValue = 1.f;
                    break;
                case Voice::State::FROZEN:
                    lightValue = 0.75f;
                    break;
                case Voice::State::FADING:
                    lightValue = 0.35f;
                    break;
                case Voice::State::IDLE:
                case Voice::State::RELEASED:
                default:
                    lightValue = 0.f;
                    break;
            }
            lights[VOICE1_LIGHT + i].setBrightness(lightValue);
        }

        float mixGain = (activeVoices > 0) ? 1.f / std::sqrt(static_cast<float>(activeVoices)) : 0.f;
        outputs[MAIN_L_OUTPUT].setVoltage(rack::math::clamp(mainLeft * mixGain, -10.f, 10.f));
        outputs[MAIN_R_OUTPUT].setVoltage(rack::math::clamp(mainRight * mixGain, -10.f, 10.f));

        for (int i = 0; i < (int)DegradationStyle::COUNT; ++i) {
            float brightness = (i == (int)currentStyle) ? 1.f : 0.12f;
            lights[STYLE_TAPE_LIGHT + i].setBrightness(brightness);
        }
    }

    void allocateVoice() {
        for (auto& voice : voices) {
            if (!voice.isActive()) {
                voice.trigger(currentTime);
                return;
            }
        }

        int oldestIndex = 0;
        float oldestTime = voices[0].getStartTime();
        for (int i = 1; i < NUM_VOICES; ++i) {
            if (voices[i].getStartTime() < oldestTime) {
                oldestTime = voices[i].getStartTime();
                oldestIndex = i;
            }
        }

        voices[oldestIndex].reset();
        voices[oldestIndex].trigger(currentTime);
    }

    void clearAllVoices() {
        for (auto& voice : voices) {
            voice.reset();
        }
    }

    void fadeAllVoices() {
        for (auto& voice : voices) {
            voice.fade();
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "currentStyle", json_integer((int)currentStyle));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* styleJ = json_object_get(rootJ, "currentStyle");
        if (styleJ) {
            int value = json_integer_value(styleJ);
            value = rack::math::clamp(value, 0, (int)DegradationStyle::COUNT - 1);
            currentStyle = static_cast<DegradationStyle>(value);
        }
    }
};

struct DissolutionWidget : ModuleWidget {
    DissolutionWidget(Dissolution* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Dissolution.svg")));

        using shapetaker::ui::LayoutHelper;
        LayoutHelper::ScrewPositions::addStandardScrews<ScrewBlack>(
            this, LayoutHelper::getModuleWidth(LayoutHelper::ModuleWidth::WIDTH_20HP));

        auto mm = [](float x, float y) {
            return shapetaker::ui::LayoutHelper::mm2px(Vec(x, y));
        };

        const float colA = 16.f;
        const float colB = 34.f;
        const float colC = 52.f;
        const float colD = 70.f;
        const float colE = 88.f;

        float row = 26.f;
        const float rowStep = 15.5f;

        // Capture controls
        addParam(createParamCentered<ShapetakerKnobAltMedium>(mm(colA, row),
            module, Dissolution::DEGRADATION_TIME_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltMedium>(mm(colB, row),
            module, Dissolution::SUSTAIN_TIME_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colC, row),
            module, Dissolution::FADE_TIME_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltMedium>(mm(colD, row),
            module, Dissolution::DEGRADE_MIX_PARAM));
        addParam(createParamCentered<rack::componentlibrary::CKSSThree>(mm(colE, row),
            module, Dissolution::MODE_PARAM));

        // Style indicator lights adjacent to the mode switch
        const float styleLightX = colE + 7.f;
        const float styleLightStart = row - 9.f;
        const float styleLightSpacing = 5.f;
        addChild(createLightCentered<SmallLight<YellowLight>>(mm(styleLightX, styleLightStart),
            module, Dissolution::STYLE_TAPE_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm(styleLightX, styleLightStart + styleLightSpacing),
            module, Dissolution::STYLE_DIGITAL_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm(styleLightX, styleLightStart + styleLightSpacing * 2.f),
            module, Dissolution::STYLE_AMBIENT_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(mm(styleLightX, styleLightStart + styleLightSpacing * 3.f),
            module, Dissolution::STYLE_CHAOS_LIGHT));

        // Character row
        row += rowStep;
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colA, row),
            module, Dissolution::WOW_FLUTTER_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colB, row),
            module, Dissolution::LOFI_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colC, row),
            module, Dissolution::FILTER_SWEEP_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colD, row),
            module, Dissolution::SATURATION_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(colE, row),
            module, Dissolution::NOISE_PARAM));

        // Buttons
        row += rowStep;
        addParam(createParamCentered<rack::componentlibrary::LEDButton>(mm(colA, row),
            module, Dissolution::STYLE_BUTTON_PARAM));
        addParam(createParamCentered<rack::componentlibrary::LEDButton>(mm(colB, row),
            module, Dissolution::CLEAR_BUTTON_PARAM));
        addParam(createParamCentered<rack::componentlibrary::LEDButton>(mm(colC, row),
            module, Dissolution::FADE_TRIGGER_PARAM));

        // Voice level controls and state lights
        row += rowStep + 2.f;
        const float voiceBaseX = 22.f;
        const float voiceSpacing = 18.f;
        for (int i = 0; i < Dissolution::NUM_VOICES; ++i) {
            float x = voiceBaseX + voiceSpacing * i;
            addParam(createParamCentered<ShapetakerKnobAltSmall>(mm(x, row),
                module, Dissolution::VOICE1_LEVEL_PARAM + i));
            addChild(createLightCentered<SmallLight<GreenLight>>(mm(x, row + 6.f),
                module, Dissolution::VOICE1_LIGHT + i));
        }

        // CV inputs
        row += rowStep + 9.f;
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colA, row),
            module, Dissolution::AUDIO_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colB, row),
            module, Dissolution::GATE_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colC, row),
            module, Dissolution::DEGRADATION_TIME_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colD, row),
            module, Dissolution::SUSTAIN_TIME_CV_INPUT));

        row += rowStep * 0.9f;
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colA, row),
            module, Dissolution::DEGRADE_MIX_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colB, row),
            module, Dissolution::FILTER_SWEEP_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(colC, row),
            module, Dissolution::FADE_TRIGGER_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colD, row),
            module, Dissolution::MAIN_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(colE, row),
            module, Dissolution::MAIN_R_OUTPUT));

        row += rowStep * 0.9f;
        for (int i = 0; i < Dissolution::NUM_VOICES; ++i) {
            float x = voiceBaseX + voiceSpacing * i;
            addOutput(createOutputCentered<ShapetakerBNCPort>(mm(x, row),
                module, Dissolution::VOICE1_OUTPUT + i));
        }
    }
};

Model* modelDissolution = createModel<Dissolution, DissolutionWidget>("Dissolution");
