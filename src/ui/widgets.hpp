#pragma once
#include <rack.hpp>
#include <nanovg.h>
#include <cmath>
#include <vector>
#include "../graphics/lighting.hpp"

// Forward declaration so bezels can load bundled assets without including plugin.hpp here
extern Plugin* pluginInstance;

using namespace rack;

namespace shapetaker {
namespace ui {

// ============================================================================
// CUSTOM LED WIDGETS
// ============================================================================

// Global brightness cap for jewel LEDs.  Lower values let the SVG facets
// show through at full drive, like a guitar-amp power jewel.
static constexpr float kJewelMaxBrightness = 0.55f;

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
        
        // Dark background (no hard rim to keep bezel clean)
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.8f);
        nvgFillColor(args.vg, nvgRGBA(24, 24, 28, 160));
        nvgFill(args.vg);
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
                       fmaxf(brightness[0], fmaxf(brightness[1], brightness[2])) * kJewelMaxBrightness);
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

// Jewel LED with integrated brass bezel (keeps lens smaller than the ring)
class SmallBezelJewelLED : public SmallJewelLED {
private:
    widget::SvgWidget* bezel = nullptr;

public:
    SmallBezelJewelLED() {
        // Overall footprint slightly larger so the bezel frames the lens
        box.size = mm2px(Vec(9.f, 9.f));

        bezel = new widget::SvgWidget;
        bezel->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ui/brass_bezel_small.svg")));
        bezel->box.size = box.size;
        bezel->box.pos = Vec(0.f, 0.f);
        this->addChild(bezel);
    }

    void onAdd(const widget::Widget::AddEvent& e) override {
        SmallJewelLED::onAdd(e);
        if (bezel && !bezel->parent) {
            addChild(bezel);
        }
    }

    void draw(const widget::Widget::DrawArgs& args) override {
        // Draw bezel via child; just render the lens smaller than the ring
        nvgSave(args.vg);
        const float s = 0.6f;
        nvgTranslate(args.vg, box.size.x * (1.f - s) * 0.5f, box.size.y * (1.f - s) * 0.5f);
        nvgScale(args.vg, s, s);
        SmallJewelLED::draw(args);
        nvgRestore(args.vg);
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

/**
 * Compact indicator light with an engraved brass bezel
 * Used for stage position LEDs on Torsion (gives hardware depth around the emitter)
 */
template<typename LIGHT = WhiteLight>
class BrassBezelSmallLight : public SmallLight<LIGHT> {
private:
    widget::SvgWidget* bezel = nullptr;

public:
    BrassBezelSmallLight() {
        this->box.size = mm2px(Vec(5.0f, 5.0f)); // slightly larger to frame the LED
        this->bgColor = nvgRGBA(0, 0, 0, 0);
        this->borderColor = nvgRGBA(0, 0, 0, 0);

        // Hide the stock SmallLight artwork and size the framebuffer to our bezel footprint
        if (this->sw) {
            this->sw->visible = false;
        }
        if (this->fb) {
            this->fb->box.size = this->box.size;
        }

        bezel = new widget::SvgWidget;
        bezel->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ui/brass_bezel_small.svg")));
        bezel->box.size = this->box.size;
        bezel->box.pos = Vec(0.f, 0.f);
        this->addChild(bezel);
    }

    void drawLight(const widget::Widget::DrawArgs& args) override {
        nvgSave(args.vg);

        // Keep the lit lens well inside the bezel aperture (5 mm widget ≈ 14 px)
        const float minSize = std::min(this->box.size.x, this->box.size.y);
        const float centerX = this->box.size.x * 0.5f;
        const float centerY = this->box.size.y * 0.5f;
        const float lensOffsetX = -0.35f;
        const float lensOffsetY = -0.35f;
        const float cx = centerX + lensOffsetX;
        const float cy = centerY + lensOffsetY;
        const float radius = 0.26f * minSize;        // slightly larger lens within the bezel
        const float innerRadius = radius * 0.58f;
        const float aperture = 0.3f * minSize;       // clip box roughly matching the bezel opening

        // Clip lens drawing so nothing bleeds past the aperture
        nvgScissor(args.vg, cx - aperture, cy - aperture, aperture * 2.f, aperture * 2.f);

        // Determine brightness from the incoming light color; fall back to alpha if channels are zero
        float brightness = std::max({this->color.a, this->color.r, this->color.g, this->color.b});

        // Always draw a neutral “unlit” lens so all stages look consistent when off
        NVGcolor unlitOuter = nvgRGBAf(0.82f, 0.82f, 0.82f, 0.04f);
        NVGcolor unlitInner = nvgRGBAf(1.f, 1.f, 1.f, 0.08f);
        NVGpaint unlitPaint = nvgRadialGradient(args.vg, cx, cy, innerRadius * 0.9f, radius,
            unlitInner, unlitOuter);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius);
        nvgFillPaint(args.vg, unlitPaint);
        nvgFill(args.vg);

        if (brightness > 1e-3f) {
            NVGcolor inner = this->color;
            inner.a *= 0.9f;
            NVGcolor outer = this->color;
            outer.a *= 0.18f;

            NVGpaint paint = nvgRadialGradient(args.vg, cx, cy, innerRadius, radius, inner, outer);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx, cy, radius);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);

            // Small highlight for depth, still within the aperture
            NVGcolor highlight = nvgRGBAf(1.f, 1.f, 1.f, inner.a * 0.22f);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx - radius * 0.12f, cy - radius * 0.12f, radius * 0.14f);
            nvgFillColor(args.vg, highlight);
            nvgFill(args.vg);
        }

        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }

    void drawHalo(const widget::Widget::DrawArgs& args) override {
        // Tuck the halo inside the bezel so the glow does not spill past the brass ring
        float brightness = std::max({this->color.a, this->color.r, this->color.g, this->color.b});
        if (brightness <= 1e-3f) {
            return;
        }

        // Soften intensity and clamp radius aggressively to keep light inside brass
        const float brightnessScale = 0.28f;
        NVGcolor haloColor = this->color;
        haloColor.a *= brightnessScale;

        const float cx = this->box.size.x * 0.5f - 0.35f;
        const float cy = this->box.size.y * 0.5f - 0.35f;
        const float radius = 0.25f * std::min(this->box.size.x, this->box.size.y); // fits inside bezel opening
        const float innerRadius = radius * 0.58f;

        NVGcolor inner = haloColor;
        inner.a *= 0.42f;
        NVGcolor outer = haloColor;
        outer.a *= 0.18f;

        NVGpaint halo = nvgRadialGradient(args.vg, cx, cy, innerRadius, radius, inner, outer);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, cx - radius, cy - radius, radius * 2.f, radius * 2.f);
        nvgFillPaint(args.vg, halo);
        nvgFill(args.vg);
    }

    void draw(const widget::Widget::DrawArgs& args) override {
        // Draw lens/halo first, then bezel over the top to cover any overhang
        drawLight(args);
        drawHalo(args);
        widget::Widget::draw(args);
    }
};

// Teal-colored LED for Sequence A (pre-configured for teal color)
class TealJewelLEDMedium : public MediumJewelLED {
public:
    TealJewelLEDMedium() {
        // Override with teal color only
        baseColors.clear();
        addBaseColor(nvgRGB(0, 154, 122));   // Teal (#009A7A)
    }
};

// Purple-colored LED for Sequence B (pre-configured for purple color)
class PurpleJewelLEDMedium : public MediumJewelLED {
public:
    PurpleJewelLEDMedium() {
        // Override with purple color only
        baseColors.clear();
        addBaseColor(nvgRGB(111, 31, 183));  // Purple (#6F1FB7)
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
        // Draw the vintage VU meter SVG face (always, including module browser).
        // Clip to the inner aperture so the baked-in thick SVG housing does not
        // visually override our custom slim bezel treatment.
        std::shared_ptr<window::Svg> svg = APP->window->loadSvg(svgPath);
        if (svg && svg->handle) {
            constexpr float kViewBoxW = 259.0896f;
            constexpr float kViewBoxH = 271.0356f;
            constexpr float kInnerX = 7.37f;
            constexpr float kInnerY = 7.07f;
            constexpr float kInnerW = 244.35f;
            constexpr float kInnerH = 256.97f;

            nvgSave(args.vg);
            float sx = box.size.x / svg->handle->width;
            float sy = box.size.y / svg->handle->height;
            nvgScale(args.vg, sx, sy);
            nvgScissor(args.vg,
                kInnerX * (svg->handle->width / kViewBoxW),
                kInnerY * (svg->handle->height / kViewBoxH),
                kInnerW * (svg->handle->width / kViewBoxW),
                kInnerH * (svg->handle->height / kViewBoxH));
            svgDraw(args.vg, svg->handle);
            nvgResetScissor(args.vg);
            nvgRestore(args.vg);
        }

        // Bakelite/powder-coat material finish (always drawn, even in browser)
        drawHousingFinish(args);

        if (!module) return;

        // Warm backlight glow on top of the meter face
        drawBacklightGlow(args);

        // Draw animated needle based on VU level
        if (lightId >= 0 && lightId < (int)module->lights.size()) {
            float value = clamp(module->lights[lightId].getBrightness(), 0.f, 1.f);
            drawVUNeedle(args, value);
        }
    }
private:

    void drawHousingFinish(const DrawArgs& args) {
        float w = box.size.x;
        float h = box.size.y;

        // --- Geometry matching the SVG housing rects (viewBox 259 x 271) ---
        // Outer housing ("housing main")
        float ox = w * (3.49f / 259.09f);
        float oy = h * (2.86f / 271.04f);
        float ow = w * (252.11f / 259.09f);
        float oh = h * (265.31f / 271.04f);
        float ocr = std::min(w, h) * 0.099f;

        // Original SVG inner rect (the boundary we're shrinking from)
        float svgIx = w * (7.37f / 259.09f);
        float svgIy = h * (7.07f / 271.04f);
        float svgIw = w * (244.35f / 259.09f);
        float svgIh = h * (256.97f / 271.04f);
        float svgIcr = std::min(w, h) * 0.096f;

        // Pull the inner boundary aggressively toward the outer frame for a
        // slimmer, sleeker bezel treatment.
        auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
        constexpr float kThinLerp = 0.88f;
        float ix = mix(svgIx, ox, kThinLerp);
        float iy = mix(svgIy, oy, kThinLerp);
        float iw = mix(svgIw, ow, kThinLerp);
        float ih = mix(svgIh, oh, kThinLerp);
        float icr = mix(svgIcr, ocr, kThinLerp);

        float bezelWidth = std::max(0.75f, std::min(ix - ox, iy - oy));
        float lipInset = std::max(0.4f, bezelWidth * 0.42f);
        float gx = ix + lipInset;
        float gy = iy + lipInset;
        float gw = std::max(1.f, iw - lipInset * 2.f);
        float gh = std::max(1.f, ih - lipInset * 2.f);
        float gcr = std::max(0.75f, icr - lipInset * 0.45f);

        // 0) Cover only the strip of SVG gray frame between the old and new
        //    inner boundaries so no gray peeks through, without hiding the face.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ix, iy, iw, ih, icr);
        nvgRoundedRect(args.vg, svgIx, svgIy, svgIw, svgIh, svgIcr);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);

        // 1) Main bezel ring.
        NVGpaint frameGrad = nvgLinearGradient(args.vg,
            0.f, oy, 0.f, oy + oh,
            nvgRGBA(148, 122, 94, 238),
            nvgRGBA(16, 12, 9, 246));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ox, oy, ow, oh, ocr);
        nvgRoundedRect(args.vg, ix, iy, iw, ih, icr);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, frameGrad);
        nvgFill(args.vg);

        // Strong outer edge definition so the thin bezel reads clearly at small sizes.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ox + 0.2f, oy + 0.2f, ow - 0.4f, oh - 0.4f, std::max(0.8f, ocr - 0.2f));
        nvgStrokeWidth(args.vg, 0.95f);
        nvgStrokeColor(args.vg, nvgRGBA(186, 162, 132, 68));
        nvgStroke(args.vg);

        // 2) Thin top catch-light for a polished edge.
        NVGpaint topCatch = nvgLinearGradient(args.vg,
            0.f, oy + oh * 0.02f, 0.f, oy + oh * 0.15f,
            nvgRGBA(236, 214, 186, 52), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ox + 0.35f, oy + 0.35f, ow - 0.7f, oh - 0.7f, ocr - 0.35f);
        nvgRoundedRect(args.vg, ix + 0.2f, iy + 0.2f, iw - 0.4f, ih - 0.4f, icr - 0.2f);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, topCatch);
        nvgFill(args.vg);

        // 3) Inner lip ring.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ix, iy, iw, ih, icr);
        nvgRoundedRect(args.vg, gx, gy, gw, gh, gcr);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint lipShade = nvgLinearGradient(args.vg,
            0.f, iy, 0.f, iy + ih,
            nvgRGBA(8, 8, 10, 220), nvgRGBA(74, 60, 44, 118));
        nvgFillPaint(args.vg, lipShade);
        nvgFill(args.vg);

        // Crisp inner edge line to emphasize the reduced bezel thickness.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, gx + 0.08f, gy + 0.08f, gw - 0.16f, gh - 0.16f, std::max(0.7f, gcr - 0.08f));
        nvgStrokeWidth(args.vg, 0.85f);
        nvgStrokeColor(args.vg, nvgRGBA(220, 192, 154, 38));
        nvgStroke(args.vg);

        // 4) Tight gasket so no legacy frame color can leak next to the face.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, gx, gy, gw, gh, gcr);
        float fx = gx + 0.35f;
        float fy = gy + 0.35f;
        float fw = std::max(1.f, gw - 0.7f);
        float fh = std::max(1.f, gh - 0.7f);
        float fcr = std::max(0.65f, gcr - 0.15f);
        nvgRoundedRect(args.vg, fx, fy, fw, fh, fcr);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGBA(8, 8, 10, 230));
        nvgFill(args.vg);

        // 5) Subtle inner boundary stroke.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, ix, iy, iw, ih, icr);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 92));
        nvgStroke(args.vg);
    }

    void drawBacklightGlow(const DrawArgs& args) {
        float w = box.size.x;
        float h = box.size.y;
        float cx = w * 0.5f;
        float cy = h * 0.45f;  // Center on the meter face
        float radius = std::max(w, h) * 0.38f;

        // Warm circular backlight glow — gradient fades to transparent naturally
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius);
        NVGpaint glow = nvgRadialGradient(args.vg,
            cx, cy, 0, radius,
            nvgRGBA(255, 220, 140, 115),
            nvgRGBA(255, 220, 140, 0));
        nvgFillPaint(args.vg, glow);
        nvgFill(args.vg);
    }

    void drawVUNeedle(const DrawArgs& args, float normalized) {
        normalized = clamp(normalized, 0.f, 1.f);

        float angle;
        constexpr float kAngleMin = 146.f;
        constexpr float kAngleMid = 90.f;
        constexpr float kAngleMax = 35.f;

        if (normalized <= 0.5f) {
            float t = rescale(normalized, 0.f, 0.5f, 0.f, 1.f);
            angle = kAngleMin - t * (kAngleMin - kAngleMid);
        } else {
            float t = rescale(normalized, 0.5f, 1.f, 0.f, 1.f);
            angle = kAngleMid - t * (kAngleMid - kAngleMax);
        }

        Vec center = box.size.mult(0.5f);
        Vec pivotPoint = Vec(center.x, box.size.y * 0.65f);
        float needleLength = box.size.y * 0.35f;

        nvgSave(args.vg);
        nvgTranslate(args.vg, pivotPoint.x, pivotPoint.y);
        nvgRotate(args.vg, (90.f - angle) * M_PI / 180.0f);

        NVGcolor bodyColor = nvgRGBA(12, 12, 12, 245);
        NVGcolor highlightColor = nvgRGBA(45, 45, 45, 180);

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, 0);
        nvgLineTo(args.vg, 0, -needleLength);
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStrokeColor(args.vg, bodyColor);
        nvgStroke(args.vg);

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 1.0f, -needleLength * 0.2f);
        nvgLineTo(args.vg, 1.0f, -needleLength);
        nvgStrokeWidth(args.vg, 0.9f);
        nvgStrokeColor(args.vg, highlightColor);
        nvgStroke(args.vg);

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 0.f, 0.f, needleLength * 0.08f);
        nvgFillColor(args.vg, nvgRGBA(30, 30, 30, 255));
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 0.f, 0.f, needleLength * 0.05f);
        nvgFillColor(args.vg, highlightColor);
        nvgFill(args.vg);

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

struct Trimpot : app::SvgKnob {
    Trimpot() {
        minAngle = -0.5 * M_PI;
        maxAngle = 0.5 * M_PI;
        setSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/Trimpot.svg")));
        box.size = mm2px(Vec(6, 6));
    }
};

struct VUCalibrationKnob : app::SvgKnob {
    VUCalibrationKnob() {
        minAngle = -0.5f * M_PI;
        maxAngle = 0.5f * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/knobs/vintage_vu_calibration.svg")));
        // Match the inner circle's proportional size in the meter
        // Inner circle diameter: 23.57 SVG units out of 259.09 meter width = 4.0mm
        float sizeMm = 23.57f / 259.0896f * (46.0f * 259.0896f / 271.0356f);
        box.size = mm2px(Vec(sizeMm, sizeMm));
    }
};

}} // namespace shapetaker::ui
