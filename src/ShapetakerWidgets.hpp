#pragma once
#include "plugin.hpp"
#include "ShapetakerUtils.hpp"

namespace shapetaker {

// ============================================================================
// CUSTOM LED WIDGETS
// ============================================================================

// Base class for jewel LEDs with RGB mixing
template<int SIZE>
class JewelLEDBase : public ModuleLightWidget {
protected:
    NVGcolor getLayeredColor(float r, float g, float b, float maxBrightness) const {
        return nvgRGBAf(r, g, b, fmaxf(r, fmaxf(g, b)) * maxBrightness);
    }
    
    void drawJewelLayers(const DrawArgs& args, float r, float g, float b, float maxBrightness) {
        if (maxBrightness < 0.01f) {
            drawOffState(args);
            return;
        }
        
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float radius = SIZE * 0.5f;
        
        // Layer 1: Large outer glow
        NVGpaint outerGlow = nvgRadialGradient(args.vg, cx, cy, radius * 0.5f, radius * 1.0f,
            nvgRGBAf(r, g, b, 0.6f * maxBrightness), nvgRGBAf(r, g, b, 0.0f));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius);
        nvgFillPaint(args.vg, outerGlow);
        nvgFill(args.vg);
        
        // Layer 2: Medium ring
        NVGpaint mediumRing = nvgRadialGradient(args.vg, cx, cy, radius * 0.25f, radius * 0.7f,
            nvgRGBAf(r * 1.2f, g * 1.2f, b * 1.2f, 0.9f * maxBrightness),
            nvgRGBAf(r, g, b, 0.3f * maxBrightness));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.7f);
        nvgFillPaint(args.vg, mediumRing);
        nvgFill(args.vg);
        
        // Layer 3: Inner core
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.5f);
        nvgFillColor(args.vg, nvgRGBAf(
            fminf(r * 1.5f, 1.0f), 
            fminf(g * 1.5f, 1.0f), 
            fminf(b * 1.5f, 1.0f), 
            1.0f
        ));
        nvgFill(args.vg);
        
        // Layer 4: Highlights for faceted effect
        drawJewelHighlights(args, cx, cy, radius, maxBrightness);
        
        // Layer 5: Dark rim
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.9f);
        nvgStrokeColor(args.vg, nvgRGBAf(0.2f, 0.2f, 0.2f, 0.8f));
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);
    }
    
    void drawJewelHighlights(const DrawArgs& args, float cx, float cy, float radius, float intensity) {
        // Main highlight (upper left)
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx - radius * 0.2f, cy - radius * 0.2f, radius * 0.15f);
        nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, intensity * 0.9f));
        nvgFill(args.vg);
        
        // Secondary highlight (right side)
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx + radius * 0.15f, cy - radius * 0.05f, radius * 0.08f);
        nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, intensity * 0.6f));
        nvgFill(args.vg);
        
        // Tiny sparkle
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx - radius * 0.05f, cy + radius * 0.1f, radius * 0.05f);
        nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, intensity * 0.8f));
        nvgFill(args.vg);
    }
    
    void drawOffState(const DrawArgs& args) {
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float radius = SIZE * 0.5f;
        
        // Base jewel when off
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.9f);
        nvgFillColor(args.vg, nvgRGBA(60, 60, 70, 255));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.7f);
        nvgFillColor(args.vg, nvgRGBA(30, 30, 35, 255));
        nvgFill(args.vg);
        
        // Subtle highlight when off
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx - radius * 0.15f, cy - radius * 0.15f, radius * 0.1f);
        nvgFillColor(args.vg, nvgRGBA(120, 120, 140, 100));
        nvgFill(args.vg);
    }

public:
    JewelLEDBase() {
        box.size = Vec(SIZE, SIZE);
        // Set up RGB color mixing
        addBaseColor(nvgRGB(0xff, 0x00, 0x00)); // Red
        addBaseColor(nvgRGB(0x00, 0xff, 0x00)); // Green
        addBaseColor(nvgRGB(0x00, 0x00, 0xff)); // Blue
    }
    
    void draw(const DrawArgs& args) override {
        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness();
            float g = module->lights[firstLightId + 1].getBrightness();
            float b = module->lights[firstLightId + 2].getBrightness();
            float maxBrightness = fmaxf(r, fmaxf(g, b));
            
            drawJewelLayers(args, r, g, b, maxBrightness);
        }
        
        // Draw SVG on top if it exists
        if (!children.empty()) {
            nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE, NVG_ONE_MINUS_SRC_ALPHA);
            Widget::draw(args);
            nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE, NVG_ONE_MINUS_SRC_ALPHA);
        }
    }
};

// Specific LED sizes
class LargeJewelLED : public JewelLEDBase<30> {
public:
    LargeJewelLED() {
        // Try to load large jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/leds/jewel_led_large.svg"));
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
    }
};

class SmallJewelLED : public JewelLEDBase<15> {
public:
    SmallJewelLED() {
        // Try to load small jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/leds/jewel_led_small.svg"));
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
    }
};

// ============================================================================
// VU METER WIDGET
// ============================================================================

class VUMeterWidget : public widget::Widget {
protected:
    Module* module;
    float* vuValue;
    std::string facePath;
    std::string needlePath;
    float meterSize;

public:
    VUMeterWidget(Module* module, float* vuValue, std::string facePath, 
                  std::string needlePath, float size = 50.0f) 
        : module(module), vuValue(vuValue), facePath(facePath), 
          needlePath(needlePath), meterSize(size) {
        box.size = Vec(size, size);
    }
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        
        // Draw the VU meter face
        std::shared_ptr<Svg> faceSvg = Svg::load(asset::plugin(pluginInstance, facePath));
        if (faceSvg) {
            NVGcontext* vg = args.vg;
            nvgSave(vg);
            nvgTranslate(vg, box.size.x / 2 - meterSize/2, box.size.y / 2 - meterSize/2);
            nvgScale(vg, meterSize / 100.0f, meterSize / 100.0f); // Assume 100x100 SVG
            faceSvg->draw(vg);
            nvgRestore(vg);
        }
        
        // Draw the needle
        if (module && vuValue) {
            std::shared_ptr<Svg> needleSvg = Svg::load(asset::plugin(pluginInstance, needlePath));
            if (needleSvg) {
                NVGcontext* vg = args.vg;
                nvgSave(vg);
                
                // Position and scale needle
                nvgTranslate(vg, box.size.x / 2 - meterSize/2, box.size.y / 2 - meterSize/2);
                nvgScale(vg, meterSize / 100.0f, meterSize / 100.0f);
                
                // Move to center and rotate
                nvgTranslate(vg, 50, 50); // Center of 100x100 face
                float angle = (*vuValue - 0.5f) * 90.0f * M_PI / 180.0f;
                nvgRotate(vg, angle);
                nvgTranslate(vg, -25, -25); // Center 50x50 needle
                
                needleSvg->draw(vg);
                nvgRestore(vg);
            }
        }
    }
};

// ============================================================================
// VISUALIZER WIDGETS
// ============================================================================

// Base class for oscilloscope-style visualizers
class VisualizerWidget : public Widget {
protected:
    Module* module;
    float time = 0.0f;
    
    void drawOscilloscopeFrame(const DrawArgs& args, float width, float height) {
        NVGcontext* vg = args.vg;
        float cx = width / 2.0f;
        float cy = height / 2.0f;
        
        // Background with backlit effect
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, width, height);
        NVGpaint bg = nvgRadialGradient(vg, cx, cy, 0, width * 0.6f,
            nvgRGB(18, 22, 28), nvgRGB(8, 10, 12));
        nvgFillPaint(vg, bg);
        nvgFill(vg);
        
        // Grid lines
        nvgStrokeColor(vg, nvgRGBA(0, 100, 255, 20));
        nvgStrokeWidth(vg, 0.5f);
        
        // Horizontal lines
        for (int i = 1; i < 5; i++) {
            float y = i * height / 5.0f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, 0, y);
            nvgLineTo(vg, width, y);
            nvgStroke(vg);
        }
        
        // Vertical lines
        for (int i = 1; i < 5; i++) {
            float x = i * width / 5.0f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, 0);
            nvgLineTo(vg, x, height);
            nvgStroke(vg);
        }
        
        // Phosphor glow effect
        drawPhosphorGlow(args, cx, cy, width * 0.4f);
        
        // Scanlines
        drawScanlines(args, width, height);
    }
    
    void drawPhosphorGlow(const DrawArgs& args, float cx, float cy, float radius) {
        // Outer glow
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        NVGpaint outerGlow = nvgRadialGradient(args.vg, cx, cy, radius * 0.7f, radius * 1.2f, 
            nvgRGBA(0, 110, 140, 60), nvgRGBA(0, 30, 40, 0));
        nvgFillPaint(args.vg, outerGlow);
        nvgFill(args.vg);
        
        // Inner glow
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        NVGpaint innerGlow = nvgRadialGradient(args.vg, cx, cy, radius * 0.5f, radius * 0.8f, 
            nvgRGBA(0, 150, 200, 120), nvgRGBA(0, 45, 60, 0));
        nvgFillPaint(args.vg, innerGlow);
        nvgFill(args.vg);
    }
    
    void drawScanlines(const DrawArgs& args, float width, float height) {
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 40));
        nvgStrokeWidth(args.vg, 0.5f);
        for (int i = 0; i < 20; i++) {
            float y = (i / 19.0f) * height;
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, y);
            nvgLineTo(args.vg, width, y);
            nvgStroke(args.vg);
        }
    }

public:
    VisualizerWidget(Module* module, float width = 120, float height = 100) 
        : module(module) {
        box.size = Vec(width, height);
    }
    
    void step() override {
        Widget::step();
        time += 1.0f / APP->window->getMonitorRefreshRate();
    }
};

// ============================================================================
// HELPER FUNCTIONS FOR COMMON WIDGET OPERATIONS
// ============================================================================

// Helper to create and position standard Shapetaker controls
namespace WidgetHelper {
    
    // Create input/output with standard positioning
    template<typename T>
    T* createIOCentered(Vec pos, Module* module, int portId) {
        return createInputCentered<T>(pos, module, portId);
    }
    
    // Create parameter with standard positioning and scaling
    template<typename T>
    T* createParamCentered(Vec pos, Module* module, int paramId) {
        return rack::createParamCentered<T>(pos, module, paramId);
    }
    
    // Create light with standard positioning
    template<typename T>
    T* createLightCentered(Vec pos, Module* module, int lightId) {
        return rack::createLightCentered<T>(pos, module, lightId);
    }
    
    // Standard screw placement for modules
    inline void addStandardScrews(ModuleWidget* widget) {
        widget->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        widget->addChild(createWidget<ScrewSilver>(Vec(widget->box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        widget->addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        widget->addChild(createWidget<ScrewSilver>(Vec(widget->box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    }
}

} // namespace shapetaker