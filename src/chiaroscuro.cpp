#include "plugin.hpp"
#include "sidechainDetector.hpp"
#include "distortionEngine.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>

// Custom larger VU meter widget for better visibility
struct LargeVUMeterWidget : widget::Widget {
    Module* module;
    float* vu;
    std::string face_path;
    std::string needle_path;
    
    LargeVUMeterWidget(Module* module, float* vu, std::string face_path, std::string needle_path) 
        : module(module), vu(vu), face_path(face_path), needle_path(needle_path) {
        box.size = Vec(50, 50); // Larger widget size
    }
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            // Draw the VU meter face
            std::shared_ptr<Svg> face_svg = Svg::load(asset::plugin(pluginInstance, face_path));
            if (face_svg) {
                NVGcontext* vg = args.vg;
                nvgSave(vg);
                nvgTranslate(vg, box.size.x / 2 - 25, box.size.y / 2 - 25); // Center the face SVG
                nvgScale(vg, 0.7f, 0.7f); // Larger scale than original (was 0.5f)
                face_svg->draw(vg);
                nvgRestore(vg);
            }
            
            // Draw the needle if module exists and has valid VU data
            if (module && vu) {
                std::shared_ptr<Svg> needle_svg = Svg::load(asset::plugin(pluginInstance, needle_path));
                if (needle_svg) {
                    NVGcontext* vg = args.vg;
                    nvgSave(vg);
                    
                    // Position needle to match face coordinate system
                    nvgTranslate(vg, box.size.x / 2 - 25, box.size.y / 2 - 25); // Same as face positioning
                    nvgScale(vg, 0.7f, 0.7f); // Same scale as face
                    
                    // Move to center of the 100x100 face coordinate system
                    nvgTranslate(vg, 50, 50); // Center of 100x100 face SVG
                    
                    // Rotate needle based on VU value
                    // Full scale rotation: -45 degrees (left) to +45 degrees (right)
                    float angle = (*vu - 0.5f) * 90.0f * M_PI / 180.0f; // Convert to radians
                    nvgRotate(vg, angle);
                    
                    // Center the 50x50 needle SVG on the rotation point
                    nvgTranslate(vg, -25, -25);
                    needle_svg->draw(vg);
                    nvgRestore(vg);
                }
            }
        }
    }
};

// Custom textured jewel LED with layered opacity effects
struct TexturedJewelLED : ModuleLightWidget {
    TexturedJewelLED() {
        box.size = Vec(30, 30);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_large.svg"));
        
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing
        addBaseColor(nvgRGB(0xff, 0x00, 0x00)); // Red channel
        addBaseColor(nvgRGB(0x00, 0xff, 0x00)); // Green channel  
        addBaseColor(nvgRGB(0x00, 0x00, 0xff)); // Blue channel
    }
    
    void step() override {
        ModuleLightWidget::step();
        
        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness();
            float g = module->lights[firstLightId + 1].getBrightness();
            float b = module->lights[firstLightId + 2].getBrightness();
            
            // Create layered color effect with different opacities
            color = nvgRGBAf(r, g, b, fmaxf(r, fmaxf(g, b)));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness();
            float g = module->lights[firstLightId + 1].getBrightness();
            float b = module->lights[firstLightId + 2].getBrightness();
            float maxBrightness = fmaxf(r, fmaxf(g, b));
            
            float cx = box.size.x * 0.5f;
            float cy = box.size.y * 0.5f;
            
            // Draw dramatic layered jewel effect even when dim
            if (maxBrightness > 0.01f) {
                // Save current transform
                nvgSave(args.vg);
                
                // Layer 1: Large outer glow with gradient
                NVGpaint outerGlow = nvgRadialGradient(args.vg, cx, cy, 8.0f, 16.0f,
                    nvgRGBAf(r, g, b, 0.6f * maxBrightness), nvgRGBAf(r, g, b, 0.0f));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 16.0f);
                nvgFillPaint(args.vg, outerGlow);
                nvgFill(args.vg);
                
                // Layer 2: Medium ring with stronger color saturation
                NVGpaint mediumRing = nvgRadialGradient(args.vg, cx, cy, 4.0f, 11.0f,
                    nvgRGBAf(r * 1.2f, g * 1.2f, b * 1.2f, 0.9f * maxBrightness), 
                    nvgRGBAf(r, g, b, 0.3f * maxBrightness));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 11.0f);
                nvgFillPaint(args.vg, mediumRing);
                nvgFill(args.vg);
                
                // Layer 3: Inner core with high contrast
                NVGpaint innerCore = nvgRadialGradient(args.vg, cx, cy, 2.0f, 7.0f,
                    nvgRGBAf(fminf(r * 1.5f, 1.0f), fminf(g * 1.5f, 1.0f), fminf(b * 1.5f, 1.0f), 1.0f), 
                    nvgRGBAf(r, g, b, 0.7f));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 7.0f);
                nvgFillPaint(args.vg, innerCore);
                nvgFill(args.vg);
                
                // Layer 4: Bright center hotspot
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 3.5f);
                nvgFillColor(args.vg, nvgRGBAf(fminf(r * 2.0f, 1.0f), fminf(g * 2.0f, 1.0f), fminf(b * 2.0f, 1.0f), 1.0f));
                nvgFill(args.vg);
                
                // Layer 5: Multiple highlight spots for faceted jewel effect
                float highlightIntensity = 0.9f * maxBrightness;
                
                // Main highlight (upper left)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx - 3.0f, cy - 3.0f, 2.0f);
                nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, highlightIntensity));
                nvgFill(args.vg);
                
                // Secondary highlight (right side)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx + 2.5f, cy - 1.0f, 1.2f);
                nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, highlightIntensity * 0.6f));
                nvgFill(args.vg);
                
                // Tiny sparkle highlights
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx - 1.0f, cy + 2.0f, 0.7f);
                nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, highlightIntensity * 0.8f));
                nvgFill(args.vg);
                
                // Layer 6: Dark rim for definition
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 14.0f);
                nvgStrokeColor(args.vg, nvgRGBAf(0.2f, 0.2f, 0.2f, 0.8f));
                nvgStrokeWidth(args.vg, 0.8f);
                nvgStroke(args.vg);
                
                nvgRestore(args.vg);
            } else {
                // Draw subtle base jewel when off
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 14.0f);
                nvgFillColor(args.vg, nvgRGBA(60, 60, 70, 255));
                nvgFill(args.vg);
                
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 11.0f);
                nvgFillColor(args.vg, nvgRGBA(30, 30, 35, 255));
                nvgFill(args.vg);
                
                // Subtle highlight even when off
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx - 2.0f, cy - 2.0f, 1.5f);
                nvgFillColor(args.vg, nvgRGBA(120, 120, 140, 100));
                nvgFill(args.vg);
            }
        }
        
        // Draw the SVG on top if it exists (with blend mode for better integration)
        if (!children.empty()) {
            nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE, NVG_ONE_MINUS_SRC_ALPHA);
            Widget::draw(args);
            nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE, NVG_ONE_MINUS_SRC_ALPHA);
        }
    }
};

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
    
    // Polyphonic support (up to 6 voices)
    static const int MAX_POLY_VOICES = 6;
    
    SidechainDetector detector;
    DistortionEngine distortion_l[MAX_POLY_VOICES], distortion_r[MAX_POLY_VOICES];
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
        
        vu_l_filter.setTau(0.1f);  // Slower release for more natural look
        vu_r_filter.setTau(0.1f);
        detector.setTiming(10.0f, 200.0f);
        
        // Initialize distortion smoothing - fast enough to be responsive but slow enough to avoid clicks
        distortion_slew.setRiseFall(1000.f, 1000.f);
    }
    
    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();
        for (int ch = 0; ch < MAX_POLY_VOICES; ch++) {
            distortion_l[ch].setSampleRate(sr);
            distortion_r[ch].setSampleRate(sr);
        }
        vu_l_filter.setTau(0.1f);  // Slower release for more natural look
        vu_r_filter.setTau(0.1f);
    }
    
    void process(const ProcessArgs& args) override {
        // Determine number of polyphonic voices (max 6)
        int channels = std::min(std::max(inputs[AUDIO_L_INPUT].getChannels(), 1), MAX_POLY_VOICES);
        
        // Set output channels to match
        outputs[AUDIO_L_OUTPUT].setChannels(channels);
        outputs[AUDIO_R_OUTPUT].setChannels(channels);
        
        // Link switch state
        bool linked = params[LINK_PARAM].getValue() > 0.5f;
        
        // Sidechain processing (shared across all voices)
        float sidechain = inputs[SIDECHAIN_INPUT].isConnected() ? 
            inputs[SIDECHAIN_INPUT].getVoltage() : 0.0f;
        sidechain = clamp(fabsf(sidechain) * 0.1f, 0.0f, 1.0f);
        
        float sc_env = detector.process(sidechain);
        
        // Global parameters (shared across all voices)
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
        
        // LED brightness calculation
        float red_brightness, green_brightness, blue_brightness;
        const float base_brightness = 0.6f;
        const float max_brightness = base_brightness;
        
        if (smoothed_distortion <= 0.5f) {
            // 0 to 0.5: Teal to bright blue-purple
            red_brightness = smoothed_distortion * 2.0f * max_brightness;
            green_brightness = max_brightness;
            blue_brightness = max_brightness;
        } else {
            // 0.5 to 1.0: Bright blue-purple to dark purple
            red_brightness = max_brightness;
            green_brightness = 2.0f * (1.0f - smoothed_distortion) * max_brightness;
            blue_brightness = max_brightness * (1.7f - smoothed_distortion * 0.7f);
        }
        
        lights[DIST_LED_R].setBrightness(red_brightness);   
        lights[DIST_LED_G].setBrightness(green_brightness); 
        lights[DIST_LED_B].setBrightness(blue_brightness);
        
        // VCA gain calculation (polyphonic CV support)
        float base_vca_gain = params[VCA_PARAM].getValue();
        bool exponential_response = params[RESPONSE_PARAM].getValue() > 0.5f;
        
        // Initialize VU meter accumulators
        float vu_l_sum = 0.0f;
        float vu_r_sum = 0.0f;
        
        // Process each voice
        for (int ch = 0; ch < channels; ch++) {
            // Per-voice VCA gain calculation
            float vca_gain = base_vca_gain;
            
            if (inputs[VCA_CV_INPUT].isConnected()) {
                float cv = inputs[VCA_CV_INPUT].getPolyVoltage(ch) * 0.1f; // 10V -> 1.0f
                cv = clamp(cv, -1.0f, 1.0f);
                float cv_mod = cv * params[VCA_ATT_PARAM].getValue(); // Full range when attenuverter at 100%
                vca_gain += cv_mod;
            }
            
            vca_gain = clamp(vca_gain, 0.0f, 2.0f);
            
            // Apply response curve
            if (exponential_response) {
                vca_gain = vca_gain * vca_gain; // Square for exponential
            }
            
            // Get audio inputs for this voice
            float input_l = inputs[AUDIO_L_INPUT].getPolyVoltage(ch);
            float input_r = linked ? input_l : 
                           (inputs[AUDIO_R_INPUT].isConnected() ? inputs[AUDIO_R_INPUT].getPolyVoltage(ch) : input_l);
            
            float vca_l = input_l * vca_gain;
            float vca_r = input_r * vca_gain;
            
            // Process distortion for this voice
            float distorted_l = distortion_l[ch].process(vca_l, distortion_amount, 
                                                       (DistortionEngine::Type)distortion_type);
            float distorted_r = distortion_r[ch].process(vca_r, distortion_amount, 
                                                       (DistortionEngine::Type)distortion_type);
            
            // Apply level compensation to match Wave Fold (type 1) as reference level
            float level_compensation = 1.0f;
            switch(distortion_type) {
                case 0: level_compensation = 2.0f; break;   // Hard Clip - boost to match Wave Fold
                case 1: level_compensation = 0.7f; break;   // Wave Fold - reduce more
                case 2: level_compensation = 2.0f; break;   // Bit Crush - boost to match Wave Fold
                case 3: level_compensation = 2.0f; break;   // Destroy - boost to match Wave Fold
                case 4: level_compensation = 2.0f; break;   // Ring Mod - boost to match Wave Fold
                case 5: level_compensation = 2.0f; break;   // Tube Sat - boost to match Wave Fold
                default: level_compensation = 1.0f; break;
            }
            
            distorted_l *= level_compensation;
            distorted_r *= level_compensation;
            
            // Mix between clean and distorted signals
            float output_l = vca_l + mix * (distorted_l - vca_l);
            float output_r = vca_r + mix * (distorted_r - vca_r);
            
            outputs[AUDIO_L_OUTPUT].setVoltage(output_l, ch);
            outputs[AUDIO_R_OUTPUT].setVoltage(output_r, ch);
            
            // Accumulate for VU meters (sum all voices)
            vu_l_sum += fabsf(output_l);
            vu_r_sum += fabsf(output_r);
        }
        
        // VU meters (average level across all voices, scaled for full range)
        float vu_l_raw = clamp((vu_l_sum / channels) * 0.6f, 0.0f, 1.0f);
        float vu_r_raw = clamp((vu_r_sum / channels) * 0.6f, 0.0f, 1.0f);
        
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
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.17642, 341.98523), module, Chiaroscuro::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(73.836395, 341.98523), module, Chiaroscuro::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(Vec(100.33237, 341.98523), module, Chiaroscuro::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(Vec(153.73065, 341.98523), module, Chiaroscuro::AUDIO_R_OUTPUT));
        
        // Main VCA knob - red circle "vca_gain"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(Vec(134.39859, 235.87756), module, Chiaroscuro::VCA_PARAM));
        
        // VCA CV input - green circle "vca_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(111.31203, 288.73596), module, Chiaroscuro::VCA_CV_INPUT));
        
        // VCA attenuverter - yellow circle "attenu_vca"
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(Vec(153.73065, 278.37918), module, Chiaroscuro::VCA_ATT_PARAM));
        
        // Linear/Exponential response switch - yellow circle "lin_exp_switch" (updated position, currently hidden in SVG)
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(Vec(91.794441, 63.786713), module, Chiaroscuro::RESPONSE_PARAM));
        
        // Link switch - yellow circle "link_switch" (updated position, currently hidden in SVG)
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(Vec(92.406746, 112.10049), module, Chiaroscuro::LINK_PARAM));
        
        // Sidechain input - green circle "sidechain_in"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.17642, 175.29152), module, Chiaroscuro::SIDECHAIN_INPUT));
        
        // Manual distortion knob - red circle "dist_amount"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(73.836395, 175.29152), module, Chiaroscuro::MANUAL_DIST_PARAM));
        
        // Distortion type selector - red circle "distortion_type_select"
        addParam(createParamCentered<ShapetakerVintageSelector>(Vec(117.15861, 184.57472), module, Chiaroscuro::TYPE_PARAM));
        
        // Type CV input - green circle "type_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(153.94666, 184.57472), module, Chiaroscuro::TYPE_CV_INPUT));
        
        // Drive knob - red circle "drive_amount"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(73.836395, 235.87756), module, Chiaroscuro::DRIVE_PARAM));
        
        // Drive CV input - green circle "drive_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.17642, 235.87756), module, Chiaroscuro::DRIVE_CV_INPUT));
        
        // Mix knob - red circle "mix_amount"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(73.836395, 288.9314), module, Chiaroscuro::MIX_PARAM));
        
        // Mix CV input - green circle "mix_cv"
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(29.17642, 288.9314), module, Chiaroscuro::MIX_CV_INPUT));

        // Add VU meters - larger size for better visibility
        LargeVUMeterWidget* vu_meter_l = new LargeVUMeterWidget(module, &module->vu_l, "res/meters/vu_meter_face_bordered.svg", "res/meters/vu_meter_needle.svg");
        vu_meter_l->box.pos = Vec(34.897591 - 25, 79.596138 - 25); // Center 50x50 widget on vu_meter_l position (updated)
        addChild(vu_meter_l);
        LargeVUMeterWidget* vu_meter_r = new LargeVUMeterWidget(module, &module->vu_r, "res/meters/vu_meter_face_bordered.svg", "res/meters/vu_meter_needle.svg");
        vu_meter_r->box.pos = Vec(125.15861 - 25, 79.596138 - 25); // Center 50x50 widget on vu_meter_r position (updated)
        addChild(vu_meter_r);
        addChild(createLightCentered<TexturedJewelLED>(Vec(52.037258, 203.7605), module, Chiaroscuro::DIST_LED_R));
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
