#include "plugin.hpp"
#include "reverie/dattorro_plate.hpp"
#include "reverie/reverb_modes.hpp"

struct Reverie : Module {
    enum ParamIds {
        MODE_PARAM,
        DECAY_PARAM,
        MIX_PARAM,
        TONE_PARAM,
        PARAM1_PARAM,
        PARAM2_PARAM,
        DECAY_ATT_PARAM,
        MIX_ATT_PARAM,
        PARAM1_ATT_PARAM,
        PARAM2_ATT_PARAM,
        BLEND_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_L_INPUT,
        AUDIO_R_INPUT,
        DECAY_CV_INPUT,
        MIX_CV_INPUT,
        PARAM1_CV_INPUT,
        PARAM2_CV_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        AUDIO_L_OUTPUT,
        AUDIO_R_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        MODE_LED_R,
        MODE_LED_G,
        MODE_LED_B,
        NUM_LIGHTS
    };

    enum Mode {
        FIELD_BLUR = 0,
        AFTERIMAGE = 1,
        REVERSE = 2,
        LOFI = 3,
        MODULATED = 4
    };

    // DSP
    shapetaker::PolyphonicProcessor polyProcessor;
    shapetaker::VoiceArray<shapetaker::reverie::DattorroPlate> plates;
    shapetaker::VoiceArray<shapetaker::reverie::ReverbModeProcessor> modeProcessors;

    // Parameter smoothing
    float smoothedDecay = 0.5f;
    float smoothedMix = 0.5f;
    float smoothedTone = 0.5f;
    float smoothedParam1 = 0.5f;
    float smoothedParam2 = 0.5f;
    float smoothedBlend = 1.0f;
    float smoothAlpha = 0.001f;

    // DC blocking state
    shapetaker::FloatVoices dcBlockLastInL, dcBlockLastOutL;
    shapetaker::FloatVoices dcBlockLastInR, dcBlockLastOutR;

    float currentSampleRate = 44100.0f;
    int currentMode = 0;

    Reverie() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        shapetaker::ParameterHelper::configSwitch(this, MODE_PARAM, "reverb mode",
            {"field blur", "afterimage", "reverse", "lo-fi", "modulated"}, 0);
        shapetaker::ParameterHelper::configGain(this, DECAY_PARAM, "decay", 0.5f);
        shapetaker::ParameterHelper::configMix(this, MIX_PARAM, "mix", 0.5f);
        shapetaker::ParameterHelper::configGain(this, TONE_PARAM, "tone", 0.5f);
        shapetaker::ParameterHelper::configGain(this, PARAM1_PARAM, "param 1", 0.5f);
        shapetaker::ParameterHelper::configGain(this, PARAM2_PARAM, "param 2", 0.5f);
        shapetaker::ParameterHelper::configAttenuverter(this, DECAY_ATT_PARAM, "decay cv");
        shapetaker::ParameterHelper::configAttenuverter(this, MIX_ATT_PARAM, "mix cv");
        shapetaker::ParameterHelper::configAttenuverter(this, PARAM1_ATT_PARAM, "param 1 cv");
        shapetaker::ParameterHelper::configAttenuverter(this, PARAM2_ATT_PARAM, "param 2 cv");
        shapetaker::ParameterHelper::configGain(this, BLEND_PARAM, "effect blend", 1.0f);

        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_L_INPUT, "L");
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_R_INPUT, "R");
        shapetaker::ParameterHelper::configCVInput(this, DECAY_CV_INPUT, "decay cv");
        shapetaker::ParameterHelper::configCVInput(this, MIX_CV_INPUT, "mix cv");
        shapetaker::ParameterHelper::configCVInput(this, PARAM1_CV_INPUT, "param 1 cv");
        shapetaker::ParameterHelper::configCVInput(this, PARAM2_CV_INPUT, "param 2 cv");

        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_L_OUTPUT, "L");
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_R_OUTPUT, "R");

        currentSampleRate = APP->engine->getSampleRate();
        updateSampleRate();

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void updateSampleRate() {
        plates.forEach([this](shapetaker::reverie::DattorroPlate& plate) {
            plate.setSampleRate(currentSampleRate);
        });
        modeProcessors.forEach([this](shapetaker::reverie::ReverbModeProcessor& mp) {
            mp.setSampleRate(currentSampleRate);
        });
        smoothAlpha = 1.0f - std::exp(-2.0f * (float)M_PI * 30.0f / currentSampleRate);
    }

    void onSampleRateChange() override {
        currentSampleRate = APP->engine->getSampleRate();
        updateSampleRate();
    }

    float readParam(int paramId, int cvInputId, int attId) {
        float value = params[paramId].getValue();
        if (inputs[cvInputId].isConnected()) {
            float cv = inputs[cvInputId].getVoltage() / 10.0f;
            float att = params[attId].getValue();
            value += cv * att;
        }
        return rack::math::clamp(value, 0.0f, 1.0f);
    }

    static void getModeColor(int mode, float& r, float& g, float& b) {
        switch (mode) {
            case FIELD_BLUR: r = 0.0f; g = 0.6f; b = 0.45f; break;
            case AFTERIMAGE: r = 0.35f; g = 0.1f; b = 0.55f; break;
            case REVERSE:    r = 0.55f; g = 0.35f; b = 0.1f; break;
            case LOFI:       r = 0.15f; g = 0.35f; b = 0.55f; break;
            case MODULATED:  r = 0.1f; g = 0.5f; b = 0.55f; break;
            default:         r = 0.3f; g = 0.3f; b = 0.3f; break;
        }
    }

    void process(const ProcessArgs& args) override {
        int channels = polyProcessor.getChannelCount(inputs[AUDIO_L_INPUT]);
        if (channels < 1) channels = 1;
        outputs[AUDIO_L_OUTPUT].setChannels(channels);
        outputs[AUDIO_R_OUTPUT].setChannels(channels);

        // Read mode
        int mode = rack::math::clamp((int)std::round(params[MODE_PARAM].getValue()), 0, 4);
        currentMode = mode;

        // Read and smooth parameters
        float targetDecay = readParam(DECAY_PARAM, DECAY_CV_INPUT, DECAY_ATT_PARAM);
        float targetMix = readParam(MIX_PARAM, MIX_CV_INPUT, MIX_ATT_PARAM);
        float targetTone = params[TONE_PARAM].getValue();
        float targetParam1 = readParam(PARAM1_PARAM, PARAM1_CV_INPUT, PARAM1_ATT_PARAM);
        float targetParam2 = readParam(PARAM2_PARAM, PARAM2_CV_INPUT, PARAM2_ATT_PARAM);
        float targetBlend = rack::math::clamp(params[BLEND_PARAM].getValue(), 0.0f, 1.0f);

        smoothedDecay += smoothAlpha * (targetDecay - smoothedDecay);
        smoothedMix += smoothAlpha * (targetMix - smoothedMix);
        smoothedTone += smoothAlpha * (targetTone - smoothedTone);
        smoothedParam1 += smoothAlpha * (targetParam1 - smoothedParam1);
        smoothedParam2 += smoothAlpha * (targetParam2 - smoothedParam2);
        smoothedBlend += smoothAlpha * (targetBlend - smoothedBlend);

        // Map parameters to DSP values
        float decay = 0.2f + smoothedDecay * 0.79f; // 0.2 to 0.99
        float damping = 1.0f - smoothedTone; // tone 0 = dark (high damping), tone 1 = bright (low damping)

        // Update mode LED
        float ledR, ledG, ledB;
        getModeColor(mode, ledR, ledG, ledB);
        lights[MODE_LED_R].setBrightness(ledR);
        lights[MODE_LED_G].setBrightness(ledG);
        lights[MODE_LED_B].setBrightness(ledB);

        // Process each voice
        for (int ch = 0; ch < channels; ch++) {
            float inL = inputs[AUDIO_L_INPUT].getPolyVoltage(ch);
            float inR = inputs[AUDIO_R_INPUT].isConnected()
                ? inputs[AUDIO_R_INPUT].getPolyVoltage(ch)
                : inL;

            // Normalize to ~-1..1 for DSP
            float dspInL = inL * 0.2f;
            float dspInR = inR * 0.2f;

            float wetL = 0.0f, wetR = 0.0f;

            // Blend scales P1/P2: at blend=0 both are 0 (clean plate),
            // at blend=1 they're at full value. Single signal path, no clicks.
            float blendedP1 = smoothedParam1 * smoothedBlend;
            float blendedP2 = smoothedParam2 * smoothedBlend;

            modeProcessors[ch].process(plates[ch], dspInL, dspInR,
                                       decay, damping,
                                       blendedP1, blendedP2,
                                       mode, wetL, wetR);

            // DC block wet signal
            wetL = shapetaker::AudioProcessor::processDCBlock(
                wetL, dcBlockLastInL[ch], dcBlockLastOutL[ch]);
            wetR = shapetaker::AudioProcessor::processDCBlock(
                wetR, dcBlockLastInR[ch], dcBlockLastOutR[ch]);

            // Constant power wet/dry mix
            float outL, outR;
            shapetaker::AudioProcessor::stereoConstantPowerCrossfade(
                dspInL, dspInR, wetL, wetR, smoothedMix, outL, outR);

            // Scale back to modular level and soft limit
            outL = shapetaker::AudioProcessor::softLimit(outL * 5.0f, 10.0f);
            outR = shapetaker::AudioProcessor::softLimit(outR * 5.0f, 10.0f);

            outputs[AUDIO_L_OUTPUT].setVoltage(outL, ch);
            outputs[AUDIO_R_OUTPUT].setVoltage(outR, ch);
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "mode", json_integer(currentMode));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* modeJ = json_object_get(rootJ, "mode");
        if (modeJ) {
            currentMode = json_integer_value(modeJ);
        }
    }
};

struct ReverieWidget : ModuleWidget {

    void draw(const DrawArgs& args) override {
        // Leather texture background (same pattern as Chiaroscuro)
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;
            nvgSave(args.vg);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
            nvgFill(args.vg);
            nvgRestore(args.vg);
        }
        ModuleWidget::draw(args);

        // Black inner frame
        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }

    void appendContextMenu(Menu* menu) override {
        Reverie* module = dynamic_cast<Reverie*>(this->module);
        if (!module)
            return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Current Mode Parameters"));

        int mode = module->currentMode;
        const char* p1Label = "Param 1";
        const char* p2Label = "Param 2";

        switch (mode) {
            case Reverie::FIELD_BLUR:
                p1Label = "Chorus Depth";
                p2Label = "Shimmer";
                break;
            case Reverie::AFTERIMAGE:
                p1Label = "Mod Rate";
                p2Label = "Diffusion";
                break;
            case Reverie::REVERSE:
                p1Label = "Window Size";
                p2Label = "Feedback";
                break;
            case Reverie::LOFI:
                p1Label = "Degradation";
                p2Label = "Wow/Flutter";
                break;
            case Reverie::MODULATED:
                p1Label = "Mod Depth";
                p2Label = "Detune";
                break;
        }

        std::string p1Str = std::string("Param 1: ") + p1Label;
        std::string p2Str = std::string("Param 2: ") + p2Label;
        menu->addChild(createMenuLabel(p1Str.c_str()));
        menu->addChild(createMenuLabel(p2Str.c_str()));
    }

    ReverieWidget(Reverie* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Reverie.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        auto svgPath = asset::plugin(pluginInstance, "res/panels/Reverie.svg");
        auto centerPx = LayoutHelper::createCenterPxHelper(svgPath);

        // Mode blade selector (5 positions)
        Vec selectorCenter = centerPx("mode-select", 30.0f, 15.0f);
        auto* selector = createParamCentered<ShapetakerBladeDistortionSelector>(selectorCenter, module, Reverie::MODE_PARAM);
        selector->drawDetents = true;
        addParam(selector);

        // Mode LED
        addChild(createLightCentered<JewelLEDMedium>(
            centerPx("mode-led", 57.0f, 15.0f), module, Reverie::MODE_LED_R));

        // Main knobs - XLarge (22mm)
        addParam(createParamCentered<ShapetakerKnobVintageXLarge>(
            centerPx("decay-knob", 22.0f, 32.0f), module, Reverie::DECAY_PARAM));
        addParam(createParamCentered<ShapetakerKnobVintageXLarge>(
            centerPx("mix-knob", 69.0f, 32.0f), module, Reverie::MIX_PARAM));

        // Tone knob - Medium (18mm)
        addParam(createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("tone-knob", 45.72f, 50.0f), module, Reverie::TONE_PARAM));

        // Effect blend knob - Small (between tone and params)
        addParam(createParamCentered<ShapetakerKnobVintageSmall>(
            centerPx("effect-blend", 45.72f, 57.0f), module, Reverie::BLEND_PARAM));

        // Param 1/2 knobs - Medium (18mm)
        addParam(createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("param1-knob", 22.0f, 68.0f), module, Reverie::PARAM1_PARAM));
        addParam(createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("param2-knob", 69.0f, 68.0f), module, Reverie::PARAM2_PARAM));

        // Attenuverters (8mm)
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("decay-atten", 15.0f, 80.0f), module, Reverie::DECAY_ATT_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("mix-atten", 35.0f, 80.0f), module, Reverie::MIX_ATT_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("param1-atten", 55.0f, 80.0f), module, Reverie::PARAM1_ATT_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("param2-atten", 76.0f, 80.0f), module, Reverie::PARAM2_ATT_PARAM));

        // CV inputs
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("decay-cv", 15.0f, 92.0f), module, Reverie::DECAY_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("mix-cv", 35.0f, 92.0f), module, Reverie::MIX_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("param1-cv", 55.0f, 92.0f), module, Reverie::PARAM1_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("param2-cv", 76.0f, 92.0f), module, Reverie::PARAM2_CV_INPUT));

        // Audio I/O
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("audio-in-l", 13.0f, 114.0f), module, Reverie::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("audio-in-r", 30.0f, 114.0f), module, Reverie::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("audio-out-l", 61.0f, 114.0f), module, Reverie::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("audio-out-r", 78.0f, 114.0f), module, Reverie::AUDIO_R_OUTPUT));
    }
};

Model* modelReverie = createModel<Reverie, ReverieWidget>("Reverie");
