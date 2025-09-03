#pragma once

#include <rack.hpp>
#include <nanovg.h>
#include "../graphics/lighting.hpp"

using namespace rack;

namespace shapetaker {
namespace ui {

/**
 * Visual theme management system providing consistent colors,
 * styling, and visual effects across all Shapetaker modules
 */
class ThemeManager {
public:
    // ============================================================================
    // BRAND COLORS
    // ============================================================================
    
    /**
     * Primary Shapetaker brand colors
     */
    struct BrandColors {
        // Signature dual-channel colors
        static constexpr NVGcolor TEAL = {{{0.0f, 1.0f, 0.706f, 1.0f}}}; // #00FFB4
        static constexpr NVGcolor PURPLE = {{{0.706f, 0.0f, 1.0f, 1.0f}}}; // #B400FF
        
        // Mixed state color (both channels active)
        static constexpr NVGcolor CYAN_MAGENTA = {{{0.5f, 1.0f, 1.0f, 1.0f}}}; // Cyan-magenta mix
        
        // Supporting colors
        static constexpr NVGcolor GOLD = {{{1.0f, 0.843f, 0.0f, 1.0f}}}; // #FFD700
        static constexpr NVGcolor SILVER = {{{0.753f, 0.753f, 0.753f, 1.0f}}}; // #C0C0C0
        
        // Convenience functions for RGB values
        static graphics::RGBColor tealRGB() { return graphics::RGBColor(0.0f, 1.0f, 0.706f); }
        static graphics::RGBColor purpleRGB() { return graphics::RGBColor(0.706f, 0.0f, 1.0f); }
    };
    
    // ============================================================================
    // LIGHT COLORS AND STATES
    // ============================================================================
    
    /**
     * LED and light color management
     */
    class LightTheme {
    public:
        /**
         * Chiaroscuro-style color progression: Teal → Bright blue-purple → Dark purple
         */
        static graphics::RGBColor getChiaroscuroColor(float value) {
            value = clamp(value, 0.0f, 1.0f);
            
            if (value <= 0.5f) {
                // 0 to 0.5: Teal to bright blue-purple
                float t = value * 2.0f;
                return graphics::RGBColor(t, 1.0f, 1.0f); // Red increases, Green=max, Blue=max
            } else {
                // 0.5 to 1.0: Bright blue-purple to dark purple
                float t = (value - 0.5f) * 2.0f;
                return graphics::RGBColor(1.0f, 1.0f - t, 1.0f); // Red=max, Green decreases, Blue=max
            }
        }
        
        /**
         * VU meter color progression for audio level indication
         */
        static graphics::RGBColor getVUColor(float level) {
            level = clamp(level, 0.0f, 1.0f);
            
            if (level < 0.6f) {
                // Green zone: 0-60%
                return graphics::RGBColor(0.0f, 1.0f, 0.0f);
            } else if (level < 0.85f) {
                // Yellow zone: 60-85%
                float t = (level - 0.6f) / 0.25f;
                return graphics::RGBColor(t, 1.0f, 0.0f); // Green to yellow
            } else {
                // Red zone: 85-100%
                float t = (level - 0.85f) / 0.15f;
                return graphics::RGBColor(1.0f, 1.0f - t, 0.0f); // Yellow to red
            }
        }
        
        /**
         * Matrix LED states for sequencer displays
         */
        enum MatrixState {
            EMPTY,      // Dark/off
            SEQUENCE_A, // Teal
            SEQUENCE_B, // Purple
            BOTH,       // Mixed cyan-magenta
            PLAYHEAD_A, // Bright teal
            PLAYHEAD_B, // Bright purple
            PLAYHEAD_BOTH, // Bright mixed
            EDIT_MODE   // Animated glow
        };
        
        static graphics::RGBColor getMatrixColor(MatrixState state, float brightness = 1.0f) {
            switch (state) {
                case EMPTY: return graphics::RGBColor(0.1f, 0.1f, 0.15f) * brightness;
                case SEQUENCE_A: return BrandColors::tealRGB() * brightness;
                case SEQUENCE_B: return BrandColors::purpleRGB() * brightness;
                case BOTH: return graphics::RGBColor(0.5f, 1.0f, 1.0f) * brightness;
                case PLAYHEAD_A: return BrandColors::tealRGB() * (brightness * 1.5f);
                case PLAYHEAD_B: return BrandColors::purpleRGB() * (brightness * 1.5f);
                case PLAYHEAD_BOTH: return graphics::RGBColor(0.8f, 1.0f, 1.0f) * (brightness * 1.5f);
                case EDIT_MODE: {
                    // Animated pulse based on time
                    float time = glfwGetTime();
                    float pulse = 0.5f + 0.5f * sinf(time * 4.0f);
                    return graphics::RGBColor(1.0f, 1.0f, 1.0f) * (brightness * pulse);
                }
                default: return graphics::RGBColor(0.5f, 0.5f, 0.5f) * brightness;
            }
        }
        
        /**
         * Set RGB light using Shapetaker color values
         */
        static void setRGBLight(Module* module, int lightId, const graphics::RGBColor& color) {
            if (!module || lightId < 0) return;
            
            if (lightId + 2 < (int)module->lights.size()) {
                module->lights[lightId + 0].setBrightness(color.r);
                module->lights[lightId + 1].setBrightness(color.g);
                module->lights[lightId + 2].setBrightness(color.b);
            }
        }
    };
    
    // ============================================================================
    // PANEL AND UI COLORS
    // ============================================================================
    
    /**
     * Panel background and UI element colors
     */
    struct PanelColors {
        // Background colors
        static constexpr NVGcolor BACKGROUND_DARK = {{{0.08f, 0.1f, 0.12f, 1.0f}}}; // Dark blue-grey
        static constexpr NVGcolor BACKGROUND_MEDIUM = {{{0.15f, 0.17f, 0.2f, 1.0f}}}; // Medium grey
        static constexpr NVGcolor BACKGROUND_LIGHT = {{{0.25f, 0.27f, 0.3f, 1.0f}}}; // Light grey
        
        // Text colors
        static constexpr NVGcolor TEXT_PRIMARY = {{{0.94f, 0.94f, 0.94f, 1.0f}}}; // White
        static constexpr NVGcolor TEXT_SECONDARY = {{{0.7f, 0.7f, 0.75f, 1.0f}}}; // Light grey
        static constexpr NVGcolor TEXT_ACCENT = {{{1.0f, 1.0f, 1.0f, 1.0f}}}; // Pure white
        
        // Border and outline colors
        static constexpr NVGcolor BORDER_SUBTLE = {{{0.4f, 0.4f, 0.45f, 1.0f}}}; // Subtle border
        static constexpr NVGcolor BORDER_ACCENT = {{{0.6f, 0.6f, 0.7f, 1.0f}}}; // Accent border
        
        // Control colors
        static constexpr NVGcolor KNOB_DARK = {{{0.2f, 0.2f, 0.25f, 1.0f}}}; // Dark knob
        static constexpr NVGcolor KNOB_LIGHT = {{{0.75f, 0.75f, 0.75f, 1.0f}}}; // Light knob
    };
    
    // ============================================================================
    // SCREEN AND DISPLAY EFFECTS  
    // ============================================================================
    
    /**
     * CRT and vintage screen effects
     */
    class ScreenEffects {
    public:
        /**
         * Draw CRT-style background with grid lines
         */
        static void drawCRTBackground(NVGcontext* vg, Vec size, NVGcolor backgroundColor = nvgRGB(5, 10, 5)) {
            // Dark background
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, size.x, size.y);
            nvgFillColor(vg, backgroundColor);
            nvgFill(vg);
            
            // Grid lines
            nvgStrokeColor(vg, nvgRGBA(0, 80, 0, 40));
            nvgStrokeWidth(vg, 0.5f);
            
            // Vertical grid
            for (float x = 0; x < size.x; x += size.x / 8) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, x, 0);
                nvgLineTo(vg, x, size.y);
                nvgStroke(vg);
            }
            
            // Horizontal grid
            for (float y = 0; y < size.y; y += size.y / 6) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, 0, y);
                nvgLineTo(vg, size.x, y);
                nvgStroke(vg);
            }
        }
        
        /**
         * Apply phosphor glow effect to paths
         */
        static void drawPhosphorGlow(NVGcontext* vg, const graphics::RGBColor& color, float glowWidth = 3.0f, float alpha = 0.3f) {
            nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
            nvgStrokeColor(vg, nvgRGBAf(color.r, color.g, color.b, alpha));
            nvgStrokeWidth(vg, glowWidth);
            nvgStroke(vg);
            nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
        }
        
        /**
         * Spooky TV effect for Transmutation module
         */
        static void drawSpookyTVEffect(NVGcontext* vg, Vec pos, Vec size, float time) {
            // Static noise overlay
            nvgSave(vg);
            nvgTranslate(vg, pos.x, pos.y);
            
            // Random static pattern
            for (int i = 0; i < 20; i++) {
                float x = (random::uniform() * size.x);
                float y = (random::uniform() * size.y);
                float w = random::uniform() * 3.0f + 1.0f;
                float h = random::uniform() * 2.0f + 0.5f;
                
                nvgBeginPath(vg);
                nvgRect(vg, x, y, w, h);
                nvgFillColor(vg, nvgRGBAf(1, 1, 1, random::uniform() * 0.3f));
                nvgFill(vg);
            }
            
            // Scanlines
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 20));
            nvgStrokeWidth(vg, 0.5f);
            for (float y = 0; y < size.y; y += 4) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, 0, y);
                nvgLineTo(vg, size.x, y);
                nvgStroke(vg);
            }
            
            // Color distortion
            float distortion = sinf(time * 2.0f) * 2.0f;
            nvgTranslate(vg, distortion, 0);
            
            nvgRestore(vg);
        }
    };
    
    // ============================================================================
    // WIDGET STYLING
    // ============================================================================
    
    /**
     * Standard widget appearance settings
     */
    struct WidgetStyle {
        /**
         * LED jewel styling configuration
         */
        struct JewelLED {
            static constexpr float OUTER_GLOW_RADIUS = 1.0f;
            static constexpr float MEDIUM_RING_RADIUS = 0.7f;
            static constexpr float INNER_CORE_RADIUS = 0.35f;
            static constexpr float HIGHLIGHT_RADIUS = 0.2f;
            static constexpr float RIM_RADIUS = 0.8f;
            
            static constexpr float OUTER_GLOW_ALPHA = 0.6f;
            static constexpr float MEDIUM_RING_ALPHA = 0.9f;
            static constexpr float INNER_CORE_ALPHA = 0.8f;
            static constexpr float HIGHLIGHT_ALPHA = 0.9f;
            static constexpr float RIM_ALPHA = 0.4f;
        };
        
        /**
         * Button and knob styling
         */
        struct Controls {
            static constexpr NVGcolor BUTTON_UP = PanelColors::KNOB_LIGHT;
            static constexpr NVGcolor BUTTON_DOWN = PanelColors::KNOB_DARK;
            static constexpr NVGcolor KNOB_BACKGROUND = PanelColors::KNOB_DARK;
            static constexpr NVGcolor KNOB_INDICATOR = PanelColors::TEXT_PRIMARY;
        };
    };
    
    // ============================================================================
    // CONVENIENCE FUNCTIONS
    // ============================================================================
    
    /**
     * Apply consistent module styling
     */
    static void styleModule(ModuleWidget* widget) {
        if (!widget) return;
        // Future: Apply consistent styling across all modules
    }
    
    /**
     * Get color for dual-channel systems (A/B, L/R, etc.)
     */
    static graphics::RGBColor getChannelColor(int channel, float brightness = 1.0f) {
        switch (channel) {
            case 0: return BrandColors::tealRGB() * brightness; // Channel A/Left
            case 1: return BrandColors::purpleRGB() * brightness; // Channel B/Right
            default: return graphics::RGBColor(0.7f, 0.7f, 0.7f) * brightness; // Default
        }
    }
    
    /**
     * Interpolate between two colors
     */
    static graphics::RGBColor mixColors(const graphics::RGBColor& a, const graphics::RGBColor& b, float t) {
        t = clamp(t, 0.0f, 1.0f);
        return graphics::RGBColor(
            a.r + t * (b.r - a.r),
            a.g + t * (b.g - a.g),
            a.b + t * (b.b - a.b)
        );
    }
    
    /**
     * Convert RGB color to NVG color
     */
    static NVGcolor toNVG(const graphics::RGBColor& color, float alpha = 1.0f) {
        return nvgRGBAf(color.r, color.g, color.b, alpha);
    }
    
    /**
     * Create gradient paint with Shapetaker colors
     */
    static NVGpaint createBrandGradient(NVGcontext* vg, Vec start, Vec end, bool useChannelColors = true) {
        if (useChannelColors) {
            return nvgLinearGradient(vg, start.x, start.y, end.x, end.y,
                                   BrandColors::TEAL, BrandColors::PURPLE);
        } else {
            return nvgLinearGradient(vg, start.x, start.y, end.x, end.y,
                                   PanelColors::BACKGROUND_DARK, PanelColors::BACKGROUND_LIGHT);
        }
    }
};

} // namespace ui
} // namespace shapetaker