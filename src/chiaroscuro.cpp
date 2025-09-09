#include "plugin.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>
#include <cmath>

// Custom larger VU meter widget for better visibility
// Using VU meter from utilities (implementation moved to ui/widgets.hpp)

// Custom textured jewel LED with layered opacity effects
struct TexturedJewelLED : ModuleLightWidget {
    TexturedJewelLED() {
        box.size = Vec(25, 25);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_large.svg"));
        
        if (svg) {
            sw->setSvg(svg);
            // Center the SVG within the widget box
            sw->box.pos = Vec((box.size.x - sw->box.size.x) * 0.5f, (box.size.y - sw->box.size.y) * 0.5f);
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
                
                // Layer 1: Large outer glow with gradient (scaled down)
                NVGpaint outerGlow = nvgRadialGradient(args.vg, cx, cy, 6.5f, 13.5f,
                    nvgRGBAf(r, g, b, 0.6f * maxBrightness), nvgRGBAf(r, g, b, 0.0f));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 13.5f);
                nvgFillPaint(args.vg, outerGlow);
                nvgFill(args.vg);
                
                // Layer 2: Medium ring with stronger color saturation (scaled down)
                NVGpaint mediumRing = nvgRadialGradient(args.vg, cx, cy, 3.5f, 9.5f,
                    nvgRGBAf(r * 1.2f, g * 1.2f, b * 1.2f, 0.9f * maxBrightness), 
                    nvgRGBAf(r, g, b, 0.3f * maxBrightness));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 9.5f);
                nvgFillPaint(args.vg, mediumRing);
                nvgFill(args.vg);
                
                // Layer 3: Inner core with high contrast (scaled down)
                NVGpaint innerCore = nvgRadialGradient(args.vg, cx, cy, 1.5f, 6.0f,
                    nvgRGBAf(fminf(r * 1.5f, 1.0f), fminf(g * 1.5f, 1.0f), fminf(b * 1.5f, 1.0f), 1.0f), 
                    nvgRGBAf(r, g, b, 0.7f));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 6.0f);
                nvgFillPaint(args.vg, innerCore);
                nvgFill(args.vg);
                
                // Layer 4: Bright center hotspot (scaled down)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 3.0f);
                nvgFillColor(args.vg, nvgRGBAf(fminf(r * 2.0f, 1.0f), fminf(g * 2.0f, 1.0f), fminf(b * 2.0f, 1.0f), 1.0f));
                nvgFill(args.vg);
                
                // Layer 5: Multiple highlight spots for faceted jewel effect (scaled down)
                float highlightIntensity = 0.9f * maxBrightness;
                
                // Main highlight (upper left) - scaled down
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx - 2.5f, cy - 2.5f, 1.7f);
                nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, highlightIntensity));
                nvgFill(args.vg);
                
                // Secondary highlight (right side) - scaled down
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx + 2.0f, cy - 0.8f, 1.0f);
                nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, highlightIntensity * 0.6f));
                nvgFill(args.vg);
                
                // Tiny sparkle highlights - scaled down
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx - 0.8f, cy + 1.7f, 0.6f);
                nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, highlightIntensity * 0.8f));
                nvgFill(args.vg);
                
                // Layer 6: Dark rim for definition (scaled down)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 12.0f);
                nvgStrokeColor(args.vg, nvgRGBAf(0.2f, 0.2f, 0.2f, 0.8f));
                nvgStrokeWidth(args.vg, 0.7f);
                nvgStroke(args.vg);
                
                nvgRestore(args.vg);
            } else {
                // Draw subtle base jewel when off (scaled down)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 12.0f);
                nvgFillColor(args.vg, nvgRGBA(60, 60, 70, 255));
                nvgFill(args.vg);
                
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx, cy, 9.5f);
                nvgFillColor(args.vg, nvgRGBA(30, 30, 35, 255));
                nvgFill(args.vg);
                
                // Subtle highlight even when off (scaled down)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cx - 1.7f, cy - 1.7f, 1.3f);
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
        TYPE_PARAM,
        DIST_PARAM,        // New: Distortion knob (dist-knob)
        DIST_ATT_PARAM,    // New: Distortion attenuverter
        DRIVE_PARAM,
        DRIVE_ATT_PARAM,   // New: Drive attenuverter  
        MIX_PARAM,
        MIX_ATT_PARAM,     // New: Mix attenuverter
        LINK_PARAM,
        RESPONSE_PARAM,    // Linear/Exponential response switch
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
        JEWEL_LED_R,       // New: Jewel LED for distortion amount indication
        JEWEL_LED_G,
        JEWEL_LED_B,
        VU_L_LED,
        VU_R_LED,
        NUM_LIGHTS
    };
    
    shapetaker::PolyphonicProcessor polyProcessor;
    shapetaker::SidechainDetector detector;
    shapetaker::VoiceArray<shapetaker::DistortionEngine> distortion_l, distortion_r;
    dsp::ExponentialFilter vu_l_filter, vu_r_filter;
    dsp::ExponentialSlewLimiter distortion_slew;
    
    float vu_l = 0.0f;
    float vu_r = 0.0f;
    
    Chiaroscuro() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        shapetaker::ParameterHelper::configGain(this, VCA_PARAM, "VCA Gain");
        shapetaker::ParameterHelper::configDiscrete(this, TYPE_PARAM, "Distortion Type", 0, 5, 0);
        shapetaker::ParameterHelper::configGain(this, DIST_PARAM, "Distortion Amount");
        shapetaker::ParameterHelper::configAttenuverter(this, DIST_ATT_PARAM, "Distortion CV Attenuverter");
        shapetaker::ParameterHelper::configDrive(this, DRIVE_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, DRIVE_ATT_PARAM, "Drive CV Attenuverter");
        shapetaker::ParameterHelper::configMix(this, MIX_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, MIX_ATT_PARAM, "Mix CV Attenuverter");
        shapetaker::ParameterHelper::configToggle(this, LINK_PARAM, "Link L/R Channels");
        shapetaker::ParameterHelper::configToggle(this, RESPONSE_PARAM, "VCA Response: Linear/Exponential");
        
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_L_INPUT, "Audio Left");
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_R_INPUT, "Audio Right");
        shapetaker::ParameterHelper::configCVInput(this, VCA_CV_INPUT, "VCA Control Voltage");
        shapetaker::ParameterHelper::configAudioInput(this, SIDECHAIN_INPUT, "Sidechain Detector");
        shapetaker::ParameterHelper::configCVInput(this, TYPE_CV_INPUT, "Distortion Type CV");
        shapetaker::ParameterHelper::configCVInput(this, DIST_CV_INPUT, "Distortion Amount CV");
        shapetaker::ParameterHelper::configCVInput(this, DRIVE_CV_INPUT, "Drive Amount CV");
        shapetaker::ParameterHelper::configCVInput(this, MIX_CV_INPUT, "Mix Control CV");
        
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_L_OUTPUT, "Audio Left");
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_R_OUTPUT, "Audio Right");
        
        vu_l_filter.setTau(0.1f);  // Slower release for more natural look
        vu_r_filter.setTau(0.1f);
        detector.setTiming(10.0f, 200.0f);
        
        // Initialize distortion smoothing - fast enough to be responsive but slow enough to avoid clicks
        distortion_slew.setRiseFall(1000.f, 1000.f);
    }
    
    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();
        distortion_l.forEach([sr](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(sr);
        });
        distortion_r.forEach([sr](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(sr);
        });
        vu_l_filter.setTau(0.1f);  // Slower release for more natural look
        vu_r_filter.setTau(0.1f);
    }
    
    void process(const ProcessArgs& args) override {
        // Update polyphonic channel count and set outputs
        int channels = polyProcessor.updateChannels(inputs[AUDIO_L_INPUT], {outputs[AUDIO_L_OUTPUT], outputs[AUDIO_R_OUTPUT]});
        
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
            float cv_amount = params[DRIVE_ATT_PARAM].getValue();
            drive += inputs[DRIVE_CV_INPUT].getVoltage() * cv_amount * 0.1f;
        }
        drive = clamp(drive, 0.0f, 1.0f);
        
        float mix = params[MIX_PARAM].getValue();
        if (inputs[MIX_CV_INPUT].isConnected()) {
            float cv_amount = params[MIX_ATT_PARAM].getValue();
            mix += inputs[MIX_CV_INPUT].getVoltage() * cv_amount * 0.1f;
        }
        mix = clamp(mix, 0.0f, 1.0f);
        
        int distortion_type = (int)params[TYPE_PARAM].getValue();
        if (inputs[TYPE_CV_INPUT].isConnected()) {
            float cv = inputs[TYPE_CV_INPUT].getVoltage() * 0.1f;
            distortion_type = (int)(params[TYPE_PARAM].getValue() + cv * 6.0f);
        }
        distortion_type = clamp(distortion_type, 0, 5);
        
        // Calculate distortion amount - main dist knob + CV + optional sidechain envelope
        float dist_amount = params[DIST_PARAM].getValue();
        if (inputs[DIST_CV_INPUT].isConnected()) {
            float cv_amount = params[DIST_ATT_PARAM].getValue();
            dist_amount += inputs[DIST_CV_INPUT].getVoltage() * cv_amount * 0.1f;
        }
        dist_amount = clamp(dist_amount, 0.0f, 1.0f);
        float sidechain_contribution = inputs[SIDECHAIN_INPUT].isConnected() ? sc_env : 0.0f;
        float combined_distortion = clamp(dist_amount + sidechain_contribution, 0.0f, 1.0f);
        
        // Enhanced jewel LED with drive and mix effects - restored original brightness
        // Base red intensity from distortion amount (core effect)
        float red_intensity = combined_distortion; // Restored to full brightness
        
        // Drive adds green component - creates yellow/orange tint when combined with red
        float green_intensity = drive * 0.8f; // Restored to 0.8
        
        // Mix adds blue component - creates purple/magenta when combined with red
        float blue_intensity = mix * 0.6f; // Restored to 0.6
        
        // Apply full intensity scaling for maximum visual impact
        lights[JEWEL_LED_R].setBrightness(red_intensity);
        lights[JEWEL_LED_G].setBrightness(green_intensity);
        lights[JEWEL_LED_B].setBrightness(blue_intensity);
        
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
                vca_gain += cv; // Direct CV control without attenuverter
            }
            
            vca_gain = clamp(vca_gain, 0.0f, 2.0f);

            // Apply response curve
            if (exponential_response) {
                vca_gain = vca_gain * vca_gain; // Square for exponential
            }

            // No automatic polyphonic normalization here; leave gain as-is
            
            // Get audio inputs for this voice
            float input_l = inputs[AUDIO_L_INPUT].getPolyVoltage(ch);
            float input_r = linked ? input_l : 
                           (inputs[AUDIO_R_INPUT].isConnected() ? inputs[AUDIO_R_INPUT].getPolyVoltage(ch) : input_l);
            
            float vca_l = input_l * vca_gain;
            float vca_r = input_r * vca_gain;
            
            // Process distortion for this voice
            float distorted_l = distortion_l[ch].process(vca_l, distortion_amount, 
                                                       (shapetaker::DistortionEngine::Type)distortion_type);
            float distorted_r = distortion_r[ch].process(vca_r, distortion_amount, 
                                                       (shapetaker::DistortionEngine::Type)distortion_type);
            
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
    // Draw panel background texture to match other modules
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

    ChiaroscuroWidget(Chiaroscuro* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg")));
        
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // Audio I/O - BNC connectors for vintage oscilloscope look - updated positions from panel
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(7.5756826, 115.80798)), module, Chiaroscuro::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(22.049751, 115.80798)), module, Chiaroscuro::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(Vec(36.523819, 115.80798)), module, Chiaroscuro::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm2px(Vec(50.997887, 115.80798)), module, Chiaroscuro::AUDIO_R_OUTPUT));
        
        // Main VCA knob - red circle "vca_gain"
        // VCA knob - position updated from panel: cx="42.967007" cy="101.61994"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(mm2px(Vec(42.967007, 101.61994)), module, Chiaroscuro::VCA_PARAM));
        
        // VCA CV input - updated position from panel: cx="22.049751" cy="101.61994"
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(22.049751, 101.61994)), module, Chiaroscuro::VCA_CV_INPUT));
        
        // Linear/Exponential response switch - centered on yellow rectangle: x="5.9992962" y="42.221786"
        float left_switch_center_x = 5.9992962 + 9.4644022/2;
        float left_switch_center_y = 42.221786 + 8.9940262/2;
        auto* leftSwitch = createParamCentered<ShapetakerOscilloscopeSwitch>(mm2px(Vec(left_switch_center_x, left_switch_center_y)), module, Chiaroscuro::RESPONSE_PARAM);
        Vec leftCenter = leftSwitch->box.pos.plus(leftSwitch->box.size.div(2.f));
        leftSwitch->box.size = Vec(70.0f, 65.0f);
        leftSwitch->box.pos = leftCenter.minus(leftSwitch->box.size.div(2.f));
        addParam(leftSwitch);
        
        // Link switch - centered on yellow rectangle: x="25.967707" y="42.221786"
        float right_switch_center_x = 25.967707 + 9.4644022/2;
        float right_switch_center_y = 42.221786 + 8.9940262/2;
        auto* rightSwitch = createParamCentered<ShapetakerOscilloscopeSwitch>(mm2px(Vec(right_switch_center_x, right_switch_center_y)), module, Chiaroscuro::LINK_PARAM);
        Vec rightCenter = rightSwitch->box.pos.plus(rightSwitch->box.size.div(2.f));
        rightSwitch->box.size = Vec(70.0f, 65.0f);
        rightSwitch->box.pos = rightCenter.minus(rightSwitch->box.size.div(2.f));
        addParam(rightSwitch);
        
        // Sidechain input - updated position from panel: cx="7.5756826" cy="101.61994"
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(7.5756826, 101.61994)), module, Chiaroscuro::SIDECHAIN_INPUT));
        
        
        // Distortion type selector dial - updated position from panel: cx="49.862827" cy="46.7188"
        addParam(createParamCentered<ShapetakerVintageSelector>(mm2px(Vec(49.862827, 46.7188)), module, Chiaroscuro::TYPE_PARAM));
        
        // Distortion type CV input - near the selector: cx="50.280388" cy="60.808102"
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(50.280388, 60.808102)), module, Chiaroscuro::TYPE_CV_INPUT));
        
        // NEW: Distortion knob - position from panel: cx="7.5756826" cy="60.808102"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(7.5756826, 60.808102)), module, Chiaroscuro::DIST_PARAM));
        
        // Distortion CV input - position from panel: cx="7.5756826" cy="87.4319"
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(7.5756826, 87.4319)), module, Chiaroscuro::DIST_CV_INPUT));
        
        // Drive knob - red circle "drive_amount"
        // Drive knob - updated position from panel: cx="22.049751" cy="60.808102"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(22.049751, 60.808102)), module, Chiaroscuro::DRIVE_PARAM));
        
        // Drive CV input - updated position from panel: cx="22.049751" cy="87.4319"
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(22.049751, 87.4319)), module, Chiaroscuro::DRIVE_CV_INPUT));
        
        // Mix knob - red circle "mix_amount"
        // Mix knob - updated position from panel: cx="36.523819" cy="60.808102"
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm2px(Vec(36.523819, 60.808102)), module, Chiaroscuro::MIX_PARAM));
        
        // NEW ATTENUVERTERS - updated positions under their respective knobs
        // Distortion attenuverter - under distortion knob (dist-atten at cx="7.5756826" cy="74.2659")
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(7.5756826, 74.2659)), module, Chiaroscuro::DIST_ATT_PARAM));
        
        // Drive attenuverter - under drive knob (drive-atten cx="22.049751" cy="74.2659") 
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(22.049751, 74.2659)), module, Chiaroscuro::DRIVE_ATT_PARAM));
        
        // Mix attenuverter - under mix knob (id="mix-cntrl-cv" cx="36.523819" cy="74.2659")
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(mm2px(Vec(36.523819, 74.2659)), module, Chiaroscuro::MIX_ATT_PARAM));
        
        // Mix CV input - updated position from panel: cx="36.523819" cy="87.4319"
        addInput(createInputCentered<ShapetakerBNCPort>(mm2px(Vec(36.523819, 87.4319)), module, Chiaroscuro::MIX_CV_INPUT));

        // Add vintage VU meters using single SVG file - updated positions from new panel
        auto* vu_meter_l = new shapetaker::VintageVUMeterWidget(module, Chiaroscuro::VU_L_LED, 
            asset::plugin(pluginInstance, "res/meters/vintage_vu.svg"));
        // Panel coordinates: x="3.8985527" y="13.64045" width="24.23" height="21.729599" 
        // Center the 80px widget in the panel rectangle: x + width/2 - widget_size_mm/2
        float widget_size_mm = 80.0f / RACK_GRID_WIDTH * 5.08f; // Convert 80px to mm
        float vu_l_center_x = 3.8985527 + 24.23/2;
        float vu_l_center_y = 13.64045 + 21.729599/2;
        vu_meter_l->box.pos = mm2px(Vec(vu_l_center_x - widget_size_mm/2, vu_l_center_y - widget_size_mm/2));
        vu_meter_l->box.size = Vec(80, 80);
        addChild(vu_meter_l);
        
        auto* vu_meter_r = new shapetaker::VintageVUMeterWidget(module, Chiaroscuro::VU_R_LED,
            asset::plugin(pluginInstance, "res/meters/vintage_vu.svg"));
        // Panel coordinates: x="33.271664" y="13.64045" width="24.229599" height="21.729599"
        float vu_r_center_x = 33.271664 + 24.229599/2;
        float vu_r_center_y = 13.64045 + 21.729599/2;
        vu_meter_r->box.pos = mm2px(Vec(vu_r_center_x - widget_size_mm/2, vu_r_center_y - widget_size_mm/2));
        vu_meter_r->box.size = Vec(80, 80);
        addChild(vu_meter_r);
        
        // Jewel LED for distortion amount indication - centered on placeholder: cx="49.605007" cy="79.869637"
        addChild(createLightCentered<TexturedJewelLED>(mm2px(Vec(49.605007, 79.869637)), module, Chiaroscuro::JEWEL_LED_R));
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
