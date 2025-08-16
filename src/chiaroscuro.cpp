#include "plugin.hpp"
#include "SidechainDetector.hpp"
#include "DistortionEngine.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>

struct Chiaroscuro : Module {
    enum ParamIds {
        VCA_PARAM,
        VCA_ATT_PARAM,
        TYPE_PARAM,
        DRIVE_PARAM,
        MIX_PARAM,
        LINK_PARAM,
        RESPONSE_PARAM,    // Linear/Exponential response switch
        MANUAL_DIST_PARAM, // Manual distortion amount
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_L_INPUT,
        AUDIO_R_INPUT,
        VCA_CV_INPUT,
        SIDECHAIN_INPUT,
        TYPE_CV_INPUT,
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
        VU_L_LED,
        VU_R_LED,
        NUM_LIGHTS
    };
    
    SidechainDetector detector;
    DistortionEngine distortion_l, distortion_r;
    dsp::ExponentialFilter vu_l_filter, vu_r_filter;
    dsp::ExponentialSlewLimiter distortion_slew;
    
    float vu_l = 0.0f;
    float vu_r = 0.0f;
    
    Chiaroscuro() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        configParam(VCA_PARAM, 0.0f, 1.0f, 0.0f, "VCA Gain", "%", 0.0f, 100.0f);
        configParam(VCA_ATT_PARAM, -1.0f, 1.0f, 0.0f, "VCA CV Attenuverter", "%", 0.0f, 100.0f);
        configParam(TYPE_PARAM, 0.0f, 5.0f, 0.0f, "Distortion Type");
        paramQuantities[TYPE_PARAM]->snapEnabled = true;
        paramQuantities[TYPE_PARAM]->smoothEnabled = false;
        configParam(DRIVE_PARAM, 0.0f, 1.0f, 0.0f, "Drive", "%", 0.0f, 100.0f);
        configParam(MIX_PARAM, 0.0f, 1.0f, 0.0f, "Mix", "%", 0.0f, 100.0f);
        configParam(LINK_PARAM, 0.0f, 1.0f, 0.0f, "Link L/R Channels");
        configParam(RESPONSE_PARAM, 0.0f, 1.0f, 0.0f, "VCA Response: Linear/Exponential");
        configParam(MANUAL_DIST_PARAM, 0.0f, 1.0f, 0.0025f, "Manual Distortion", "%", 0.0f, 100.0f);
        
        configInput(AUDIO_L_INPUT, "Audio Left");
        configInput(AUDIO_R_INPUT, "Audio Right");
        configInput(VCA_CV_INPUT, "VCA Control Voltage");
        configInput(SIDECHAIN_INPUT, "Sidechain Detector");
        configInput(TYPE_CV_INPUT, "Distortion Type CV");
        configInput(DRIVE_CV_INPUT, "Drive Amount CV");
        configInput(MIX_CV_INPUT, "Mix Control CV");
        
        configOutput(AUDIO_L_OUTPUT, "Audio Left");
        configOutput(AUDIO_R_OUTPUT, "Audio Right");
        
        vu_l_filter.setTau(0.01f);
        vu_r_filter.setTau(0.01f);
        detector.setTiming(10.0f, 200.0f);
        
        // Initialize distortion smoothing - fast enough to be responsive but slow enough to avoid clicks
        distortion_slew.setRiseFall(1000.f, 1000.f);
    }
    
    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();
        distortion_l.setSampleRate(sr);
        distortion_r.setSampleRate(sr);
        vu_l_filter.setTau(0.01f);
        vu_r_filter.setTau(0.01f);
    }
    
    void process(const ProcessArgs& args) override {
        // Link switch state
        bool linked = params[LINK_PARAM].getValue() > 0.5f;
        
        // VCA gain calculation
        float vca_gain = params[VCA_PARAM].getValue();
        
        if (inputs[VCA_CV_INPUT].isConnected()) {
            float cv = inputs[VCA_CV_INPUT].getVoltage() * 0.1f;
            cv = clamp(cv, -1.0f, 1.0f);
            float cv_mod = cv * params[VCA_ATT_PARAM].getValue() * 0.5f;
            vca_gain += cv_mod;
        }
        
        vca_gain = clamp(vca_gain, 0.0f, 1.2f);
        // Apply response curve - ONLY DEFINED ONCE HERE
        bool exponential_response = params[RESPONSE_PARAM].getValue() > 0.5f;
        if (exponential_response) {
            vca_gain = vca_gain * vca_gain; // Square for exponential
        }
        
        // Sidechain processing
        float sidechain = inputs[SIDECHAIN_INPUT].isConnected() ? 
            inputs[SIDECHAIN_INPUT].getVoltage() : 0.0f;
        sidechain = clamp(fabsf(sidechain) * 0.1f, 0.0f, 1.0f);
        
        float sc_env = detector.process(sidechain);
        
        // Distortion parameters
        float drive = params[DRIVE_PARAM].getValue();
        if (inputs[DRIVE_CV_INPUT].isConnected()) {
            drive += inputs[DRIVE_CV_INPUT].getVoltage() * 0.1f;
        }
        drive = clamp(drive, 0.0f, 1.0f);
        
        float mix = params[MIX_PARAM].getValue();
        if (inputs[MIX_CV_INPUT].isConnected()) {
            mix += inputs[MIX_CV_INPUT].getVoltage() * 0.1f;
        }
        mix = clamp(mix, 0.0f, 1.0f);
        
        int distortion_type = (int)params[TYPE_PARAM].getValue();
        if (inputs[TYPE_CV_INPUT].isConnected()) {
            float cv = inputs[TYPE_CV_INPUT].getVoltage() * 0.1f;
            distortion_type = (int)(cv * 6.0f);
        }
        distortion_type = clamp(distortion_type, 0, 5);
        
        // Calculate distortion amount - manual knob + optional sidechain envelope
        float manual_dist = params[MANUAL_DIST_PARAM].getValue();
        float sidechain_contribution = inputs[SIDECHAIN_INPUT].isConnected() ? sc_env : 0.0f;
        float combined_distortion = clamp(manual_dist + sidechain_contribution, 0.0f, 1.0f);
        
        // Apply smoothing to the combined distortion to prevent clicks
        float smoothed_distortion = distortion_slew.process(args.sampleTime, combined_distortion);
        
        // The actual distortion amount used in processing
        float distortion_amount = smoothed_distortion * drive;
        
        float red_brightness, green_brightness, blue_brightness;
        const float base_brightness = 0.6f;
        
        // Enhanced color transition with consistent brightness
        // Teal (0V): No Red, Full Green, Full Blue
        // Blue-Purple (5V): Full Red, Full Green, Full Blue (brightest)
        // Dark Purple (10V): Full Red, No Green, Reduced Blue
        const float max_brightness = base_brightness;
        
        if (smoothed_distortion <= 0.5f) {
            // 0 to 0.5: Teal to bright blue-purple
            red_brightness = smoothed_distortion * 2.0f * max_brightness;      // 0 to max
            green_brightness = max_brightness;                                 // constant max
            blue_brightness = max_brightness;                                  // constant max
        } else {
            // 0.5 to 1.0: Bright blue-purple to dark purple
            red_brightness = max_brightness;                                           // constant max
            green_brightness = 2.0f * (1.0f - smoothed_distortion) * max_brightness; // max to 0
            blue_brightness = max_brightness * (1.7f - smoothed_distortion * 0.7f);  // max to 70%
        }
        
        lights[DIST_LED_R].setBrightness(red_brightness);   
        lights[DIST_LED_G].setBrightness(green_brightness); 
        lights[DIST_LED_B].setBrightness(blue_brightness);
        
        // Audio processing
        float input_l = inputs[AUDIO_L_INPUT].getVoltage();
        float input_r = linked ? input_l : inputs[AUDIO_R_INPUT].getVoltage();
        
        float vca_l = input_l * vca_gain;
        float vca_r = input_r * vca_gain;
        
        float output_l, output_r;
        
        // Always process distortion to avoid clicks from switching between clean/distorted paths
        float distorted_l = distortion_l.process(vca_l, distortion_amount, 
                                               (DistortionEngine::Type)distortion_type);
        float distorted_r = distortion_r.process(vca_r, distortion_amount, 
                                               (DistortionEngine::Type)distortion_type);
        
        // Mix between clean and distorted signals
        output_l = vca_l + mix * (distorted_l - vca_l);
        output_r = vca_r + mix * (distorted_r - vca_r);
        
        outputs[AUDIO_L_OUTPUT].setVoltage(output_l);
        outputs[AUDIO_R_OUTPUT].setVoltage(output_r);
        
        // VU meters
        float vu_l_raw = clamp(fabsf(output_l) * 0.4f, 0.0f, 1.0f);
        float vu_r_raw = clamp(fabsf(output_r) * 0.4f, 0.0f, 1.0f);
        
        vu_l = vu_l_filter.process(args.sampleTime, vu_l_raw);
        vu_r = vu_r_filter.process(args.sampleTime, vu_r_raw);
        
        if (vu_l < 0.01f) vu_l = 0.0f;
        if (vu_r < 0.01f) vu_r = 0.0f;
        
        lights[VU_L_LED].setBrightness(vu_l);
        lights[VU_R_LED].setBrightness(vu_r);
    }
};

struct ChiaroscuroWidget : ModuleWidget {
    ChiaroscuroWidget(Chiaroscuro* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg")));
        
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // Audio I/O - BNC connectors for vintage oscilloscope look
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.18, 341.99), module, Chiaroscuro::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(73.84, 341.99), module, Chiaroscuro::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(Vec(100.33, 341.99), module, Chiaroscuro::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(Vec(153.73, 341.99), module, Chiaroscuro::AUDIO_R_OUTPUT));
        
        // Main VCA knob - oscilloscope style
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(Vec(134.40, 235.88), module, Chiaroscuro::VCA_PARAM));
        
        // VCA CV input - green circle "vca_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(111.31, 288.74), module, Chiaroscuro::VCA_CV_INPUT));
        
        // VCA attenuverter - oscilloscope style
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(Vec(153.83, 288.74), module, Chiaroscuro::VCA_ATT_PARAM));
        
        // Linear/Exponential response switch
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(Vec(39.06, 118.11), module, Chiaroscuro::RESPONSE_PARAM));
        
        // Link switch
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(Vec(90.00, 118.35), module, Chiaroscuro::LINK_PARAM));
        
        // Sidechain input - green circle "sidechain_in"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.18, 182.82), module, Chiaroscuro::SIDECHAIN_INPUT));
        
        // Manual distortion knob - positioned near sidechain input
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(73.84, 182.82), module, Chiaroscuro::MANUAL_DIST_PARAM));
        
        // Distortion type selector - vintage selector
        addParam(createParamCentered<ShapetakerVintageSelector>(Vec(117.74, 194.29), module, Chiaroscuro::TYPE_PARAM));
        
        // Type CV input - green circle "type_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(154.53, 194.29), module, Chiaroscuro::TYPE_CV_INPUT));
        
        // Drive knob - red circle "drive_amount"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(73.84, 235.88), module, Chiaroscuro::DRIVE_PARAM));
        
        // Drive CV input - green circle "drive_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.18, 235.88), module, Chiaroscuro::DRIVE_CV_INPUT));
        
        // Mix knob - red circle "mix_amount"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(73.84, 288.93), module, Chiaroscuro::MIX_PARAM));
        
        // Mix CV input - green circle "mix_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.18, 288.93), module, Chiaroscuro::MIX_CV_INPUT));

        // Add VU meters - center the widget on the yellow circle positions from panel SVG
        VUMeterWidget* vu_meter_l = new VUMeterWidget(module, &module->vu_l, "res/meters/vu_meter_face_bordered.svg", "res/meters/vu_meter_needle.svg");
        vu_meter_l->box.pos = Vec(34.505978 - 17.5, 67.63459 - 17.5); // Center widget on (34.505978, 67.63459)
        addChild(vu_meter_l);
        VUMeterWidget* vu_meter_r = new VUMeterWidget(module, &module->vu_r, "res/meters/vu_meter_face_bordered.svg", "res/meters/vu_meter_needle.svg");
        vu_meter_r->box.pos = Vec(147.92859 - 17.5, 67.63459 - 17.5); // Center widget on (147.92859, 67.63459)
        addChild(vu_meter_r);
        addChild(createLightCentered<JewelLEDLarge>(Vec(90.00, 81.712997), module, Chiaroscuro::DIST_LED_R));
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");