#include "plugin.hpp"
#include <dsp/digital.hpp>
#include <dsp/filter.hpp>
#include <cmath>
#include <array>
#include <string>
#include <cctype>
#include <cstdio>

// Forward declaration for Chiaroscuro module (needed by widgets)
struct Chiaroscuro;

// Parameter/light indices for accessing values before the full Chiaroscuro definition
static constexpr int CHIAROSC_DIST_PARAM = 2;
static constexpr int CHIAROSC_DRIVE_PARAM = 4;
static constexpr int CHIAROSC_MIX_PARAM = 6;

static constexpr int CHIAROSC_LED_R = 0;
static constexpr int CHIAROSC_LED_G = 1;
static constexpr int CHIAROSC_LED_B = 2;

static inline void getDistortionTypeColor(int type, float& r, float& g, float& b) {
    switch (type) {
        case 0: // Hard Clip - Teal (darkened for jewel lens)
            r = 0.09f; g = 0.45f; b = 0.38f;
            break;
        case 1: // Tube Sat - Aqua (darkened for jewel lens)
            r = 0.20f; g = 0.50f; b = 0.50f;
            break;
        case 2: // Wave Fold - Cyan Blue (darkened for jewel lens)
            r = 0.18f; g = 0.38f; b = 0.58f;
            break;
        case 3: // Bit Crush - Deep Blue (darkened for jewel lens)
            r = 0.13f; g = 0.25f; b = 0.58f;
            break;
        case 4: // Destroy - Violet (darkened for jewel lens)
            r = 0.28f; g = 0.20f; b = 0.58f;
            break;
        case 5: // Ring Mod - Magenta Purple (darkened for jewel lens)
            r = 0.38f; g = 0.17f; b = 0.58f;
            break;
        default:
            r = 0.26f; g = 0.34f; b = 0.46f;
    }
}

static inline const char* getDistortionTypeName(int type) {
    switch (type) {
        case 0: return "HARD CLIP";
        case 1: return "TUBE SAT";
        case 2: return "WAVE FOLD";
        case 3: return "BIT CRUSH";
        case 4: return "DESTROY";
        case 5: return "RING MOD";
        default: return "DIST TYPE";
    }
}

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
            "HARD CLIP", "TUBE SAT", "WAVE FOLD", "BIT CRUSH", "DESTROY", "RING MOD"
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
        GAIN_LED_R,
        GAIN_LED_G,
        GAIN_LED_B,
        NUM_LIGHTS
    };

    static constexpr float NOMINAL_LEVEL = 5.0f; // Reference voltage used for distortion normalization

    struct OversampleState {
        int factor = 1;
        bool bypass = true;
        float prevInput = 0.0f;
        float lp1 = 0.0f;
        float lp2 = 0.0f;
        float lp3 = 0.0f;
        float a1 = 0.0f;
        float b1 = 0.0f;
        float a2 = 0.0f;
        float b2 = 0.0f;
        float a3 = 0.0f;
        float b3 = 0.0f;

        void configure(float baseSampleRate, int newFactor) {
            factor = rack::math::clamp(newFactor, 1, 8);
            bypass = (factor <= 1);
            if (bypass) {
                a1 = b1 = a2 = b2 = a3 = b3 = 0.0f;
                return;
            }

            float oversampleRate = baseSampleRate * factor;
            float cutoff = baseSampleRate * 0.38f; // Keep below Nyquist of base rate with more attenuation
            float alpha1 = expf(-2.0f * M_PI * cutoff / oversampleRate);
            a1 = 1.0f - alpha1;
            b1 = alpha1;

            float cutoff2 = cutoff * 0.55f; // Slightly lower for second pole
            float alpha2 = expf(-2.0f * M_PI * cutoff2 / oversampleRate);
            a2 = 1.0f - alpha2;
            b2 = alpha2;

            float cutoff3 = cutoff2 * 0.55f; // Third pole for extra rejection
            float alpha3 = expf(-2.0f * M_PI * cutoff3 / oversampleRate);
            a3 = 1.0f - alpha3;
            b3 = alpha3;
        }

        void reset() {
            prevInput = 0.0f;
            lp1 = 0.0f;
            lp2 = 0.0f;
            lp3 = 0.0f;
        }

        float filter(float input) {
            if (bypass) {
                return input;
            }
            lp1 = a1 * input + b1 * lp1;
            lp2 = a2 * lp1 + b2 * lp2;
            lp3 = a3 * lp2 + b3 * lp3;
            return lp3;
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
    int currentDistortionType = 0;
    // Smoothed distortion value for LED color calculation (same value LEDs use)
    float smoothed_distortion_for_leds = 0.0f;

    // Audio-rate smoothing for control CVs
    float smoothed_drive = 0.0f;
    float smoothed_mix = 0.0f;
    float smoothed_type = 0.0f;
    bool smoothersInitialized = false;

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

    enum DisplayOverride {
        DISPLAY_NONE = 0,
        DISPLAY_DIST,
        DISPLAY_DRIVE,
        DISPLAY_MIX,
        DISPLAY_DIST_CV,
        DISPLAY_DRIVE_CV,
        DISPLAY_MIX_CV
    };

    int displayOverride = DISPLAY_NONE;
    float displayOverrideSeconds = 0.0f;
    float displayDistValue = 0.0f;
    float displayDriveValue = 0.0f;
    float displayMixValue = 0.0f;
    float displayDistAttValue = 0.0f;
    float displayDriveAttValue = 0.0f;
    float displayMixAttValue = 0.0f;
    float lastDisplayDist = 0.0f;
    float lastDisplayDrive = 0.0f;
    float lastDisplayMix = 0.0f;
    float lastDisplayDistAtt = 0.0f;
    float lastDisplayDriveAtt = 0.0f;
    float lastDisplayMixAtt = 0.0f;
    bool displayInitialized = false;

    // Cached smoothing coefficients (updated on sample-rate changes)
    float displaySmoothFast = 0.0f;
    float displaySmoothSlow = 0.0f;
    float controlSmoothCoeff = 0.0f;
    float typeSmoothCoeff = 0.0f;
    float levelSmoothCoeff = 0.0f;
    float makeupSmoothCoeff = 0.0f;

    // Sidechain mode (context menu): 0=Enhancement, 1=Ducking, 2=Direct
    int sidechainMode = 0;
    int oversampleFactor = 4;  // Increased from 2x to 4x for better aliasing suppression
    float currentSampleRate = 44100.0f;

    Chiaroscuro() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        shapetaker::ParameterHelper::configGain(this, VCA_PARAM, "vca gain");
        shapetaker::ParameterHelper::configSwitch(this, TYPE_PARAM, "dist type",
            {"hard clip", "tube sat", "wave fold", "bit crush", "destroy", "ring mod"}, 0);
        shapetaker::ParameterHelper::configGain(this, DIST_PARAM, "dist %");
        shapetaker::ParameterHelper::configAttenuverter(this, DIST_ATT_PARAM, "dist cv");
        shapetaker::ParameterHelper::configDrive(this, DRIVE_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, DRIVE_ATT_PARAM, "drive cv");
        shapetaker::ParameterHelper::configMix(this, MIX_PARAM);
        shapetaker::ParameterHelper::configAttenuverter(this, MIX_ATT_PARAM, "mix cv");
        shapetaker::ParameterHelper::configToggle(this, LINK_PARAM, "link L/R channels");
        shapetaker::ParameterHelper::configToggle(this, RESPONSE_PARAM, "vca resp: lin/exp");
        configParam(WIDTH_PARAM, -1.0f, 1.0f, 0.0f, "stereo width", "%", 0.f, 100.f);
        paramQuantities[WIDTH_PARAM]->description = "stereo field adj: -100% = mono, 0% = normal, +100% = wide";
        
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_L_INPUT, "L");
        shapetaker::ParameterHelper::configAudioInput(this, AUDIO_R_INPUT, "R");
        shapetaker::ParameterHelper::configCVInput(this, VCA_CV_INPUT, "vca cv");
        shapetaker::ParameterHelper::configAudioInput(this, SIDECHAIN_INPUT, "sidechain detect");
        shapetaker::ParameterHelper::configCVInput(this, TYPE_CV_INPUT, "dist type");
        shapetaker::ParameterHelper::configCVInput(this, DIST_CV_INPUT, "dist amt");
        shapetaker::ParameterHelper::configCVInput(this, DRIVE_CV_INPUT, "drive amt");
        shapetaker::ParameterHelper::configCVInput(this, MIX_CV_INPUT, "mix control");
        
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_L_OUTPUT, "L");
        shapetaker::ParameterHelper::configAudioOutput(this, AUDIO_R_OUTPUT, "R");
        // Initialize distortion smoothing - fast enough to be responsive but slow enough to avoid clicks
        distortion_slew.setRiseFall(1000.f, 1000.f);

        currentSampleRate = APP->engine->getSampleRate();
        detector.setTiming(10.0f, 200.0f, currentSampleRate);
        updateSmoothingCoeffs();
        configureOversampling();
        resetLevelTracking();
        resetSmoothers();

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }
    
    void onSampleRateChange() override {
        currentSampleRate = APP->engine->getSampleRate();
        detector.setTiming(10.0f, 200.0f, currentSampleRate);
        updateSmoothingCoeffs();
        configureOversampling();
        resetLevelTracking();
        resetSmoothers();
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

    void resetSmoothers() {
        smoothersInitialized = false;
        smoothed_drive = 0.0f;
        smoothed_mix = 0.0f;
        smoothed_type = 0.0f;
    }

    void updateSmoothingCoeffs() {
        float sampleTime = 1.0f / std::max(currentSampleRate, 1.0f);
        auto coeffForHz = [sampleTime](float hz) {
            return 1.0f - expf(-2.0f * M_PI * hz * sampleTime);
        };
        displaySmoothFast = coeffForHz(15.0f);
        displaySmoothSlow = coeffForHz(2.0f);
        controlSmoothCoeff = coeffForHz(60.0f);  // ~60 Hz for CV smoothing
        typeSmoothCoeff = coeffForHz(50.0f);     // ~50 Hz to limit upper response to ~200 Hz
        levelSmoothCoeff = displaySmoothFast;    // 15 Hz
        makeupSmoothCoeff = coeffForHz(6.0f);
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
        // Update polyphonic channel count and set outputs (use max of L/R for full stereo poly)
        int channels = polyProcessor.updateChannels(
            {inputs[AUDIO_L_INPUT], inputs[AUDIO_R_INPUT]},
            {outputs[AUDIO_L_OUTPUT], outputs[AUDIO_R_OUTPUT]});
        
        // Link switch state
        bool linked = params[LINK_PARAM].getValue() > 0.5f;
        
        // Sidechain processing (shared across all voices)
        float sidechain = inputs[SIDECHAIN_INPUT].isConnected() ? 
            inputs[SIDECHAIN_INPUT].getVoltage() : 0.0f;
        sidechain = clamp(fabsf(sidechain) * 0.1f, 0.0f, 1.0f);
        
        float sc_env = detector.process(sidechain);
        lights[SIDECHAIN_LED].setBrightness(sc_env);
        
        // Global parameters (shared across all voices)
        // Drive parameter with CV (smoothed to avoid zippering)
        float driveTarget = params[DRIVE_PARAM].getValue();
        if (inputs[DRIVE_CV_INPUT].isConnected()) {
            float cv_amount = params[DRIVE_ATT_PARAM].getValue(); // -1.0 to +1.0
            float cv_voltage = inputs[DRIVE_CV_INPUT].getVoltage(); // Typically 0-10V or -5V to +5V
            driveTarget += (cv_voltage / 10.0f) * cv_amount; // 10V CV = 100% parameter change when attenuverter = 100%
        }
        driveTarget = clamp(driveTarget, 0.0f, 1.0f);

        // Mix parameter with CV (smoothed to avoid zippering)
        float mixTarget = params[MIX_PARAM].getValue();
        if (inputs[MIX_CV_INPUT].isConnected()) {
            float cv_amount = params[MIX_ATT_PARAM].getValue(); // -1.0 to +1.0
            float cv_voltage = inputs[MIX_CV_INPUT].getVoltage(); // Typically 0-10V or -5V to +5V
            mixTarget += (cv_voltage / 10.0f) * cv_amount; // 10V CV = 100% parameter change when attenuverter = 100%
        }
        mixTarget = clamp(mixTarget, 0.0f, 1.0f);
        
        // Distortion type with CV (light smoothing to prevent chatter)
        float typeTarget = params[TYPE_PARAM].getValue();
        if (inputs[TYPE_CV_INPUT].isConnected()) {
            float cv = inputs[TYPE_CV_INPUT].getVoltage() * 0.1f;
            typeTarget = params[TYPE_PARAM].getValue() + cv * 6.0f;
        }
        typeTarget = clamp(typeTarget, 0.0f, 5.0f);

        if (!smoothersInitialized) {
            smoothed_drive = driveTarget;
            smoothed_mix = mixTarget;
            smoothed_type = typeTarget;
            smoothersInitialized = true;
        } else {
            smoothed_drive += controlSmoothCoeff * (driveTarget - smoothed_drive);
            smoothed_mix += controlSmoothCoeff * (mixTarget - smoothed_mix);
            smoothed_type += typeSmoothCoeff * (typeTarget - smoothed_type);
        }

        float drive = clamp(smoothed_drive, 0.0f, 1.0f);
        float mix = clamp(smoothed_mix, 0.0f, 1.0f);
        int distortion_type = clamp((int)std::round(smoothed_type), 0, 5);
        currentDistortionType = distortion_type;
        
        // Distortion amount parameter with CV
        float dist_amount = params[DIST_PARAM].getValue();
        if (inputs[DIST_CV_INPUT].isConnected()) {
            float cv_amount = params[DIST_ATT_PARAM].getValue(); // -1.0 to +1.0
            float cv_voltage = inputs[DIST_CV_INPUT].getVoltage(); // Typically 0-10V or -5V to +5V
            dist_amount += (cv_voltage / 10.0f) * cv_amount; // 10V CV = 100% parameter change when attenuverter = 100%
        }
        dist_amount = clamp(dist_amount, 0.0f, 1.0f);

        // Cache values for on-screen display
        displayDistValue = dist_amount;
        displayDriveValue = drive;
        displayMixValue = mix;
        displayDistAttValue = params[DIST_ATT_PARAM].getValue();
        displayDriveAttValue = params[DRIVE_ATT_PARAM].getValue();
        displayMixAttValue = params[MIX_ATT_PARAM].getValue();

        // Momentary display when values change
        const float displayHoldSeconds = 0.9f;
        const float displayTriggerThreshold = 0.0025f;
        if (!displayInitialized) {
            lastDisplayDist = displayDistValue;
            lastDisplayDrive = displayDriveValue;
            lastDisplayMix = displayMixValue;
            lastDisplayDistAtt = displayDistAttValue;
            lastDisplayDriveAtt = displayDriveAttValue;
            lastDisplayMixAtt = displayMixAttValue;
            displayInitialized = true;
        }
        displayOverrideSeconds = fmaxf(0.0f, displayOverrideSeconds - args.sampleTime);

        if (fabsf(displayDistValue - lastDisplayDist) > displayTriggerThreshold) {
            displayOverride = DISPLAY_DIST;
            displayOverrideSeconds = displayHoldSeconds;
            lastDisplayDist = displayDistValue;
        }
        if (fabsf(displayDriveValue - lastDisplayDrive) > displayTriggerThreshold) {
            displayOverride = DISPLAY_DRIVE;
            displayOverrideSeconds = displayHoldSeconds;
            lastDisplayDrive = displayDriveValue;
        }
        if (fabsf(displayMixValue - lastDisplayMix) > displayTriggerThreshold) {
            displayOverride = DISPLAY_MIX;
            displayOverrideSeconds = displayHoldSeconds;
            lastDisplayMix = displayMixValue;
        }
        if (fabsf(displayDistAttValue - lastDisplayDistAtt) > displayTriggerThreshold) {
            displayOverride = DISPLAY_DIST_CV;
            displayOverrideSeconds = displayHoldSeconds;
            lastDisplayDistAtt = displayDistAttValue;
        }
        if (fabsf(displayDriveAttValue - lastDisplayDriveAtt) > displayTriggerThreshold) {
            displayOverride = DISPLAY_DRIVE_CV;
            displayOverrideSeconds = displayHoldSeconds;
            lastDisplayDriveAtt = displayDriveAttValue;
        }
        if (fabsf(displayMixAttValue - lastDisplayMixAtt) > displayTriggerThreshold) {
            displayOverride = DISPLAY_MIX_CV;
            displayOverrideSeconds = displayHoldSeconds;
            lastDisplayMixAtt = displayMixAttValue;
        }

        if (displayOverrideSeconds <= 0.0f) {
            displayOverride = DISPLAY_NONE;
        }

        // Store processed values for UI display (pixel ring) using smoothed controls
        processed_distortion = dist_amount;
        processed_drive = drive;
        processed_mix = mix;

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
        processed_distortion = smoothed_distortion;
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

        float dist_smooth_factor = (dist_rate > rate_threshold) ? displaySmoothSlow : displaySmoothFast;
        float drive_smooth_factor = (drive_rate > rate_threshold) ? displaySmoothSlow : displaySmoothFast;
        float mix_smooth_factor = (mix_rate > rate_threshold) ? displaySmoothSlow : displaySmoothFast;

        smoothed_distortion_display += (processed_distortion - smoothed_distortion_display) * dist_smooth_factor;
        smoothed_drive_display += (processed_drive - smoothed_drive_display) * drive_smooth_factor;
        smoothed_mix_display += (processed_mix - smoothed_mix_display) * mix_smooth_factor;

        // The actual distortion amount used in processing - use effective drive for sidechain mode
        float distortion_amount = smoothed_distortion * effective_drive;

        // Distortion LED: product-based (reflects actual distortion heard) plus
        // a small peak hint so any single knob at ~half shows a subtle glow
        float product = smoothed_distortion_display * smoothed_drive_display * smoothed_mix_display;
        float peak = std::max(smoothed_distortion_display, std::max(smoothed_drive_display, smoothed_mix_display));
        float dist_intensity = clamp(product + peak * peak * 0.1f, 0.0f, 1.0f);
        // Boost brightness while keeping darker base hues; allow >1.0 pre-clamp for stronger glow.
        float dist_led_brightness = std::pow(dist_intensity, 0.55f) * 1.8f;

        // Color palette for each distortion type (normalized RGB)
        float dist_r = 0.26f;
        float dist_g = 0.34f;
        float dist_b = 0.46f;
        getDistortionTypeColor(distortion_type, dist_r, dist_g, dist_b);

        // Apply brightness to the type color, clamping per channel for LED intensity
        currentLEDColor = shapetaker::RGBColor(
            clamp(dist_r * dist_led_brightness, 0.0f, 1.0f),
            clamp(dist_g * dist_led_brightness, 0.0f, 1.0f),
            clamp(dist_b * dist_led_brightness, 0.0f, 1.0f));

        lights[DIST_LED_R].setBrightness(currentLEDColor.r);
        lights[DIST_LED_G].setBrightness(currentLEDColor.g);
        lights[DIST_LED_B].setBrightness(currentLEDColor.b);
        
        // VCA gain calculation (polyphonic CV support)
        float base_vca_gain = params[VCA_PARAM].getValue();
        bool exponential_response = params[RESPONSE_PARAM].getValue() > 0.5f;
        
        const float normalizationVoltage = NOMINAL_LEVEL;
        const float invNormalization = 1.0f / normalizationVoltage;
        const float minLevel = 1e-4f;

        float gain_led_peak = 0.0f;

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
            cleanLevelL[ch] += (cleanAbsL - cleanLevelL[ch]) * levelSmoothCoeff;
            wetLevelL[ch] += (wetAbsL - wetLevelL[ch]) * levelSmoothCoeff;

            float cleanAbsR = fabsf(vca_r);
            float wetAbsR = fabsf(wet_r);
            cleanLevelR[ch] += (cleanAbsR - cleanLevelR[ch]) * levelSmoothCoeff;
            wetLevelR[ch] += (wetAbsR - wetLevelR[ch]) * levelSmoothCoeff;
            gain_led_peak = fmaxf(gain_led_peak, fmaxf(cleanLevelL[ch], cleanLevelR[ch]));

            float desiredGainL = 1.0f;
            float desiredGainR = 1.0f;

            if (wetLevelL[ch] > minLevel) {
                desiredGainL = (cleanLevelL[ch] > minLevel) ? cleanLevelL[ch] / wetLevelL[ch] : 1.0f;
            }
            desiredGainL = clamp(desiredGainL, 0.25f, 4.0f);
            makeupGainL[ch] += (desiredGainL - makeupGainL[ch]) * makeupSmoothCoeff;

            if (wetLevelR[ch] > minLevel) {
                desiredGainR = (cleanLevelR[ch] > minLevel) ? cleanLevelR[ch] / wetLevelR[ch] : 1.0f;
            }
            desiredGainR = clamp(desiredGainR, 0.25f, 4.0f);
            makeupGainR[ch] += (desiredGainR - makeupGainR[ch]) * makeupSmoothCoeff;

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

        // Gain LED: Show effective VCA gain (knob + CV) with teal color
        float gain_cv_level = 0.0f;
        if (inputs[VCA_CV_INPUT].isConnected()) {
            // Use maximum effective gain across all channels
            for (int ch = 0; ch < channels; ch++) {
                float cv = inputs[VCA_CV_INPUT].getPolyVoltage(ch) * 0.1f; // 10V -> 1.0
                cv = clamp(cv, -1.0f, 1.0f);
                float effective_gain = clamp(base_vca_gain + cv, 0.0f, 1.0f);
                gain_cv_level = fmaxf(gain_cv_level, effective_gain);
            }
        } else {
            // No CV connected - show knob position (0-1 range maps directly)
            gain_cv_level = clamp(base_vca_gain, 0.0f, 1.0f);
        }

        // Teal color for gain LED (matches Channel A theme)
        // Use sqrt for better low-end visibility
        float gain_brightness = clamp(std::sqrt(gain_cv_level), 0.0f, 1.0f);
        lights[GAIN_LED_R].setSmoothBrightness(0.0f, args.sampleTime);
        lights[GAIN_LED_G].setSmoothBrightness(gain_brightness, args.sampleTime);
        lights[GAIN_LED_B].setSmoothBrightness(gain_brightness * 0.7f, args.sampleTime);
    }
};

// Unified LCD + selector base unit for distortion type
struct DistortionTypeLCD : Widget {
    Chiaroscuro* module = nullptr;
    Rect screenRect;
    float slotTop = 0.f;
    float slotHeight = 0.f;

    DistortionTypeLCD(Chiaroscuro* mod, const Rect& screenRectPx, float slotTopPx, float slotHeightPx)
        : module(mod), screenRect(screenRectPx), slotTop(slotTopPx), slotHeight(slotHeightPx) {}

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        float w = box.size.x;
        float h = box.size.y;

        nvgSave(vg);

        // Unified bezel (aged, chunkier)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, w, h, 3.2f);
        NVGpaint bezel = nvgLinearGradient(vg, 0, 0, 0, h,
            nvgRGBA(52, 46, 38, 255),
            nvgRGBA(8, 6, 5, 255));
        nvgFillPaint(vg, bezel);
        nvgFill(vg);

        // Bezel edge highlight
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.6f, 0.6f, w - 1.2f, h - 1.2f, 2.8f);
        nvgStrokeColor(vg, nvgRGBA(220, 205, 170, 70));
        nvgStrokeWidth(vg, 0.95f);
        nvgStroke(vg);

        // Inner recess
        const float inset = 1.6f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset, inset, w - inset * 2.f, h - inset * 2.f, 2.1f);
        nvgFillColor(vg, nvgRGBA(12, 10, 8, 220));
        nvgFill(vg);

        // Inner shadow lip
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset + 0.3f, inset + 0.3f, w - (inset + 0.3f) * 2.f, h - (inset + 0.3f) * 2.f, 1.9f);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 140));
        nvgStrokeWidth(vg, 0.9f);
        nvgStroke(vg);

        // Bezel screws
        float screwR = 0.8f;
        float screwInset = 2.2f;
        float screwXs[2] = {screwInset, w - screwInset};
        float screwYs[2] = {screwInset, h - screwInset};
        for (int iy = 0; iy < 2; ++iy) {
            for (int ix = 0; ix < 2; ++ix) {
                float sx = screwXs[ix];
                float sy = screwYs[iy];
                nvgBeginPath(vg);
                nvgCircle(vg, sx, sy, screwR);
                nvgFillColor(vg, nvgRGBA(130, 120, 102, 255));
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgCircle(vg, sx, sy, screwR);
                nvgStrokeColor(vg, nvgRGBA(60, 52, 42, 180));
                nvgStrokeWidth(vg, 0.4f);
                nvgStroke(vg);
                nvgBeginPath(vg);
                nvgMoveTo(vg, sx - screwR * 0.6f, sy);
                nvgLineTo(vg, sx + screwR * 0.6f, sy);
                nvgStrokeColor(vg, nvgRGBA(70, 60, 50, 200));
                nvgStrokeWidth(vg, 0.35f);
                nvgStroke(vg);
            }
        }

        // Screen area
        float sw = screenRect.size.x;
        float sh = screenRect.size.y;
        float sx = screenRect.pos.x;
        float sy = screenRect.pos.y;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sx, sy, sw, sh, 1.6f);
        NVGpaint screen = nvgLinearGradient(vg, 0, sy, 0, sy + sh,
            nvgRGBA(12, 20, 16, 255),
            nvgRGBA(6, 10, 8, 255));
        nvgFillPaint(vg, screen);
        nvgFill(vg);

        // Subtle inner bezel edge
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sx + 0.6f, sy + 0.6f, sw - 1.2f, sh - 1.2f, 1.2f);
        nvgStrokeColor(vg, nvgRGBA(70, 60, 45, 70));
        nvgStrokeWidth(vg, 0.6f);
        nvgStroke(vg);

        // Scanlines
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 14));
        nvgStrokeWidth(vg, 1.f);
        for (float y = sy + 1.2f; y < sy + sh - 1.f; y += 2.0f) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, sx + 1.f, y);
            nvgLineTo(vg, sx + sw - 1.f, y);
            nvgStroke(vg);
        }

        // Vertical phosphor mask (very subtle)
        nvgStrokeColor(vg, nvgRGBA(170, 200, 170, 12));
        nvgStrokeWidth(vg, 0.6f);
        for (float x = sx + 1.2f; x < sx + sw - 1.2f; x += 2.0f) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, sy + 1.f);
            nvgLineTo(vg, x, sy + sh - 1.f);
            nvgStroke(vg);
        }

        // Light speckle noise (static)
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 10));
        for (int i = 0; i < 26; ++i) {
            float fx = fmodf(i * 7.3f, std::max(1.f, sw - 2.f)) + sx + 1.f;
            float fy = fmodf(i * 11.1f, std::max(1.f, sh - 2.f)) + sy + 1.f;
            nvgBeginPath(vg);
            nvgCircle(vg, fx, fy, 0.4f);
            nvgFill(vg);
        }

        // Glass highlight
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sx + 0.8f, sy + 0.6f, sw * 0.6f, sh * 0.35f, 1.2f);
        NVGpaint glass = nvgLinearGradient(vg, sx, sy, sx + sw * 0.6f, sy + sh * 0.4f,
            nvgRGBA(160, 170, 120, 22),
            nvgRGBA(160, 170, 120, 0));
        nvgFillPaint(vg, glass);
        nvgFill(vg);

        // Phosphor bloom (warm)
        NVGpaint bloom = nvgRadialGradient(
            vg,
            sx + sw * 0.5f, sy + sh * 0.5f,
            std::min(sw, sh) * 0.12f,
            std::min(sw, sh) * 0.45f,
            nvgRGBA(200, 170, 90, 35),
            nvgRGBA(200, 170, 90, 0));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sx, sy, sw, sh, 1.6f);
        nvgFillPaint(vg, bloom);
        nvgFill(vg);

        // Vignette edges
        NVGpaint vignette = nvgRadialGradient(
            vg,
            sx + sw * 0.5f, sy + sh * 0.5f,
            std::min(sw, sh) * 0.2f,
            std::min(sw, sh) * 0.65f,
            nvgRGBA(0, 0, 0, 0),
            nvgRGBA(0, 0, 0, 70));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sx, sy, sw, sh, 1.6f);
        nvgFillPaint(vg, vignette);
        nvgFill(vg);

        // Slot channel (vintage instrument)
        float slotInset = w * 0.08f;
        float slotW = w - slotInset * 2.f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, slotInset, slotTop, slotW, slotHeight, slotHeight * 0.5f);
        NVGpaint slotBase = nvgLinearGradient(vg, 0, slotTop, 0, slotTop + slotHeight,
            nvgRGBA(26, 28, 22, 255),
            nvgRGBA(10, 12, 9, 255));
        nvgFillPaint(vg, slotBase);
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, slotInset, slotTop, slotW, slotHeight, slotHeight * 0.5f);
        nvgStrokeColor(vg, nvgRGBA(90, 96, 80, 120));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, slotInset + 0.5f, slotTop + 0.3f, slotW - 1.0f, slotHeight - 0.6f, slotHeight * 0.4f);
        nvgStrokeColor(vg, nvgRGBA(190, 196, 180, 45));
        nvgStrokeWidth(vg, 0.35f);
        nvgStroke(vg);

        // Get type + color
        int typeIndex = 0;
        if (module) {
            typeIndex = clamp(module->currentDistortionType, 0, 5);
        }
        const char* label = getDistortionTypeName(typeIndex);
        char overrideLabel[32] = {};
        bool showingOverride = false;
        if (module && module->displayOverrideSeconds > 0.0f && module->displayOverride != Chiaroscuro::DISPLAY_NONE) {
            float value = 0.0f;
            switch (module->displayOverride) {
                case Chiaroscuro::DISPLAY_DIST:
                    value = module->displayDistValue;
                    std::snprintf(overrideLabel, sizeof(overrideLabel), "DIST %d%%", (int)std::round(value * 100.f));
                    break;
                case Chiaroscuro::DISPLAY_DRIVE:
                    value = module->displayDriveValue;
                    std::snprintf(overrideLabel, sizeof(overrideLabel), "DRIVE %d%%", (int)std::round(value * 100.f));
                    break;
                case Chiaroscuro::DISPLAY_MIX:
                    value = module->displayMixValue;
                    std::snprintf(overrideLabel, sizeof(overrideLabel), "MIX %d%%", (int)std::round(value * 100.f));
                    break;
                case Chiaroscuro::DISPLAY_DIST_CV:
                    value = module->displayDistAttValue;
                    std::snprintf(overrideLabel, sizeof(overrideLabel), "DIST CV %c%d%%",
                        (value >= 0.f ? '+' : '-'), (int)std::round(std::fabs(value) * 100.f));
                    break;
                case Chiaroscuro::DISPLAY_DRIVE_CV:
                    value = module->displayDriveAttValue;
                    std::snprintf(overrideLabel, sizeof(overrideLabel), "DRIVE CV %c%d%%",
                        (value >= 0.f ? '+' : '-'), (int)std::round(std::fabs(value) * 100.f));
                    break;
                case Chiaroscuro::DISPLAY_MIX_CV:
                    value = module->displayMixAttValue;
                    std::snprintf(overrideLabel, sizeof(overrideLabel), "MIX CV %c%d%%",
                        (value >= 0.f ? '+' : '-'), (int)std::round(std::fabs(value) * 100.f));
                    break;
                default:
                    break;
            }
            if (overrideLabel[0] != '\0') {
                label = overrideLabel;
                showingOverride = true;
            }
        }

        float r = 0.3f, g = 0.45f, b = 0.6f;
        getDistortionTypeColor(typeIndex, r, g, b);
        float boost = 1.7f;
        NVGcolor textColor = nvgRGBAf(
            clamp(r * boost, 0.f, 1.f),
            clamp(g * boost, 0.f, 1.f),
            clamp(b * boost, 0.f, 1.f),
            1.f);

        // Text
        if (APP && APP->window && APP->window->uiFont) {
            nvgFontFaceId(vg, APP->window->uiFont->handle);
        }
        float valueFontSize = sh * 0.55f;
        float titleFontSize = sh * 0.68f;
        nvgFontSize(vg, showingOverride ? valueFontSize : titleFontSize);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        float maxW = sw - 6.f;
        if (showingOverride) {
            const char* referenceLabel = "DRIVE CV +100%";
            float refBounds[4];
            nvgTextBounds(vg, 0.f, 0.f, referenceLabel, nullptr, refBounds);
            float refW = refBounds[2] - refBounds[0];
            if (refW > maxW && refW > 0.0f) {
                float scale = maxW / refW;
                nvgFontSize(vg, valueFontSize * scale);
            }
        } else {
            const char* referenceTitle = "WAVE FOLD";
            float refBounds[4];
            nvgTextBounds(vg, 0.f, 0.f, referenceTitle, nullptr, refBounds);
            float refW = refBounds[2] - refBounds[0];
            if (refW > maxW && refW > 0.0f) {
                float scale = maxW / refW;
                nvgFontSize(vg, titleFontSize * scale);
            }
        }

        float bounds[4];
        nvgTextBounds(vg, 0.f, 0.f, label, nullptr, bounds);
        float textW = bounds[2] - bounds[0];

        float cx = sx + sw * 0.5f;
        float cy = sy + sh * 0.5f;

        // Keep text within the screen area
        nvgScissor(vg, sx + 0.6f, sy + 0.6f, sw - 1.2f, sh - 1.2f);

        // Soft phosphor glow (embedded)
        nvgFontBlur(vg, 3.6f);
        nvgFillColor(vg, nvgRGBAf(textColor.r * 0.8f + 0.2f, textColor.g * 0.8f + 0.2f, textColor.b * 0.6f, 0.38f));
        nvgText(vg, cx - 0.15f, cy + 0.25f, label, nullptr);
        nvgText(vg, cx + 0.15f, cy + 0.05f, label, nullptr);

        // Slight horizontal bleed
        nvgFontBlur(vg, 1.2f);
        nvgFillColor(vg, nvgRGBAf(textColor.r * 0.95f, textColor.g * 0.95f, textColor.b * 0.85f, 0.32f));
        nvgText(vg, cx + 0.6f, cy + 0.1f, label, nullptr);

        // Grainy core text (multiple jittered passes)
        nvgFontBlur(vg, 0.6f);
        nvgFillColor(vg, nvgRGBAf(textColor.r, textColor.g, textColor.b, 0.72f));
        nvgText(vg, cx + 0.1f, cy + 0.2f, label, nullptr);
        nvgText(vg, cx - 0.1f, cy + 0.15f, label, nullptr);
        nvgText(vg, cx + 0.0f, cy + 0.25f, label, nullptr);

        // Final crisp layer (still a touch soft)
        nvgFontBlur(vg, 0.15f);
        nvgFillColor(vg, nvgRGBAf(textColor.r, textColor.g, textColor.b, 0.88f));
        nvgText(vg, cx, cy + 0.2f, label, nullptr);

        // Phosphor speckle within text bounds
        float textH = bounds[3] - bounds[1];
        float textBoxW = (textW > 0.f) ? textW : (sw * 0.6f);
        float textBoxH = (textH > 0.f) ? textH : (sh * 0.6f);
        float tx0 = cx - textBoxW * 0.5f;
        float ty0 = cy - textBoxH * 0.5f;
        nvgFillColor(vg, nvgRGBAf(textColor.r, textColor.g, textColor.b, 0.18f));
        for (int i = 0; i < 14; ++i) {
            float fx = tx0 + fmodf(i * 7.3f, std::max(1.f, textBoxW));
            float fy = ty0 + fmodf(i * 5.9f, std::max(1.f, textBoxH));
            nvgBeginPath(vg);
            nvgRect(vg, fx, fy, 0.6f, 0.4f);
            nvgFill(vg);
        }

        nvgResetScissor(vg);

        nvgRestore(vg);
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

    // Use fixed-density leather mapping to avoid horizontal stretch on
    // wider panels; blend an offset pass to soften repeat seams.
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;
            nvgSave(args.vg);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
            nvgFill(args.vg);
            nvgRestore(args.vg);
        }
        ModuleWidget::draw(args);

        // Draw a black inner frame to fully mask any edge tinting
        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }

    ChiaroscuroWidget(Chiaroscuro* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;

        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        // Create positioning helpers from SVG panel
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Chiaroscuro.svg");
        auto centerPx = LayoutHelper::createCenterPxHelper(svgPath);

        // Jewel LEDs
        addChild(createLightCentered<JewelLEDLarge>(
            centerPx("gain_led", 9.3945856f, 19.047686f), module, Chiaroscuro::GAIN_LED_R));
        addChild(createLightCentered<JewelLEDLarge>(
            centerPx("dist_amt_led", 52.005203f, 19.047686f), module, Chiaroscuro::DIST_LED_R));
        
        // Audio I/O - BNC connectors for vintage oscilloscope look
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio-in-l", 10.691628f, 114.48771f), module, Chiaroscuro::AUDIO_L_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio-in-r", 24.030479f, 114.45824f), module, Chiaroscuro::AUDIO_R_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio-out-l", 37.369331f, 114.45824f), module, Chiaroscuro::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio-out-r", 50.708183f, 114.48771f), module, Chiaroscuro::AUDIO_R_OUTPUT));
        
        // Main VCA knob — Vintage XLarge (27mm)
        addParam(createParamCentered<ShapetakerKnobVintageXLarge>(centerPx("vca-knob", 30.699905f, 20.462957f), module, Chiaroscuro::VCA_PARAM));
        
        // VCA CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("vca-cv", 10.691628f, 101.572f), module, Chiaroscuro::VCA_CV_INPUT));
        
        // Linear/Exponential response switch
        Vec responseCenter = centerPx("lin-exp-switch", 52.005203f, 35.89994f);
        auto* responseSwitch = createParamCentered<ShapetakerDarkToggle>(responseCenter, module, Chiaroscuro::RESPONSE_PARAM);
        addParam(responseSwitch);
        
        // Link switch
        Vec linkCenter = centerPx("lin-lr-switch", 9.3945856f, 35.89994f);
        auto* linkSwitch = createParamCentered<ShapetakerDarkToggle>(linkCenter, module, Chiaroscuro::LINK_PARAM);
        addParam(linkSwitch);
        
        // Sidechain input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("sidechain-detect-cv", 50.708183f, 101.572f), module, Chiaroscuro::SIDECHAIN_INPUT));

        // Distortion type CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("dist-type-cv", 30.699905f, 101.572f), module, Chiaroscuro::TYPE_CV_INPUT));
        
        // Distortion knob — Vintage SmallMedium (15mm)
        addParam(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("dist-knob", 10.691628f, 52.975525f), module, Chiaroscuro::DIST_PARAM));

        // Distortion CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("dist-cv", 10.691628f, 88.656288f), module, Chiaroscuro::DIST_CV_INPUT));

        // Drive knob — Vintage SmallMedium (15mm)
        addParam(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("drive-knob", 30.699905f, 52.975525f), module, Chiaroscuro::DRIVE_PARAM));

        // Drive CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("drive-cv", 30.699905f, 88.656288f), module, Chiaroscuro::DRIVE_CV_INPUT));

        // Mix knob — Vintage SmallMedium (15mm)
        addParam(createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("mix-knob", 50.708183f, 52.975525f), module, Chiaroscuro::MIX_PARAM));

        // ATTENUVERTERS (knobs)
        // Distortion attenuverter
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("dist-atten", 10.691628f, 70.16468f), module, Chiaroscuro::DIST_ATT_PARAM));

        // Drive attenuverter
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("drive-atten", 30.699905f, 70.16468f), module, Chiaroscuro::DRIVE_ATT_PARAM));

        // Mix attenuverter
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("mix-atten", 50.708183f, 70.16468f), module, Chiaroscuro::MIX_ATT_PARAM));

        // Mix CV input
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("mix-cv", 50.708183f, 88.656288f), module, Chiaroscuro::MIX_CV_INPUT));

        // Distortion type selector: analog blade switch with 6 positions
        Vec selectorCenter = centerPx("dist-type-select", 30.699905f, 39.287804f);
        auto* selector = createParamCentered<ShapetakerBladeDistortionSelector>(selectorCenter, module, Chiaroscuro::TYPE_PARAM);
        selector->drawDetents = true;
        addParam(selector);
    }
};

Model* modelChiaroscuro = createModel<Chiaroscuro, ChiaroscuroWidget>("Chiaroscuro");
