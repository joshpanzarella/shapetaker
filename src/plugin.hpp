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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.90f; // Large: face diameter ~56.5 of 63 viewBox
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.78f; // Medium: face diameter ~40.4 of 52 viewBox
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.78f; // Medium variant
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.90f;
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.80f; // Small: face diameter 24 of 30 viewBox
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.92f; // XLarge: face diameter ~44 of 48 viewBox
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
        // Configure Fundamental-style shadow, tightened to the knob face
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.93f; // Huge: face diameter ~54 of 58 viewBox
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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

        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.80f;
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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

        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            Vec s = sw->box.size;
            float f = 0.82f;
            Vec shr = Vec(s.x * f, s.y * f);
            shadow->box.size = shr;
            shadow->box.pos = Vec((s.x - shr.x) * 0.5f,
                                   (s.y - shr.y) * 0.5f + s.y * 0.10f);
        }
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
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobVintageSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/shapetaker_knob_ROTATE_vintage_S.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/shapetaker_knob_BASE_vintage_S.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(20.f, 20.f));  // Much bigger! (was 16mm)

        // Disable shadow since indicator is on outer edge
        if (shadow) {
            shadow->visible = false;
        }
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
    Vec nativeSize = Vec(100.f, 100.f);

    ShapetakerKnobVintageMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;

        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/shapetaker_knob_ROTATE_vintage_M.svg")));

        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/shapetaker_knob_BASE_vintage_M.svg")));
        nativeSize = bg->box.size;
        if (fb && tw) fb->addChildBelow(bg, tw);
        box.size = mm2px(Vec(22.f, 22.f));  // Much bigger! (was 18mm)

        // Disable shadow since indicator is on outer edge
        if (shadow) {
            shadow->visible = false;
        }
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

        // Disable shadow for clean dark aesthetic
        if (shadow) {
            shadow->visible = false;
        }
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

        // Disable shadow for clean dark aesthetic
        if (shadow) {
            shadow->visible = false;
        }
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

        // Disable shadow for clean dark aesthetic
        if (shadow) {
            shadow->visible = false;
        }
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

        // Shadow disabled
        if (shadow) {
            shadow->visible = false;
        }
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

        // Shadow disabled
        if (shadow) {
            shadow->visible = false;
        }
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

        // Shadow disabled
        if (shadow) {
            shadow->visible = false;
        }
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

        // Shadow disabled
        if (shadow) {
            shadow->visible = false;
        }
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

        // Disable shadow for clean dark aesthetic
        if (shadow) {
            shadow->visible = false;
        }
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

        // Lever stem - realistic aluminum/steel finish
        nvgBeginPath(vg);
        nvgRoundedRect(vg, -stemWidth * 0.5f, -stemLength, stemWidth, stemLength, stemWidth * 0.4f);
        NVGpaint leverPaint = nvgLinearGradient(vg, -stemWidth * 0.5f, -stemLength, stemWidth * 0.5f, -stemLength,
            nvgRGBA(185, 185, 182, 255),
            nvgRGBA(145, 145, 142, 255));
        nvgFillPaint(vg, leverPaint);
        nvgFill(vg);

        // Lever edge highlights
        nvgStrokeColor(vg, nvgRGBA(210, 210, 208, 180));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);

        // Lever tip - black plastic/rubber material
        float tipR = stemWidth * 0.92f;
        // Tip shadow
        nvgBeginPath(vg);
        nvgCircle(vg, 0.2f, -stemLength + 0.2f, tipR);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
        nvgFill(vg);

        // Tip body - black with subtle gradient
        nvgBeginPath(vg);
        nvgCircle(vg, 0.f, -stemLength, tipR);
        NVGpaint tipPaint = nvgRadialGradient(vg,
            -tipR * 0.35f, -stemLength - tipR * 0.35f,
            tipR * 0.15f, tipR * 1.15f,
            nvgRGBA(45, 45, 45, 255),
            nvgRGBA(18, 18, 18, 255));
        nvgFillPaint(vg, tipPaint);
        nvgFill(vg);

        // Tip rim - dark edge
        nvgStrokeColor(vg, nvgRGBA(8, 8, 8, 220));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);

        // Tip highlight - subtle shine on black plastic
        nvgBeginPath(vg);
        nvgCircle(vg, -tipR * 0.3f, -stemLength - tipR * 0.3f, tipR * 0.35f);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 35));
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
    Vec nativeSize = Vec(100.f, 100.f);
    ShapetakerAttenuverterOscilloscope() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // The entire hexagonal knob (body + tick marks + indicator) rotates as one piece
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_attenuverter_small.svg")));
        // Measure intrinsic size via probe
        widget::SvgWidget probe;
        probe.setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_attenuverter_small.svg")));
        nativeSize = probe.box.size;
        // Force nativeSize to be square to prevent distortion when rotating
        float maxDim = std::max(nativeSize.x, nativeSize.y);
        nativeSize = Vec(maxDim, maxDim);
        // Target: Attenuverter = 10 mm (closer to compact 4ms-style attenuverters)
        box.size = mm2px(Vec(10.f, 10.f));
        // Configure Fundamental-style shadow (no shrink; face matches SVG)
        if (shadow && sw) {
            shadow->visible = true;
            shadow->blurRadius = 0.f;
            shadow->opacity = 0.15f;
            // Force shadow to be square to match the square box.size
            float size = std::max(box.size.x, box.size.y);
            shadow->box.size = Vec(size, size);
            shadow->box.pos = Vec(0.f, size * 0.10f);
        }
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
        nvgScale(args.vg, s, s);
        app::SvgSwitch::draw(args);
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

        // Force large size - no SvgKnob to interfere
        box.size = mm2px(Vec(22.0f, 22.0f));
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

/* struct JewelLEDSmall : ModuleLightWidget {
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
}; */

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
};

struct VintageOscilloscopeWidget : widget::Widget {
    IOscilloscopeSource* source;
    
    VintageOscilloscopeWidget(IOscilloscopeSource* source) : source(source) {}
    
    void step() override {
        Widget::step();
        // Buffering is now handled by the source module in the audio thread.
    }
    
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

            // Add margin to keep signal away from bezel edge (10% margin on each side)
            float margin = box.size.x * 0.10f;
            float screenLeft = margin;
            float screenRight = box.size.x - margin;
            float screenTop = margin;
            float screenBottom = box.size.y - margin;
            float screenWidth = screenRight - screenLeft;
            float screenHeight = screenBottom - screenTop;

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

                // Map normalized coordinates to screen space with margin
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

            // Threshold for detecting discontinuities (prevents swastika-like patterns in PWM)
            const float DISCONTINUITY_THRESHOLD = 3.5f; // volts

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
                Vec lastVoltage = Vec(0, 0);

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

                    Vec currentVoltage = buffer[buffer_idx];
                    Vec p = voltageToScreen(currentVoltage);

                    // Detect large voltage jumps (PWM edges, glitches, etc.)
                    // Break the line to avoid connecting discontinuous points
                    if (!firstPointInChunk) {
                        float deltaX = std::abs(currentVoltage.x - lastVoltage.x);
                        float deltaY = std::abs(currentVoltage.y - lastVoltage.y);

                        // If either channel has a large jump, start a new line segment
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
        Widget::drawLayer(args, layer);
    }
};

// Capacitive touch switch (brass touch pad like touch strip)
struct CapacitiveTouchSwitch : app::SvgSwitch {
    CapacitiveTouchSwitch() {
        momentary = false;
        latch = true;
        // No frames needed - visual state shown by LED
        box.size = Vec(27.2, 27.2);
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
