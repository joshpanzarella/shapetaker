#pragma once
#include <rack.hpp>
#include <nanovg.h>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelClairaudient;
extern Model* modelChiaroscuro;
extern Model* modelFatebinder;
extern Model* modelInvolution;
extern Model* modelEvocation;
extern Model* modelIncantation;
extern Model* modelTransmutation;

struct ShapetakerKnobLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the indicator as the rotating part (this sets up the internal SvgWidget)
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_large_indicator_light.svg")));
        
        // Add background as first child (will be drawn behind the rotating part)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_large_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerKnobMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the indicator as the rotating part (this sets up the internal SvgWidget)
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_medium_indicator_light.svg")));
        
        // Add background as first child (will be drawn behind the rotating part)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_medium_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerKnobOscilloscopeMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobOscilloscopeMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_medium.svg")));
        
        // Add spherical background as first child
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_medium_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerKnobOscilloscopeLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobOscilloscopeLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the large oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_large.svg")));
        
        // Add spherical background as first child
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_large_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerKnobOscilloscopeSmall : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobOscilloscopeSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the small oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_small.svg")));
        
        // Add spherical background as first child
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_small_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerKnobOscilloscopeXLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobOscilloscopeXLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the extra large oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_xlarge.svg")));
        
        // Add spherical background as first child
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_xlarge_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerKnobOscilloscopeHuge : app::SvgKnob {
    widget::SvgWidget* bg;
    
    ShapetakerKnobOscilloscopeHuge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the huge oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_huge.svg")));
        
        // Add spherical background as first child
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_huge_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct ShapetakerOscilloscopeSwitch : app::SvgSwitch {
    ShapetakerOscilloscopeSwitch() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/shuttle-toggle-switch-off.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/shuttle-toggle-switch-on.svg")));
        // Disable the shadow by setting it to transparent
        shadow->visible = false;
        // Scale down the switch by factor of 4
        box.size = Vec(25, 25);
    }
    
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Scale down by factor of 4 (0.25x) and center it
        nvgScale(args.vg, 0.25f, 0.25f);
        // Adjust position to center the scaled graphics
        nvgTranslate(args.vg, -37.5f, -37.5f);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerBNCPort : app::SvgPort {
    ShapetakerBNCPort() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/ports/st_bnc_connector.svg")));
    }
};

struct ShapetakerAttenuverterOscilloscope : app::SvgKnob {
    ShapetakerAttenuverterOscilloscope() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // The entire hexagonal knob (body + tick marks + indicator) rotates as one piece
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_attenuverter_large.svg")));
    }
};

// Shapetaker vintage momentary button using a single SVG with a pressed overlay
struct ShapetakerVintageMomentary : app::SvgSwitch {
    ShapetakerVintageMomentary() {
        momentary = true;
        // Use the same SVG for both frames; we add a pressed overlay in draw()
        auto svgUp = Svg::load(asset::plugin(pluginInstance, "res/buttons/vintage_momentary_button.svg"));
        addFrame(svgUp);
        addFrame(svgUp);
        if (shadow) shadow->visible = false;
        // Target size ~18x18 to match previous layout
        box.size = Vec(18.f, 18.f);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Incoming SVG viewBox is 100x100; scale to our box
        const float s = box.size.x / 100.f;
        nvgScale(args.vg, s, s);
        app::SvgSwitch::draw(args);
        nvgRestore(args.vg);

        // Pressed visual: subtle dark overlay to indicate depression
        bool pressed = false;
        if (getParamQuantity()) pressed = getParamQuantity()->getValue() > 0.5f;
        if (pressed) {
            nvgSave(args.vg);
            // Draw overlay in panel coordinates
            nvgBeginPath(args.vg);
            float cx = box.size.x * 0.5f;
            float cy = box.size.y * 0.5f;
            float r = std::min(box.size.x, box.size.y) * 0.48f;
            nvgCircle(args.vg, cx, cy, r);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 60));
            nvgFill(args.vg);
            nvgRestore(args.vg);
        }
    }
};

struct ShapetakerChickenHeadSelector : app::SvgSwitch {
    ShapetakerChickenHeadSelector() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_chicken_head_selector_0.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_chicken_head_selector_1.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_chicken_head_selector_2.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_chicken_head_selector_3.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_chicken_head_selector_4.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_chicken_head_selector_5.svg")));
        // Ensure smooth operation for 6 positions (0-5)
        box.size = Vec(35, 35);
    }
};

struct ShapetakerVintageSelector : app::SvgSwitch {
    ShapetakerVintageSelector() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_0.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_1.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_2.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_3.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_4.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_5.svg")));
        // Vintage selector - same size as chicken head selector (35x35)
        box.size = Vec(35, 35);
    }
};

struct VUMeterWidget : widget::Widget {
    Module* module;
    float* vu;
    std::string face_path;
    std::string needle_path;
    
    VUMeterWidget(Module* module, float* vu, std::string face_path, std::string needle_path) : module(module), vu(vu), face_path(face_path), needle_path(needle_path) {
        box.size = Vec(35, 35); // Smaller widget size
    }
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            // Draw the VU meter face
            std::shared_ptr<Svg> face_svg = Svg::load(asset::plugin(pluginInstance, face_path));
            if (face_svg) {
                NVGcontext* vg = args.vg;
                nvgSave(vg);
                nvgTranslate(vg, box.size.x / 2 - 25, box.size.y / 2 - 25); // Center the face SVG
                nvgScale(vg, 0.5f, 0.5f); // Same scale as needle
                face_svg->draw(vg);
                nvgRestore(vg);
            } else {
                // Fallback if face SVG is not loading
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, box.size.x / 2);
                nvgFillColor(args.vg, nvgRGB(50, 50, 50));
                nvgFill(args.vg);
            }
            
            // Safety check to prevent crashes
            if (!module || !vu) {
                return;
            }
            
            // VU meter scaling: *vu ranges 0-1 where 1.0 = clipped 10V signal
            // More uniform scaling across the full range
            float vu_value = *vu;
            float vu_scaled;
            if (vu_value <= 0.6f) {
                // Main scale: map 0-0.6 to -20 VU to 0 VU  
                vu_scaled = (vu_value / 0.6f) * 20.0f - 20.0f; // -20 to 0 VU
            } else if (vu_value <= 0.85f) {
                // Approach overload: map 0.6-0.85 to 0 VU to +3 VU
                vu_scaled = ((vu_value - 0.6f) / 0.25f) * 3.0f; // 0 to +3 VU
            } else {
                // Red overload zone: map 0.85-1.0 to +3 VU to +5 VU
                vu_scaled = 3.0f + ((vu_value - 0.85f) / 0.15f) * 2.0f; // +3 to +5 VU (red zone)
            }
            
            // Map VU range (-20 to +3) to needle angle - note +3 is max, not +5
            float normalized_vu = clamp((vu_scaled + 20.0f) / 23.0f, 0.0f, 1.0f); // 23 = range from -20 to +3
            // Needle sweeps from lower left (-20) to lower right (+3)
            // Approximate angles: -60° to +60° (120° total sweep)
            float angle = normalized_vu * (120.0f * M_PI / 180.0f) - (60.0f * M_PI / 180.0f);
            
            // Draw custom needle - positioned to match SVG center (50,50) at 0.5 scale
            nvgSave(args.vg);
            nvgTranslate(args.vg, box.size.x / 2 - 25, box.size.y / 2 - 25); // Match face center
            nvgScale(args.vg, 0.5f, 0.5f); // Match face scale  
            nvgTranslate(args.vg, 50, 50); // Move to SVG center point (50,50)
            nvgRotate(args.vg, angle);
            
            // Draw needle body
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, 8); // Start from bottom of needle
            nvgLineTo(args.vg, 0, -25); // Length of needle
            nvgStrokeColor(args.vg, nvgRGB(0, 0, 0)); // Black needle
            nvgStrokeWidth(args.vg, 2.5f);
            nvgStroke(args.vg);
            
            // Draw needle tip
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -25, 1.5f);
            nvgFillColor(args.vg, nvgRGB(0, 0, 0));
            nvgFill(args.vg);
            
            // Draw center pivot
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, 3);
            nvgFillColor(args.vg, nvgRGB(50, 50, 50));
            nvgFill(args.vg);
            
            nvgRestore(args.vg);
        }
    }
};

struct JewelLED : ModuleLightWidget {
    JewelLED() {
        // Set a fixed size
        box.size = Vec(25, 25);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_simple.svg"));
        
        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing like RedGreenBlueLight
        // Based on Chiaroscuro code: Red increases, Green decreases, Blue = 0
        addBaseColor(nvgRGB(0xff, 0x00, 0x00)); // Red channel
        addBaseColor(nvgRGB(0x00, 0xff, 0x00)); // Green channel  
        addBaseColor(nvgRGB(0x00, 0x00, 0xff)); // Blue channel (unused but needed for RGB)
    }
    
    void step() override {
        ModuleLightWidget::step();
        
        // Override the color mixing to get proper green-to-red transition
        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness(); // Red increases with intensity
            float g = module->lights[firstLightId + 1].getBrightness(); // Green decreases with intensity
            float b = module->lights[firstLightId + 2].getBrightness(); // Blue (always 0)
            
            // Set the color directly for better control
            color = nvgRGBAf(r, g, b, fmaxf(r, g));
        }
    }
    
    void draw(const DrawArgs& args) override {
        // If SVG didn't load, draw a simple jewel shape
        if (children.empty()) {
            // Draw chrome bezel
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 12.5, 12.5, 12);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            // Draw inner ring
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 12.5, 12.5, 8);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        // Call parent draw for lighting effects
        ModuleLightWidget::draw(args);
    }
};

struct JewelLEDSmall : ModuleLightWidget {
    JewelLEDSmall() {
        // Set a fixed size
        box.size = Vec(15, 15);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_small.svg"));
        
        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing like RedGreenBlueLight
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
            
            color = nvgRGBAf(r, g, b, fmaxf(r, g));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 7.5, 7.5, 7.2);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 7.5, 7.5, 4.8);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct JewelLEDMedium : ModuleLightWidget {
    JewelLEDMedium() {
        // Set a fixed size
        box.size = Vec(20, 20);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));
        
        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing like RedGreenBlueLight
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
            
            color = nvgRGBAf(r, g, b, fmaxf(r, g));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.6);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 6.4);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct JewelLEDLarge : ModuleLightWidget {
    JewelLEDLarge() {
        // Set a fixed size
        box.size = Vec(30, 30);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_large.svg"));
        
        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing like RedGreenBlueLight
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
            
            color = nvgRGBAf(r, g, b, fmaxf(r, g));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 15, 15, 14.4);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 15, 15, 9.6);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct JewelLEDXLarge : ModuleLightWidget {
    JewelLEDXLarge() {
        // Set a fixed size
        box.size = Vec(40, 40);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_xlarge.svg"));
        
        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing like RedGreenBlueLight
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
            
            color = nvgRGBAf(r, g, b, fmaxf(r, g));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 20, 20, 19.2);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 20, 20, 12.8);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct JewelLEDHuge : ModuleLightWidget {
    JewelLEDHuge() {
        // Set a fixed size
        box.size = Vec(50, 50);
        
        // Try to load the jewel SVG, fallback to simple shape if it fails
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_huge.svg"));
        
        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up proper RGB color mixing like RedGreenBlueLight
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
            
            color = nvgRGBAf(r, g, b, fmaxf(r, g));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 25, 25, 24);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 25, 25, 16);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

// An interface for modules that can provide data to the oscilloscope
struct IOscilloscopeSource {
    virtual ~IOscilloscopeSource() {}
    virtual const Vec* getOscilloscopeBuffer() const = 0;
    virtual int getOscilloscopeBufferIndex() const = 0;
    virtual int getOscilloscopeBufferSize() const = 0;
};

struct VintageOscilloscopeWidget : widget::Widget {
    IOscilloscopeSource* source;
    
    VintageOscilloscopeWidget(IOscilloscopeSource* source) : source(source) {}
    
    void step() override {
        Widget::step();
        // Buffering is now handled by the source module in the audio thread.
    }
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            NVGcontext* vg = args.vg;
            
            // --- CRT Glow Effect ---
            // Draw a soft glow behind the screen to simulate CRT phosphorescence
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x / 2.f);
            // A radial gradient from a soft teal to transparent dark green creates the glow
            NVGpaint glowPaint = nvgRadialGradient(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x * 0.1f, box.size.x * 0.5f, nvgRGBA(0, 150, 130, 90), nvgRGBA(0, 40, 40, 0));
            nvgFillPaint(vg, glowPaint);
            nvgFill(vg);
            // --- End Glow Effect ---

            // Draw background SVG
            std::shared_ptr<Svg> bg_svg = Svg::load(asset::plugin(pluginInstance, "res/meters/vintage_oscope_screen.svg"));
            if (bg_svg) {
                nvgSave(vg);
                float scaleX = box.size.x / 200.f;
                float scaleY = box.size.y / 200.f;
                nvgScale(vg, scaleX, scaleY);
                bg_svg->draw(vg);
                nvgRestore(vg);
            }
            
            // --- Enhanced Spherical CRT Effect ---
            // Create a pronounced spherical bulging effect with multiple layers
            
            // Layer 1: Main spherical highlight (larger and more prominent)
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x * 0.85f);
            NVGpaint mainHighlight = nvgRadialGradient(vg,
                box.size.x * 0.35f, box.size.y * 0.35f, // Offset highlight to top-left
                box.size.x * 0.05f, box.size.x * 0.6f,
                nvgRGBA(255, 255, 255, 35), // More visible white highlight
                nvgRGBA(255, 255, 255, 0));  // Fades to nothing
            nvgFillPaint(vg, mainHighlight);
            nvgFill(vg);
            
            // Layer 2: Bright center hotspot for glass dome effect
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x * 0.38f, box.size.y * 0.38f, box.size.x * 0.15f);
            NVGpaint centerHighlight = nvgRadialGradient(vg,
                box.size.x * 0.38f, box.size.y * 0.38f,
                0, box.size.x * 0.15f,
                nvgRGBA(255, 255, 255, 60), // Bright center
                nvgRGBA(255, 255, 255, 0));
            nvgFillPaint(vg, centerHighlight);
            nvgFill(vg);
            
            // Layer 3: Edge darkening for spherical depth
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x * 0.48f);
            NVGpaint edgeDarken = nvgRadialGradient(vg,
                box.size.x / 2.f, box.size.y / 2.f,
                box.size.x * 0.3f, box.size.x * 0.48f,
                nvgRGBA(0, 0, 0, 0),     // Transparent center
                nvgRGBA(0, 0, 0, 25));    // Dark edges
            nvgFillPaint(vg, edgeDarken);
            nvgFill(vg);
            
            // Layer 4: Subtle green phosphor glow enhancement
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x * 0.45f);
            NVGpaint phosphorGlow = nvgRadialGradient(vg,
                box.size.x / 2.f, box.size.y / 2.f,
                box.size.x * 0.1f, box.size.x * 0.45f,
                nvgRGBA(0, 180, 120, 15), // Green glow center
                nvgRGBA(0, 60, 40, 0));   // Fades to dark green
            nvgFillPaint(vg, phosphorGlow);
            nvgFill(vg);

            if (!source) return;

            nvgSave(vg);
            nvgScissor(vg, 0, 0, box.size.x, box.size.y);

            float margin = 0;
            float screenLeft = margin;
            float screenRight = box.size.x - margin;
            float screenTop = margin;
            float screenBottom = box.size.y - margin;
            float screenWidth = screenRight - screenLeft;
            float screenHeight = screenBottom - screenTop;
            
            // Function to map voltage to screen coordinates
            auto voltageToScreen = [&](Vec voltage) {
                // Normalize voltage from -10V..+10V to -1..+1
                float x_norm = clamp(voltage.x / 7.0f, -1.f, 1.f);
                float y_norm = clamp(voltage.y / 7.0f, -1.f, 1.f);
                
                // Map normalized coordinates to screen space
                float screenX = screenLeft + (x_norm + 1.f) * 0.5f * screenWidth;
                float screenY = screenTop + (1.f - (y_norm + 1.f) * 0.5f) * screenHeight;

                // Add "dust" artifact for a less clean, more analog look
                float fuzz_amount = 0.4f; // pixels of random noise
                float fuzz_x = (random::uniform() - 0.5f) * fuzz_amount;
                float fuzz_y = (random::uniform() - 0.5f) * fuzz_amount;
                
                return Vec(screenX + fuzz_x, screenY + fuzz_y);
            };
            
            // --- Vintage Persistence Drawing Logic ---
            const Vec* buffer = source->getOscilloscopeBuffer();
            const int bufferSize = source->getOscilloscopeBufferSize();
            const int currentIndex = source->getOscilloscopeBufferIndex();
            const int numChunks = 24; // More chunks for a smoother fade
            const int chunkSize = bufferSize / numChunks;

            // Set line style for smooth corners, which prevents the "starburst" artifact
            nvgLineJoin(vg, NVG_ROUND);
            nvgLineCap(vg, NVG_ROUND);

            for (int c = 0; c < numChunks; c++) {
                // Calculate age and alpha for this chunk (0.0=newest, 1.0=oldest)
                float age = (float)c / (numChunks - 1);
                // A balanced power curve for a noticeable but smooth decay effect
                float alpha = powf(1.0f - age, 1.8f);
                alpha = clamp(alpha, 0.f, 1.f);

                // Don't draw chunks that are fully faded
                if (alpha < 0.01f) continue;

                nvgBeginPath(vg);
                bool firstPointInChunk = true;

                // Iterate through points in this chunk, from newest to oldest
                for (int i = 0; i < chunkSize; i++) {
                    int point_offset = c * chunkSize + i;
                    if (point_offset >= bufferSize - 1) continue;

                    int buffer_idx = (currentIndex - 1 - point_offset + bufferSize) % bufferSize;

                    // Avoid the wrap-around point which causes a glitchy line
                    if (buffer_idx == currentIndex) {
                        firstPointInChunk = true; // This will cause a moveTo on the next valid point
                        continue;
                    }

                    Vec p = voltageToScreen(buffer[buffer_idx]);

                    if (firstPointInChunk) {
                        nvgMoveTo(vg, p.x, p.y);
                        firstPointInChunk = false;
                    } else {
                        nvgLineTo(vg, p.x, p.y);
                    }
                }

                // Stroke the entire chunk path with the calculated alpha
                // Glow effect
                nvgStrokeColor(vg, nvgRGBAf(0.2f, 1.f, 0.3f, alpha * 0.30f));
                nvgStrokeWidth(vg, 1.2f + alpha * 1.2f); // Thicker base, less variation
                nvgStroke(vg);

                // Main line
                nvgStrokeColor(vg, nvgRGBAf(0.4f, 1.f, 0.5f, alpha * 0.65f));
                nvgStrokeWidth(vg, 0.6f + alpha * 0.3f); // Thicker base, less variation
                nvgStroke(vg);
            }
            
            nvgRestore(vg);
        }
    }
};
