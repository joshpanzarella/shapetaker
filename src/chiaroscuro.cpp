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

// Vintage Dot Matrix Display Widget with Sun/Moon Eclipse
struct VintageDotMatrix : Widget {
    Module* module;
    static const int MATRIX_WIDTH = 48;  // High resolution for smooth eclipse effects
    static const int MATRIX_HEIGHT = 14; // High resolution for smooth eclipse effects

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

        // Get parameters from Chiaroscuro module
        // For now, derive the LED color values from the actual LED brightness values
        // This ensures perfect color matching since we're using the exact same data the LEDs use
        float distortion = 0.0f;
        float drive = module->params[4].getValue();      // DRIVE_PARAM
        float mix = module->params[6].getValue();        // MIX_PARAM

        if (module) {
            // Extract distortion value from LED brightness calculations
            // The LEDs use red/green/blue brightness values that encode the distortion level
            float red_brightness = module->lights[0].getBrightness();   // DIST_LED_R
            float green_brightness = module->lights[1].getBrightness(); // DIST_LED_G

            // Reverse-calculate distortion from LED values using the same logic as LEDs
            const float base_brightness = 0.6f;
            const float max_brightness = base_brightness;

            if (red_brightness <= max_brightness && green_brightness >= max_brightness * 0.9f) {
                // In the 0 to 0.5 range: red increases from 0 to max, green and blue stay max
                distortion = (red_brightness / max_brightness) * 0.5f;
            } else if (red_brightness >= max_brightness * 0.9f) {
                // In the 0.5 to 1.0 range: red stays max, green decreases
                distortion = 0.5f + (1.0f - (green_brightness / max_brightness)) * 0.5f;
            }

            // Clamp to valid range
            distortion = clamp(distortion, 0.0f, 1.0f);
        }

        // Check if we should show type name instead of eclipse
        bool showTypeName = (typeDisplayTimer > 0.0f);

        // Calculate eclipse parameters (scaled for high resolution matrix)
        float sunRadius = 6.0f + drive * 6.0f;  // Sun grows with drive (larger for high-res)
        float moonRadius = 4.5f + distortion * 6.0f;  // Moon grows with distortion
        float eclipseOffset = mix * 15.0f;  // Eclipse position based on mix (wider range)

        // Center positions
        float centerX = MATRIX_WIDTH / 2.0f;
        float centerY = MATRIX_HEIGHT / 2.0f;
        float moonX = centerX + eclipseOffset - 7.5f;
        float moonY = centerY + sin(animationPhase * 0.5f) * 1.2f;

        // Screen area with some padding for the bezel
        float screenPadding = 3.0f;
        float screenWidth = box.size.x - (screenPadding * 2);
        float screenHeight = box.size.y - (screenPadding * 2);

        // Calculate dot spacing within the screen area
        float dotSpacingX = screenWidth / MATRIX_WIDTH;
        float dotSpacingY = screenHeight / MATRIX_HEIGHT;
        float dotSize = std::min(dotSpacingX, dotSpacingY) * 0.6f; // 60% of spacing for visible gaps between LEDs

        // Draw red dot matrix
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                float dotX = screenPadding + x * dotSpacingX + dotSpacingX * 0.5f;
                float dotY = screenPadding + y * dotSpacingY + dotSpacingY * 0.5f;

                // Distance from sun center
                float sunDist = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));

                // Distance from moon center
                float moonDist = sqrt((x - moonX) * (x - moonX) + (y - moonY) * (y - moonY));

                // Calculate dot brightness
                float brightness = 0.0f;

                if (showTypeName) {
                    // Show distortion type name using dot matrix pattern
                    brightness = getTextPixelBrightness(x, y, lastDistortionType);
                } else {
                    // Normal eclipse animation
                    // Sun contribution
                    if (sunDist <= sunRadius) {
                        float sunIntensity = 1.0f - (sunDist / sunRadius);
                        brightness += sunIntensity * 0.9f;
                    }

                    // Moon eclipse
                    if (moonDist <= moonRadius) {
                        float moonIntensity = 1.0f - (moonDist / moonRadius);
                        brightness -= moonIntensity * 0.7f;  // Eclipse effect
                    }
                }

                // Add vintage LED flicker
                brightness += sin(animationPhase + x * 0.3f + y * 0.2f) * 0.03f;
                brightness = clamp(brightness, 0.0f, 1.0f);

                // LED matrix colors matching selector strip exactly
                NVGcolor dotColor;
                if (brightness > 0.05f) {
                    // Use EXACT same color scheme as selector LED strip
                    float led_r, led_g, led_b;
                    const float base_brightness = 0.6f;
                    const float max_brightness = base_brightness;

                    if (distortion <= 0.5f) {
                        // 0 to 0.5: Teal to bright blue-purple
                        led_r = distortion * 2.0f * max_brightness;
                        led_g = max_brightness;
                        led_b = max_brightness;
                    } else {
                        // 0.5 to 1.0: Bright blue-purple to dark purple
                        led_r = max_brightness;
                        led_g = 2.0f * (1.0f - distortion) * max_brightness;
                        led_b = max_brightness * (1.7f - distortion * 0.7f);
                    }

                    // Scale by dot brightness
                    float r = led_r * brightness / max_brightness;
                    float g = led_g * brightness / max_brightness;
                    float b = led_b * brightness / max_brightness;

                    dotColor = nvgRGBAf(r, g, b, brightness);
                } else {
                    // Very dim background dots with same color logic
                    float led_r, led_g, led_b;
                    if (distortion <= 0.5f) {
                        led_r = distortion * 2.0f * 0.1f;
                        led_g = 0.1f;
                        led_b = 0.1f;
                    } else {
                        led_r = 0.1f;
                        led_g = 2.0f * (1.0f - distortion) * 0.1f;
                        led_b = 0.1f * (1.7f - distortion * 0.7f);
                    }
                    dotColor = nvgRGBAf(led_r, led_g, led_b, 0.4f);
                }

                // Draw the LED dot with slight glow
                if (brightness > 0.3f) {
                    // Glow effect using same color calculation
                    float glow_r, glow_g, glow_b;
                    if (distortion <= 0.5f) {
                        glow_r = distortion * 2.0f * brightness;
                        glow_g = brightness;
                        glow_b = brightness;
                    } else {
                        glow_r = brightness;
                        glow_g = 2.0f * (1.0f - distortion) * brightness;
                        glow_b = brightness * (1.7f - distortion * 0.7f);
                    }

                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, dotX, dotY, dotSize * 0.8f);
                    nvgFillColor(args.vg, nvgRGBAf(glow_r, glow_g, glow_b, brightness * 0.3f));
                    nvgFill(args.vg);
                }

                // Main LED dot
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, dotX, dotY, dotSize * 0.5f);
                nvgFillColor(args.vg, dotColor);
                nvgFill(args.vg);
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
        NUM_LIGHTS
    };

    static constexpr float NOMINAL_LEVEL = 5.0f; // Reference voltage used for distortion normalization

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

    // Wet/dry level tracking for auto-compensation
    shapetaker::FloatVoices cleanLevelL;
    shapetaker::FloatVoices cleanLevelR;
    shapetaker::FloatVoices wetLevelL;
    shapetaker::FloatVoices wetLevelR;
    shapetaker::FloatVoices makeupGainL;
    shapetaker::FloatVoices makeupGainR;

    // Rate-of-change tracking for adaptive smoothing
    float prev_distortion = 0.0f;
    float prev_drive = 0.0f;
    float prev_mix = 0.0f;
    
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

        resetLevelTracking();
    }
    
    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();
        distortion_l.forEach([sr](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(sr);
        });
        distortion_r.forEach([sr](shapetaker::DistortionEngine& engine) {
            engine.setSampleRate(sr);
        });

        resetLevelTracking();
    }

    void resetLevelTracking() {
        cleanLevelL.reset();
        cleanLevelR.reset();
        wetLevelL.reset();
        wetLevelR.reset();
        makeupGainL.forEach([](float& g) { g = 1.0f; });
        makeupGainR.forEach([](float& g) { g = 1.0f; });
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

        // Enhanced sidechain behavior: when connected, modulate distortion AND drive together
        float combined_distortion, effective_drive, effective_mix;

        if (inputs[SIDECHAIN_INPUT].isConnected()) {
            // Sidechain mode: knobs set maximum range, sidechain signal controls intensity
            float sidechain_intensity = sc_env; // 0.0 to 1.0 based on sidechain input

            // Both distortion and drive scale together with sidechain
            // Knob positions set the maximum values that sidechain can reach
            combined_distortion = dist_amount * sidechain_intensity;
            effective_drive = drive * sidechain_intensity;

            // Mix stays at maximum (or near maximum) for full effect
            effective_mix = fmaxf(mix, 0.8f); // At least 80% wet, respects knob if higher
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
            
            // Process distortion for this voice with consistent headroom reference
            float normalized_l = (vca_l * pre_drive_boost) * invNormalization;
            float normalized_r = (vca_r * pre_drive_boost) * invNormalization;

            float wetNormL = distortion_l[ch].process(normalized_l, distortion_amount,
                                                     (shapetaker::DistortionEngine::Type)distortion_type);
            float wetNormR = distortion_r[ch].process(normalized_r, distortion_amount,
                                                     (shapetaker::DistortionEngine::Type)distortion_type);

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
            float output_l = vca_l + effective_mix * (compensated_l - vca_l);
            float output_r = vca_r + effective_mix * (compensated_r - vca_r);

            const float headroom = 9.5f;
            output_l = headroom * std::tanh(output_l / headroom);
            output_r = headroom * std::tanh(output_r / headroom);
            
            outputs[AUDIO_L_OUTPUT].setVoltage(output_l, ch);
            outputs[AUDIO_R_OUTPUT].setVoltage(output_r, ch);
        }
    }
};



// Forward declaration for Chiaroscuro module
struct Chiaroscuro;

// 8-bit style pixel ring display that surrounds the distortion selector
// Hardware-feasible design that could be built with 24-32 RGB LEDs in a circle
struct PixelRingWidget : TransparentWidget {
    Module* module = nullptr;
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

    PixelRingWidget() {
        box.size = Vec(80, 80); // Smaller size for single ring
    }

    void step() override {
        TransparentWidget::step();

        if (!module) return;

        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        deltaTime = clamp(deltaTime, 0.0f, 1.0f / 30.0f);

        // Get smoothed display parameters (prevents audio-rate flickering)
        Chiaroscuro* chiaroscuroModule = dynamic_cast<Chiaroscuro*>(module);
        if (chiaroscuroModule) {
            // Use smoothed values for LED display to prevent glitching at audio rates
            eclipseProgress = clamp(chiaroscuroModule->smoothed_distortion_display, 0.0f, 1.0f);
            driveIntensity = clamp(chiaroscuroModule->smoothed_drive_display, 0.0f, 1.0f);
            mixLevel = clamp(chiaroscuroModule->smoothed_mix_display, 0.0f, 1.0f);
            distortionType = (typeParamId >= 0) ? (int)module->params[typeParamId].getValue() : 0;
        } else {
            // Fallback to raw parameter values if cast fails
            float distAmount = (distParamId >= 0) ? module->params[distParamId].getValue() : 0.0f;
            float driveAmount = (driveParamId >= 0) ? module->params[driveParamId].getValue() : 0.0f;
            float mixAmount = (mixParamId >= 0) ? module->params[mixParamId].getValue() : 0.0f;
            distortionType = (typeParamId >= 0) ? (int)module->params[typeParamId].getValue() : 0;

            eclipseProgress = clamp(distAmount, 0.0f, 1.0f);
            driveIntensity = clamp(driveAmount, 0.0f, 1.0f);
            mixLevel = clamp(mixAmount, 0.0f, 1.0f);
        }
        animationPhase += deltaTime * 2.0f; // Slow animation for retro feel

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
        if (!module) return;

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

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg"));
        auto centerPx = [&](const std::string& id, float defx, float defy) -> Vec {
            return parser.centerPx(id, defx, defy);
        };
        
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
        auto* responseSwitch = createParamCentered<ShapetakerVintageToggleSwitch>(responseCenter, module, Chiaroscuro::RESPONSE_PARAM);
        responseSwitch->box.size = mm2px(Vec(8.1225f, 16.245f));
        responseSwitch->box.pos = responseCenter.minus(responseSwitch->box.size.div(2.f));
        addParam(responseSwitch);
        
        // Link switch
        Vec linkCenter = centerPx("lin-lr-switch", 34.048016f, 20.758846f);
        auto* linkSwitch = createParamCentered<ShapetakerVintageToggleSwitch>(linkCenter, module, Chiaroscuro::LINK_PARAM);
        linkSwitch->box.size = mm2px(Vec(8.1225f, 16.245f));
        linkSwitch->box.pos = linkCenter.minus(linkSwitch->box.size.div(2.f));
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
        pixelRing->distParamId = Chiaroscuro::DIST_PARAM;
        pixelRing->driveParamId = Chiaroscuro::DRIVE_PARAM;
        pixelRing->mixParamId = Chiaroscuro::MIX_PARAM;
        pixelRing->typeParamId = Chiaroscuro::TYPE_PARAM;
        addChild(pixelRing);

        // Vintage dot matrix display with sun/moon eclipse
        VintageDotMatrix* dotMatrix = new VintageDotMatrix(module);
        Vec dotMatrixCenter = centerPx("dot_matrix", 30.7f, 104.272f);  // Center of the 48.36x14.61 rectangle
        dotMatrix->box.pos = dotMatrixCenter.minus(dotMatrix->box.size.div(2));
        addChild(dotMatrix);
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
