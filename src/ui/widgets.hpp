#pragma once
#include <rack.hpp>
#include <nanovg.h>
#include "../graphics/lighting.hpp"

using namespace rack;

namespace shapetaker {
namespace ui {

// ============================================================================
// CUSTOM LED WIDGETS
// ============================================================================

// Base class for jewel LEDs with RGB mixing and layered effects
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
        float radius = 0.5f * std::min(box.size.x, box.size.y);
        
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
        NVGpaint innerCore = nvgRadialGradient(args.vg, cx, cy, 0, radius * 0.35f,
            nvgRGBAf(1, 1, 1, 0.8f * maxBrightness), 
            nvgRGBAf(r * 1.5f, g * 1.5f, b * 1.5f, 0.6f * maxBrightness));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.35f);
        nvgFillPaint(args.vg, innerCore);
        nvgFill(args.vg);
        
        // Layer 4: Highlight
        NVGpaint highlight = nvgRadialGradient(args.vg, cx - radius * 0.15f, cy - radius * 0.15f,
            0, radius * 0.2f, nvgRGBAf(1, 1, 1, 0.9f * maxBrightness), nvgRGBAf(1, 1, 1, 0));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx - radius * 0.15f, cy - radius * 0.15f, radius * 0.2f);
        nvgFillPaint(args.vg, highlight);
        nvgFill(args.vg);
        
        // Layer 5: Rim definition
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.8f);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBAf(r * 0.8f, g * 0.8f, b * 0.8f, 0.4f * maxBrightness));
        nvgStroke(args.vg);
    }
    
    void drawOffState(const DrawArgs& args) {
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float radius = 0.5f * std::min(box.size.x, box.size.y);
        
        // Dark background
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.8f);
        nvgFillColor(args.vg, nvgRGBA(20, 20, 25, 180));
        nvgFill(args.vg);
        
        // Subtle rim
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.8f);
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStrokeColor(args.vg, nvgRGBA(60, 60, 70, 100));
        nvgStroke(args.vg);
    }
    
public:
    JewelLEDBase() {
        // Default; specific LEDs will set mm-based sizes in their constructors
        box.size = Vec(SIZE, SIZE);
    }
    
    void drawLight(const DrawArgs& args) override {
        if (!module) return;
        
        // Get the RGB light values from the module's lights array
        float brightness[3] = {};
        for (int i = 0; i < 3; i++) {
            if (firstLightId + i < (int)module->lights.size()) {
                brightness[i] = module->lights[firstLightId + i].getBrightness();
            }
        }
        drawJewelLayers(args, brightness[0], brightness[1], brightness[2], 
                       fmaxf(brightness[0], fmaxf(brightness[1], brightness[2])));
    }
};

// Specific LED sizes
class LargeJewelLED : public JewelLEDBase<30> {
public:
    LargeJewelLED() {
        bgColor = nvgRGBA(0, 0, 0, 0);
        borderColor = nvgRGBA(0, 0, 0, 0);
        // Add RGB base colors for the MultiLightWidget
        addBaseColor(nvgRGB(255, 0, 0));   // Red
        addBaseColor(nvgRGB(0, 255, 0));   // Green  
        addBaseColor(nvgRGB(0, 0, 255));   // Blue
        // Hardware-friendly lens: 12 mm
        box.size = mm2px(Vec(12.f, 12.f));
    }
};

class SmallJewelLED : public JewelLEDBase<15> {
public:
    SmallJewelLED() {
        bgColor = nvgRGBA(0, 0, 0, 0);
        borderColor = nvgRGBA(0, 0, 0, 0);
        // Add RGB base colors for the MultiLightWidget
        addBaseColor(nvgRGB(255, 0, 0));   // Red
        addBaseColor(nvgRGB(0, 255, 0));   // Green  
        addBaseColor(nvgRGB(0, 0, 255));   // Blue
        // Hardware-friendly lens: 10 mm
        box.size = mm2px(Vec(10.f, 10.f));
    }
};

// Medium-sized LEDs (20px) for transmutation module
class MediumJewelLED : public JewelLEDBase<20> {
public:
    MediumJewelLED() {
        bgColor = nvgRGBA(0, 0, 0, 0);
        borderColor = nvgRGBA(0, 0, 0, 0);
        // Add RGB base colors for the MultiLightWidget
        addBaseColor(nvgRGB(255, 0, 0));   // Red
        addBaseColor(nvgRGB(0, 255, 0));   // Green  
        addBaseColor(nvgRGB(0, 0, 255));   // Blue
        // Hardware-friendly lens: 12 mm (matches large for prominent use)
        box.size = mm2px(Vec(12.f, 12.f));
    }
};

// Teal-colored LED for Sequence A (pre-configured for teal color)
class TealJewelLEDMedium : public MediumJewelLED {
public:
    TealJewelLEDMedium() {
        // Override with teal color only
        baseColors.clear();
        addBaseColor(nvgRGB(0, 255, 180));   // Teal (#00FFB4)
    }
};

// Purple-colored LED for Sequence B (pre-configured for purple color)
class PurpleJewelLEDMedium : public MediumJewelLED {
public:
    PurpleJewelLEDMedium() {
        // Override with purple color only
        baseColors.clear();
        addBaseColor(nvgRGB(180, 0, 255));   // Purple (#B400FF)
    }
};

// ============================================================================
// MEASUREMENT/DISPLAY WIDGETS
// ============================================================================

// VU meter with configurable face and needle graphics
class VUMeterWidget : public widget::Widget {
private:
    Module* module = nullptr;
    int paramId = -1;
    int lightId = -1;
    std::string faceSvgPath;
    std::string needleSvgPath;
    
public:
    VUMeterWidget(Module* m, int pId, int lId, const std::string& faceSvg, const std::string& needleSvg)
        : module(m), paramId(pId), lightId(lId), faceSvgPath(faceSvg), needleSvgPath(needleSvg) {
        box.size = Vec(60, 60); // Default size
    }
    
    void draw(const DrawArgs& args) override {
        // Draw VU meter face
        std::shared_ptr<window::Svg> faceSvg = APP->window->loadSvg(faceSvgPath);
        if (faceSvg && faceSvg->handle) {
            nvgSave(args.vg);
            nvgScale(args.vg, box.size.x / faceSvg->handle->width, box.size.y / faceSvg->handle->height);
            svgDraw(args.vg, faceSvg->handle);
            nvgRestore(args.vg);
        }
        
        // Draw needle based on parameter value
        if (module && paramId >= 0) {
            float value = module->params[paramId].getValue();
            drawNeedle(args, value);
        }
        
        // Update lighting if applicable
        if (module && lightId >= 0) {
            float level = module->params[paramId].getValue();
            auto color = graphics::LightingHelper::getVUColor(level);
            graphics::LightingHelper::setRGBLight(module, lightId, color);
        }
    }
    
private:
    void drawNeedle(const DrawArgs& args, float value) {
        float angle = rescale(value, 0.f, 1.f, -45.f, 45.f); // -45° to +45°
        Vec center = box.size.mult(0.5f);
        
        nvgSave(args.vg);
        nvgTranslate(args.vg, center.x, center.y);
        nvgRotate(args.vg, angle * M_PI / 180.f);
        
        // Draw needle as a line
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, 0);
        nvgLineTo(args.vg, 0, -box.size.y * 0.35f);
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStrokeColor(args.vg, nvgRGB(220, 220, 220));
        nvgStroke(args.vg);
        
        nvgRestore(args.vg);
    }
};

// Vintage VU meter using a single SVG file with integrated meter and needle
class VintageVUMeterWidget : public widget::Widget {
private:
    Module* module = nullptr;
    int lightId = -1;
    std::string svgPath;
    
public:
    VintageVUMeterWidget(Module* m, int lId, const std::string& svg)
        : module(m), lightId(lId), svgPath(svg) {
        box.size = Vec(50, 50); // Default size
    }
    
    void draw(const DrawArgs& args) override {
        if (!module) return;
        
        // Draw the vintage VU meter SVG at full opacity
        std::shared_ptr<window::Svg> svg = APP->window->loadSvg(svgPath);
        if (svg && svg->handle) {
            nvgSave(args.vg);
            nvgScale(args.vg, box.size.x / svg->handle->width, box.size.y / svg->handle->height);
            svgDraw(args.vg, svg->handle);
            nvgRestore(args.vg);
        }
        
        // Draw animated needle based on VU level
        if (lightId >= 0 && lightId < (int)module->lights.size()) {
            float level = module->lights[lightId].getBrightness();
            drawVUNeedle(args, level);
        }
    }
    
private:
    void drawVUNeedle(const DrawArgs& args, float level) {
        // Map level (0.0-1.0) to needle angle
        // Start closer to -20 position and sweep to +3 position (slightly right of center)
        // This mimics typical VU meter range from -20dB to +3dB
        float angle = rescale(level, 0.0f, 1.0f, -55.0f, 15.0f); // Fine-tuned 5 degrees left to perfectly align with -20 symbol
        
        Vec center = box.size.mult(0.5f);
        // Pivot point at the semi-circle at bottom of meter screen (not the calibration circle)
        Vec pivotPoint = Vec(center.x, box.size.y * 0.65f); // 65% down - at bottom of meter screen
        float needleLength = box.size.y * 0.35f; // Length to reach the meter scale
        
        nvgSave(args.vg);
        nvgTranslate(args.vg, pivotPoint.x, pivotPoint.y);
        nvgRotate(args.vg, angle * M_PI / 180.0f);
        
        // Draw thin black needle from pivot point up to meter scale
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, 0); // Start at pivot (bottom of meter screen)
        nvgLineTo(args.vg, 0, -needleLength); // Go up to meter scale
        nvgStrokeWidth(args.vg, 1.0f); // Thinner needle
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 255)); // Black needle
        nvgStroke(args.vg);
        
        nvgRestore(args.vg);
    }
};

// Base class for oscilloscope-style visualizers with CRT effects
class VisualizerWidget : public Widget {
protected:
    Module* module = nullptr;
    std::vector<float> waveform;
    int maxSamples = 512;
    float timeScale = 1.0f;
    graphics::RGBColor traceColor = graphics::RGBColor(0, 1, 0.5f); // Default green
    
public:
    VisualizerWidget(Module* m) : module(m) {
        box.size = Vec(200, 100);
        waveform.resize(maxSamples, 0.f);
    }
    
    void setTraceColor(const graphics::RGBColor& color) { traceColor = color; }
    void setTimeScale(float scale) { timeScale = scale; }
    
    virtual void updateWaveform() = 0; // Implement in derived classes
    
    void draw(const DrawArgs& args) override {
        if (!module) return;
        
        updateWaveform();
        
        // Draw CRT background
        drawCRTBackground(args);
        
        // Draw waveform trace
        drawWaveform(args);
        
        // Add phosphor glow effect
        drawPhosphorEffect(args);
    }
    
private:
    void drawCRTBackground(const DrawArgs& args) {
        // Dark CRT background
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, nvgRGB(5, 10, 5));
        nvgFill(args.vg);
        
        // Grid lines
        nvgStrokeColor(args.vg, nvgRGBA(0, 80, 0, 40));
        nvgStrokeWidth(args.vg, 0.5f);
        
        // Vertical grid
        for (float x = 0; x < box.size.x; x += box.size.x / 8) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 0);
            nvgLineTo(args.vg, x, box.size.y);
            nvgStroke(args.vg);
        }
        
        // Horizontal grid
        for (float y = 0; y < box.size.y; y += box.size.y / 6) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, y);
            nvgLineTo(args.vg, box.size.x, y);
            nvgStroke(args.vg);
        }
    }
    
    void drawWaveform(const DrawArgs& args) {
        if (waveform.empty()) return;
        
        nvgBeginPath(args.vg);
        for (size_t i = 0; i < waveform.size(); i++) {
            float x = rescale(i, 0, waveform.size() - 1, 0, box.size.x);
            float y = rescale(waveform[i], -1.f, 1.f, box.size.y, 0);
            
            if (i == 0) {
                nvgMoveTo(args.vg, x, y);
            } else {
                nvgLineTo(args.vg, x, y);
            }
        }
        
        nvgStrokeColor(args.vg, traceColor.toNVG());
        nvgStrokeWidth(args.vg, 1.5f);
        nvgStroke(args.vg);
    }
    
    void drawPhosphorEffect(const DrawArgs& args) {
        // Add subtle phosphor glow along the trace
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        
        nvgBeginPath(args.vg);
        for (size_t i = 0; i < waveform.size(); i++) {
            float x = rescale(i, 0, waveform.size() - 1, 0, box.size.x);
            float y = rescale(waveform[i], -1.f, 1.f, box.size.y, 0);
            
            if (i == 0) {
                nvgMoveTo(args.vg, x, y);
            } else {
                nvgLineTo(args.vg, x, y);
            }
        }
        
        nvgStrokeColor(args.vg, nvgRGBAf(traceColor.r, traceColor.g, traceColor.b, 0.3f));
        nvgStrokeWidth(args.vg, 3.0f);
        nvgStroke(args.vg);
        
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
    }
};

}} // namespace shapetaker::ui
