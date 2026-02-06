#pragma once
#include <rack.hpp>
#include <nanovg.h>

using namespace rack;

extern Plugin* pluginInstance;

// Include reorganized utilities
#include "utilities.hpp"
#include "ui/label_formatter.hpp"

extern Model* modelClairaudient;
extern Model* modelChiaroscuro;
extern Model* modelFatebinder;
extern Model* modelInvolution;
extern Model* modelEvocation;
extern Model* modelIncantation;
extern Model* modelTransmutation;
extern Model* modelSpecula;
extern Model* modelChimera;
extern Model* modelTorsion;
extern Model* modelTessellation;
extern Model* modelPatina;

/**
 * Base knob class with universal fallback indicator support.
 * ONLY draws a procedural indicator if the knob uses blank_indicator.svg.
 * Knobs with custom indicators (set via setSvg after construction) are automatically skipped.
 */
struct ShapetakerKnobBase : app::SvgKnob {
    std::shared_ptr<Svg> blankIndicatorSvg;  // Cache the blank indicator SVG for comparison
    float indicatorRadiusFraction = 0.80f;   // Position on skirt - closer to center
    float indicatorSizeFraction = 0.05f;     // Smaller, more subtle indicator

    void checkForFallbackIndicator(const std::string& svgPath) {
        // Load and cache the blank indicator SVG for later comparison
        if (svgPath.find("blank_indicator.svg") != std::string::npos) {
            blankIndicatorSvg = Svg::load(svgPath);
        }
    }

    bool shouldDrawFallback() const {
        // Check at draw time if the current SVG is still the blank indicator
        // This handles cases where setSvg() is called after construction to set a custom indicator
        if (!blankIndicatorSvg) return false;
        if (!sw || !sw->svg) return false;
        return (sw->svg == blankIndicatorSvg);
    }

    void drawFallbackIndicator(const DrawArgs& args) {
        // Skip if not using blank indicator (i.e., has a custom indicator)
        if (!shouldDrawFallback()) return;

        // Get the current knob value and convert to angle
        float value = 0.f;
        if (getParamQuantity()) {
            value = getParamQuantity()->getScaledValue();
        }
        float angle = rack::math::rescale(value, 0.f, 1.f, minAngle, maxAngle);

        // Calculate indicator position on the skirt
        Vec center = box.size.div(2);
        float knobRadius = std::min(center.x, center.y);
        float indicatorDistance = knobRadius * indicatorRadiusFraction;
        float indicatorSize = knobRadius * indicatorSizeFraction;

        // Calculate indicator position
        float x = center.x + indicatorDistance * std::sin(angle);
        float y = center.y - indicatorDistance * std::cos(angle);

        // Draw small filled circle indicator using beige color from panel text (#c8c8b6)
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, x, y, indicatorSize);
        nvgFillColor(args.vg, nvgRGB(0xc8, 0xc8, 0xb6));  // Beige color matching panel aesthetics
        nvgFill(args.vg);
    }
};

/**
 * Jet black screw - procedurally drawn for a sleek modern look.
 */
struct ScrewJetBlack : widget::Widget {
    ScrewJetBlack() {
        box.size = Vec(15, 15);
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        float cx = box.size.x / 2.f;
        float cy = box.size.y / 2.f;
        float r = std::min(cx, cy) * 0.85f;

        // Outer ring - jet black
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, nvgRGB(8, 8, 8));
        nvgFill(vg);

        // Inner recess - slightly lighter for depth
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r * 0.7f);
        nvgFillColor(vg, nvgRGB(18, 18, 18));
        nvgFill(vg);

        // Cross slot - horizontal
        nvgBeginPath(vg);
        nvgRect(vg, cx - r * 0.5f, cy - r * 0.08f, r * 1.0f, r * 0.16f);
        nvgFillColor(vg, nvgRGB(2, 2, 2));
        nvgFill(vg);

        // Cross slot - vertical
        nvgBeginPath(vg);
        nvgRect(vg, cx - r * 0.08f, cy - r * 0.5f, r * 0.16f, r * 1.0f);
        nvgFillColor(vg, nvgRGB(2, 2, 2));
        nvgFill(vg);

        // Subtle highlight for dimension
        nvgBeginPath(vg);
        nvgCircle(vg, cx - r * 0.25f, cy - r * 0.25f, r * 0.15f);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 12));
        nvgFill(vg);
    }
};

/**
 * Knob shadow widget - draws a realistic drop shadow beneath knobs.
 * Used by all Shapetaker modules for consistent visual depth.
 */
struct KnobShadowWidget : widget::TransparentWidget {
    float padding = 6.f;
    float alpha = 0.32f;
    float offset = 0.f;
    float verticalScale = 0.7f;

    KnobShadowWidget(const Vec& knobSize, float paddingPx, float alpha_) {
        padding = paddingPx;
        alpha = alpha_;
        box.size = knobSize.plus(Vec(padding * 2.f, padding * 2.f));
        offset = padding * 0.58f;
        verticalScale = 0.65f;
    }

    void draw(const DrawArgs& args) override {
        Vec center = box.size.div(2.f);
        float outerR = std::max(box.size.x, box.size.y) * 0.5f;
        float innerR = std::max(0.f, outerR - padding);

        nvgSave(args.vg);
        nvgTranslate(args.vg, center.x, center.y + offset);
        nvgScale(args.vg, 1.f, verticalScale);

        NVGpaint paint = nvgRadialGradient(
            args.vg,
            0.f,
            0.f,
            innerR * 0.25f,
            outerR,
            nvgRGBAf(0.f, 0.f, 0.f, alpha),
            nvgRGBAf(0.f, 0.f, 0.f, 0.f));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 0.f, 0.f, outerR);
        nvgFillPaint(args.vg, paint);
        nvgFill(args.vg);

        nvgRestore(args.vg);
    }
};

/**
 * Helper function to add a knob with shadow to a module widget.
 * Usage: addKnobWithShadow(this, knob) instead of addParam(knob)
 */
inline void addKnobWithShadow(ModuleWidget* widget, app::ParamWidget* knob) {
    if (!knob || !widget) return;

    float diameter = std::max(knob->box.size.x, knob->box.size.y);
    // Subtle shadow: reduced padding and alpha to avoid bleeding into nearby text
    float padding = std::max(6.f, std::min(diameter * 0.25f, 14.f));
    float alpha = std::max(0.22f, std::min(0.38f, 0.18f + diameter * 0.004f));

    auto* shadow = new KnobShadowWidget(knob->box.size, padding, alpha);
    shadow->offset = padding * 0.45f;  // Subtle vertical offset
    shadow->box.pos = knob->box.pos.minus(Vec(padding, padding));
    widget->addChild(shadow);
    widget->addParam(knob);
}

// Shadow helpers currently disabled per request (leave hooks for future tuning).
inline void applyCircularShadow(app::SvgKnob* knob, float, float, float = 0.f, float = 0.f) {
    if (!knob || !knob->shadow) return;
    knob->shadow->visible = false;
}

// Hexagonal attenuverter shadow helper (disabled).
inline void applyHexShadow(app::SvgKnob* knob, float = 0.f, float = 0.f, float = 0.f, float = 0.f) {
    if (!knob || !knob->shadow) return;
    knob->shadow->visible = false;
}

struct ShapetakerKnobLarge : ShapetakerKnobBase {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the oscilloscope indicator variant
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_large.svg")));
        
        // Add background into the framebuffer below the rotating SVG (matches VCV Fundamental pattern)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_large_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: Large = 24 mm
        box.size = mm2px(Vec(24.f, 24.f));
        applyCircularShadow(this, 0.90f, 0.10f); // Large: face diameter ~56.5 of 63 viewBox
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the oscilloscope indicator variant
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_medium.svg")));
        
        // Add background into the framebuffer below the rotating SVG (matches VCV Fundamental pattern)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_medium_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: Medium = 20 mm
        box.size = mm2px(Vec(20.f, 20.f));
        applyCircularShadow(this, 0.78f, 0.10f); // Medium: face diameter ~40.4 of 52 viewBox
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobOscilloscopeMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobOscilloscopeMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_medium.svg")));
        
        // Add spherical background into the framebuffer below the rotating SVG
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_medium_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: Medium = 20 mm
        box.size = mm2px(Vec(20.f, 20.f));
        applyCircularShadow(this, 0.78f, 0.10f); // Medium variant
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobOscilloscopeLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobOscilloscopeLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the large oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_large.svg")));
        
        // Add spherical background into the framebuffer below the rotating SVG
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_large_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: Large = 24 mm
        box.size = mm2px(Vec(24.f, 24.f));
        applyCircularShadow(this, 0.90f, 0.10f);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobOscilloscopeSmall : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobOscilloscopeSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the small oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_small.svg")));
        
        // Add spherical background into the framebuffer below the rotating SVG
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_small_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: Small = 16 mm
        box.size = mm2px(Vec(16.f, 16.f));
        applyCircularShadow(this, 0.80f, 0.10f); // Small: face diameter 24 of 30 viewBox
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobOscilloscopeXLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobOscilloscopeXLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the extra large oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_xlarge.svg")));
        
        // Add spherical background into the framebuffer below the rotating SVG
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_xlarge_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: XLarge = 28 mm
        box.size = mm2px(Vec(28.f, 28.f));
        applyCircularShadow(this, 0.92f, 0.10f); // XLarge: face diameter ~44 of 48 viewBox
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobOscilloscopeHuge : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    
    ShapetakerKnobOscilloscopeHuge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the huge oscilloscope indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_huge.svg")));
        
        // Add spherical background into the framebuffer below the rotating SVG
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_huge_bg_light.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        // Target: Huge = 30 mm
        box.size = mm2px(Vec(30.f, 30.f));
        applyCircularShadow(this, 0.93f, 0.10f); // Huge: face diameter ~54 of 58 viewBox
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

// ============================================================================
// CHARRED KNOBS (for testing alternative aesthetics)
// ============================================================================

struct ShapetakerKnobCharredSmall : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobCharredSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/shapetaker_knob_ROTATE_charred_S.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/shapetaker_knob_BASE_charred_S.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(16.f, 16.f));

        applyCircularShadow(this, 0.80f, 0.10f);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobCharredMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobCharredMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/shapetaker_knob_ROTATE_charred_M.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/shapetaker_knob_BASE_charred_M.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(18.f, 18.f));

        applyCircularShadow(this, 0.82f, 0.10f);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

// ============================================================================
// VINTAGE CHUNKY KNOBS (tactile oscilloscope style)
// ============================================================================

struct ShapetakerKnobVintageSmall : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerKnobVintageSmall() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_rotate.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_background.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // Small: 12mm
        box.size = mm2px(Vec(12.f, 12.f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobVintageSmallMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerKnobVintageSmallMedium() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_rotate.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_background.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // Small-Medium: 15mm (between 12mm small and 18mm medium)
        box.size = mm2px(Vec(15.f, 15.f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobVintageMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerKnobVintageMedium() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        // Load the rotating indicator
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_rotate.svg")));

        // Load and add the static background
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_background.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // Medium: 19.8mm (18mm + 10%)
        box.size = mm2px(Vec(19.8f, 19.8f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobVintageLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerKnobVintageLarge() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_rotate.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_background.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // Large: 22mm
        box.size = mm2px(Vec(22.f, 22.f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobVintageXLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerKnobVintageXLarge() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_rotate.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_background.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // XLarge: 27mm (matches Davies XLarge at 1.5x)
        box.size = mm2px(Vec(27.f, 27.f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobVintageAttenuverter : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerKnobVintageAttenuverter() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_rotate.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/vintage_knob_background.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // Attenuverter: 10mm
        box.size = mm2px(Vec(10.f, 10.f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        Vec c = box.size.div(2);
        float scale = box.size.x / nativeSize.x;
        nvgTranslate(args.vg, c.x, c.y);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -c.x, -c.y);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

// ============================================================================
// DARK KNOBS (new charred aesthetic)
// ============================================================================

struct ShapetakerKnobDarkSmall : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobDarkSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/small-dark-rotating.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/small-dark-stationary.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(8.f, 8.f));

        applyCircularShadow(this, 0.88f, 0.08f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        Vec c = box.size.div(2);
        float scale = box.size.x / nativeSize.x;
        nvgTranslate(args.vg, c.x, c.y);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -c.x, -c.y);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobDarkMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobDarkMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/medium-dark-rotating.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/medium-dark-stationary.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(18.f, 18.f));

        applyCircularShadow(this, 0.88f, 0.08f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        Vec c = box.size.div(2);
        float scale = box.size.x / nativeSize.x;
        nvgTranslate(args.vg, c.x, c.y);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -c.x, -c.y);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobDarkLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobDarkLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/large-dark-rotating.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/large-dark-stationary.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(22.f, 22.f));

        applyCircularShadow(this, 0.88f, 0.08f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        Vec c = box.size.div(2);
        float scale = box.size.x / nativeSize.x;
        nvgTranslate(args.vg, c.x, c.y);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -c.x, -c.y);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

// ============================================================================
// DAVIES 1900H (dot indicator variants)
// ============================================================================

struct ShapetakerDavies1900hSmallDot : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerDavies1900hSmallDot() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        bg = new widget::SvgWidget;
        if (fb && tw) fb->addChildBelow(bg, tw);

        // Use the large Davies assets and scale down to the small size.
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_davies_1900h_large_indicator.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_davies_1900h_large_bg.svg")));
        nativeSize = bg->box.size;
        // Match the previous small knob footprint, then scale up 10%.
        box.size = Vec(39.6f, 39.6f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerDavies1900hLargeDot : app::SvgKnob {
    widget::SvgWidget* bg;

    ShapetakerDavies1900hLargeDot() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        bg = new widget::SvgWidget;
        if (fb && tw) fb->addChildBelow(bg, tw);

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_davies_1900h_large_indicator.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_davies_1900h_large_bg.svg")));
    }
};

// Davies 1900h extra-large (scaled up from large) with dot indicator
struct ShapetakerDavies1900hXLargeDot : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(54.f, 54.f);

    ShapetakerDavies1900hXLargeDot() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;

        bg = new widget::SvgWidget;
        if (fb && tw) fb->addChildBelow(bg, tw);

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_davies_1900h_large_indicator.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_davies_1900h_large_bg.svg")));
        nativeSize = bg->box.size;

        // Scale up 50% from the large size for the VCA control.
        box.size = nativeSize.mult(1.5f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobAltHuge : ShapetakerKnobBase {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobAltHuge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        // Use blank indicator - fallback will be drawn procedurally
        std::string indicatorPath = asset::plugin(pluginInstance, "res/knobs/indicators/blank_indicator.svg");
        setSvg(Svg::load(indicatorPath));
        checkForFallbackIndicator(indicatorPath);

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_alt_knob_huge.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(30.f, 30.f));

        applyCircularShadow(this, 0.90f, 0.10f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);

        // Draw fallback indicator if using blank_indicator.svg
        drawFallbackIndicator(args);
    }
};

struct ShapetakerKnobAltLarge : ShapetakerKnobBase {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobAltLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        // Use blank indicator - fallback will be drawn procedurally
        std::string indicatorPath = asset::plugin(pluginInstance, "res/knobs/indicators/blank_indicator.svg");
        setSvg(Svg::load(indicatorPath));
        checkForFallbackIndicator(indicatorPath);

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_alt_knob_large.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(24.f, 24.f));

        applyCircularShadow(this, 0.90f, 0.10f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);

        // Draw fallback indicator if using blank_indicator.svg
        drawFallbackIndicator(args);
    }
};

struct ShapetakerKnobAltMedium : ShapetakerKnobBase {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobAltMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        // Use blank indicator - fallback will be drawn procedurally
        std::string indicatorPath = asset::plugin(pluginInstance, "res/knobs/indicators/blank_indicator.svg");
        setSvg(Svg::load(indicatorPath));
        checkForFallbackIndicator(indicatorPath);

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_alt_knob_medium.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(20.f, 20.f));

        applyCircularShadow(this, 0.90f, 0.10f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);

        // Draw fallback indicator if using blank_indicator.svg
        drawFallbackIndicator(args);
    }
};

struct ShapetakerKnobAltSmall : ShapetakerKnobBase {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobAltSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        // Use blank indicator - fallback will be drawn procedurally
        std::string indicatorPath = asset::plugin(pluginInstance, "res/knobs/indicators/blank_indicator.svg");
        setSvg(Svg::load(indicatorPath));
        checkForFallbackIndicator(indicatorPath);

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_alt_knob_small.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(16.f, 16.f));

        applyCircularShadow(this, 0.90f, 0.10f);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);

        // Draw fallback indicator if using blank_indicator.svg
        drawFallbackIndicator(args);
    }
};

// Hallicrafters-inspired vintage knob - rugged bakelite with metallic collar
// Inspired by Hallicrafters SX-28, Collins radio equipment, classic military receivers
// 54x54px - same size as Davies1900h large for drop-in replacement
struct ShapetakerKnobHallicraftersMedium : app::SvgKnob {
    widget::SvgWidget* bg;

    ShapetakerKnobHallicraftersMedium() {
        minAngle = -0.83 * M_PI;  // Match Davies1900h rotation range
        maxAngle = 0.83 * M_PI;

        bg = new widget::SvgWidget;
        if (fb && tw) fb->addChildBelow(bg, tw);

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_hallicrafters_medium_indicator.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_hallicrafters_medium_bg.svg")));
    }
};

// Vintage British-style scalloped knob (medium) - rotating full knob
struct ShapetakerKnobNeveMedium : app::SvgKnob {
    ShapetakerKnobNeveMedium() {
        minAngle = -0.83 * M_PI;
        maxAngle = 0.83 * M_PI;
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_neve_medium_rotating.svg")));
    }
};

// Gear-style knob (medium) - PNG gear teeth + SVG body
struct ShapetakerKnobGearMedium : app::Knob {
    int imageHandle = -1;
    float minAngle = -0.83f * M_PI;
    float maxAngle = 0.83f * M_PI;
    std::string imagePath;
    bool imageLoaded = false;

    ShapetakerKnobGearMedium() {
        box.size = Vec(65, 65);
        imagePath = asset::plugin(pluginInstance, "res/knobs/indicators/064.png");
    }

    void draw(const DrawArgs& args) override {
        // Load image on first draw (needs valid NanoVG context)
        if (!imageLoaded) {
            imageHandle = nvgCreateImage(args.vg, imagePath.c_str(), 0);
            imageLoaded = true;
        }

        // Get rotation angle from parameter
        float angle = 0.f;
        if (getParamQuantity()) {
            float value = getParamQuantity()->getValue();
            float minVal = getParamQuantity()->getMinValue();
            float maxVal = getParamQuantity()->getMaxValue();
            float normalized = (value - minVal) / (maxVal - minVal);
            angle = math::rescale(normalized, 0.f, 1.f, minAngle, maxAngle);
        }

        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;

        // ============================================
        // ROTATING PART: Gear teeth + SVG body + pointer
        // ============================================
        nvgSave(args.vg);
        nvgTranslate(args.vg, cx, cy);
        nvgRotate(args.vg, angle);
        nvgTranslate(args.vg, -cx, -cy);

        // 1. Draw the PNG gear teeth (outer ring) - only if loaded
        if (imageHandle > 0) {
            NVGpaint imgPaint = nvgImagePattern(args.vg, 0, 0, box.size.x, box.size.y, 0, imageHandle, 1.0f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
            nvgFillPaint(args.vg, imgPaint);
            nvgFill(args.vg);
        }

        // 2. SVG knob body - covers PNG center, leaving gear teeth visible
        // Much smaller radius to reveal more of the gear teeth
        float bodyRadius = box.size.x * 0.20f;

        // Base dark circle
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, bodyRadius);
        nvgFillColor(args.vg, nvgRGB(30, 30, 30));
        nvgFill(args.vg);

        // Main body gradient (darker at bottom for depth)
        NVGpaint bodyGrad = nvgLinearGradient(args.vg,
            cx, cy - bodyRadius,
            cx, cy + bodyRadius,
            nvgRGB(55, 55, 55), nvgRGB(20, 20, 20));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, bodyRadius - 1);
        nvgFillPaint(args.vg, bodyGrad);
        nvgFill(args.vg);

        // Edge highlight (top arc)
        nvgBeginPath(args.vg);
        nvgArc(args.vg, cx, cy, bodyRadius - 1, -M_PI * 0.8f, -M_PI * 0.2f, NVG_CW);
        nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 80));
        nvgStrokeWidth(args.vg, 1.5f);
        nvgStroke(args.vg);

        // Edge shadow (bottom arc)
        nvgBeginPath(args.vg);
        nvgArc(args.vg, cx, cy, bodyRadius - 1, M_PI * 0.2f, M_PI * 0.8f, NVG_CW);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 100));
        nvgStrokeWidth(args.vg, 1.5f);
        nvgStroke(args.vg);

        // 3. Center cap
        float capRadius = box.size.x * 0.14f;
        NVGpaint capGrad = nvgLinearGradient(args.vg,
            cx, cy - capRadius,
            cx, cy + capRadius,
            nvgRGB(60, 60, 60), nvgRGB(25, 25, 25));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, capRadius);
        nvgFillPaint(args.vg, capGrad);
        nvgFill(args.vg);

        // Cap edge
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, capRadius);
        nvgStrokeColor(args.vg, nvgRGBA(90, 90, 90, 100));
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStroke(args.vg);

        // 4. Arrow pointer indicator
        float pointerDist = box.size.x * 0.26f;
        float pointerTip = box.size.x * 0.40f;
        float pointerWidth = box.size.x * 0.05f;

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, cx, cy - pointerTip);
        nvgLineTo(args.vg, cx - pointerWidth, cy - pointerDist);
        nvgLineTo(args.vg, cx + pointerWidth, cy - pointerDist);
        nvgClosePath(args.vg);

        NVGpaint pointerGrad = nvgLinearGradient(args.vg,
            cx, cy - pointerTip,
            cx, cy - pointerDist,
            nvgRGB(255, 255, 255), nvgRGB(180, 180, 180));
        nvgFillPaint(args.vg, pointerGrad);
        nvgFill(args.vg);

        nvgRestore(args.vg);

        // ============================================
        // STATIC PART: Light source gradient (top to bottom)
        // Counter-rotate to cancel out the knob rotation
        // ============================================
        nvgSave(args.vg);
        nvgTranslate(args.vg, cx, cy);
        nvgRotate(args.vg, -angle);  // Counter-rotate
        nvgTranslate(args.vg, -cx, -cy);

        NVGpaint lightGrad = nvgLinearGradient(args.vg,
            cx, 0,
            cx, box.size.y,
            nvgRGBA(255, 255, 255, 25), nvgRGBA(0, 0, 0, 30));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, box.size.x * 0.48f);
        nvgFillPaint(args.vg, lightGrad);
        nvgFill(args.vg);

        nvgRestore(args.vg);
    }
};

struct ShapetakerKnobDarkChicken : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobDarkChicken() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/chicken-dark-rotating.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/chicken-dark-stationary.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(22.f, 22.f));

        applyCircularShadow(this, 0.90f, 0.08f);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerOscilloscopeSwitch : app::SvgSwitch {
    ShapetakerOscilloscopeSwitch() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/shuttle-toggle-switch-off.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/shuttle-toggle-switch-on.svg")));
        // Disable the shadow by setting it to transparent
        shadow->visible = false;
        // Target widget box size increased by 50%: 9.5mm → 14.25mm (very noticeable change)
        box.size = mm2px(Vec(14.25f, 14.25f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Scale SVG frames (200x200) to fit our current box and center them
        const float svgSize = 200.f;
        float s = std::min(box.size.x, box.size.y) / svgSize;
        float tx = (box.size.x - svgSize * s) * 0.5f;
        float ty = (box.size.y - svgSize * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerVintageRussianToggle : app::SvgSwitch {
    ShapetakerVintageRussianToggle() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/vintage_toggle_switch_off.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/vintage_toggle_switch_on.svg")));
        shadow->visible = false;
        // Sized appropriately for vintage aesthetic: 12mm
        box.size = mm2px(Vec(12.0f, 12.0f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Scale SVG frames (22.592 x 32) to fit our current box and center them
        const float svgWidth = 22.592375f;
        const float svgHeight = 32.0f;
        float sx = box.size.x / svgWidth;
        float sy = box.size.y / svgHeight;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgWidth * s) * 0.5f;
        float ty = (box.size.y - svgHeight * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
};

// Art Deco paddle toggle switch - 1940s Hallicrafters SX-28 inspired
struct ShapetakerPaddleToggle : app::SvgSwitch {
    ShapetakerPaddleToggle() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/paddle_toggle_off.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/paddle_toggle_on.svg")));
        shadow->visible = false;
        // 7mm x 10mm paddle toggle
        box.size = mm2px(Vec(7.0f, 10.0f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Scale SVG frames (20 x 28 viewBox) to fit our current box
        const float svgWidth = 20.0f;
        const float svgHeight = 28.0f;
        float sx = box.size.x / svgWidth;
        float sy = box.size.y / svgHeight;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgWidth * s) * 0.5f;
        float ty = (box.size.y - svgHeight * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
};

// Classic bakelite bat-handle toggle - 1940s Hallicrafters style
struct ShapetakerBakeliteToggle : app::SvgSwitch {
    ShapetakerBakeliteToggle() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/bakelite_toggle_off.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/bakelite_toggle_on.svg")));
        shadow->visible = false;
        // 6.6mm x 9.9mm bakelite toggle (10% larger)
        box.size = mm2px(Vec(6.6f, 9.9f));
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Scale SVG frames (18 x 27 viewBox) to fit our current box
        const float svgWidth = 18.0f;
        const float svgHeight = 27.0f;
        float sx = box.size.x / svgWidth;
        float sy = box.size.y / svgHeight;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgWidth * s) * 0.5f;
        float ty = (box.size.y - svgHeight * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
};

// Dark slide toggle - Befaco size (9.5x10.7mm) with black body and grey lever
struct ShapetakerDarkToggle : app::SvgSwitch {
    ShapetakerDarkToggle() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/dark_toggle_off.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/dark_toggle_on.svg")));
        shadow->opacity = 0.0;
    }
};

struct ShapetakerVintageToggleSwitch : app::Switch {
    ShapetakerVintageToggleSwitch() {
        momentary = false;
        box.size = mm2px(Vec(2.99475f, 6.32225f));
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        float w = box.size.x;
        float h = box.size.y;

        nvgSave(vg);

        // Drop shadow for depth
        nvgBeginPath(vg);
        float shadowRadius = h * 0.35f;
        nvgRoundedRect(vg, w * 0.08f, h * 0.06f, w * 0.84f, h * 0.88f, shadowRadius);
        NVGpaint shadowPaint = nvgBoxGradient(vg, w * 0.5f, h * 0.5f, w * 0.7f, h * 0.8f, shadowRadius, w * 0.3f,
            nvgRGBA(0, 0, 0, 80), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);

        // Base plate - more realistic metal texture
        float radius = h * 0.3f;
        float inset = w * 0.08f;
        float insetY = h * 0.04f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset, insetY, w - inset * 2.f, h - insetY * 2.f, radius);
        NVGpaint basePaint = nvgLinearGradient(vg, inset, insetY, inset, h - insetY,
            nvgRGBA(52, 54, 58, 255),
            nvgRGBA(32, 33, 36, 255));
        nvgFillPaint(vg, basePaint);
        nvgFill(vg);

        // Outer rim shadow for depth
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 200));
        nvgStrokeWidth(vg, 0.8f);
        nvgStroke(vg);

        // Inner bevel highlight
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset + 0.6f, insetY + 0.5f, (w - inset * 2.f) - 1.2f, (h - insetY * 2.f) - 1.0f, radius * 0.85f);
        nvgStrokeColor(vg, nvgRGBA(75, 77, 82, 180));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        // Top sheen - subtler, more realistic
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset + 0.8f, insetY + 0.7f, (w - inset * 2.f) - 1.6f, (h - insetY * 2.f) * 0.45f, radius * 0.7f);
        NVGpaint sheen = nvgLinearGradient(vg, inset, insetY, inset, insetY + (h - insetY * 2.f) * 0.4f,
            nvgRGBA(255, 255, 255, 25),
            nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(vg, sheen);
        nvgFill(vg);

        // Mounting screws - smaller and more realistic
        auto drawScrew = [&](float x, float y) {
            float sr = w * 0.11f;
            // Screw shadow
            nvgBeginPath(vg);
            nvgCircle(vg, x + 0.15f, y + 0.15f, sr * 0.95f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
            nvgFill(vg);

            // Screw body
            nvgBeginPath(vg);
            nvgCircle(vg, x, y, sr);
            NVGpaint screwPaint = nvgRadialGradient(vg, x - sr * 0.3f, y - sr * 0.3f,
                sr * 0.1f, sr * 1.1f,
                nvgRGBA(195, 192, 185, 255),
                nvgRGBA(85, 85, 85, 255));
            nvgFillPaint(vg, screwPaint);
            nvgFill(vg);
            nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 180));
            nvgStrokeWidth(vg, 0.35f);
            nvgStroke(vg);

            // Screw slot
            nvgBeginPath(vg);
            nvgRect(vg, x - sr * 0.65f, y - sr * 0.12f, sr * 1.3f, sr * 0.24f);
            nvgFillColor(vg, nvgRGBA(30, 30, 30, 200));
            nvgFill(vg);
        };
        drawScrew(inset + (w - inset * 2.f) * 0.32f, insetY + (h - insetY * 2.f) * 0.25f);
        drawScrew(inset + (w - inset * 2.f) * 0.68f, insetY + (h - insetY * 2.f) * 0.75f);

        // Determine lever angle (blend between -30° and +30°)
        float value = 0.f;
        if (auto* pq = getParamQuantity()) {
            value = rack::math::clamp(pq->getValue(), 0.f, 1.f);
        }
        float theta = 30.f * (M_PI / 180.f);
        float angle = -theta + (2.f * theta) * value;

        float pivotX = w * 0.5f;
        float pivotY = insetY + (h - insetY * 2.f) * 0.63f;
        nvgTranslate(vg, pivotX, pivotY);
        nvgRotate(vg, angle);

        // Lever shadow
        nvgBeginPath(vg);
        float stemWidth = w * 0.26f;
        float stemLength = (h - insetY * 2.f) * 0.94f;
        nvgRoundedRect(vg, -stemWidth * 0.5f + 0.3f, -stemLength + 0.3f, stemWidth, stemLength, stemWidth * 0.4f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 50));
        nvgFill(vg);

        // Lever stem - brass finish (slightly darker for contrast)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, -stemWidth * 0.5f, -stemLength, stemWidth, stemLength, stemWidth * 0.4f);
        NVGpaint leverPaint = nvgLinearGradient(vg, -stemWidth * 0.5f, -stemLength, stemWidth * 0.5f, -stemLength,
            nvgRGBA(217, 191, 121, 255),
            nvgRGBA(125, 95, 40, 255));
        nvgFillPaint(vg, leverPaint);
        nvgFill(vg);

        // Lever edge highlights
        nvgStrokeColor(vg, nvgRGBA(235, 215, 155, 180));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);

        // Lever tip - brass cap
        float tipR = stemWidth * 0.92f;
        // Tip shadow
        nvgBeginPath(vg);
        nvgCircle(vg, 0.2f, -stemLength + 0.2f, tipR);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
        nvgFill(vg);

        // Tip body - darkened brass gradient
        nvgBeginPath(vg);
        nvgCircle(vg, 0.f, -stemLength, tipR);
        NVGpaint tipPaint = nvgRadialGradient(vg,
            -tipR * 0.35f, -stemLength - tipR * 0.35f,
            tipR * 0.15f, tipR * 1.15f,
            nvgRGBA(189, 152, 70, 255),
            nvgRGBA(96, 71, 29, 255));
        nvgFillPaint(vg, tipPaint);
        nvgFill(vg);

        // Tip rim - darker brass edge
        nvgStrokeColor(vg, nvgRGBA(72, 52, 22, 220));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);

        // Tip highlight - subtle metallic shine
        nvgBeginPath(vg);
        nvgCircle(vg, -tipR * 0.3f, -stemLength - tipR * 0.3f, tipR * 0.35f);
        nvgFillColor(vg, nvgRGBA(255, 235, 180, 50));
        nvgFill(vg);

        // Pivot collar - brushed metal appearance
        float collarR = stemWidth * 0.85f;
        // Collar shadow
        nvgBeginPath(vg);
        nvgCircle(vg, 0.15f, 0.15f, collarR);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 70));
        nvgFill(vg);

        // Collar body
        nvgBeginPath(vg);
        nvgCircle(vg, 0.f, 0.f, collarR);
        NVGpaint pivotPaint = nvgRadialGradient(vg,
            -collarR * 0.3f, -collarR * 0.3f,
            collarR * 0.15f, collarR * 1.1f,
            nvgRGBA(175, 175, 175, 255),
            nvgRGBA(95, 95, 95, 255));
        nvgFillPaint(vg, pivotPaint);
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 210));
        nvgStrokeWidth(vg, 0.45f);
        nvgStroke(vg);

        // Inner collar detail
        nvgBeginPath(vg);
        nvgCircle(vg, 0.f, 0.f, collarR * 0.6f);
        nvgStrokeColor(vg, nvgRGBA(65, 65, 65, 150));
        nvgStrokeWidth(vg, 0.3f);
        nvgStroke(vg);

        nvgRestore(vg);

        app::Switch::draw(args);
    }
};

struct ShapetakerVintageTripleSwitch : app::Switch {
    ShapetakerVintageTripleSwitch() {
        momentary = false;
        // Match the footprint of the brass 2-way toggle
        box.size = mm2px(Vec(12.f, 12.f));
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        float w = box.size.x;
        float h = box.size.y;

        nvgSave(vg);

        // Drop shadow
        nvgBeginPath(vg);
        float shadowRadius = h * 0.32f;
        nvgRoundedRect(vg, w * 0.08f, h * 0.06f, w * 0.84f, h * 0.88f, shadowRadius);
        NVGpaint shadowPaint = nvgBoxGradient(vg, w * 0.5f, h * 0.5f, w * 0.7f, h * 0.8f, shadowRadius, w * 0.3f,
            nvgRGBA(0, 0, 0, 80), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);

        // Base plate
        float radius = h * 0.28f;
        float inset = w * 0.08f;
        float insetY = h * 0.05f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset, insetY, w - inset * 2.f, h - insetY * 2.f, radius);
        NVGpaint basePaint = nvgLinearGradient(vg, inset, insetY, inset, h - insetY,
            nvgRGBA(52, 54, 58, 255),
            nvgRGBA(32, 33, 36, 255));
        nvgFillPaint(vg, basePaint);
        nvgFill(vg);

        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 200));
        nvgStrokeWidth(vg, 0.8f);
        nvgStroke(vg);

        // Inner bevel
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset + 0.6f, insetY + 0.5f, (w - inset * 2.f) - 1.2f, (h - insetY * 2.f) - 1.0f, radius * 0.85f);
        nvgStrokeColor(vg, nvgRGBA(75, 77, 82, 180));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        // Top sheen
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset + 0.8f, insetY + 0.7f, (w - inset * 2.f) - 1.6f, (h - insetY * 2.f) * 0.45f, radius * 0.7f);
        NVGpaint sheen = nvgLinearGradient(vg, inset, insetY, inset, insetY + (h - insetY * 2.f) * 0.4f,
            nvgRGBA(255, 255, 255, 25),
            nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(vg, sheen);
        nvgFill(vg);

        // Screws
        auto drawScrew = [&](float x, float y) {
            float sr = w * 0.11f;
            nvgBeginPath(vg);
            nvgCircle(vg, x + 0.15f, y + 0.15f, sr * 0.95f);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
            nvgFill(vg);

            nvgBeginPath(vg);
            nvgCircle(vg, x, y, sr);
            NVGpaint screwPaint = nvgRadialGradient(vg, x - sr * 0.3f, y - sr * 0.3f,
                sr * 0.1f, sr * 1.1f,
                nvgRGBA(195, 192, 185, 255),
                nvgRGBA(85, 85, 85, 255));
            nvgFillPaint(vg, screwPaint);
            nvgFill(vg);
            nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 180));
            nvgStrokeWidth(vg, 0.35f);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgRect(vg, x - sr * 0.65f, y - sr * 0.12f, sr * 1.3f, sr * 0.24f);
            nvgFillColor(vg, nvgRGBA(30, 30, 30, 200));
            nvgFill(vg);
        };
        drawScrew(inset + (w - inset * 2.f) * 0.32f, insetY + (h - insetY * 2.f) * 0.28f);
        drawScrew(inset + (w - inset * 2.f) * 0.68f, insetY + (h - insetY * 2.f) * 0.72f);

        // Angle across three states (-30°, 0°, +30°)
        float state = 0.f;
        if (auto* pq = getParamQuantity()) {
            state = rack::math::clamp(pq->getValue(), 0.f, 2.f);
        }
        float theta = 24.f * (M_PI / 180.f);
        float t = rack::math::rescale(state, 0.f, 2.f, -1.f, 1.f);
        float angle = t * theta;

        float pivotX = w * 0.5f;
        float pivotY = insetY + (h - insetY * 2.f) * 0.60f;
        nvgTranslate(vg, pivotX, pivotY);
        nvgRotate(vg, angle);

        // Lever shadow
        nvgBeginPath(vg);
        float stemWidth = w * 0.22f;
        float stemLength = (h - insetY * 2.f) * 0.78f;
        nvgRoundedRect(vg, -stemWidth * 0.5f + 0.3f, -stemLength + 0.3f, stemWidth, stemLength, stemWidth * 0.4f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 50));
        nvgFill(vg);

        // Lever stem - brass finish
        nvgBeginPath(vg);
        nvgRoundedRect(vg, -stemWidth * 0.5f, -stemLength, stemWidth, stemLength, stemWidth * 0.4f);
        NVGpaint leverPaint = nvgLinearGradient(vg, -stemWidth * 0.5f, -stemLength, stemWidth * 0.5f, -stemLength,
            nvgRGBA(217, 191, 121, 255),
            nvgRGBA(125, 95, 40, 255));
        nvgFillPaint(vg, leverPaint);
        nvgFill(vg);

        nvgStrokeColor(vg, nvgRGBA(235, 215, 155, 180));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);

        // Lever tip - brass cap
        float tipR = stemWidth * 0.92f;
        nvgBeginPath(vg);
        nvgCircle(vg, 0.2f, -stemLength + 0.2f, tipR);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgCircle(vg, 0.f, -stemLength, tipR);
        NVGpaint tipPaint = nvgRadialGradient(vg,
            -tipR * 0.35f, -stemLength - tipR * 0.35f,
            tipR * 0.15f, tipR * 1.15f,
            nvgRGBA(189, 152, 70, 255),
            nvgRGBA(96, 71, 29, 255));
        nvgFillPaint(vg, tipPaint);
        nvgFill(vg);

        nvgStrokeColor(vg, nvgRGBA(72, 52, 22, 220));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgCircle(vg, -tipR * 0.3f, -stemLength - tipR * 0.3f, tipR * 0.35f);
        nvgFillColor(vg, nvgRGBA(255, 235, 180, 50));
        nvgFill(vg);

        nvgRestore(vg);

        app::Switch::draw(args);
    }
};

// Note: LED glow for shuttle switches lives in the SVGs themselves (panel LEDs).

struct ShapetakerBNCPort : app::SvgPort {
    ShapetakerBNCPort() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/ports/st_bnc_connector.svg")));
        // Reduce overall footprint by ~5% from 9.2 mm -> 8.7 mm OD so plugs cover the knurl
        box.size = mm2px(Vec(8.7f, 8.7f));
    }
    void draw(const DrawArgs& args) override {
        // Scale the SVG to fit the current box (SVG viewBox is 20x20)
        const float svgSize = 20.f;
        float s = std::min(box.size.x, box.size.y) / svgSize;
        float tx = (box.size.x - svgSize * s) * 0.5f;
        float ty = (box.size.y - svgSize * s) * 0.5f;
        nvgSave(args.vg);
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgPort::draw(args);
        nvgRestore(args.vg);
    }
};

struct ShapetakerAttenuverterOscilloscope : app::SvgKnob {
    widget::SvgWidget* bg;
    Vec nativeSize = Vec(100.f, 100.f);
    ShapetakerAttenuverterOscilloscope() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        // Rotating indicator only
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_attenuverter_small_indicator.svg")));

        // Stationary background (gradient stays fixed while the indicator rotates)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_attenuverter_small_bg.svg")));
        nativeSize = bg->box.size;

        // Add background below the rotating transform widget (stationary)
        if (fb && tw) {
            fb->addChildBelow(bg, tw);
        }

        // Target: Attenuverter = 10 mm
        box.size = mm2px(Vec(10.f, 10.f));
        applyHexShadow(this, 0.94f, 0.08f, 0.20f, 0.65f);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float svgW = std::max(1.f, nativeSize.x);
        float svgH = std::max(1.f, nativeSize.y);
        float sx = box.size.x / svgW;
        float sy = box.size.y / svgH;
        float s = std::min(sx, sy);
        float tx = (box.size.x - svgW * s) * 0.5f;
        float ty = (box.size.y - svgH * s) * 0.5f;
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);
        app::SvgKnob::draw(args);
        nvgRestore(args.vg);
    }
};

// Shadow behavior is now integrated into the base Shapetaker knob classes above.

// Shapetaker vintage momentary button using a single SVG with a pressed overlay
struct ShapetakerVintageMomentary : app::SvgSwitch {
    ShapetakerVintageMomentary() {
        momentary = true;
        // Use the same SVG for both frames; we add a pressed overlay in draw()
        auto svgUp = Svg::load(asset::plugin(pluginInstance, "res/buttons/vintage_momentary_button.svg"));
        addFrame(svgUp);
        addFrame(svgUp);
        if (shadow) shadow->visible = false;
        // 9 x 9 mm footprint (hardware-friendly)
        box.size = mm2px(Vec(9.f, 9.f));
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        // Incoming SVG viewBox is 100x100; scale to our box
        const float s = box.size.x / 100.f;
        // Simulate a mechanical press: nudge downward and darken slightly when active
        const bool pressed = (getParamQuantity() && getParamQuantity()->getValue() > 0.5f);
        if (pressed) {
            nvgTranslate(args.vg, 0.f, 0.9f * s);
        }
        nvgScale(args.vg, s, s);
        app::SvgSwitch::draw(args);
        if (pressed) {
            // Subtle dark overlay to convey depth
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 50.f, 50.f, 35.f);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 40));
            nvgFill(args.vg);
        }
        nvgRestore(args.vg);
    }
};

// Lighted version of the vintage momentary button
struct ShapetakerVintageMomentaryLight : ShapetakerVintageMomentary {
    Module* module = nullptr;
    int lightId = -1;

    void draw(const DrawArgs& args) override {
        // Get light brightness
        float brightness = 0.f;
        if (module && lightId >= 0) {
            brightness = module->lights[lightId].getBrightness();
        }

        nvgSave(args.vg);
        // Incoming SVG viewBox is 100x100; scale to our box
        const float s = box.size.x / 100.f;
        // Simulate a mechanical press: nudge downward and darken slightly when active
        const bool pressed = (getParamQuantity() && getParamQuantity()->getValue() > 0.5f);
        if (pressed) {
            nvgTranslate(args.vg, 0.f, 0.9f * s);
        }
        nvgScale(args.vg, s, s);

        // Draw the button with brightness modulation
        if (brightness > 0.f) {
            // Add a bright glow to the button when lit
            nvgGlobalAlpha(args.vg, 1.0f);
        }

        app::SvgSwitch::draw(args);

        // Add white glow overlay when lit
        if (brightness > 0.f) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 50.f, 50.f, 35.f);
            // White glow proportional to brightness
            nvgFillColor(args.vg, nvgRGBA(255, 255, 255, (int)(brightness * 180.f)));
            nvgFill(args.vg);

            // Outer glow halo
            NVGcolor innerGlow = nvgRGBA(255, 255, 255, (int)(brightness * 100.f));
            NVGcolor outerGlow = nvgRGBA(255, 255, 255, 0);
            NVGpaint glow = nvgRadialGradient(args.vg, 50.f, 50.f, 35.f, 55.f, innerGlow, outerGlow);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 50.f, 50.f, 55.f);
            nvgFillPaint(args.vg, glow);
            nvgFill(args.vg);
        }

        if (pressed) {
            // Subtle dark overlay to convey depth
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 50.f, 50.f, 35.f);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 40));
            nvgFill(args.vg);
        }
        nvgRestore(args.vg);
    }
};

// Latching vintage button with built-in amber backlight
struct ShapetakerVintageLatchLED : app::SvgSwitch {
    ShapetakerVintageLatchLED() {
        momentary = false;
        auto svgUp = Svg::load(asset::plugin(pluginInstance, "res/buttons/vintage_momentary_button.svg"));
        addFrame(svgUp);
        addFrame(svgUp);
        if (shadow) shadow->visible = false;
        box.size = mm2px(Vec(9.f, 9.f));
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        const float s = box.size.x / 100.f;
        bool active = getParamQuantity() && getParamQuantity()->getValue() > 0.5f;
        if (active) {
            nvgTranslate(args.vg, 0.f, 0.8f * s);
        }
        nvgScale(args.vg, s, s);
        app::SvgSwitch::draw(args);
        if (active) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 50.f, 50.f, 33.f);
            nvgFillColor(args.vg, nvgRGBA(0xff, 0xb4, 0x3a, 120)); // warm amber
            nvgFill(args.vg);
        }
        nvgRestore(args.vg);
    }
};

struct RingLight : app::ModuleLightWidget {
    float ringThickness = 4.f;
    float glowThickness = 2.f;
    float innerRadiusOverride = -1.f;
    float outerRadiusOverride = -1.f;

    RingLight() {
        box.size = mm2px(Vec(11.f, 11.f));
        color = nvgRGB(0x2e, 0xea, 0xd8);
    }

    void draw(const DrawArgs& args) override {
        float brightness = module ? module->lights[firstLightId].getBrightness() : 0.f;
        if (brightness <= 0.f) return;

        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float outerRadius = outerRadiusOverride > 0.f ? outerRadiusOverride : std::min(box.size.x, box.size.y) * 0.5f;
        float innerRadius = innerRadiusOverride >= 0.f ? innerRadiusOverride : std::max(0.f, outerRadius - ringThickness);
        outerRadius = std::max(innerRadius + 1.f, outerRadius);

        NVGcolor ringColor = color;
        ringColor.a *= brightness;

        // Draw outer glow
        if (glowThickness > 0.f) {
            NVGcolor transparent = nvgRGBAf(ringColor.r, ringColor.g, ringColor.b, 0.f);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx, cy, outerRadius + glowThickness);
            nvgCircle(args.vg, cx, cy, innerRadius);
            nvgPathWinding(args.vg, NVG_HOLE);
            NVGpaint glow = nvgRadialGradient(args.vg, cx, cy, innerRadius, outerRadius + glowThickness, ringColor, transparent);
            nvgFillPaint(args.vg, glow);
            nvgFill(args.vg);
        }

        // Draw crisp ring stroke
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, (innerRadius + outerRadius) * 0.5f);
        nvgStrokeColor(args.vg, ringColor);
        nvgStrokeWidth(args.vg, std::max(1.f, outerRadius - innerRadius));
        nvgStroke(args.vg);
    }
};

// Alchemical-styled momentary buttons for REST/TIE to match symbol buttons
struct ShapetakerRestMomentary : app::SvgSwitch {
    ShapetakerRestMomentary() {
        momentary = true;
        if (shadow) shadow->visible = false;
        // 9 x 9 mm footprint
        box.size = mm2px(Vec(9.f, 9.f));
    }
    void draw(const DrawArgs& args) override {
        // Background — match AlchemicalSymbolWidget normal state and bevels
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(40, 40, 40, 100));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 150));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);

        // Inner shadow ring
        float inset = 1.0f;
        float rOuter = 3.0f;
        float rInner = std::max(0.0f, rOuter - 1.0f);
        NVGpaint innerShadow = nvgBoxGradient(
            args.vg,
            inset, inset,
            box.size.x - inset * 2.0f,
            box.size.y - inset * 2.0f,
            rInner, 3.5f,
            nvgRGBA(0, 0, 0, 50), nvgRGBA(0, 0, 0, 0)
        );
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, innerShadow);
        nvgFill(args.vg);

        // Top highlight
        nvgSave(args.vg);
        nvgScissor(args.vg, 0, 0, box.size.x, std::min(6.0f, box.size.y));
        NVGpaint topHi = nvgLinearGradient(args.vg, 0, 0, 0, 6.0f, nvgRGBA(255, 255, 255, 28), nvgRGBA(255, 255, 255, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, topHi);
        nvgFill(args.vg);
        nvgRestore(args.vg);

        // Side highlights
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint leftHi = nvgLinearGradient(args.vg, inset - 0.5f, 0, inset + 4.5f, 0, nvgRGBA(255, 255, 255, 18), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, leftHi);
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint rightHi = nvgLinearGradient(args.vg, box.size.x - (inset - 0.5f), 0, box.size.x - (inset + 4.5f), 0, nvgRGBA(255, 255, 255, 12), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, rightHi);
        nvgFill(args.vg);

        // Draw REST glyph in vintage ink
        NVGcolor ink = nvgRGBA(232, 224, 200, 230);
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float w = std::min(box.size.x, box.size.y) * 0.60f;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, cx - w * 0.5f, cy);
        nvgLineTo(args.vg, cx + w * 0.5f, cy);
        nvgStrokeColor(args.vg, ink);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgStrokeWidth(args.vg, rack::clamp(w * 0.10f, 1.0f, 2.0f));
        nvgStroke(args.vg);

        // Pressed overlay for feedback
        bool pressed = false;
        if (getParamQuantity()) pressed = getParamQuantity()->getValue() > 0.5f;
        if (pressed) {
            nvgSave(args.vg);
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 40));
            nvgFill(args.vg);
            nvgRestore(args.vg);
        }
    }
};

struct ShapetakerTieMomentary : app::SvgSwitch {
    ShapetakerTieMomentary() {
        momentary = true;
        if (shadow) shadow->visible = false;
        // 9 x 9 mm footprint
        box.size = mm2px(Vec(9.f, 9.f));
        if (shadow) shadow->visible = false;
        box.size = Vec(18.f, 18.f);
    }
    void draw(const DrawArgs& args) override {
        // Background — match AlchemicalSymbolWidget normal state and bevels
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(40, 40, 40, 100));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 150));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);

        // Inner shadow ring
        float inset = 1.0f;
        float rOuter = 3.0f;
        float rInner = std::max(0.0f, rOuter - 1.0f);
        NVGpaint innerShadow = nvgBoxGradient(
            args.vg,
            inset, inset,
            box.size.x - inset * 2.0f,
            box.size.y - inset * 2.0f,
            rInner, 3.5f,
            nvgRGBA(0, 0, 0, 50), nvgRGBA(0, 0, 0, 0)
        );
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, innerShadow);
        nvgFill(args.vg);

        // Top highlight
        nvgSave(args.vg);
        nvgScissor(args.vg, 0, 0, box.size.x, std::min(6.0f, box.size.y));
        NVGpaint topHi = nvgLinearGradient(args.vg, 0, 0, 0, 6.0f, nvgRGBA(255, 255, 255, 28), nvgRGBA(255, 255, 255, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, topHi);
        nvgFill(args.vg);
        nvgRestore(args.vg);

        // Side highlights
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint leftHi = nvgLinearGradient(args.vg, inset - 0.5f, 0, inset + 4.5f, 0, nvgRGBA(255, 255, 255, 18), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, leftHi);
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint rightHi = nvgLinearGradient(args.vg, box.size.x - (inset - 0.5f), 0, box.size.x - (inset + 4.5f), 0, nvgRGBA(255, 255, 255, 12), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, rightHi);
        nvgFill(args.vg);

        // Draw TIE glyph (lower arc) in vintage ink
        NVGcolor ink = nvgRGBA(232, 224, 200, 230);
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.52f;
        float r = std::min(box.size.x, box.size.y) * 0.32f;
        nvgBeginPath(args.vg);
        nvgArc(args.vg, cx, cy, r, M_PI * 1.15f, M_PI * 1.85f, NVG_CW);
        nvgStrokeColor(args.vg, ink);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgStrokeWidth(args.vg, rack::clamp(r * 0.28f, 1.0f, 2.0f));
        nvgStroke(args.vg);

        // Pressed overlay for feedback
        bool pressed = false;
        if (getParamQuantity()) pressed = getParamQuantity()->getValue() > 0.5f;
        if (pressed) {
            nvgSave(args.vg);
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 40));
            nvgFill(args.vg);
            nvgRestore(args.vg);
        }
    }
};

struct ShapetakerVintageSelector : app::ParamWidget {
    std::shared_ptr<Svg> bgSvg;
    std::shared_ptr<Svg> pointerSvg;
    float minAngle = 0.f;
    float maxAngle = 5.f * M_PI / 3.f;
    bool snap = true;
    float accumulatedDelta = 0.f; // Accumulate small movements

    ShapetakerVintageSelector() {
        // Load SVGs
        bgSvg = Svg::load(asset::plugin(pluginInstance, "res/switches/distortion_selector.svg"));
        pointerSvg = Svg::load(asset::plugin(pluginInstance, "res/switches/distortion_selector_pointer.svg"));

        // Size to fit within 80px LED ring (20mm ≈ 75.6px)
        box.size = mm2px(Vec(20.0f, 20.0f));
    }

    void onDragMove(const event::DragMove& e) override {
        if (getParamQuantity()) {
            // Accumulate movement for very responsive control
            float sensitivity = 1.0f;
            float delta = sensitivity * (e.mouseDelta.x - e.mouseDelta.y);
            accumulatedDelta += delta;

            // For discrete selector - step when accumulated movement reaches threshold
            if (snap) {
                float stepThreshold = 75.0f; // Moderate threshold for natural stepping

                if (fabsf(accumulatedDelta) >= stepThreshold) {
                    float oldValue = getParamQuantity()->getValue();
                    float step = (accumulatedDelta > 0) ? 1.0f : -1.0f;
                    float newValue = clamp(oldValue + step,
                                         getParamQuantity()->minValue,
                                         getParamQuantity()->maxValue);
                    getParamQuantity()->setValue(newValue);

                    // Reset accumulator after step, but keep remainder
                    accumulatedDelta = fmodf(accumulatedDelta, stepThreshold);
                }
            } else {
                // Continuous mode - very sensitive, immediate response
                float paramRange = getParamQuantity()->maxValue - getParamQuantity()->minValue;
                float newValue = getParamQuantity()->getValue() + delta * paramRange * 0.003f;
                newValue = clamp(newValue, getParamQuantity()->minValue, getParamQuantity()->maxValue);
                getParamQuantity()->setValue(newValue);
            }
        }
    }

    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            e.consume(this);
        }
        ParamWidget::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        // Draw background at full widget size
        if (bgSvg) {
            nvgSave(args.vg);
            float scale = box.size.x / bgSvg->handle->width;
            nvgScale(args.vg, scale, scale);
            svgDraw(args.vg, bgSvg->handle);
            nvgRestore(args.vg);
        }

        // Draw pointer rotated based on parameter value
        if (pointerSvg && getParamQuantity()) {
            nvgSave(args.vg);

            // Calculate rotation angle
            float t = getParamQuantity()->getScaledValue();
            float angle = math::rescale(t, 0.f, 1.f, minAngle, maxAngle);

            // Rotate around center
            nvgTranslate(args.vg, box.size.x * 0.5f, box.size.y * 0.5f);
            nvgRotate(args.vg, angle);
            nvgTranslate(args.vg, -box.size.x * 0.5f, -box.size.y * 0.5f);

            float scale = box.size.x / pointerSvg->handle->width;
            nvgScale(args.vg, scale, scale);
            svgDraw(args.vg, pointerSvg->handle);
            nvgRestore(args.vg);
        }
    }
};

// Horizontal 6-way selector for Chiaroscuro distortion types
struct ShapetakerHorizontalDistortionSelector : app::ParamWidget {
    float accumulatedDelta = 0.f;

    ShapetakerHorizontalDistortionSelector() {
        box.size = mm2px(Vec(23.0f, 6.0f));
    }

    int stepCount() {
        if (auto* pq = getParamQuantity()) {
            float range = pq->maxValue - pq->minValue;
            return std::max(2, (int)std::round(range) + 1);
        }
        return 6;
    }

    void geometry(float& left, float& right, float& trackY, float& trackH, float& knobR) const {
        knobR = box.size.y * 0.45f;
        float margin = box.size.x * 0.02f;
        left = knobR + margin;
        right = box.size.x - knobR - margin;
        trackH = box.size.y * 0.36f;
        trackY = (box.size.y - trackH) * 0.5f;
    }

    void setValueFromPos(float x) {
        auto* pq = getParamQuantity();
        if (!pq) {
            return;
        }
        float left = 0.f;
        float right = 0.f;
        float trackY = 0.f;
        float trackH = 0.f;
        float knobR = 0.f;
        geometry(left, right, trackY, trackH, knobR);
        float t = 0.f;
        if (right > left) {
            t = clamp((x - left) / (right - left), 0.f, 1.f);
        }
        int steps = stepCount();
        float value = pq->minValue + std::round(t * (steps - 1));
        pq->setValue(clamp(value, pq->minValue, pq->maxValue));
    }

    void onDragStart(const event::DragStart& e) override {
        accumulatedDelta = 0.f;
        ParamWidget::onDragStart(e);
    }

    void onDragMove(const event::DragMove& e) override {
        auto* pq = getParamQuantity();
        if (!pq) {
            return;
        }
        accumulatedDelta += e.mouseDelta.x;
        float stepThreshold = 32.f;
        if (std::fabs(accumulatedDelta) >= stepThreshold) {
            float dir = (accumulatedDelta > 0.f) ? 1.f : -1.f;
            float value = pq->getValue() + dir;
            pq->setValue(clamp(value, pq->minValue, pq->maxValue));
            accumulatedDelta = 0.f;
        }
    }

    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            setValueFromPos(e.pos.x);
            e.consume(this);
        }
        ParamWidget::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        auto* pq = getParamQuantity();
        float left = 0.f;
        float right = 0.f;
        float trackY = 0.f;
        float trackH = 0.f;
        float knobR = 0.f;
        geometry(left, right, trackY, trackH, knobR);

        NVGcontext* vg = args.vg;
        float w = box.size.x;
        float h = box.size.y;
        float cy = h * 0.5f;

        nvgSave(vg);

        // === HOUSING: recessed anodized aluminum mounting plate ===

        // Outer lip shadow (housing sits recessed into the panel)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, -1.0f, -0.5f, w + 2.0f, h + 2.0f, 4.0f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 50));
        nvgFill(vg);

        // Panel lip highlight above housing (light catches top edge of recess)
        nvgBeginPath(vg);
        nvgMoveTo(vg, 1.0f, -0.5f);
        nvgLineTo(vg, w - 1.0f, -0.5f);
        nvgStrokeColor(vg, nvgRGBA(95, 98, 105, 60));
        nvgStrokeWidth(vg, 0.6f);
        nvgStroke(vg);

        // Drop shadow beneath housing body (depth from panel surface)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.5f, 2.0f, w - 1.f, h, 3.0f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 85));
        nvgFill(vg);

        // Main housing body
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, w, h, 3.0f);
        NVGpaint housing = nvgLinearGradient(vg, 0, 0, 0, h,
            nvgRGBA(82, 84, 90, 255),
            nvgRGBA(38, 40, 44, 255));
        nvgFillPaint(vg, housing);
        nvgFill(vg);

        // Brushed metal texture (horizontal grain)
        for (int i = 0; i < 16; i++) {
            float ly = 1.5f + (h - 3.0f) * ((float)i / 15.0f);
            nvgBeginPath(vg);
            nvgMoveTo(vg, 2.5f, ly);
            nvgLineTo(vg, w - 2.5f, ly);
            int alpha = (i % 3 == 0) ? 22 : ((i % 3 == 1) ? 12 : 8);
            nvgStrokeColor(vg, nvgRGBA(115, 118, 124, alpha));
            nvgStrokeWidth(vg, 0.3f);
            nvgStroke(vg);
        }

        // Top chamfer highlight
        nvgBeginPath(vg);
        nvgMoveTo(vg, 3.0f, 0.8f);
        nvgLineTo(vg, w - 3.0f, 0.8f);
        nvgStrokeColor(vg, nvgRGBA(140, 145, 150, 80));
        nvgStrokeWidth(vg, 0.7f);
        nvgStroke(vg);

        // Bottom shadow edge (housing bottom is darker)
        nvgBeginPath(vg);
        nvgMoveTo(vg, 3.0f, h - 0.5f);
        nvgLineTo(vg, w - 3.0f, h - 0.5f);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 70));
        nvgStrokeWidth(vg, 0.7f);
        nvgStroke(vg);

        // Housing edge bevel (dark outline)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, w, h, 3.0f);
        nvgStrokeColor(vg, nvgRGBA(12, 12, 15, 220));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        // Inner highlight (inset border for recessed plate look)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 1.0f, 1.0f, w - 2.0f, h - 2.0f, 2.5f);
        nvgStrokeColor(vg, nvgRGBA(100, 103, 110, 35));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        // === MACHINED CHANNEL GROOVE (deep recess) ===
        float channelH = h * 0.32f;
        float channelY = cy - channelH * 0.5f;
        float channelLeft = left - knobR * 0.35f;
        float channelRight = right + knobR * 0.35f;
        float channelW = channelRight - channelLeft;
        float channelR = channelH * 0.45f;

        // Outer shadow halo (soft shadow around the groove for depth)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, channelLeft - 1.5f, channelY - 1.5f,
            channelW + 3.f, channelH + 3.f, channelR + 1.0f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 50));
        nvgFill(vg);

        // Inner shadow (sharp dark edge at top of groove = deep cut)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, channelLeft - 0.5f, channelY - 1.0f,
            channelW + 1.f, channelH + 1.5f, channelR);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 150));
        nvgFill(vg);

        // Channel body (dark machined slot)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, channelLeft, channelY, channelW, channelH, channelR);
        NVGpaint channelPaint = nvgLinearGradient(vg, 0, channelY, 0, channelY + channelH,
            nvgRGBA(5, 5, 8, 255),
            nvgRGBA(18, 19, 24, 255));
        nvgFillPaint(vg, channelPaint);
        nvgFill(vg);

        // Channel floor specular (faint reflection on the groove floor)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, channelLeft + 2.0f, channelY + channelH * 0.55f,
            channelW - 4.0f, channelH * 0.35f, channelR * 0.5f);
        nvgFillColor(vg, nvgRGBA(55, 58, 65, 20));
        nvgFill(vg);

        // Channel top inner edge (dark bevel inside groove)
        nvgBeginPath(vg);
        nvgMoveTo(vg, channelLeft + channelR, channelY + 0.5f);
        nvgLineTo(vg, channelRight - channelR, channelY + 0.5f);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 100));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        // Channel bottom lip highlight (light catches the lower edge)
        nvgBeginPath(vg);
        nvgMoveTo(vg, channelLeft + channelR, channelY + channelH);
        nvgLineTo(vg, channelRight - channelR, channelY + channelH);
        nvgStrokeColor(vg, nvgRGBA(90, 92, 100, 55));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        // === HORIZONTAL GUIDE RAIL (silver throttle bar) ===
        {
            float railH = channelH * 0.38f;
            float railY = cy - railH * 0.5f;
            float railInset = channelR * 0.6f;
            float railLeft = channelLeft + railInset;
            float railW = channelW - railInset * 2.0f;
            float railR = railH * 0.35f;

            // Rail shadow (sits slightly recessed)
            nvgBeginPath(vg);
            nvgRoundedRect(vg, railLeft, railY + 0.6f, railW, railH, railR);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 90));
            nvgFill(vg);

            // Rail body — brushed silver cylinder
            nvgBeginPath(vg);
            nvgRoundedRect(vg, railLeft, railY, railW, railH, railR);
            NVGpaint railPaint = nvgLinearGradient(vg, 0, railY, 0, railY + railH,
                nvgRGBA(170, 175, 185, 180),
                nvgRGBA(85, 88, 98, 180));
            nvgFillPaint(vg, railPaint);
            nvgFill(vg);

            // Top specular highlight (cylindrical reflection)
            nvgBeginPath(vg);
            nvgRoundedRect(vg, railLeft + 1.5f, railY + 0.4f,
                railW - 3.0f, railH * 0.30f, railR * 0.5f);
            nvgFillColor(vg, nvgRGBA(220, 225, 235, 55));
            nvgFill(vg);

            // Bottom edge shadow
            nvgBeginPath(vg);
            nvgMoveTo(vg, railLeft + railR, railY + railH - 0.3f);
            nvgLineTo(vg, railLeft + railW - railR, railY + railH - 0.3f);
            nvgStrokeColor(vg, nvgRGBA(30, 32, 38, 90));
            nvgStrokeWidth(vg, 0.4f);
            nvgStroke(vg);

            // Subtle top edge line
            nvgBeginPath(vg);
            nvgMoveTo(vg, railLeft + railR, railY + 0.3f);
            nvgLineTo(vg, railLeft + railW - railR, railY + 0.3f);
            nvgStrokeColor(vg, nvgRGBA(200, 205, 215, 40));
            nvgStrokeWidth(vg, 0.3f);
            nvgStroke(vg);
        }

        // Parameter values for handle positioning
        int steps = stepCount();
        float value = pq ? pq->getValue() : 0.f;
        float minV = pq ? pq->minValue : 0.f;
        float maxV = pq ? pq->maxValue : (float)(steps - 1);

        // === SLIDER HANDLE (3D extruded knurled tab) ===
        float t = (maxV > minV) ? (value - minV) / (maxV - minV) : 0.f;
        float handleCX = math::rescale(t, 0.f, 1.f, left, right);

        float handleW = h * 0.62f;
        float handleH = h * 0.92f;
        float handleX = handleCX - handleW * 0.5f;
        float handleY = cy - handleH * 0.5f;
        float handleR = 2.0f;

        // Soft drop shadow (handle floats above housing)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX - 0.5f, handleY + 2.5f, handleW + 1.0f, handleH, handleR + 0.5f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX + 0.5f, handleY + 1.8f, handleW, handleH, handleR);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 100));
        nvgFill(vg);

        // Handle base layer (darker underside showing handle thickness)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX - 0.3f, handleY + 0.8f, handleW + 0.6f, handleH, handleR + 0.3f);
        nvgFillColor(vg, nvgRGBA(60, 58, 52, 255));
        nvgFill(vg);

        // Handle body - brushed stainless steel top face
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX, handleY, handleW, handleH - 1.0f, handleR);
        NVGpaint handlePaint = nvgLinearGradient(vg, handleX, handleY, handleX, handleY + handleH,
            nvgRGBA(205, 202, 194, 255),
            nvgRGBA(125, 122, 115, 255));
        nvgFillPaint(vg, handlePaint);
        nvgFill(vg);

        // Left edge highlight (directional light from upper-left)
        nvgBeginPath(vg);
        nvgMoveTo(vg, handleX + 0.5f, handleY + handleR);
        nvgLineTo(vg, handleX + 0.5f, handleY + handleH - handleR - 1.0f);
        nvgStrokeColor(vg, nvgRGBA(225, 222, 215, 70));
        nvgStrokeWidth(vg, 0.6f);
        nvgStroke(vg);

        // Right edge shadow (opposite side from light)
        nvgBeginPath(vg);
        nvgMoveTo(vg, handleX + handleW - 0.5f, handleY + handleR);
        nvgLineTo(vg, handleX + handleW - 0.5f, handleY + handleH - handleR - 1.0f);
        nvgStrokeColor(vg, nvgRGBA(30, 28, 24, 80));
        nvgStrokeWidth(vg, 0.6f);
        nvgStroke(vg);

        // Top face specular highlight (bright spot on raised surface)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX + 1.5f, handleY + 0.8f,
            handleW - 3.0f, handleH * 0.18f, handleR * 0.5f);
        nvgFillColor(vg, nvgRGBA(240, 238, 232, 65));
        nvgFill(vg);

        // Secondary specular (wider, dimmer reflection below)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX + 2.0f, handleY + handleH * 0.22f,
            handleW - 4.0f, handleH * 0.12f, 1.0f);
        nvgFillColor(vg, nvgRGBA(220, 218, 212, 25));
        nvgFill(vg);

        // Handle edge outline (crisp machined edge)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, handleX, handleY, handleW, handleH - 1.0f, handleR);
        nvgStrokeColor(vg, nvgRGBA(42, 40, 36, 210));
        nvgStrokeWidth(vg, 0.9f);
        nvgStroke(vg);

        // Knurling/grip lines (machined horizontal grooves)
        int gripLines = 5;
        float gripZoneTop = cy - (handleH - 1.0f) * 0.28f;
        float gripZoneH = (handleH - 1.0f) * 0.56f;
        float gripSpacing = gripZoneH / (float)(gripLines - 1);
        float gripInset = 2.5f;
        for (int g = 0; g < gripLines; g++) {
            float gy = gripZoneTop + g * gripSpacing;
            // Dark groove (cut into metal)
            nvgBeginPath(vg);
            nvgMoveTo(vg, handleX + gripInset, gy);
            nvgLineTo(vg, handleX + handleW - gripInset, gy);
            nvgStrokeColor(vg, nvgRGBA(40, 38, 32, 155));
            nvgStrokeWidth(vg, 0.6f);
            nvgStroke(vg);
            // Light ridge below (sharp edge catches light)
            nvgBeginPath(vg);
            nvgMoveTo(vg, handleX + gripInset, gy + 0.6f);
            nvgLineTo(vg, handleX + handleW - gripInset, gy + 0.6f);
            nvgStrokeColor(vg, nvgRGBA(215, 212, 205, 50));
            nvgStrokeWidth(vg, 0.35f);
            nvgStroke(vg);
        }

        nvgRestore(vg);
    }
};

// VUMeterWidget moved to shapetakerWidgets.hpp (namespace shapetaker)

// Legacy JewelLED variants removed in favor of shapetakerWidgets.hpp LEDs
/* struct JewelLED : ModuleLightWidget {
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
}; */

struct JewelLEDSmall : ModuleLightWidget {
    widget::SvgWidget* sw = nullptr;

    JewelLEDSmall() {
        // Set a smaller size to reduce glow radius
        box.size = Vec(10, 10);

        // Try to load the jewel SVG, fallback to simple shape if it fails
        sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_small.svg"));

        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            // Center the SVG within the smaller box (SVG is ~15px, box is 10px)
            sw->box.pos = Vec(-2.5, -2.5);
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
            // Fallback drawing if SVG fails to load (centered on 5,5 instead of 7.5,7.5)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 5, 5, 4.8);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 5, 5, 3.2);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }

        ModuleLightWidget::draw(args);
    }
};

struct JewelLEDCompact : ModuleLightWidget {
    widget::SvgWidget* sw = nullptr;

    JewelLEDCompact() {
        // Intermediate size between Small (10px) and Medium (30px)
        box.size = Vec(18.f, 18.f);

        // Use the medium SVG, scaled to fit
        sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));

        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }

        // Set up proper RGB color mixing
        addBaseColor(nvgRGB(0xff, 0x00, 0x00));
        addBaseColor(nvgRGB(0x00, 0xff, 0x00));
        addBaseColor(nvgRGB(0x00, 0x00, 0xff));
    }

    void step() override {
        ModuleLightWidget::step();

        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness();
            float g = module->lights[firstLightId + 1].getBrightness();
            float b = module->lights[firstLightId + 2].getBrightness();

            color = nvgRGBAf(r, g, b, fmaxf(fmaxf(r, g), b));
        }
    }

    void draw(const DrawArgs& args) override {
        constexpr float svgSize = 20.f;
        float s = std::min(box.size.x, box.size.y) / svgSize;
        float tx = (box.size.x - svgSize * s) * 0.5f;
        float ty = (box.size.y - svgSize * s) * 0.5f;

        nvgSave(args.vg);
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);

        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.6);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 8.0);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }

        ModuleLightWidget::draw(args);
        nvgRestore(args.vg);
    }
};

struct JewelLEDMedium : ModuleLightWidget {
    widget::SvgWidget* sw = nullptr;

    JewelLEDMedium() {
        // Set a fixed size (target footprint 30x30 px)
        box.size = Vec(30.f, 30.f);

        // Try to load the jewel SVG, fallback to simple shape if it fails
        sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));

        if (svg) {
            // SVG loaded successfully
            sw->setSvg(svg);
            addChild(sw);
        }

        // Set up proper RGB color mixing like RedGreenBlueLight
        addBaseColor(nvgRGB(0xff, 0x00, 0x00));
        addBaseColor(nvgRGB(0x00, 0xff, 0x00));
        addBaseColor(nvgRGB(0x00, 0x00, 0xff));
    }

    void step() override {
        ModuleLightWidget::step();

        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness();
            float g = module->lights[firstLightId + 1].getBrightness();
            float b = module->lights[firstLightId + 2].getBrightness();

            color = nvgRGBAf(r, g, b, fmaxf(fmaxf(r, g), b));
        }
    }

    void draw(const DrawArgs& args) override {
        constexpr float svgSize = 20.f;
        float s = std::min(box.size.x, box.size.y) / svgSize;
        float tx = (box.size.x - svgSize * s) * 0.5f;
        float ty = (box.size.y - svgSize * s) * 0.5f;

        nvgSave(args.vg);
        nvgTranslate(args.vg, tx, ty);
        nvgScale(args.vg, s, s);

        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.6);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 8.0);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }

        ModuleLightWidget::draw(args);
        nvgRestore(args.vg);
    }
};

// TealJewelLEDMedium and PurpleJewelLEDMedium are now defined in shapetakerWidgets.hpp

struct JewelLEDLarge : ModuleLightWidget {
    JewelLEDLarge() {
        // Set a fixed size (25% larger than 20x20 = 25x25)
        box.size = Vec(25, 25);

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

            color = nvgRGBAf(r, g, b, fmaxf(fmaxf(r, g), b));
        }
    }

    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 12.5, 12.5, 12.0);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 12.5, 12.5, 8.0);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }

        ModuleLightWidget::draw(args);
    }
};

/* struct JewelLEDXLarge : ModuleLightWidget {
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
}; */

/* struct JewelLEDHuge : ModuleLightWidget {
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
}; */

// An interface for modules that can provide data to the oscilloscope
struct IOscilloscopeSource {
    virtual ~IOscilloscopeSource() {}
    virtual const Vec* getOscilloscopeBuffer() const = 0;
    virtual int getOscilloscopeBufferIndex() const = 0;
    virtual int getOscilloscopeBufferSize() const = 0;
    // 0 = green, 1 = blue, 2 = yellow, 3 = amber
    virtual int getOscilloscopeTheme() const { return 0; }
};

struct VintageOscilloscopeWidget : widget::Widget {
    IOscilloscopeSource* source;

    VintageOscilloscopeWidget(IOscilloscopeSource* source) : source(source) {}
    
    void drawLayer(const DrawArgs& args, int layer) override {
        // Layer 0: panel seating shadow (subtle drop beneath the circular screen)
        if (layer == 0) {
            NVGcontext* vg = args.vg;
            float w = box.size.x;
            float h = box.size.y;
            float cx = w * 0.5f;
            float cy = h * 0.5f + h * 0.10f; // slight downward bias like VCV knob shadows
            float r  = std::min(w, h) * 0.48f; // hug the bezel edge
            NVGpaint p = nvgRadialGradient(vg, cx, cy, r * 0.90f, r,
                                           nvgRGBA(0, 0, 0, 36), nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, w, h);
            nvgFillPaint(vg, p);
            nvgFill(vg);
        }
        if (layer == 1) {
            NVGcontext* vg = args.vg;

            // Use centralized DisplayTheme system for consistent colors
            using DisplayTheme = shapetaker::ui::ThemeManager::DisplayTheme;
            int themeIndex = 0;
            if (source) {
                themeIndex = clamp(source->getOscilloscopeTheme(), 0, DisplayTheme::THEME_COUNT - 1);
            }
            DisplayTheme::Theme theme = static_cast<DisplayTheme::Theme>(themeIndex);

            // Get theme colors from centralized system
            NVGcolor glowInner = DisplayTheme::getGlowInnerColor(theme);
            NVGcolor glowOuter = DisplayTheme::getGlowOuterColor(theme);
            NVGcolor phosphorInner = DisplayTheme::getPhosphorInnerColor(theme);
            NVGcolor phosphorOuter = DisplayTheme::getPhosphorOuterColor(theme);
            float traceDimR, traceDimG, traceDimB;
            float traceBrightR, traceBrightG, traceBrightB;
            DisplayTheme::getTraceDimRGB(theme, traceDimR, traceDimG, traceDimB);
            DisplayTheme::getTraceBrightRGB(theme, traceBrightR, traceBrightG, traceBrightB);
            const char* screenSvg = DisplayTheme::getOscilloscopeScreenSVG(theme);
            
            // --- CRT Glow Effect ---
            // Draw a soft glow behind the screen to simulate CRT phosphorescence
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x / 2.f);
            // A radial gradient from themed glow color to transparent
            NVGpaint glowPaint = nvgRadialGradient(
                vg,
                box.size.x / 2.f,
                box.size.y / 2.f,
                box.size.x * 0.1f,
                box.size.x * 0.5f,
                glowInner,
                glowOuter);
            nvgFillPaint(vg, glowPaint);
            nvgFill(vg);
            // --- End Glow Effect ---

            // Draw background SVG
            std::shared_ptr<Svg> bg_svg = Svg::load(asset::plugin(pluginInstance, screenSvg));
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
            
            // Layer 1: Main spherical highlight (subdued for vintage look)
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x * 0.85f);
            NVGpaint mainHighlight = nvgRadialGradient(vg,
                box.size.x * 0.35f, box.size.y * 0.35f, // Offset highlight to top-left
                box.size.x * 0.05f, box.size.x * 0.6f,
                nvgRGBA(255, 255, 255, 18), // Subtle white highlight for vintage look
                nvgRGBA(255, 255, 255, 0));  // Fades to nothing
            nvgFillPaint(vg, mainHighlight);
            nvgFill(vg);

            // Layer 2: Subtle center hotspot for glass dome effect
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x * 0.38f, box.size.y * 0.38f, box.size.x * 0.15f);
            NVGpaint centerHighlight = nvgRadialGradient(vg,
                box.size.x * 0.38f, box.size.y * 0.38f,
                0, box.size.x * 0.15f,
                nvgRGBA(255, 255, 255, 30), // Muted center for aged glass
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
            
            // Layer 4: Themed phosphor glow enhancement
            nvgBeginPath(vg);
            nvgCircle(vg, box.size.x / 2.f, box.size.y / 2.f, box.size.x * 0.45f);
            NVGpaint phosphorGlow = nvgRadialGradient(
                vg,
                box.size.x / 2.f,
                box.size.y / 2.f,
                box.size.x * 0.1f,
                box.size.x * 0.45f,
                phosphorInner,
                phosphorOuter);
            nvgFillPaint(vg, phosphorGlow);
            nvgFill(vg);

            if (!source) return;

            nvgSave(vg);
            nvgScissor(vg, 0, 0, box.size.x, box.size.y);

            // Add margin to keep signal away from bezel edge (10% margin on each side)
            float margin = box.size.x * 0.10f;
            float screenLeft = margin;
            float screenRight = box.size.x - margin;
            float screenTop = margin;
            float screenBottom = box.size.y - margin;
            float screenWidth = screenRight - screenLeft;
            float screenHeight = screenBottom - screenTop;
            Vec screenCenter = Vec((screenLeft + screenRight) * 0.5f, (screenTop + screenBottom) * 0.5f);
            float screenRadius = std::min(screenWidth, screenHeight) * 0.5f;

            // Maximum voltage before hard clipping (prevents signal from reaching bezel)
            const float MAX_DISPLAY_VOLTAGE = 6.0f;

            // Function to map voltage to screen coordinates
            auto voltageToScreen = [&](Vec voltage) {
                // Hard clamp voltage to prevent exceeding screen bounds
                float clampedX = clamp(voltage.x, -MAX_DISPLAY_VOLTAGE, MAX_DISPLAY_VOLTAGE);
                float clampedY = clamp(voltage.y, -MAX_DISPLAY_VOLTAGE, MAX_DISPLAY_VOLTAGE);

                // Normalize voltage to -1..+1 range
                float x_norm = clampedX / MAX_DISPLAY_VOLTAGE;
                float y_norm = clampedY / MAX_DISPLAY_VOLTAGE;

                // Map normalized coordinates to a circular screen area
                float normX = x_norm;
                float normY = y_norm;
                float r = std::sqrt(normX * normX + normY * normY);
                if (r > 1.f) {
                    normX /= r;
                    normY /= r;
                    r = 1.f;
                }

                // Slight barrel distortion to imply a spherical CRT surface
                const float warp = 0.12f;
                float rWarped = std::min(1.f, r * (1.f + warp * r * r));
                if (r > 0.f) {
                    float scale = rWarped / r;
                    normX *= scale;
                    normY *= scale;
                }

                float screenX = screenCenter.x + normX * screenRadius;
                float screenY = screenCenter.y - normY * screenRadius;

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

            // Set line style for smooth corners
            nvgLineJoin(vg, NVG_ROUND);
            nvgLineCap(vg, NVG_ROUND);

            // --- X-Y Lissajous Display ---
            const int numChunks = 24;
            const int chunkSize = bufferSize / numChunks;
            const float DISCONTINUITY_THRESHOLD = 5.0f;

            for (int c = 0; c < numChunks; c++) {
                float age = (float)c / (numChunks - 1);
                float alpha = powf(1.0f - age, 1.8f);
                alpha = clamp(alpha, 0.f, 1.f);

                    if (alpha < 0.01f) continue;

                    nvgBeginPath(vg);
                    bool firstPointInChunk = true;
                    Vec lastVoltage = Vec(0, 0);

                    for (int i = 0; i < chunkSize; i++) {
                        int point_offset = c * chunkSize + i;
                        if (point_offset >= bufferSize - 1) continue;

                        int buffer_idx = (currentIndex - 1 - point_offset + bufferSize) % bufferSize;

                        if (buffer_idx == currentIndex) {
                            firstPointInChunk = true;
                            continue;
                        }

                        Vec currentVoltage = buffer[buffer_idx];
                        Vec p = voltageToScreen(currentVoltage);

                        if (!firstPointInChunk) {
                            float deltaX = std::abs(currentVoltage.x - lastVoltage.x);
                            float deltaY = std::abs(currentVoltage.y - lastVoltage.y);
                            if (deltaX > DISCONTINUITY_THRESHOLD || deltaY > DISCONTINUITY_THRESHOLD) {
                                firstPointInChunk = true;
                            }
                        }

                        if (firstPointInChunk) {
                            nvgMoveTo(vg, p.x, p.y);
                            firstPointInChunk = false;
                        } else {
                            nvgLineTo(vg, p.x, p.y);
                        }

                        lastVoltage = currentVoltage;
                    }

                    nvgStrokeColor(vg, nvgRGBAf(traceDimR, traceDimG, traceDimB, alpha * 0.30f));
                    nvgStrokeWidth(vg, 1.2f + alpha * 1.2f);
                    nvgStroke(vg);

                    nvgStrokeColor(vg, nvgRGBAf(traceBrightR, traceBrightG, traceBrightB, alpha * 0.65f));
                    nvgStrokeWidth(vg, 0.6f + alpha * 0.3f);
                    nvgStroke(vg);
                }

            nvgRestore(vg);
        }
        Widget::drawLayer(args, layer);
    }
};

// Capacitive touch switch (brass touch pad like touch strip)
struct CapacitiveTouchSwitch : app::SvgSwitch {
    CapacitiveTouchSwitch() {
        momentary = false;
        latch = true;
        // No frames needed - visual state shown by LED
        box.size = Vec(27.2f, 27.2f);
        if (shadow) shadow->visible = false;
    }

    void draw(const DrawArgs& args) override {
        float radius = box.size.x / 2.0f;
        float cx = box.size.x / 2.0f;
        float cy = box.size.y / 2.0f;

        // Base brass gradient
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius);
        NVGpaint base = nvgLinearGradient(args.vg, cx, cy - radius, cx, cy + radius,
            nvgRGBA(148, 122, 82, 255), // Lighter top color
            nvgRGBA(76, 60, 46, 255));  // Lighter bottom color
        nvgFillPaint(args.vg, base);
        nvgFill(args.vg);

        // Subtle center glow
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * 0.95f);
        NVGpaint centerGlow = nvgRadialGradient(args.vg,
            cx, cy, radius * 0.1f, radius * 0.95f,
            nvgRGBA(240, 210, 130, 110), // Brighter glow
            nvgRGBA(100, 70, 38, 0));
        nvgFillPaint(args.vg, centerGlow);
        nvgFill(args.vg);

        // Edge sheen
        NVGpaint edgeSheen = nvgRadialGradient(args.vg,
            cx, cy, radius * 0.8f, radius,
            nvgRGBA(255, 235, 150, 48), // Brighter sheen
            nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius);
        nvgFillPaint(args.vg, edgeSheen);
        nvgFill(args.vg);

        // Border
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius - 0.5f);
        nvgStrokeColor(args.vg, nvgRGBA(88, 62, 36, 160));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
    }

    void onChange(const ChangeEvent& e) override {
        SvgSwitch::onChange(e);
    }
};

// Vintage slider widget using the new slider SVGs
struct VintageSlider : app::SvgSlider {
    VintageSlider() {
        // Set the background (track) SVG - 8x60px (small compact version)
        setBackgroundSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_track_small.svg"))
        );

        // Set the handle SVG - 12x18px (small compact version)
        setHandleSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_handle_small.svg"))
        );

        // SVG dimensions: track is 8x60px, handle is 12x18px
        // Widget box size matches track width and height
        box.size = Vec(12.f, 60.f);

        // Configure the slider travel range
        // Handle travels vertically within the track
        // The handle is 18px tall, track is 60px tall
        // So handle can travel (60 - 18) = 42px
        // Position from top (0) to bottom (42)
        maxHandlePos = Vec(-2.f, 0.f);      // Top position (param minimum = 0), offset left 2px to center
        minHandlePos = Vec(-2.f, 42.f);     // Bottom position (param maximum = 1)
    }
};

// Horizontal variant of the vintage slider
struct VintageSliderHorizontal : app::SvgSlider {
    VintageSliderHorizontal() {
        // Set horizontal track
        setBackgroundSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_track_horizontal.svg"))
        );

        // Set horizontal handle
        setHandleSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_handle_horizontal.svg"))
        );

        // Configure horizontal travel
        maxHandlePos = mm2px(Vec(-20, 0));  // Left position (minimum)
        minHandlePos = mm2px(Vec(20, 0));   // Right position (maximum)

        // Set widget box for horizontal orientation
        box.size = mm2px(Vec(45, 14));
    }
};
