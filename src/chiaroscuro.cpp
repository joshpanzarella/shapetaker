#include "plugin.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>
#include <cmath>
#include <unordered_map>
#include <array>
#include <string>
#include <cctype>

// Forward declaration for Chiaroscuro module (needed by widgets)
struct Chiaroscuro;

// Parameter/light indices for accessing values before the full Chiaroscuro definition
static constexpr int CHIAROSC_DIST_PARAM = 2;
static constexpr int CHIAROSC_DRIVE_PARAM = 4;
static constexpr int CHIAROSC_MIX_PARAM = 6;

static constexpr int CHIAROSC_LED_R = 0;
static constexpr int CHIAROSC_LED_G = 1;
static constexpr int CHIAROSC_LED_B = 2;

// Vintage Dot Matrix Display Widget with Sun/Moon Eclipse
struct VintageDotMatrix : Widget {
    Module* module;
    static const int MATRIX_WIDTH = 64;   // Increased resolution for smoother animation
    static const int MATRIX_HEIGHT = 18;  // Increased resolution for smoother animation

    float animationPhase = 0.0f;

    // Distortion type name display
    int lastDistortionType = -1;
    float typeDisplayTimer = 0.0f;
    float lastDrawTime = 0.0f;
    static constexpr float TYPE_DISPLAY_DURATION = 1.5f; // Extended for full words

    VintageDotMatrix(Module* mod) : module(mod) {
        // Set size to match the updated SVG rectangle: 48.362167mm x 14.613997mm
        box.size = mm2px(Vec(48.362167f, 14.613997f));
    }

    void draw(const DrawArgs& args) override {
        if (!module) return;

        animationPhase += 0.02f;

        // Get current distortion type and detect changes
        int currentDistortionType = (int)module->params[1].getValue(); // TYPE_PARAM (2nd parameter, 0-indexed)

        if (currentDistortionType != lastDistortionType) {
            lastDistortionType = currentDistortionType;
            typeDisplayTimer = TYPE_DISPLAY_DURATION; // Start timer
        }

        // Update timer using time delta
        float currentTime = glfwGetTime();
        if (lastDrawTime > 0.0f) {
            float deltaTime = currentTime - lastDrawTime;
            if (typeDisplayTimer > 0.0f) {
                typeDisplayTimer -= deltaTime;
                if (typeDisplayTimer < 0.0f) typeDisplayTimer = 0.0f;
            }
        }
        lastDrawTime = currentTime;
        if (animationPhase > 2.0f * M_PI) animationPhase -= 2.0f * M_PI;

        // Draw hardware bezel and screen background
        drawHardwareFrame(args);

        // Get parameters from Chiaroscuro module (raw knob positions)
        float distortion = clamp(module->params[CHIAROSC_DIST_PARAM].getValue(), 0.0f, 1.0f);
        float drive = clamp(module->params[CHIAROSC_DRIVE_PARAM].getValue(), 0.0f, 1.0f);
        float mix = clamp(module->params[CHIAROSC_MIX_PARAM].getValue(), 0.0f, 1.0f);

        // Approximate processed distortion using LED color (captures CV + smoothing)
        const float maxLedBrightness = 0.6f;
        shapetaker::RGBColor ledColor = shapetaker::LightingHelper::getChiaroscuroColor(distortion, maxLedBrightness);
        if (module) {
            float r = module->lights[CHIAROSC_LED_R].getBrightness();
            float g = module->lights[CHIAROSC_LED_G].getBrightness();
            float b = module->lights[CHIAROSC_LED_B].getBrightness();
            ledColor = {r, g, b};
            distortion = clamp((r / maxLedBrightness) * 0.5f + (1.0f - g / maxLedBrightness) * 0.5f, 0.0f, 1.0f);
        }

        // Check if we should show type name instead of eclipse
        bool showTypeName = (typeDisplayTimer > 0.0f);

        // Calculate eclipse parameters (scaled for high resolution matrix)
        const float widthScale = static_cast<float>(MATRIX_WIDTH) / 48.0f;
        const float heightScale = static_cast<float>(MATRIX_HEIGHT) / 14.0f;

        float sunRadius = (5.5f * widthScale) + drive * (4.5f * widthScale);
        float moonRadius = sunRadius * 0.92f;
        float moonTravel = std::max((MATRIX_WIDTH * 0.5f) - (moonRadius + 2.0f * widthScale), 0.0f);

        // Center positions in matrix coordinates
        float centerX = MATRIX_WIDTH * 0.5f;
        float centerY = MATRIX_HEIGHT * 0.5f;
        float moonX = centerX + (1.0f - mix) * moonTravel;
        float moonY = centerY + std::sin(animationPhase * (0.6f + distortion * 0.8f)) * ((0.8f + distortion * 1.2f) * heightScale);

        // Screen area with some padding for the bezel
        float screenPadding = 3.0f;
        float screenWidth = box.size.x - (screenPadding * 2.0f);
        float screenHeight = box.size.y - (screenPadding * 2.0f);

        // Calculate dot spacing within the screen area
        float dotSpacingX = screenWidth / MATRIX_WIDTH;
        float dotSpacingY = screenHeight / MATRIX_HEIGHT;
        float dotRadius = std::min(dotSpacingX, dotSpacingY) * 0.40f;

        // Normalise LED color to 0-1
        float colorNormR = maxLedBrightness > 0.f ? ledColor.r / maxLedBrightness : 0.0f;
        float colorNormG = maxLedBrightness > 0.f ? ledColor.g / maxLedBrightness : 0.0f;
        float colorNormB = maxLedBrightness > 0.f ? ledColor.b / maxLedBrightness : 0.0f;

        const float baseAmbient = 0.06f + distortion * 0.04f;
        const float driveLuminance = 0.35f + drive * 0.65f;

        for (int x = 0; x < MATRIX_WIDTH; x++) {
            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                float dotX = screenPadding + x * dotSpacingX + dotSpacingX * 0.5f;
                float dotY = screenPadding + y * dotSpacingY + dotSpacingY * 0.5f;

                float brightness = baseAmbient;

                if (showTypeName) {
                    brightness = getTextPixelBrightness(x, y, lastDistortionType);
                    if (brightness > 0.0f) {
                        brightness = 0.85f * brightness;
                    }
                } else {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float sunDist = std::sqrt(dx * dx + dy * dy);
                    float sunSurface = clamp(1.0f - sunDist / sunRadius, 0.0f, 1.0f);

                    // Drive brightens and enlarges the sun
                    float sunContribution = sunSurface * driveLuminance;

                    // Distortion adds a sizzling corona near the edge
                    float edgeRegion = clamp((sunDist - (sunRadius - 1.4f)) / 1.4f, 0.0f, 1.0f);
                    float ripple = 0.5f + 0.5f * std::sin(animationPhase * (2.0f + distortion * 3.5f) + dx * 0.35f + dy * 0.28f);
                    float corona = distortion * edgeRegion * ripple * 0.7f;

                    // Moon occlusion travels based on mix
                    float mdx = x - moonX;
                    float mdy = y - moonY;
                    float moonDist = std::sqrt(mdx * mdx + mdy * mdy);
                    float moonCoverage = clamp(1.0f - moonDist / moonRadius, 0.0f, 1.0f);
                    float moonShadow = moonCoverage * (0.35f + mix * 0.65f);

                    brightness += sunContribution + corona;
                    brightness -= moonShadow;

                    // Slight heat shimmer ahead of the moon when partially covering
                    if (mix > 0.05f && moonCoverage > 0.1f) {
                        float shimmer = moonCoverage * distortion * 0.3f * std::sin(animationPhase * 3.2f + mdx * 0.55f);
                        brightness += shimmer;
                    }

                    brightness = clamp(brightness, 0.0f, 1.0f);
                }

                if (brightness <= 0.01f) {
                    continue;
                }

                float alpha = clamp(brightness, 0.0f, 1.0f);
                float r = clamp(colorNormR * brightness, 0.0f, 1.0f);
                float g = clamp(colorNormG * brightness, 0.0f, 1.0f);
                float b = clamp(colorNormB * brightness, 0.0f, 1.0f);

                nvgBeginPath(args.vg);
                nvgCircle(args.vg, dotX, dotY, dotRadius * (brightness > 0.7f ? 1.15f : 1.0f));
                nvgFillColor(args.vg, nvgRGBAf(r, g, b, alpha));
                nvgFill(args.vg);

                if (!showTypeName && brightness > 0.65f) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, dotX, dotY, dotRadius * 1.6f);
                    nvgFillColor(args.vg, nvgRGBAf(r, g, b, alpha * 0.25f));
                    nvgFill(args.vg);
                }
            }
        }

        Widget::draw(args);
    }

private:
    float getTextPixelBrightness(int x, int y, int distortionType) {
        // Define full distortion type names for 48x14 matrix
        static const char* fullNames[] = {
            "HARD CLIP", "WAVE FOLD", "BIT CRUSH", "DESTROY", "RING MOD", "TUBE SAT"
        };

        if (distortionType < 0 || distortionType >= 6) return 0.0f;

        const char* text = fullNames[distortionType];
        int textLen = strlen(text);

        // Proper 3x5 bitmap font for better readability
        static const uint8_t font3x5[][5] = {
            // A - 0
            {0b010, 0b101, 0b111, 0b101, 0b101},
            // B - 1
            {0b110, 0b101, 0b110, 0b101, 0b110},
            // C - 2
            {0b111, 0b100, 0b100, 0b100, 0b111},
            // D - 3
            {0b110, 0b101, 0b101, 0b101, 0b110},
            // E - 4
            {0b111, 0b100, 0b110, 0b100, 0b111},
            // F - 5
            {0b111, 0b100, 0b110, 0b100, 0b100},
            // G - 6
            {0b111, 0b100, 0b101, 0b101, 0b111},
            // H - 7
            {0b101, 0b101, 0b111, 0b101, 0b101},
            // I - 8
            {0b111, 0b010, 0b010, 0b010, 0b111},
            // L - 9
            {0b100, 0b100, 0b100, 0b100, 0b111},
            // M - 10
            {0b101, 0b111, 0b101, 0b101, 0b101},
            // N - 11
            {0b101, 0b111, 0b111, 0b101, 0b101},
            // O - 12
            {0b111, 0b101, 0b101, 0b101, 0b111},
            // P - 13
            {0b111, 0b101, 0b111, 0b100, 0b100},
            // R - 14
            {0b111, 0b101, 0b110, 0b101, 0b101},
            // S - 15
            {0b111, 0b100, 0b111, 0b001, 0b111},
            // T - 16
            {0b111, 0b010, 0b010, 0b010, 0b010},
            // U - 17
            {0b101, 0b101, 0b101, 0b101, 0b111},
            // V - 18
            {0b101, 0b101, 0b101, 0b101, 0b010},
            // W - 19
            {0b101, 0b101, 0b101, 0b111, 0b101},
            // Y - 20
            {0b101, 0b101, 0b010, 0b010, 0b010},
            // Space - 21
            {0b000, 0b000, 0b000, 0b000, 0b000}
        };


        // Calculate text positioning (centered)
        int charWidth = 3; // 3x5 font
        int charHeight = 5;
        int spacing = 1;
        int totalWidth = textLen * charWidth + (textLen - 1) * spacing;
        int startX = (MATRIX_WIDTH - totalWidth) / 2;
        int startY = (MATRIX_HEIGHT - charHeight) / 2;

        // Check if current pixel is part of text
        for (int i = 0; i < textLen; i++) {
            int charStartX = startX + i * (charWidth + spacing);

            if (x >= charStartX && x < charStartX + charWidth &&
                y >= startY && y < startY + charHeight) {

                int charIndex = -1;
                switch(text[i]) {
                    case 'A': charIndex = 0; break; case 'B': charIndex = 1; break; case 'C': charIndex = 2; break;
                    case 'D': charIndex = 3; break; case 'E': charIndex = 4; break; case 'F': charIndex = 5; break;
                    case 'G': charIndex = 6; break; case 'H': charIndex = 7; break; case 'I': charIndex = 8; break;
                    case 'L': charIndex = 9; break; case 'M': charIndex = 10; break; case 'N': charIndex = 11; break;
                    case 'O': charIndex = 12; break; case 'P': charIndex = 13; break; case 'R': charIndex = 14; break;
                    case 'S': charIndex = 15; break; case 'T': charIndex = 16; break; case 'U': charIndex = 17; break;
                    case 'V': charIndex = 18; break; case 'W': charIndex = 19; break; case 'Y': charIndex = 20; break;
                    case ' ': charIndex = 21; break; // Space character
                }
                if (charIndex >= 0) {
                    int localX = x - charStartX;
                    int localY = y - startY;

                    // Check if bit is set in font bitmap (3-bit wide)
                    if (font3x5[charIndex][localY] & (1 << (2 - localX))) {
                        // Apply fade effect
                        float alpha = clamp(typeDisplayTimer / TYPE_DISPLAY_DURATION, 0.0f, 1.0f);
                        return alpha;
                    }
                }
            }
        }

        return 0.0f; // Background pixels are off
    }

    void drawHardwareFrame(const DrawArgs& args) {
        // Outer bezel - dark metallic frame
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.0f);
        nvgFillColor(args.vg, nvgRGB(45, 45, 50)); // Dark metal
        nvgFill(args.vg);

        // Bezel highlight (top/left)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.0f);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGB(70, 70, 75)); // Lighter edge
        nvgStroke(args.vg);

        // Inner screen recess
        float recessPadding = 1.5f;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, recessPadding, recessPadding,
                      box.size.x - recessPadding * 2, box.size.y - recessPadding * 2, 1.5f);
        nvgFillColor(args.vg, nvgRGB(25, 25, 30)); // Darker recess
        nvgFill(args.vg);

        // Screen glass with slight reflection
        float glassPadding = 2.5f;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, glassPadding, glassPadding,
                      box.size.x - glassPadding * 2, box.size.y - glassPadding * 2, 1.0f);
        nvgFillColor(args.vg, nvgRGB(15, 15, 20)); // Very dark screen
        nvgFill(args.vg);

        // Subtle glass reflection
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, glassPadding, glassPadding,
                      box.size.x - glassPadding * 2, box.size.y - glassPadding * 2, 1.0f);
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStrokeColor(args.vg, nvgRGBA(80, 80, 90, 100)); // Subtle reflection
        nvgStroke(args.vg);
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
        WIDTH_PARAM,       // Stereo width control
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
        SIDECHAIN_LED,     // Visual feedback for sidechain activity
        NUM_LIGHTS
    };

    static constexpr float NOMINAL_LEVEL = 5.0f; // Reference voltage used for distortion normalization

    struct OversampleState {
        int factor = 1;
        bool bypass = true;
        float prevInput = 0.0f;
        float lp1 = 0.0f;
        float lp2 = 0.0f;
        float a1 = 0.0f;
        float b1 = 0.0f;
        float a2 = 0.0f;
        float b2 = 0.0f;

        void configure(float baseSampleRate, int newFactor) {
            factor = rack::math::clamp(newFactor, 1, 8);
            bypass = (factor <= 1);
            if (bypass) {
                a1 = b1 = a2 = b2 = 0.0f;
                return;
            }

            float oversampleRate = baseSampleRate * factor;
            float cutoff = baseSampleRate * 0.45f; // Keep below Nyquist of base rate
            float alpha1 = expf(-2.0f * M_PI * cutoff / oversampleRate);
            a1 = 1.0f - alpha1;
            b1 = alpha1;

            float cutoff2 = cutoff * 0.6f; // Slightly lower for second pole
            float alpha2 = expf(-2.0f * M_PI * cutoff2 / oversampleRate);
            a2 = 1.0f - alpha2;
            b2 = alpha2;
        }

        void reset() {
            prevInput = 0.0f;
            lp1 = 0.0f;
            lp2 = 0.0f;
        }

        float filter(float input) {
            if (bypass) {
                return input;
            }
            lp1 = a1 * input + b1 * lp1;
            lp2 = a2 * lp1 + b2 * lp2;
            return lp2;
        }
    };

    shapetaker::PolyphonicProcessor polyProcessor;
    shapetaker::SidechainDetector detector;
    shapetaker::VoiceArray<shapetaker::DistortionEngine> distortion_l, distortion_r;
    dsp::ExponentialSlewLimiter distortion_slew;

    // Processed parameter values (including CV modulation) for UI display
    float processed_distortion = 0.0f;
    float processed_drive = 0.0f;
    float processed_mix = 0.0f;

    // Smoothed values for LED display (prevents audio-rate flickering)
    float smoothed_distortion_display = 0.0f;
    float smoothed_drive_display = 0.0f;
    float smoothed_mix_display = 0.0f;
    // Smoothed distortion value for LED color calculation (same value LEDs use)
    float smoothed_distortion_for_leds = 0.0f;

    // Cached LED color for sharing with widgets (calculated once per process cycle)
    shapetaker::RGBColor currentLEDColor;

    // Wet/dry level tracking for auto-compensation
    shapetaker::FloatVoices cleanLevelL;
    shapetaker::FloatVoices cleanLevelR;
    shapetaker::FloatVoices wetLevelL;
    shapetaker::FloatVoices wetLevelR;
    shapetaker::FloatVoices makeupGainL;
    shapetaker::FloatVoices makeupGainR;

    std::array<OversampleState, shapetaker::PolyphonicProcessor::MAX_VOICES> oversampleStateL{};
    std::array<OversampleState, shapetaker::PolyphonicProcessor::MAX_VOICES> oversampleStateR{};

    // Rate-of-change tracking for adaptive smoothing
    float prev_distortion = 0.0f;
    float prev_drive = 0.0f;
    float prev_mix = 0.0f;

    // Sidechain mode (context menu): 0=Enhancement, 1=Ducking, 2=Direct
    int sidechainMode = 0;
    int oversampleFactor = 2;
    float currentSampleRate = 44100.0f;

    Chiaroscuro() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        shapetaker::ParameterHelper::configGain(this, VCA_PARAM, "VCA Gain");
        shapetaker::ParameterHelper::configSwitch(this, TYPE_PARAM, "Distortion Type",
            {"Hard Clip", "Wave Fold", "Bit Crush", "Destroy", "Ring Mod", "Tube Sat"}, 0);
        shapetaker::ParameterHelper::configGain(this, DIST_PARAM, "Distortion Amount");
        shapetaker::ParameterHelper::configAttenuverter(this, DIST_ATT_PARAM, "Distortion CV Attenuverter");
        shapetaker::ParameterHelper::configDrive(this, DRIVE_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, DRIVE_ATT_PARAM, "Drive CV Attenuverter");
        shapetaker::ParameterHelper::configMix(this, MIX_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, MIX_ATT_PARAM, "Mix CV Attenuverter");
        shapetaker::ParameterHelper::configToggle(this, LINK_PARAM, "Link L/R Channels");
        shapetaker::ParameterHelper::configToggle(this, RESPONSE_PARAM, "VCA Response: Linear/Exponential");
        configParam(WIDTH_PARAM, -1.0f, 1.0f, 0.0f, "Stereo Width", "%", 0.f, 100.f);
        paramQuantities[WIDTH_PARAM]->description = "Adjust stereo field: -100% = mono, 0% = normal, +100% = wide";
        
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
        detector.setTiming(10.0f, 200.0f);
        
        // Initialize distortion smoothing - fast enough to be responsive but slow enough to avoid clicks
        distortion_slew.setRiseFall(1000.f, 1000.f);

        currentSampleRate = APP->engine->getSampleRate();
        configureOversampling();
        resetLevelTracking();
    }
    
    void onSampleRateChange() override {
        currentSampleRate = APP->engine->getSampleRate();
        configureOversampling();
        resetLevelTracking();
    }

    void resetLevelTracking() {
        cleanLevelL.reset();
        cleanLevelR.reset();
        wetLevelL.reset();
        wetLevelR.reset();
        makeupGainL.forEach([](float& g) { g = 1.0f; });
        makeupGainR.forEach([](float& g) { g = 1.0f; });
        for (auto& state : oversampleStateL) {
            state.reset();
        }
        for (auto& state : oversampleStateR) {
            state.reset();
        }
    }

    void configureOversampling() {
        float oversampleRate = currentSampleRate * oversampleFactor;
        distortion_l.forEach([oversampleRate](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(oversampleRate);
        });
        distortion_r.forEach([oversampleRate](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(oversampleRate);
        });
        for (auto& state : oversampleStateL) {
            state.configure(currentSampleRate, oversampleFactor);
        }
        for (auto& state : oversampleStateR) {
            state.configure(currentSampleRate, oversampleFactor);
        }
    }

    void setOversampleFactor(int factor) {
        factor = rack::math::clamp(factor, 1, 8);
        if (factor == oversampleFactor)
            return;
        oversampleFactor = factor;
        configureOversampling();
        resetLevelTracking();
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "sidechainMode", json_integer(sidechainMode));
        json_object_set_new(rootJ, "oversampleFactor", json_integer(oversampleFactor));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* sidechainModeJ = json_object_get(rootJ, "sidechainMode");
        if (sidechainModeJ)
            sidechainMode = json_integer_value(sidechainModeJ);
        json_t* oversampleJ = json_object_get(rootJ, "oversampleFactor");
        if (oversampleJ) {
            int factor = json_integer_value(oversampleJ);
            setOversampleFactor(factor);
        }
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
        // Drive parameter with CV
        float drive = params[DRIVE_PARAM].getValue();
        if (inputs[DRIVE_CV_INPUT].isConnected()) {
            float cv_amount = params[DRIVE_ATT_PARAM].getValue(); // -1.0 to +1.0
            float cv_voltage = inputs[DRIVE_CV_INPUT].getVoltage(); // Typically 0-10V or -5V to +5V
            drive += (cv_voltage / 10.0f) * cv_amount; // 10V CV = 100% parameter change when attenuverter = 100%
        }
        drive = clamp(drive, 0.0f, 1.0f);

        // Mix parameter with CV
        float mix = params[MIX_PARAM].getValue();
        if (inputs[MIX_CV_INPUT].isConnected()) {
            float cv_amount = params[MIX_ATT_PARAM].getValue(); // -1.0 to +1.0
            float cv_voltage = inputs[MIX_CV_INPUT].getVoltage(); // Typically 0-10V or -5V to +5V
            mix += (cv_voltage / 10.0f) * cv_amount; // 10V CV = 100% parameter change when attenuverter = 100%
        }
        mix = clamp(mix, 0.0f, 1.0f);
        
        int distortion_type = (int)params[TYPE_PARAM].getValue();
        if (inputs[TYPE_CV_INPUT].isConnected()) {
            float cv = inputs[TYPE_CV_INPUT].getVoltage() * 0.1f;
            distortion_type = (int)(params[TYPE_PARAM].getValue() + cv * 6.0f);
        }
        distortion_type = clamp(distortion_type, 0, 5);
        
        // Distortion amount parameter with CV
        float dist_amount = params[DIST_PARAM].getValue();
        if (inputs[DIST_CV_INPUT].isConnected()) {
            float cv_amount = params[DIST_ATT_PARAM].getValue(); // -1.0 to +1.0
            float cv_voltage = inputs[DIST_CV_INPUT].getVoltage(); // Typically 0-10V or -5V to +5V
            dist_amount += (cv_voltage / 10.0f) * cv_amount; // 10V CV = 100% parameter change when attenuverter = 100%
        }
        dist_amount = clamp(dist_amount, 0.0f, 1.0f);

        // Store processed values for UI display (pixel ring)
        processed_distortion = dist_amount;
        processed_drive = drive;
        processed_mix = mix;

        // Apply adaptive smoothing to display values to prevent audio-rate flickering
        // Calculate rate of change for each parameter
        float dist_rate = fabsf(processed_distortion - prev_distortion) / args.sampleTime;
        float drive_rate = fabsf(processed_drive - prev_drive) / args.sampleTime;
        float mix_rate = fabsf(processed_mix - prev_mix) / args.sampleTime;

        // Update previous values
        prev_distortion = processed_distortion;
        prev_drive = processed_drive;
        prev_mix = processed_mix;

        // Adaptive cutoff frequency: good response up to 10Hz, then averaging above
        // Rate threshold of ~6.28 corresponds to 10Hz sine wave at full amplitude
        float rate_threshold = 6.28f; // 2 * pi * 10Hz

        // Calculate adaptive cutoff frequencies
        float dist_cutoff = (dist_rate > rate_threshold) ? 2.0f : 15.0f; // 2Hz avg, 15Hz responsive
        float drive_cutoff = (drive_rate > rate_threshold) ? 2.0f : 15.0f;
        float mix_cutoff = (mix_rate > rate_threshold) ? 2.0f : 15.0f;

        // Apply adaptive smoothing filters
        float dist_smooth_factor = 1.0f - expf(-2.0f * M_PI * dist_cutoff * args.sampleTime);
        float drive_smooth_factor = 1.0f - expf(-2.0f * M_PI * drive_cutoff * args.sampleTime);
        float mix_smooth_factor = 1.0f - expf(-2.0f * M_PI * mix_cutoff * args.sampleTime);

        smoothed_distortion_display += (processed_distortion - smoothed_distortion_display) * dist_smooth_factor;
        smoothed_drive_display += (processed_drive - smoothed_drive_display) * drive_smooth_factor;
        smoothed_mix_display += (processed_mix - smoothed_mix_display) * mix_smooth_factor;

        // Enhanced sidechain behavior with three modes
        float combined_distortion, effective_drive, effective_mix;

        if (inputs[SIDECHAIN_INPUT].isConnected()) {
            float sidechain_intensity = sc_env; // 0.0 to 1.0 based on sidechain input

            switch (sidechainMode) {
                case 0: // Enhancement mode (original behavior)
                    // Both distortion and drive scale together with sidechain
                    // Knob positions set the maximum values that sidechain can reach
                    combined_distortion = dist_amount * sidechain_intensity;
                    effective_drive = drive * sidechain_intensity;
                    effective_mix = fmaxf(mix, 0.8f); // At least 80% wet
                    break;

                case 1: // Ducking mode (inverse)
                    // Reduce distortion when sidechain is hot
                    combined_distortion = dist_amount * (1.0f - sidechain_intensity);
                    effective_drive = drive * (1.0f - sidechain_intensity * 0.7f); // Partial ducking
                    effective_mix = mix;
                    break;

                case 2: // Direct mode
                    // Sidechain directly controls distortion amount
                    combined_distortion = sidechain_intensity;
                    effective_drive = drive;
                    effective_mix = mix;
                    break;

                default:
                    combined_distortion = dist_amount;
                    effective_drive = drive;
                    effective_mix = mix;
                    break;
            }
        } else {
            // Normal mode: knobs control directly
            combined_distortion = dist_amount;
            effective_drive = drive;
            effective_mix = mix;
        }

        // Clamp final values
        combined_distortion = clamp(combined_distortion, 0.0f, 1.0f);
        
        
        // Apply smoothing to the combined distortion to prevent clicks
        float smoothed_distortion = distortion_slew.process(args.sampleTime, combined_distortion);
        // Store for LED color matching in dot matrix
        smoothed_distortion_for_leds = smoothed_distortion;

        // The actual distortion amount used in processing - use effective drive for sidechain mode
        float distortion_amount = smoothed_distortion * effective_drive;

        // LED brightness calculation using shared utility
        const float max_brightness = 0.6f;
        currentLEDColor = shapetaker::LightingHelper::getChiaroscuroColor(smoothed_distortion, max_brightness);

        lights[DIST_LED_R].setBrightness(currentLEDColor.r);
        lights[DIST_LED_G].setBrightness(currentLEDColor.g);
        lights[DIST_LED_B].setBrightness(currentLEDColor.b);
        
        // VCA gain calculation (polyphonic CV support)
        float base_vca_gain = params[VCA_PARAM].getValue();
        bool exponential_response = params[RESPONSE_PARAM].getValue() > 0.5f;
        
        const float normalizationVoltage = NOMINAL_LEVEL;
        const float invNormalization = 1.0f / normalizationVoltage;
        const float levelSmoothing = 1.0f - expf(-2.0f * M_PI * 15.0f * args.sampleTime);
        const float makeupSmoothing = 1.0f - expf(-2.0f * M_PI * 6.0f * args.sampleTime);
        const float minLevel = 1e-4f;

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
            
            float base_vca_l = input_l * vca_gain;
            float base_vca_r = input_r * vca_gain;

            float openFactor = clamp((vca_gain - 0.9f) / 0.4f, 0.0f, 1.0f);
            float signalPeak = fmaxf(fabsf(base_vca_l), fabsf(base_vca_r));
            float hotSignal = clamp((signalPeak - 6.0f) / 4.0f, 0.0f, 1.0f);
            float aggression = openFactor * hotSignal;
            float aggression_gain = 1.0f + aggression * 0.18f;
            float pre_drive_boost = 1.0f + aggression * 0.35f;

            float vca_l = base_vca_l * aggression_gain;
            float vca_r = base_vca_r * aggression_gain;
            
            // Process distortion for this voice
            float normalized_l = (vca_l * pre_drive_boost) * invNormalization;
            float normalized_r = (vca_r * pre_drive_boost) * invNormalization;

            float wetNormL = 0.0f;
            float wetNormR = 0.0f;

            if (oversampleFactor <= 1) {
                wetNormL = distortion_l[ch].process(normalized_l, distortion_amount,
                                                   (shapetaker::DistortionEngine::Type)distortion_type);
                wetNormR = distortion_r[ch].process(normalized_r, distortion_amount,
                                                   (shapetaker::DistortionEngine::Type)distortion_type);
                oversampleStateL[ch].prevInput = normalized_l;
                oversampleStateR[ch].prevInput = normalized_r;
            } else {
                auto& osL = oversampleStateL[ch];
                auto& osR = oversampleStateR[ch];
                float prevL = osL.prevInput;
                float prevR = osR.prevInput;
                float accumL = 0.0f;
                float accumR = 0.0f;
                const int factor = oversampleFactor;
                const float step = 1.0f / static_cast<float>(factor);

                for (int os = 0; os < factor; ++os) {
                    float t = (os + 1) * step;
                    float interpL = rack::math::crossfade(prevL, normalized_l, t);
                    float interpR = rack::math::crossfade(prevR, normalized_r, t);

                    float distortedL = distortion_l[ch].process(
                        interpL, distortion_amount,
                        (shapetaker::DistortionEngine::Type)distortion_type);
                    float distortedR = distortion_r[ch].process(
                        interpR, distortion_amount,
                        (shapetaker::DistortionEngine::Type)distortion_type);

                    accumL += osL.filter(distortedL);
                    accumR += osR.filter(distortedR);
                }

                osL.prevInput = normalized_l;
                osR.prevInput = normalized_r;
                wetNormL = accumL * step;
                wetNormR = accumR * step;
            }

            float wet_l = wetNormL * normalizationVoltage;
            float wet_r = wetNormR * normalizationVoltage;

            // Track RMS-like envelopes for wet/dry signals
            float cleanAbsL = fabsf(vca_l);
            float wetAbsL = fabsf(wet_l);
            cleanLevelL[ch] += (cleanAbsL - cleanLevelL[ch]) * levelSmoothing;
            wetLevelL[ch] += (wetAbsL - wetLevelL[ch]) * levelSmoothing;

            float cleanAbsR = fabsf(vca_r);
            float wetAbsR = fabsf(wet_r);
            cleanLevelR[ch] += (cleanAbsR - cleanLevelR[ch]) * levelSmoothing;
            wetLevelR[ch] += (wetAbsR - wetLevelR[ch]) * levelSmoothing;

            float desiredGainL = 1.0f;
            float desiredGainR = 1.0f;

            if (wetLevelL[ch] > minLevel) {
                desiredGainL = (cleanLevelL[ch] > minLevel) ? cleanLevelL[ch] / wetLevelL[ch] : 1.0f;
            }
            desiredGainL = clamp(desiredGainL, 0.25f, 4.0f);
            makeupGainL[ch] += (desiredGainL - makeupGainL[ch]) * makeupSmoothing;

            if (wetLevelR[ch] > minLevel) {
                desiredGainR = (cleanLevelR[ch] > minLevel) ? cleanLevelR[ch] / wetLevelR[ch] : 1.0f;
            }
            desiredGainR = clamp(desiredGainR, 0.25f, 4.0f);
            makeupGainR[ch] += (desiredGainR - makeupGainR[ch]) * makeupSmoothing;

            float compensated_l = wet_l * makeupGainL[ch];
            float compensated_r = wet_r * makeupGainR[ch];

            // Mix between clean and distorted signals - use effective mix for sidechain mode
            // Simple linear crossfade with slight boost to compensate for wet/dry balance
            float output_l = vca_l * (1.0f - effective_mix) + compensated_l * effective_mix;
            float output_r = vca_r * (1.0f - effective_mix) + compensated_r * effective_mix;

            // Apply gentle makeup gain at higher mix values to compensate for level drop
            // +1.5dB at 100% mix to maintain consistent loudness
            float mix_makeup = 1.0f + effective_mix * 0.19f;
            output_l *= mix_makeup;
            output_r *= mix_makeup;

            // Safety check for NaN/infinity
            if (!std::isfinite(output_l)) output_l = 0.0f;
            if (!std::isfinite(output_r)) output_r = 0.0f;

            // Stereo width processing (mid/side)
            float width = params[WIDTH_PARAM].getValue();
            if (width != 0.0f) {
                // Convert to mid/side
                float mid = (output_l + output_r) * 0.5f;
                float side = (output_l - output_r) * 0.5f;

                // Apply width adjustment
                // width = -1.0: full mono (side = 0)
                // width =  0.0: normal stereo (no change)
                // width = +1.0: wide stereo (side * 2)
                float width_scale = 1.0f + width;
                if (width < 0.0f) {
                    // Negative width: reduce side (towards mono)
                    width_scale = 1.0f + width; // 0.0 to 1.0
                }
                side *= width_scale;

                // Convert back to L/R
                output_l = mid + side;
                output_r = mid - side;
            }

            const float headroom = 9.5f;
            output_l = headroom * std::tanh(output_l / headroom);
            output_r = headroom * std::tanh(output_r / headroom);

            outputs[AUDIO_L_OUTPUT].setVoltage(output_l, ch);
            outputs[AUDIO_R_OUTPUT].setVoltage(output_r, ch);
        }
    }
};

// 8-bit style pixel ring display that surrounds the distortion selector
// Hardware-feasible design that could be built with 24-32 RGB LEDs in a circle
struct PixelRingWidget : TransparentWidget {
    Module* module = nullptr;
    Chiaroscuro* chiaroscuroModule = nullptr;  // Typed pointer to avoid dynamic_cast
    int distParamId = -1;
    int driveParamId = -1;
    int mixParamId = -1;
    int typeParamId = -1;
    float lastTime = 0.0f;

    // Hardware-like specifications
    static const int RING_PIXELS = 36;  // 36 RGB LEDs on single ring
    static const int PIXEL_SIZE = 2;   // 2x2 pixel blocks

    // Display state
    struct Pixel {
        uint8_t r, g, b;
        bool active;
        Pixel() : r(0), g(0), b(0), active(false) {}
        Pixel(uint8_t red, uint8_t green, uint8_t blue, bool on = true)
            : r(red), g(green), b(blue), active(on) {}
    };

    Pixel ringPixels[RING_PIXELS];
    float eclipseProgress = 0.0f;
    float driveIntensity = 0.0f;
    float mixLevel = 0.0f;
    int distortionType = 0;
    float animationPhase = 0.0f;

    // Previous values for conditional animation (#16)
    float prevEclipseProgress = 0.0f;
    float prevDriveIntensity = 0.0f;
    float prevMixLevel = 0.0f;

    PixelRingWidget() {
        box.size = Vec(80, 80); // Smaller size for single ring
    }

    void step() override {
        TransparentWidget::step();

        if (!chiaroscuroModule) return;

        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        deltaTime = clamp(deltaTime, 0.0f, 1.0f / 30.0f);

        // Get smoothed display parameters (prevents audio-rate flickering)
        // Use smoothed values for LED display to prevent glitching at audio rates
        eclipseProgress = clamp(chiaroscuroModule->smoothed_distortion_display, 0.0f, 1.0f);
        driveIntensity = clamp(chiaroscuroModule->smoothed_drive_display, 0.0f, 1.0f);
        mixLevel = clamp(chiaroscuroModule->smoothed_mix_display, 0.0f, 1.0f);
        distortionType = (typeParamId >= 0) ? (int)chiaroscuroModule->params[typeParamId].getValue() : 0;

        // Check if parameters have changed to conditionally pause animation
        const float changeThreshold = 0.001f;
        bool parametersStable =
            fabsf(eclipseProgress - prevEclipseProgress) < changeThreshold &&
            fabsf(driveIntensity - prevDriveIntensity) < changeThreshold &&
            fabsf(mixLevel - prevMixLevel) < changeThreshold;

        prevEclipseProgress = eclipseProgress;
        prevDriveIntensity = driveIntensity;
        prevMixLevel = mixLevel;

        // Only animate when parameters are changing or drive is high (for sparkles)
        if (!parametersStable || driveIntensity > 0.4f) {
            animationPhase += deltaTime * 2.0f; // Slow animation for retro feel
        }

        updatePixelRing();
    }

    void updatePixelRing() {
        // Clear all pixels first
        for (int i = 0; i < RING_PIXELS; i++) {
            ringPixels[i] = Pixel();
        }

        // Get base colors based on distortion type (retro palette)
        Pixel cleanColor, distortedColor;
        switch (distortionType) {
            case 0: // Hard Clip - classic green/black computer terminal
                cleanColor = Pixel(0, 255, 0);
                distortedColor = Pixel(255, 0, 0);
                break;
            case 1: // Wave Fold - amber monochrome
                cleanColor = Pixel(255, 191, 0);
                distortedColor = Pixel(255, 100, 0);
                break;
            case 2: // Bit Crush - classic C64 blue/cyan
                cleanColor = Pixel(0, 255, 255);
                distortedColor = Pixel(0, 0, 255);
                break;
            case 3: // Destroy - aggressive red/orange
                cleanColor = Pixel(255, 255, 0);
                distortedColor = Pixel(255, 0, 0);
                break;
            case 4: // Ring Mod - retro purple/magenta
                cleanColor = Pixel(255, 255, 255);
                distortedColor = Pixel(255, 0, 255);
                break;
            case 5: // Tube Sat - warm white/orange
                cleanColor = Pixel(255, 220, 160);
                distortedColor = Pixel(255, 100, 0);
                break;
            default:
                cleanColor = Pixel(255, 255, 255);
                distortedColor = Pixel(128, 128, 128);
        }

        // Drive only affects brightness of active distorted pixels, not baseline visibility
        // Keep base colors at full intensity, drive will be applied per-pixel
        // No modification of base colors here

        // Single-ring eclipse animation
        // Ring shows distortion progress from 0 to full circle
        int distortionPixels = (int)ceil(eclipseProgress * RING_PIXELS);

        // Process single ring
        for (int i = 0; i < RING_PIXELS; i++) {
            bool inDistortionZone = (i < distortionPixels);

            if (inDistortionZone) {
                // Distorted pixels: mix determines color, drive determines brightness
                uint8_t mixedR = (uint8_t)(cleanColor.r * (1.0f - mixLevel) + distortedColor.r * mixLevel);
                uint8_t mixedG = (uint8_t)(cleanColor.g * (1.0f - mixLevel) + distortedColor.g * mixLevel);
                uint8_t mixedB = (uint8_t)(cleanColor.b * (1.0f - mixLevel) + distortedColor.b * mixLevel);

                // Drive controls brightness of distorted pixels
                float driveBrightness = 0.4f + driveIntensity * 0.6f; // 40% to 100% based on drive
                mixedR = (uint8_t)(mixedR * driveBrightness);
                mixedG = (uint8_t)(mixedG * driveBrightness);
                mixedB = (uint8_t)(mixedB * driveBrightness);

                ringPixels[i] = Pixel(mixedR, mixedG, mixedB, true);
            } else {
                // Clean pixels: baseline clean color at low brightness
                uint8_t baseR = (uint8_t)(cleanColor.r * 0.2f);
                uint8_t baseG = (uint8_t)(cleanColor.g * 0.2f);
                uint8_t baseB = (uint8_t)(cleanColor.b * 0.2f);

                ringPixels[i] = Pixel(baseR, baseG, baseB, true);
            }
        }

        // Layer 3: Drive sparkle effect
        if (driveIntensity > 0.4f) {
            int sparkleCount = (int)(driveIntensity * 4.0f); // More sparkles with higher drive

            // Ring sparkles
            for (int s = 0; s < sparkleCount; s++) {
                int sparkleIndex = (int)(animationPhase * (2.0f + s * 1.5f)) % RING_PIXELS;
                if (ringPixels[sparkleIndex].active) {
                    ringPixels[sparkleIndex].r = fminf(ringPixels[sparkleIndex].r + 80, 255);
                    ringPixels[sparkleIndex].g = fminf(ringPixels[sparkleIndex].g + 80, 255);
                    ringPixels[sparkleIndex].b = fminf(ringPixels[sparkleIndex].b + 80, 255);
                }
            }
        }

        // Layer 4: Subtle retro flicker
        for (int i = 0; i < RING_PIXELS; i++) {
            if (ringPixels[i].active) {
                float flicker = 0.85f + 0.15f * sin(animationPhase * 6.0f + i * 0.3f);
                ringPixels[i].r = (uint8_t)(ringPixels[i].r * flicker);
                ringPixels[i].g = (uint8_t)(ringPixels[i].g * flicker);
                ringPixels[i].b = (uint8_t)(ringPixels[i].b * flicker);
            }
        }
    }

    void draw(const DrawArgs& args) override {
        if (!chiaroscuroModule) return;

        Vec center = box.size.div(2);
        float ringRadius = 28.0f;  // Single ring radius (adjusted to meet larger switch)

        nvgSave(args.vg);

        // Draw hardware substrate - dark background ring for realistic LED strip look
        float substrate_width = 3.0f; // Width of the black substrate

        // Ring substrate
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, center.x, center.y, ringRadius + substrate_width);
        nvgCircle(args.vg, center.x, center.y, ringRadius - substrate_width);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGBA(15, 15, 18, 220)); // Very dark substrate
        nvgFill(args.vg);

        // Add subtle edge highlights to simulate PCB traces
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, center.x, center.y, ringRadius + substrate_width);
        nvgStrokeColor(args.vg, nvgRGBA(40, 40, 45, 100));
        nvgStrokeWidth(args.vg, 0.3f);
        nvgStroke(args.vg);

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, center.x, center.y, ringRadius - substrate_width);
        nvgStrokeColor(args.vg, nvgRGBA(40, 40, 45, 100));
        nvgStrokeWidth(args.vg, 0.3f);
        nvgStroke(args.vg);

        // Draw ring pixels
        for (int i = 0; i < RING_PIXELS; i++) {
            if (!ringPixels[i].active) continue;

            float angle = (float)i / RING_PIXELS * 2.0f * M_PI - M_PI / 2.0f; // Start from top
            float pixelX = center.x + cos(angle) * ringRadius;
            float pixelY = center.y + sin(angle) * ringRadius;

            // Draw pixel as small square
            nvgBeginPath(args.vg);
            nvgRect(args.vg, pixelX - PIXEL_SIZE/2, pixelY - PIXEL_SIZE/2, PIXEL_SIZE, PIXEL_SIZE);
            nvgFillColor(args.vg, nvgRGBA(ringPixels[i].r, ringPixels[i].g, ringPixels[i].b, 255));
            nvgFill(args.vg);

            // Add slight glow for LED realism
            if (ringPixels[i].r > 50 || ringPixels[i].g > 50 || ringPixels[i].b > 50) {
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, pixelX, pixelY, PIXEL_SIZE * 1.2f);
                nvgFillColor(args.vg, nvgRGBA(ringPixels[i].r, ringPixels[i].g, ringPixels[i].b, 30));
                nvgFill(args.vg);
            }
        }

        nvgRestore(args.vg);
    }
};

struct ChiaroscuroWidget : ModuleWidget {

    void appendContextMenu(Menu* menu) override {
        Chiaroscuro* module = dynamic_cast<Chiaroscuro*>(this->module);
        if (!module)
            return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Oversampling"));

        struct OversampleFactorItem : MenuItem {
            Chiaroscuro* module;
            int factor;
            void onAction(const event::Action& e) override {
                module->setOversampleFactor(factor);
            }
            void step() override {
                rightText = (module->oversampleFactor == factor) ? "✔" : "";
                MenuItem::step();
            }
        };

        menu->addChild(construct<OversampleFactorItem>(&MenuItem::text, "1x", &OversampleFactorItem::module, module, &OversampleFactorItem::factor, 1));
        menu->addChild(construct<OversampleFactorItem>(&MenuItem::text, "2x", &OversampleFactorItem::module, module, &OversampleFactorItem::factor, 2));
        menu->addChild(construct<OversampleFactorItem>(&MenuItem::text, "4x", &OversampleFactorItem::module, module, &OversampleFactorItem::factor, 4));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Sidechain Mode"));

        struct SidechainModeItem : MenuItem {
            Chiaroscuro* module;
            int mode;
            void onAction(const event::Action& e) override {
                module->sidechainMode = mode;
            }
            void step() override {
                rightText = (module->sidechainMode == mode) ? "✔" : "";
                MenuItem::step();
            }
        };

        menu->addChild(construct<SidechainModeItem>(&MenuItem::text, "Enhancement (Trigger)", &SidechainModeItem::module, module, &SidechainModeItem::mode, 0));
        menu->addChild(construct<SidechainModeItem>(&MenuItem::text, "Ducking (Inverse)", &SidechainModeItem::module, module, &SidechainModeItem::mode, 1));
        menu->addChild(construct<SidechainModeItem>(&MenuItem::text, "Direct Control", &SidechainModeItem::module, module, &SidechainModeItem::mode, 2));
    }

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

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Create positioning helper from SVG panel
        using LayoutHelper = shapetaker::ui::LayoutHelper;
        auto centerPx = LayoutHelper::createCenterPxHelper(
            asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg")
        );
        
        // Audio I/O - BNC connectors for vintage oscilloscope look
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio-in-l", 7.5756826f, 114.8209f), module, Chiaroscuro::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio-in-r", 22.049751f, 114.8209f), module, Chiaroscuro::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio-out-l", 36.523819f, 114.8209f), module, Chiaroscuro::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio-out-r", 50.997887f, 114.8209f), module, Chiaroscuro::AUDIO_R_OUTPUT));
        
        // Main VCA knob
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("vca-knob", 18.328495f, 50.193539f), module, Chiaroscuro::VCA_PARAM));
        
        // VCA CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("vca-cv", 7.5756836f, 98.635521f), module, Chiaroscuro::VCA_CV_INPUT));
        
        // Linear/Exponential response switch
        Vec responseCenter = centerPx("lin-exp-switch", 34.048016f, 33.862297f);
        auto* responseSwitch = createParamCentered<ShapetakerVintageRussianToggle>(responseCenter, module, Chiaroscuro::RESPONSE_PARAM);
        addParam(responseSwitch);
        
        // Link switch
        Vec linkCenter = centerPx("lin-lr-switch", 34.048016f, 20.758846f);
        auto* linkSwitch = createParamCentered<ShapetakerVintageRussianToggle>(linkCenter, module, Chiaroscuro::LINK_PARAM);
        addParam(linkSwitch);
        
        // Sidechain input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("sidechain-detect-cv", 7.5756826f, 101.61994f), module, Chiaroscuro::SIDECHAIN_INPUT));

        // Distortion type CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("dist-type-cv", 36.523819f, 82.450134f), module, Chiaroscuro::TYPE_CV_INPUT));
        
        // Distortion knob
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("dist-knob", 50.997887f, 66.264755f), module, Chiaroscuro::DIST_PARAM));

        // Distortion CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("dist-cv", 7.5756836f, 82.450134f), module, Chiaroscuro::DIST_CV_INPUT));
        
        // Drive knob
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("drive-knob", 50.997887f, 82.717743f), module, Chiaroscuro::DRIVE_PARAM));

        // Drive CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("drive-cv", 22.049751f, 82.450134f), module, Chiaroscuro::DRIVE_CV_INPUT));
        
        // Mix knob
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("mix-knob", 50.997887f, 99.170738f), module, Chiaroscuro::MIX_PARAM));

        // ATTENUVERTERS (knobs)
        // Distortion attenuverter
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("dist-atten", 7.5756836f, 66.264755f), module, Chiaroscuro::DIST_ATT_PARAM));

        // Drive attenuverter
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("drive-atten", 22.049751f, 66.264755f), module, Chiaroscuro::DRIVE_ATT_PARAM));

        // Mix attenuverter
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("mix-atten", 36.523819f, 66.264755f), module, Chiaroscuro::MIX_ATT_PARAM));

        // Mix CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("mix-cv", 36.523819f, 99.170738f), module, Chiaroscuro::MIX_CV_INPUT));

        // Distortion type selector
        addParam(createParamCentered<ShapetakerVintageSelector>(centerPx("dist-type-select", 42.631508f, 50.193539f), module, Chiaroscuro::TYPE_PARAM));

        // 8-bit pixel ring around distortion selector
        PixelRingWidget* pixelRing = new PixelRingWidget();
        Vec selectorCenter = centerPx("dist-type-select", 42.631508f, 50.193539f);
        pixelRing->box.pos = selectorCenter.minus(pixelRing->box.size.div(2));
        pixelRing->module = module;
        pixelRing->chiaroscuroModule = module;  // Store typed pointer to avoid dynamic_cast
        pixelRing->distParamId = Chiaroscuro::DIST_PARAM;
        pixelRing->driveParamId = Chiaroscuro::DRIVE_PARAM;
        pixelRing->mixParamId = Chiaroscuro::MIX_PARAM;
        pixelRing->typeParamId = Chiaroscuro::TYPE_PARAM;
        addChild(pixelRing);

        // Vintage dot matrix display with sun/moon eclipse
#if 0
        // Temporarily disabled to remove the screen from the hardware panel while keeping the code handy.
        VintageDotMatrix* dotMatrix = new VintageDotMatrix(module);
        Vec dotMatrixCenter = centerPx("dot_matrix", 30.7f, 104.272f);  // Center of the 48.36x14.61 rectangle
        dotMatrix->box.pos = dotMatrixCenter.minus(dotMatrix->box.size.div(2));
        addChild(dotMatrix);
#endif
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
