#include "plugin.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>
#include <cmath>
#include <unordered_map>
#include <array>
#include <string>
#include <cctype>

// Custom larger VU meter widget for better visibility
// Using VU meter from utilities (implementation moved to ui/widgets.hpp)

// Vintage Tube & Chiaroscuro Art Visualization System
struct VintageChiaroscuroDisplay {
    struct TubeElement {
        float intensity;
        float warmth;
        float flicker;
        float phase;
        Vec position;
        float radius;
        
        TubeElement(Vec pos, float r) : position(pos), radius(r) {
            intensity = 0.0f;
            warmth = 0.0f;
            flicker = random::uniform();
            phase = random::uniform() * 2.0f * M_PI;
        }
    };
    
    static const int NUM_TUBES = 6;
    TubeElement tubes[NUM_TUBES];
    float shadowPattern[8][8]; // Chiaroscuro shadow matrix
    float lightBurst;
    float artPhase;
    
    VintageChiaroscuroDisplay() : tubes{
        TubeElement(Vec(40.0f, 40.0f), 8.0f), // Center tube
        TubeElement(Vec(40.0f + cos(0 * M_PI / 2.5f) * 15.0f, 40.0f + sin(0 * M_PI / 2.5f) * 15.0f), 6.0f),
        TubeElement(Vec(40.0f + cos(1 * M_PI / 2.5f) * 15.0f, 40.0f + sin(1 * M_PI / 2.5f) * 15.0f), 6.0f),
        TubeElement(Vec(40.0f + cos(2 * M_PI / 2.5f) * 15.0f, 40.0f + sin(2 * M_PI / 2.5f) * 15.0f), 6.0f),
        TubeElement(Vec(40.0f + cos(3 * M_PI / 2.5f) * 15.0f, 40.0f + sin(3 * M_PI / 2.5f) * 15.0f), 6.0f),
        TubeElement(Vec(40.0f + cos(4 * M_PI / 2.5f) * 15.0f, 40.0f + sin(4 * M_PI / 2.5f) * 15.0f), 6.0f)
    } {
        
        // Initialize shadow pattern
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                shadowPattern[x][y] = random::uniform() * 0.3f;
            }
        }
        
        lightBurst = 0.0f;
        artPhase = 0.0f;
    }
    
    void update(float deltaTime, float chaos, float drive, float mix, int distortionType) {
        artPhase += deltaTime * 2.0f;
        
        // Update each tube based on parameters
        for (int i = 0; i < NUM_TUBES; i++) {
            TubeElement& tube = tubes[i];
            tube.phase += deltaTime * (3.0f + i * 0.5f);
            
            // Base intensity from drive
            tube.intensity = drive * 0.8f + chaos * 0.6f;
            
            // Distortion type affects tube behavior
            switch (distortionType) {
                case 0: // Hard Clip - harsh, digital glow
                    tube.intensity = (tube.intensity > 0.5f) ? 1.0f : 0.2f;
                    tube.warmth = 0.3f;
                    tube.flicker = 0.1f;
                    break;
                case 1: // Wave Fold - smooth wave-like pulsing
                    tube.intensity *= (1.0f + sin(tube.phase * 2.0f + artPhase) * 0.3f);
                    tube.warmth = 0.4f + sin(artPhase * 0.5f) * 0.2f;
                    tube.flicker = 0.05f;
                    break;
                case 2: // Bit Crush - quantized, digital flickering
                    tube.intensity = floor(tube.intensity * 4.0f) / 4.0f;
                    tube.warmth = 0.2f;
                    tube.flicker = 0.3f;
                    break;
                case 3: // Destroy - violent, explosive flashing
                    tube.intensity += chaos * random::uniform() * 0.5f;
                    tube.warmth = 0.8f + random::uniform() * 0.2f;
                    tube.flicker = 0.4f + chaos * 0.3f;
                    break;
                case 4: { // Ring Mod - modulated, oscillating
                    float ringMod = sin(glfwGetTime() * 12.0f) * cos(glfwGetTime() * 7.0f);
                    tube.intensity *= (1.0f + ringMod * 0.4f);
                    tube.warmth = 0.6f + ringMod * 0.3f;
                    tube.flicker = abs(ringMod) * 0.2f;
                    break;
                }
                case 5: // Tube Sat - warm, vintage tube glow
                    tube.intensity = tanh(tube.intensity * 2.0f);
                    tube.warmth = 0.9f + sin(tube.phase * 0.3f) * 0.1f;
                    tube.flicker = 0.02f; // Very stable vintage glow
                    break;
            }
            
            // Mix affects individual tube modulation
            if (mix > 0.1f) {
                float mixPhase = artPhase * mix * 2.0f + i * 0.8f;
                tube.intensity *= (1.0f + sin(mixPhase) * mix * 0.4f);
            }
            
            // Clamp values
            tube.intensity = clamp(tube.intensity, 0.0f, 1.0f);
            tube.warmth = clamp(tube.warmth, 0.0f, 1.0f);
            tube.flicker = clamp(tube.flicker, 0.0f, 0.5f);
        }
        
        // Update Chiaroscuro shadow pattern
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                float basePattern = sin((x + y) * 0.5f + artPhase) * 0.5f + 0.5f;
                shadowPattern[x][y] = basePattern * chaos * 0.4f + drive * 0.2f;
                
                // Add dramatic light/dark contrasts (Chiaroscuro effect)
                if (chaos > 0.3f) {
                    float contrast = (basePattern > 0.6f) ? 1.0f : 0.1f;
                    shadowPattern[x][y] *= contrast;
                }
            }
        }
        
        // Light burst effect for high intensity
        lightBurst = (drive + chaos + mix) / 3.0f;
        if (lightBurst > 0.7f) {
            lightBurst += sin(artPhase * 8.0f) * 0.2f;
        }
    }
};

struct VintageChiaroscuroWidget : TransparentWidget {
    VintageChiaroscuroDisplay display;
    Module* module = nullptr;
    int distLightId = -1, driveLightId = -1, mixLightId = -1;
    int typeParamId = -1; // Add distortion type parameter
    float lastTime = 0.0f;
    
    VintageChiaroscuroWidget() {
        box.size = Vec(80, 80);
    }
    
    void step() override {
        TransparentWidget::step();
        
        if (!module) return;
        
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // Limit delta time to prevent jumps
        deltaTime = clamp(deltaTime, 0.0f, 1.0f / 30.0f);
        
        // Get parameter values from the module lights and parameters
        float chaos = (distLightId >= 0) ? module->lights[distLightId].getBrightness() : 0.0f;
        float drive = (driveLightId >= 0) ? module->lights[driveLightId].getBrightness() : 0.0f;
        float mix = (mixLightId >= 0) ? module->lights[mixLightId].getBrightness() : 0.0f;
        int distortionType = (typeParamId >= 0) ? (int)module->params[typeParamId].getValue() : 0;
        
        // Update vintage tube & chiaroscuro display
        display.update(deltaTime, chaos, drive, mix, distortionType);
    }
    
    void drawVintageTube(NVGcontext* vg, const VintageChiaroscuroDisplay::TubeElement& tube) {
        // Main tube body (dark base)
        nvgBeginPath(vg);
        nvgCircle(vg, tube.position.x, tube.position.y, tube.radius);
        nvgFillColor(vg, nvgRGBAf(0.1f, 0.05f, 0.02f, 0.8f));
        nvgFill(vg);
        
        // Tube glow (warm vintage color)
        if (tube.intensity > 0.1f) {
            nvgBeginPath(vg);
            nvgCircle(vg, tube.position.x, tube.position.y, tube.radius * 1.3f);
            
            // Color based on warmth and flicker
            float flickerAmount = 1.0f - tube.flicker + sin(glfwGetTime() * 60.0f) * tube.flicker * 0.3f;
            float r = 0.9f + tube.warmth * 0.1f;
            float g = 0.4f + tube.warmth * 0.5f;
            float b = 0.1f + tube.warmth * 0.2f;
            
            NVGcolor glowColor = nvgRGBAf(r, g, b, tube.intensity * flickerAmount * 0.6f);
            nvgFillColor(vg, glowColor);
            nvgFill(vg);
        }
        
        // Inner filament
        if (tube.intensity > 0.3f) {
            nvgBeginPath(vg);
            nvgCircle(vg, tube.position.x, tube.position.y, tube.radius * 0.4f);
            nvgFillColor(vg, nvgRGBAf(1.0f, 0.9f, 0.7f, tube.intensity * 0.8f));
            nvgFill(vg);
        }
    }
    
    void drawChiaroscuroPattern(NVGcontext* vg) {
        // Draw dramatic light/shadow pattern
        float cellSize = 10.0f;
        
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                float shadowIntensity = display.shadowPattern[x][y];
                
                nvgBeginPath(vg);
                nvgRect(vg, x * cellSize, y * cellSize, cellSize, cellSize);
                
                if (shadowIntensity > 0.5f) {
                    // Light areas - warm glow
                    NVGcolor lightColor = nvgRGBAf(1.0f, 0.9f, 0.7f, shadowIntensity * 0.3f);
                    nvgFillColor(vg, lightColor);
                } else {
                    // Dark areas - deep shadows
                    NVGcolor shadowColor = nvgRGBAf(0.1f, 0.05f, 0.02f, (1.0f - shadowIntensity) * 0.5f);
                    nvgFillColor(vg, shadowColor);
                }
                nvgFill(vg);
            }
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (!module) return;
        
        nvgSave(args.vg);
        
        // Draw Chiaroscuro background pattern
        drawChiaroscuroPattern(args.vg);
        
        // Light burst effect for high intensity
        if (display.lightBurst > 0.7f) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 40.0f, 40.0f, 35.0f);
            
            float burstAlpha = (display.lightBurst - 0.7f) * 0.3f;
            NVGcolor burstColor = nvgRGBAf(1.0f, 0.9f, 0.6f, burstAlpha);
            nvgFillColor(args.vg, burstColor);
            nvgFill(args.vg);
        }
        
        // Draw vintage tubes
        for (int i = 0; i < VintageChiaroscuroDisplay::NUM_TUBES; i++) {
            drawVintageTube(args.vg, display.tubes[i]);
        }
        
        nvgRestore(args.vg);
    }
};

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
        
        // VU meters (average level across all voices, 0dB = 5V)
        // Scaling: 5V * 0.1739 = 0.8696 brightness = 0dB position on meter
        float vu_l_raw = clamp((vu_l_sum / channels) * 0.1739f, 0.0f, 1.0f);
        float vu_r_raw = clamp((vu_r_sum / channels) * 0.1739f, 0.0f, 1.0f);
        
        vu_l = vu_l_filter.process(args.sampleTime, vu_l_raw);
        vu_r = vu_r_filter.process(args.sampleTime, vu_r_raw);
        
        if (vu_l < 0.01f) vu_l = 0.0f;
        if (vu_r < 0.01f) vu_r = 0.0f;
        
        lights[VU_L_LED].setBrightness(vu_l);
        lights[VU_R_LED].setBrightness(vu_r);
    }
};

// ---- Dot-matrix text helpers -----------------------------------------------

struct DotFont5x7 {
    // Each glyph is 5 columns x 7 rows, packed LSB->MSB across columns
    // We'll store as 7 bytes; lowest 5 bits in each byte are the columns for that row.
    // 1 = lit dot, 0 = off
    std::unordered_map<char, std::array<uint8_t,7>> glyphs;

    DotFont5x7() {
        using G = std::array<uint8_t,7>;
        auto put = [&](char c, G rows){ glyphs[c] = rows; };

        // Basic charset (uppercase, digits, space, dash)
        put(' ', G{0,0,0,0,0,0,0});
        put('-', G{0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000});
        put('0', G{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110});
        put('1', G{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110});
        put('2', G{0b01110,0b10001,0b00001,0b00110,0b01000,0b10000,0b11111});
        put('3', G{0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110});
        put('4', G{0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010});
        put('5', G{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110});
        put('6', G{0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110});
        put('7', G{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000});
        put('8', G{0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110});
        put('9', G{0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100});

        put('A', G{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001});
        put('B', G{0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110});
        put('C', G{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110});
        put('D', G{0b11100,0b10010,0b10001,0b10001,0b10001,0b10010,0b11100});
        put('E', G{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111});
        put('F', G{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000});
        put('G', G{0b01110,0b10001,0b10000,0b10000,0b10011,0b10001,0b01111});
        put('H', G{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001});
        put('I', G{0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110});
        put('J', G{0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100});
        put('K', G{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001});
        put('L', G{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111});
        put('M', G{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001});
        put('N', G{0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001});
        put('O', G{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110});
        put('P', G{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000});
        put('Q', G{0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101});
        put('R', G{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001});
        put('S', G{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110});
        put('T', G{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100});
        put('U', G{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110});
        put('V', G{0b10001,0b10001,0b10001,0b10001,0b01010,0b01010,0b00100});
        put('W', G{0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001});
        put('X', G{0b10001,0b01010,0b00100,0b00100,0b00100,0b01010,0b10001});
        put('Y', G{0b10001,0b01010,0b00100,0b00100,0b00100,0b00100,0b00100});
        put('Z', G{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111});

        put(':', G{0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000});
        put('/', G{0b00001,0b00010,0b00100,0b00100,0b01000,0b10000,0b00000});
    }

    const std::array<uint8_t,7>& get(char c) const {
        auto it = glyphs.find(c);
        if (it != glyphs.end()) return it->second;
        static std::array<uint8_t,7> blank {0,0,0,0,0,0,0};
        return blank; // unknown -> blank
    }
};

// Draw dot-matrix text centered within (x,y,w,h)
// dotR = radius of crisp dots; pitchX/Y = spacing between dot centers
// glowScale: draw a soft glow pass with radius dotR*glowScale (e.g., 1.8f); set 0 to disable
static void drawDotTextCentered(NVGcontext* vg, const std::string& s,
    float x, float y, float w, float h,
    NVGcolor dotColor, float dotR, float pitchX, float pitchY,
    float glowScale, float glowAlpha)
{
    static DotFont5x7 FONT;

    // Layout: each glyph is 5 columns + 1 col spacing (except last), width = 5*pitchX + glyphSpacing
    const float glyphW = 5.f * pitchX;
    const float glyphH = 7.f * pitchY;
    const float glyphSpacing = pitchX * 1.2f;

    // Compute total text width
    float totalW = 0.f;
    for (size_t i = 0; i < s.size(); ++i) {
        totalW += glyphW + (i + 1 < s.size() ? glyphSpacing : 0.f);
    }

    // Top-left origin for centered placement
    float startX = x + (w - totalW) * 0.5f;
    float startY = y + (h - glyphH) * 0.5f;

    // Two passes: glow (optional) then crisp
    auto drawPass = [&](float radius, float alpha){
        NVGcolor col = dotColor; col.a *= alpha;
        nvgFillColor(vg, col);

        float penX = startX;
        for (char ch : s) {
            const auto& rows = FONT.get((char)std::toupper((unsigned char)ch));
            // 7 rows top -> bottom
            for (int row = 0; row < 7; ++row) {
                uint8_t bits = rows[row];
                for (int colBit = 0; colBit < 5; ++colBit) {
                    if (bits & (1u << (4 - colBit))) {  // Reverse bit order
                        float cx = penX + colBit * pitchX + pitchX * 0.5f;
                        float cy = startY + row * pitchY + pitchY * 0.5f;
                        nvgBeginPath(vg);
                        nvgCircle(vg, cx, cy, radius);
                        nvgFill(vg);
                    }
                }
            }
            penX += glyphW + glyphSpacing;
        }
    };

    if (glowScale > 0.f && glowAlpha > 0.f)
        drawPass(dotR * glowScale, glowAlpha);
    drawPass(dotR, 1.f);
}

struct EclipseBadgeScreen : Widget {
    Chiaroscuro* module = nullptr;

    void draw(const DrawArgs& args) override {
        // Draw a NanoVG badge that matches the eclipse_badge.svg layout (no SVG dependencies).
        // Canvas: 120x140, Screen window: rect(18,18,84,84). Render order:
        // bezel/frame -> dark substrate -> dynamic dots -> glass overlays.
        const float CANVAS_W = 120.f;
        const float CANVAS_H = 140.f;
        const float SCREEN_X = 18.f / CANVAS_W;
        const float SCREEN_Y = 18.f / CANVAS_H;
        const float SCREEN_W = 84.f / CANVAS_W;
        const float SCREEN_H = 84.f / CANVAS_H;

        // Compute local screen rect within this widget's box
        float W = box.size.x;
        float H = box.size.y;
        float screenX = W * SCREEN_X;
        float screenY = H * SCREEN_Y;
        float screenW = W * SCREEN_W;
        float screenH = H * SCREEN_H;

        // Bezel/frame
        float bezelR = std::min(W, H) * 0.06f;
        NVGpaint bezelPaint = nvgLinearGradient(args.vg, 0, 0, 0, H,
            nvgRGBAf(0.19f, 0.19f, 0.19f, 1.f),
            nvgRGBAf(0.07f, 0.07f, 0.07f, 1.f));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, W, H, bezelR);
        nvgFillPaint(args.vg, bezelPaint);
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, W * 0.07f, H * 0.06f, W * 0.86f, H * 0.88f, bezelR * 0.7f);
        nvgStrokeColor(args.vg, nvgRGBAf(0.29f, 0.29f, 0.29f, 1.f));
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);

        // Display frame around the screen
        float frameX = W * (16.f / CANVAS_W);
        float frameY = H * (16.f / CANVAS_H);
        float frameW = W * (88.f / CANVAS_W);
        float frameH = H * (88.f / CANVAS_H);
        float frameR = std::min(frameW, frameH) * 0.1f;
        NVGpaint framePaint = nvgLinearGradient(args.vg, 0, frameY, 0, frameY + frameH,
            nvgRGBAf(0.20f, 0.20f, 0.20f, 1.f), nvgRGBAf(0.11f, 0.11f, 0.11f, 1.f));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, frameX, frameY, frameW, frameH, frameR);
        nvgFillPaint(args.vg, framePaint);
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, frameX, frameY, frameW, frameH, frameR);
        nvgStrokeColor(args.vg, nvgRGBAf(0.17f, 0.17f, 0.17f, 1.f));
        nvgStrokeWidth(args.vg, 0.6f);
        nvgStroke(args.vg);

        // Dark glass substrate (under dots)
        float substrateR = std::min(screenW, screenH) * 0.095f;
        NVGpaint vignette = nvgRadialGradient(args.vg, screenX + screenW * 0.5f, screenY + screenH * 0.45f,
            0, std::max(screenW, screenH) * 0.8f,
            nvgRGBAf(0.04f, 0.04f, 0.04f, 1.f), nvgRGBAf(0.f, 0.f, 0.f, 1.f));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, substrateR);
        nvgFillPaint(args.vg, vignette);
        nvgFill(args.vg);

        // Get parameters for animation
        float mix = 0.f, distortion = 0.f, drive = 0.f;
        if (module) {
            mix = module->params[Chiaroscuro::MIX_PARAM].getValue();
            if (module->inputs[Chiaroscuro::MIX_CV_INPUT].isConnected()) {
                mix += module->inputs[Chiaroscuro::MIX_CV_INPUT].getVoltage() *
                       module->params[Chiaroscuro::MIX_ATT_PARAM].getValue() / 10.f;
            }
            mix = clamp(mix, 0.f, 1.f);

            float dist_amount = module->params[Chiaroscuro::DIST_PARAM].getValue();
            if (module->inputs[Chiaroscuro::DIST_CV_INPUT].isConnected()) {
                dist_amount += module->inputs[Chiaroscuro::DIST_CV_INPUT].getVoltage() *
                             module->params[Chiaroscuro::DIST_ATT_PARAM].getValue() / 10.f;
            }
            distortion = clamp(dist_amount, 0.f, 1.f);

            drive = module->params[Chiaroscuro::DRIVE_PARAM].getValue();
            if (module->inputs[Chiaroscuro::DRIVE_CV_INPUT].isConnected()) {
                drive += module->inputs[Chiaroscuro::DRIVE_CV_INPUT].getVoltage() *
                       module->params[Chiaroscuro::DRIVE_ATT_PARAM].getValue() / 10.f;
            }
            drive = clamp(drive, 0.f, 1.f);
        }

        // --- Dot Matrix Rendering ---
        // Base geometry centered in the SVG screen window
        float centerX = screenX + screenW * 0.5f;
        float centerY = screenY + screenH * 0.5f;
        float radius = std::min(screenW, screenH) * 0.5f;
        float sunRadius = radius * 0.85f;
        float moonRadius = sunRadius * 0.95f;

        // Moon position (eclipsing body)
        float moonOffsetX = (1.f - mix) * (radius + moonRadius);
        float moonCenterX = centerX + moonOffsetX;
        float moonCenterY = centerY;

        float dotRadius = 0.3f;
        float dotPitch = 1.5f;
        float shimmerSpeed = 0.1f + drive * 2.f;

        // Clip drawing to the screen window
        nvgSave(args.vg);
        nvgScissor(args.vg, screenX, screenY, screenW, screenH);

        for (float y = screenY; y < screenY + screenH; y += dotPitch) {
            for (float x = screenX; x < screenX + screenW; x += dotPitch) {
                Vec p = Vec(x, y);
                float distToSun = (p - Vec(centerX, centerY)).norm();
                float distToMoon = (p - Vec(moonCenterX, moonCenterY)).norm();

                if (distToMoon < moonRadius) {
                    continue; // Moon casts a perfect shadow
                }

                // --- Sun & Corona ---
                if (distToSun < sunRadius) {
                    // --- Main sun body ---
                    uint32_t hash = std::hash<float>()(x * 10.f) ^ std::hash<float>()(y * 3.f);
                    float stable_random = (float)(hash & 0xFF) / 255.f;

                    float probability = 0.85f;
                    float brightness = 0.9f;

                    brightness += (sin(x * 0.1f + glfwGetTime() * shimmerSpeed * 0.5f) * 0.5f + 0.5f) * distortion * 0.1f;
                    brightness += (cos(y * 0.1f + glfwGetTime() * shimmerSpeed * 0.8f) * 0.5f + 0.5f) * drive * 0.1f;

                    if (stable_random < probability) {
                        nvgBeginPath(args.vg);
                        nvgCircle(args.vg, x, y, dotRadius);
                        nvgFillColor(args.vg, nvgRGBAf(0.9f, 0.8f, 0.6f, clamp(brightness, 0.f, 1.f)));
                        nvgFill(args.vg);
                    }
                } else if (distToSun < sunRadius * 1.8f) {
                    // --- Corona ---
                    float probability = (1.f - (distToSun - sunRadius) / (sunRadius * 0.8f)) * 0.3f;
                    float brightness = probability * 2.f;
                    probability *= (1.f + distortion * 2.f);

                    if (random::uniform() < probability) {
                        nvgBeginPath(args.vg);
                        nvgCircle(args.vg, x, y, dotRadius * 1.2f);
                        nvgFillColor(args.vg, nvgRGBAf(0.9f, 0.8f, 0.6f, clamp(brightness, 0.f, 1.f) * 0.5f));
                        nvgFill(args.vg);
                    }
                }
            }
        }

        // --- Diamond Ring Effect ---
        if (mix > 0.95f) {
            float diamondIntensity = (mix - 0.95f) / 0.05f;
            float angle = -M_PI / 4.f; // Top-right
            float distance = sunRadius * 0.9f;
            float diamondX = centerX + cos(angle) * distance;
            float diamondY = centerY + sin(angle) * distance;

            for (int i = 0; i < 30; i++) {
                float r_angle = random::uniform() * 2.f * M_PI;
                float r_dist = random::uniform() * 4.f * diamondIntensity;
                Vec pos = Vec(diamondX + cos(r_angle) * r_dist, diamondY + sin(r_angle) * r_dist);
                
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, pos.x, pos.y, dotRadius * (1.f + random::uniform() * 1.5f));
                nvgFillColor(args.vg, nvgRGBAf(1.f, 0.95f, 0.8f, random::uniform() * diamondIntensity));
                nvgFill(args.vg);
            }
        }

        // Glass overlays (clipped) drawn above dots
        // Transmission band
        NVGpaint glassTX = nvgLinearGradient(args.vg, screenX, screenY, screenX, screenY + screenH,
            nvgRGBAf(1.f, 1.f, 1.f, 0.06f), nvgRGBAf(1.f, 1.f, 1.f, 0.08f));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, substrateR);
        nvgFillPaint(args.vg, glassTX);
        nvgFill(args.vg);

        // Edge highlight stroke
        NVGpaint edgeHi = nvgLinearGradient(args.vg, screenX, screenY, screenX + screenW, screenY + screenH,
            nvgRGBAf(1.f, 1.f, 1.f, 0.28f), nvgRGBAf(1.f, 1.f, 1.f, 0.0f));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX + 0.6f, screenY + 0.6f, screenW - 1.2f, screenH - 1.2f, substrateR * 0.92f);
        nvgStrokePaint(args.vg, edgeHi);
        nvgStrokeWidth(args.vg, 1.2f);
        nvgStroke(args.vg);

        // Specular hotspot
        NVGpaint hotspot = nvgRadialGradient(args.vg, screenX + screenW * 0.3f, screenY + screenH * 0.27f,
            0, std::max(screenW, screenH) * 0.4f,
            nvgRGBAf(1.f, 1.f, 1.f, 0.20f), nvgRGBAf(1.f, 1.f, 1.f, 0.0f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, screenX + screenW * 0.3f, screenY + screenH * 0.27f, screenW * 0.27f, screenH * 0.17f);
        nvgFillPaint(args.vg, hotspot);
        nvgFill(args.vg);

        // Diagonal glare bands
        NVGpaint glare = nvgLinearGradient(args.vg, screenX, screenY, screenX, screenY + screenH,
            nvgRGBAf(1.f, 1.f, 1.f, 0.18f), nvgRGBAf(1.f, 1.f, 1.f, 0.0f));
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, screenX - screenW * 0.1f, screenY + screenH * 0.0f);
        nvgLineTo(args.vg, screenX + screenW * 1.2f, screenY - screenH * 0.2f);
        nvgLineTo(args.vg, screenX + screenW * 1.2f, screenY + screenH * 0.1f);
        nvgLineTo(args.vg, screenX - screenW * 0.1f, screenY + screenH * 0.3f);
        nvgClosePath(args.vg);
        nvgFillPaint(args.vg, glare);
        nvgFill(args.vg);

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, screenX - screenW * 0.3f, screenY + screenH * 0.6f);
        nvgLineTo(args.vg, screenX + screenW * 1.2f, screenY + screenH * 0.25f);
        nvgLineTo(args.vg, screenX + screenW * 1.2f, screenY + screenH * 0.4f);
        nvgLineTo(args.vg, screenX - screenW * 0.3f, screenY + screenH * 0.75f);
        nvgClosePath(args.vg);
        nvgFillPaint(args.vg, nvgLinearGradient(args.vg, screenX, screenY, screenX, screenY + screenH,
            nvgRGBAf(1.f, 1.f, 1.f, 0.16f), nvgRGBAf(1.f, 1.f, 1.f, 0.0f)));
        nvgFill(args.vg);

        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }
};

struct ChiaroscuroWidget : ModuleWidget {
    // Eclipse badge screen positioned at jewel LED location
    static constexpr float SCREEN_X = 49.605007f;  // X coordinate of jewel LED
    static constexpr float SCREEN_Y = 79.869637f;  // Y coordinate of jewel LED  
    static constexpr float SCREEN_W = 15.f;        // Jewel LED sized width
    static constexpr float SCREEN_H = 15.f;        // Jewel LED sized height (square)
    const NVGcolor PHOSPHOR = nvgRGBAf(0.85f, 0.76f, 0.48f, 1.f); // amber
    
    std::shared_ptr<window::Font> mono;
    int lastIndex = -1;
    double showUntil = -1.0;   // wall-clock (seconds) 
    float fadeMs = 120.f;      // quick fade at end of the 500ms window
    
    const char* NAMES[6] = {
        "SAT",
        "FOL", 
        "FUL",
        "CRU",
        "TUB",
        "OPT"
    };

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
        
        // Load a mono font for text display
        mono = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
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
        auto* leftSwitch = createParamCentered<ShapetakerVintageToggleSwitch>(mm2px(Vec(left_switch_center_x, left_switch_center_y)), module, Chiaroscuro::RESPONSE_PARAM);
        Vec leftCenter = leftSwitch->box.pos.plus(leftSwitch->box.size.div(2.f));
        // Match global ShapetakerVintageToggleSwitch size (another -5%): 8.1225 x 16.245 mm
        leftSwitch->box.size = mm2px(Vec(8.1225f, 16.245f));
        leftSwitch->box.pos = leftCenter.minus(leftSwitch->box.size.div(2.f));
        addParam(leftSwitch);
        
        // Link switch - centered on yellow rectangle: x="25.967707" y="42.221786"
        float right_switch_center_x = 25.967707 + 9.4644022/2;
        float right_switch_center_y = 42.221786 + 8.9940262/2;
        auto* rightSwitch = createParamCentered<ShapetakerVintageToggleSwitch>(mm2px(Vec(right_switch_center_x, right_switch_center_y)), module, Chiaroscuro::LINK_PARAM);
        Vec rightCenter = rightSwitch->box.pos.plus(rightSwitch->box.size.div(2.f));
        // Match global ShapetakerVintageToggleSwitch size (another -5%): 8.1225 x 16.245 mm
        rightSwitch->box.size = mm2px(Vec(8.1225f, 16.245f));
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
        
        // Dynamic eclipse screen (drawn on top of the SVG)
        EclipseBadgeScreen* eclipse = new EclipseBadgeScreen;
        eclipse->module = module;
        // Match dynamic overlay bounds to the intended badge aspect and center
        const float badgeWmm = 15.f;
        const float badgeHmm = badgeWmm * (140.f/120.f);
        eclipse->box.size = mm2px(Vec(badgeWmm, badgeHmm));
        eclipse->box.pos = mm2px(Vec(SCREEN_X - badgeWmm * 0.5f, SCREEN_Y - badgeHmm * 0.5f));
        addChild(eclipse);
        
    }
    
    // Poll the param each frame and start a 500ms display when it changes
    void step() override {
        if (auto* m = dynamic_cast<Chiaroscuro*>(module)) {
            int idx = math::clamp((int) std::round(m->params[Chiaroscuro::TYPE_PARAM].getValue()), 0, 5);
            if (idx != lastIndex) {
                lastIndex = idx;
                showUntil = system::getTime() + 0.5; // 500 ms
            }
        }
        ModuleWidget::step();
    }
    
    // Draw text overlay for distortion type
    void drawLayer(const DrawArgs& args, int layer) override {
        ModuleWidget::drawLayer(args, layer);
        if (layer != 1) return;

        double now = system::getTime();
        if (now >= showUntil) return;

        // Fade out alpha over last fadeMs
        float alpha = 1.f;
        if (showUntil - now < fadeMs / 1000.0) {
            alpha = (float)((showUntil - now) * 1000.0 / fadeMs);
        }

        if (lastIndex >= 0 && lastIndex < 6) {
            Vec pos = mm2px(Vec(SCREEN_X - SCREEN_W/2.f, SCREEN_Y - SCREEN_H/2.f));
            Vec size = mm2px(Vec(SCREEN_W, SCREEN_H));

            NVGcolor textColor = PHOSPHOR;
            textColor.a *= alpha;

            // Start with simple, safe parameters
            drawDotTextCentered(args.vg, NAMES[lastIndex],
                pos.x, pos.y, size.x, size.y,
                textColor,
                0.4f,  // dot radius
                2.0f, 2.0f, // pitch x, y
                0.0f,  // no glow
                0.0f); // no glow
        }
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
