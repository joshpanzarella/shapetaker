#include "plugin.hpp"
#include <cmath>
#include <random>
#include <limits>
#include "involution/liquid_filter.hpp"

struct Involution : Module {
    enum ParamId {
        CUTOFF_A_PARAM,
        RESONANCE_A_PARAM,
        CUTOFF_B_PARAM,
        RESONANCE_B_PARAM,
        // New magical parameters
        CHAOS_AMOUNT_PARAM,
        CHAOS_RATE_PARAM,
        FILTER_MORPH_PARAM,
        // Link switches
        LINK_CUTOFF_PARAM,
        LINK_RESONANCE_PARAM,
        // Attenuverters for CV inputs
        CUTOFF_A_ATTEN_PARAM,
        RESONANCE_A_ATTEN_PARAM,
        CUTOFF_B_ATTEN_PARAM,
        RESONANCE_B_ATTEN_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_A_INPUT,
        AUDIO_B_INPUT,
        CUTOFF_A_CV_INPUT,
        RESONANCE_A_CV_INPUT,
        CUTOFF_B_CV_INPUT,
        RESONANCE_B_CV_INPUT,
        CHAOS_CV_INPUT,
        CHAOS_RATE_CV_INPUT,
        FILTER_MORPH_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_A_OUTPUT,
        AUDIO_B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        CHAOS_LIGHT,
        CHAOS_LIGHT_GREEN,
        CHAOS_LIGHT_BLUE,
        LIGHTS_LEN
    };
    // Liquid 6th-order filters - one per voice per channel
    shapetaker::dsp::VoiceArray<LiquidFilter> filtersA;
    shapetaker::dsp::VoiceArray<LiquidFilter> filtersB;

    // Parameter smoothing
    
    shapetaker::FastSmoother cutoffASmooth, cutoffBSmooth, resonanceASmooth, resonanceBSmooth;
    shapetaker::FastSmoother chaosRateSmooth;
    shapetaker::FastSmoother morphSmooth;
    
    // Parameter change tracking for bidirectional linking
    float lastCutoffA = -1.f, lastCutoffB = -1.f;
    float lastResonanceA = -1.f, lastResonanceB = -1.f;
    bool lastLinkCutoff = false, lastLinkResonance = false;

    // Smoothed values for visualizer access
    float smoothedChaosRate = 0.5f;
    float effectiveResonanceA = 0.707f;
    float effectiveResonanceB = 0.707f;
    float effectiveCutoffA = 1.0f; // Store final modulated cutoff values
    float effectiveCutoffB = 1.0f;

    Involution() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(CUTOFF_A_PARAM, 0.f, 1.f, 1.f, "Filter A Cutoff", " Hz", std::pow(2.f, 10.f), 20.f);
        configParam(RESONANCE_A_PARAM, 0.707f, 1.5f, 0.707f, "Filter A Resonance");
        configParam(CUTOFF_B_PARAM, 0.f, 1.f, 1.f, "Filter B Cutoff", " Hz", std::pow(2.f, 10.f), 20.f);
        configParam(RESONANCE_B_PARAM, 0.707f, 1.5f, 0.707f, "Filter B Resonance");
        
        // Drive / character controls
        configParam(CHAOS_AMOUNT_PARAM, 0.f, 1.f, 1.f, "Drive", "%", 0.f, 100.f);
        configParam(CHAOS_RATE_PARAM, 0.01f, 10.f, 0.5f, "Chaos LFO Rate", " Hz", 0.f, 0.f);

        // Custom parameter quantity to show real-time chaos rate including CV modulation
        struct ChaosRateQuantity : engine::ParamQuantity {
            float getDisplayValue() override {
                if (!module) return ParamQuantity::getDisplayValue();

                Involution* inv = static_cast<Involution*>(module);
                // Use the same calculation as the main process function
                float displayRate = getValue(); // Base knob value
                if (inv->inputs[Involution::CHAOS_RATE_CV_INPUT].isConnected()) {
                    float rateCv = inv->inputs[Involution::CHAOS_RATE_CV_INPUT].getVoltage();
                    displayRate += rateCv * 0.5f; // Same CV scaling as main module
                }
                displayRate = clamp(displayRate, 0.001f, 20.0f);

                return displayRate;
            }
        };

        // Replace the default param quantity with our custom one
        paramQuantities[CHAOS_RATE_PARAM] = new ChaosRateQuantity;
        paramQuantities[CHAOS_RATE_PARAM]->module = this;
        paramQuantities[CHAOS_RATE_PARAM]->paramId = CHAOS_RATE_PARAM;
        paramQuantities[CHAOS_RATE_PARAM]->minValue = 0.01f;
        paramQuantities[CHAOS_RATE_PARAM]->maxValue = 10.f;
        paramQuantities[CHAOS_RATE_PARAM]->defaultValue = 0.5f;
        paramQuantities[CHAOS_RATE_PARAM]->name = "Chaos LFO Rate";
        paramQuantities[CHAOS_RATE_PARAM]->unit = " Hz";
        configParam(FILTER_MORPH_PARAM, 0.f, 1.f, 0.f, "Filter Type Morph");

        // Link switches
        configSwitch(LINK_CUTOFF_PARAM, 0.f, 1.f, 0.f, "Link Cutoff Frequencies", {"Independent", "Linked"});
        configSwitch(LINK_RESONANCE_PARAM, 0.f, 1.f, 0.f, "Link Resonance Amounts", {"Independent", "Linked"});

        // Attenuverters for CV inputs
        configParam(CUTOFF_A_ATTEN_PARAM, -1.f, 1.f, 0.f, "Cutoff A CV Attenuverter", "%", 0.f, 100.f);
        configParam(RESONANCE_A_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance A CV Attenuverter", "%", 0.f, 100.f);
        configParam(CUTOFF_B_ATTEN_PARAM, -1.f, 1.f, 0.f, "Cutoff B CV Attenuverter", "%", 0.f, 100.f);
        configParam(RESONANCE_B_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance B CV Attenuverter", "%", 0.f, 100.f);

        configInput(AUDIO_A_INPUT, "Audio A");
        configInput(AUDIO_B_INPUT, "Audio B");
        configInput(CUTOFF_A_CV_INPUT, "Filter A Cutoff CV");
        configInput(RESONANCE_A_CV_INPUT, "Filter A Resonance CV");
        configInput(CUTOFF_B_CV_INPUT, "Filter B Cutoff CV");
        configInput(RESONANCE_B_CV_INPUT, "Filter B Resonance CV");
        configInput(CHAOS_CV_INPUT, "Drive CV (Inactive)");
        configInput(CHAOS_RATE_CV_INPUT, "Chaos Rate CV");
        configInput(FILTER_MORPH_CV_INPUT, "Filter Morph CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");

        configLight(CHAOS_LIGHT, "Drive Activity");

        // Initialize filters with default sample rate
        onSampleRateChange();
    }

    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();

        // Update all liquid filters with new sample rate
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            filtersA[v].setSampleRate(sr);
            filtersB[v].setSampleRate(sr);
        }
    }

    void process(const ProcessArgs& args) override {
        // Read link switch states
        bool linkCutoff = params[LINK_CUTOFF_PARAM].getValue() > 0.5f;
        bool linkResonance = params[LINK_RESONANCE_PARAM].getValue() > 0.5f;
        
        // Get current raw parameter values
        float currentCutoffA = params[CUTOFF_A_PARAM].getValue();
        float currentCutoffB = params[CUTOFF_B_PARAM].getValue();
        float currentResonanceA = params[RESONANCE_A_PARAM].getValue();
        float currentResonanceB = params[RESONANCE_B_PARAM].getValue();
        
        // Handle bidirectional cutoff linking
        if (linkCutoff) {
            // Check if linking was just enabled
            if (!lastLinkCutoff) {
                // Sync B to A when linking is first enabled
                params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                currentCutoffB = currentCutoffA;
            } else {
                // Check which parameter changed and sync the other
                const float epsilon = 1e-6f;
                bool aChanged = std::abs(currentCutoffA - lastCutoffA) > epsilon;
                bool bChanged = std::abs(currentCutoffB - lastCutoffB) > epsilon;
                
                if (aChanged && !bChanged) {
                    // A changed, sync B to A
                    params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                    currentCutoffB = currentCutoffA;
                } else if (bChanged && !aChanged) {
                    // B changed, sync A to B
                    params[CUTOFF_A_PARAM].setValue(currentCutoffB);
                    currentCutoffA = currentCutoffB;
                } else if (aChanged && bChanged) {
                    // Both changed (shouldn't happen normally), prefer A
                    params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                    currentCutoffB = currentCutoffA;
                }
            }
        }
        
        // Handle bidirectional resonance linking
        if (linkResonance) {
            // Check if linking was just enabled
            if (!lastLinkResonance) {
                // Sync B to A when linking is first enabled
                params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                currentResonanceB = currentResonanceA;
            } else {
                // Check which parameter changed and sync the other
                const float epsilon = 1e-6f;
                bool aChanged = std::abs(currentResonanceA - lastResonanceA) > epsilon;
                bool bChanged = std::abs(currentResonanceB - lastResonanceB) > epsilon;
                
                if (aChanged && !bChanged) {
                    // A changed, sync B to A
                    params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                    currentResonanceB = currentResonanceA;
                } else if (bChanged && !aChanged) {
                    // B changed, sync A to B
                    params[RESONANCE_A_PARAM].setValue(currentResonanceB);
                    currentResonanceA = currentResonanceB;
                } else if (aChanged && bChanged) {
                    // Both changed (shouldn't happen normally), prefer A
                    params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                    currentResonanceB = currentResonanceA;
                }
            }
        }
        
        // Store current values for next frame comparison
        lastCutoffA = currentCutoffA;
        lastCutoffB = currentCutoffB;
        lastResonanceA = currentResonanceA;
        lastResonanceB = currentResonanceB;
        lastLinkCutoff = linkCutoff;
        lastLinkResonance = linkResonance;
        
        // Apply smoothing to the synchronized values
        float cutoffA = cutoffASmooth.process(currentCutoffA, args.sampleTime);
        float cutoffB = cutoffBSmooth.process(currentCutoffB, args.sampleTime);
        float resonanceA = resonanceASmooth.process(currentResonanceA, args.sampleTime);
        float resonanceB = resonanceBSmooth.process(currentResonanceB, args.sampleTime);
        
        // Magical parameters - smoothed for immediate response
        // Drive is now fixed at its maximum value for a permanently saturated character
        constexpr float DRIVE_FIXED_NORMALIZED = 1.f;
        constexpr float DRIVE_FIXED_AMOUNT = 1.f + DRIVE_FIXED_NORMALIZED * 4.f;
        float driveLight = DRIVE_FIXED_NORMALIZED;
        float baseChaosRate = chaosRateSmooth.process(params[CHAOS_RATE_PARAM].getValue(), args.sampleTime);

        // Add CV modulation to chaos rate (additive, ±5Hz range)
        float chaosRate = baseChaosRate;
        if (inputs[CHAOS_RATE_CV_INPUT].isConnected()) {
            float rateCv = inputs[CHAOS_RATE_CV_INPUT].getVoltage(); // ±10V
            chaosRate += rateCv * 0.5f; // ±5Hz range when using ±10V CV
        }
        chaosRate = clamp(chaosRate, 0.001f, 20.0f); // Keep within reasonable bounds

        // Store smoothed chaos rate for visualizer access
        smoothedChaosRate = chaosRate;

        // Store effective resonance values for visualizer (always calculate, even without inputs)
        float displayResonanceA = resonanceA;
        float displayResonanceB = resonanceB;

        if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
            float attenA = params[RESONANCE_A_ATTEN_PARAM].getValue();
            displayResonanceA += inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(0) * attenA / 10.f;
        }
        displayResonanceA = clamp(displayResonanceA, 0.707f, 1.5f);

        if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
            float attenB = params[RESONANCE_B_ATTEN_PARAM].getValue();
            displayResonanceB += inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(0) * attenB / 10.f;
        }
        displayResonanceB = clamp(displayResonanceB, 0.707f, 1.5f);

        effectiveResonanceA = displayResonanceA;
        effectiveResonanceB = displayResonanceB;

        // Store effective cutoff values for visualizer (always calculate, even without inputs)
        float displayCutoffA = cutoffA;
        float displayCutoffB = cutoffB;

        if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
            float attenA = params[CUTOFF_A_ATTEN_PARAM].getValue();
            displayCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(0) * attenA / 10.f;
        }
        displayCutoffA = clamp(displayCutoffA, 0.f, 1.f);

        if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
            float attenB = params[CUTOFF_B_ATTEN_PARAM].getValue();
            displayCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(0) * attenB / 10.f;
        }
        displayCutoffB = clamp(displayCutoffB, 0.f, 1.f);

        effectiveCutoffA = displayCutoffA;
        effectiveCutoffB = displayCutoffB;

        [[maybe_unused]] float filterMorph = morphSmooth.process(params[FILTER_MORPH_PARAM].getValue(), args.sampleTime);
        
        // Add CV modulation to filter morph
        if (inputs[FILTER_MORPH_CV_INPUT].isConnected()) {
            float morphCv = inputs[FILTER_MORPH_CV_INPUT].getVoltage() / 10.f; // 10V -> 1.0
            filterMorph = clamp(morphCv, 0.f, 1.f);
        }

        // Determine number of polyphonic channels (up to 8)
        int channelsA = inputs[AUDIO_A_INPUT].getChannels();
        int channelsB = inputs[AUDIO_B_INPUT].getChannels();
        int channels = std::max(channelsA, channelsB);
        channels = std::min(channels, shapetaker::PolyphonicProcessor::MAX_VOICES); // Limit to max voices
        
        // If no inputs connected, set no output channels
        if (!inputs[AUDIO_A_INPUT].isConnected() && !inputs[AUDIO_B_INPUT].isConnected()) {
            outputs[AUDIO_A_OUTPUT].setChannels(0);
            outputs[AUDIO_B_OUTPUT].setChannels(0);
        } else {
            // Set output channel count
            outputs[AUDIO_A_OUTPUT].setChannels(channels);
            outputs[AUDIO_B_OUTPUT].setChannels(channels);

            // Process each voice
            for (int c = 0; c < channels; c++) {
                // Get audio inputs for this voice
                float audioA = 0.f, audioB = 0.f;
                bool hasInputA = inputs[AUDIO_A_INPUT].isConnected();
                bool hasInputB = inputs[AUDIO_B_INPUT].isConnected();
                
                if (hasInputA && hasInputB) {
                    audioA = inputs[AUDIO_A_INPUT].getVoltage(c);
                    audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                } else if (hasInputA) {
                    audioA = audioB = inputs[AUDIO_A_INPUT].getVoltage(c);
                } else if (hasInputB) {
                    audioA = audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                }
                

                // ============================================================================
                // LIQUID 6TH-ORDER FILTER PROCESSING
                // ============================================================================
                float processedA = audioA;
                float processedB = audioB;

                // Only process if we have valid, finite audio input
                if (std::isfinite(audioA) && std::isfinite(audioB)) {
                    float voiceCutoffA = cutoffA;
                    float voiceCutoffB = cutoffB;
                    float voiceResonanceA = resonanceA;
                    float voiceResonanceB = resonanceB;

                    if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
                        float attenA = params[CUTOFF_A_ATTEN_PARAM].getValue();
                        voiceCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(c) * attenA / 10.f;
                    }
                    voiceCutoffA = clamp(voiceCutoffA, 0.f, 1.f);

                    if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
                        float attenB = params[CUTOFF_B_ATTEN_PARAM].getValue();
                        voiceCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(c) * attenB / 10.f;
                    }
                    voiceCutoffB = clamp(voiceCutoffB, 0.f, 1.f);

                    if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
                        float attenA = params[RESONANCE_A_ATTEN_PARAM].getValue();
                        voiceResonanceA += inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(c) * attenA / 10.f;
                    }
                    voiceResonanceA = clamp(voiceResonanceA, 0.707f, 1.5f);

                    if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
                        float attenB = params[RESONANCE_B_ATTEN_PARAM].getValue();
                        voiceResonanceB += inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(c) * attenB / 10.f;
                    }
                    voiceResonanceB = clamp(voiceResonanceB, 0.707f, 1.5f);

                    // Convert cutoff from normalized (0-1) to Hz (20Hz - 20480Hz)
                    float cutoffAHz = 20.f * std::pow(2.f, voiceCutoffA * 10.f);
                    float cutoffBHz = 20.f * std::pow(2.f, voiceCutoffB * 10.f);

                    // Process through liquid filters with fixed drive amount
                    processedA = filtersA[c].process(audioA, cutoffAHz, voiceResonanceA, DRIVE_FIXED_AMOUNT);
                    processedB = filtersB[c].process(audioB, cutoffBHz, voiceResonanceB, DRIVE_FIXED_AMOUNT);
                }
                
                
                // Set output voltages for this voice
                outputs[AUDIO_A_OUTPUT].setVoltage(processedA, c);
                outputs[AUDIO_B_OUTPUT].setVoltage(processedB, c);
            }

        }
        
        // Update lights to show parameter values with Chiaroscuro-style color progression
        // Both lights: Teal (0%) -> Bright blue-purple (50%) -> Dark purple (100%)
        const float base_brightness = 0.4f;
        const float max_brightness = base_brightness;
        
        // Drive light with Chiaroscuro progression
        float driveValue = driveLight;
        float drive_red, drive_green, drive_blue;
        if (driveValue <= 0.5f) {
            // 0 to 0.5: Teal to bright blue-purple
            drive_red = driveValue * 2.0f * max_brightness;
            drive_green = max_brightness;
            drive_blue = max_brightness;
        } else {
            // 0.5 to 1.0: Bright blue-purple to dark purple
            drive_red = max_brightness;
            drive_green = 2.0f * (1.0f - driveValue) * max_brightness;
            drive_blue = max_brightness * (1.7f - driveValue * 0.7f);
        }
        lights[CHAOS_LIGHT].setBrightness(drive_red);
        lights[CHAOS_LIGHT + 1].setBrightness(drive_green);
        lights[CHAOS_LIGHT + 2].setBrightness(drive_blue);
    }
    
    // Integrate with Rack's default "Randomize" menu item
    void onRandomize() override {
        // Randomize filter parameters with musical ranges
        std::mt19937 rng(rack::random::u32());
        
        // Cutoff frequencies - keep in musical range (100Hz to 8kHz)
        std::uniform_real_distribution<float> cutoffDist(0.2f, 0.9f);
        params[CUTOFF_A_PARAM].setValue(cutoffDist(rng));
        params[CUTOFF_B_PARAM].setValue(cutoffDist(rng));
        
        // Resonance - moderate range to avoid harsh sounds
        std::uniform_real_distribution<float> resDist(0.1f, 0.7f);
        params[RESONANCE_A_PARAM].setValue(resDist(rng));
        params[RESONANCE_B_PARAM].setValue(resDist(rng));
        
        // Highpass is now static at 12Hz - no randomization needed
        
        // Drive is fixed at maximum saturation; keep the stored parameter at 1.0
        params[CHAOS_AMOUNT_PARAM].setValue(1.f);
        
        // Rate parameters - varied but not too extreme
        std::uniform_real_distribution<float> rateDist(0.2f, 0.8f);
        params[CHAOS_RATE_PARAM].setValue(rateDist(rng));
        
        // Filter morph - full range for variety
        std::uniform_real_distribution<float> morphDist(0.0f, 1.0f);
        params[FILTER_MORPH_PARAM].setValue(morphDist(rng));

        // Link switches - randomly enable/disable
        std::uniform_int_distribution<int> linkDist(0, 1);
        params[LINK_CUTOFF_PARAM].setValue((float)linkDist(rng));
        params[LINK_RESONANCE_PARAM].setValue((float)linkDist(rng));
    }
};

// Chaos visualizer widget - extracted to separate files
#include "involution/chaos_visualizer.hpp"
#include "involution/chaos_visualizer_impl.hpp"

// Custom SVG-based JewelLED for chaos light
struct ChaosJewelLED : ModuleLightWidget {
    ChaosJewelLED() {
        box.size = Vec(20, 20);  // Medium size

        // Try to load the medium jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));

        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }

        // Set up RGB colors for chaos activity
        addBaseColor(nvgRGB(255, 0, 0));   // Red
        addBaseColor(nvgRGB(0, 255, 0));   // Green
        addBaseColor(nvgRGB(0, 0, 255));   // Blue
    }

    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            // Fallback drawing if SVG doesn't load (medium size)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.5);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 6.5);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }

        ModuleLightWidget::draw(args);
    }
};

struct InvolutionWidget : ModuleWidget {
    InvolutionWidget(Involution* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Involution.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Parse SVG panel for precise positioning
        shapetaker::ui::LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/Involution.svg"));

        // Helper function that uses SVG parser with fallbacks to direct millimeter coordinates
        // Usage: centerPx("svg_element_id", fallback_x_mm, fallback_y_mm)
        // When SVG elements are added to the panel with matching IDs, they will automatically
        // position controls precisely. Until then, fallback coordinates are used.
        auto centerPx = [&](const std::string& id, float defx, float defy) -> Vec {
            return parser.centerPx(id, defx, defy);
        };
        
        // Main Filter Section - using SVG parser for automatic positioning
        addParam(createParamCentered<ShapetakerKnobLarge>(
            centerPx("cutoff_a", 24.026f, 24.174f),
            module, Involution::CUTOFF_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(
            centerPx("resonance_a", 11.935f, 57.750f),
            module, Involution::RESONANCE_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobLarge>(
            centerPx("cutoff_b", 66.305f, 24.174f),
            module, Involution::CUTOFF_B_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(
            centerPx("resonance_b", 78.397f, 57.750f),
            module, Involution::RESONANCE_B_PARAM));
        
        // Link switches - using SVG parser with fallbacks
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("link_cutoff", 45.166f, 29.894f),
            module, Involution::LINK_CUTOFF_PARAM));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("link_resonance", 45.166f, 84.630f),
            module, Involution::LINK_RESONANCE_PARAM));

        // Attenuverters for CV inputs
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_a_atten", 9.027f, 41.042f),
            module, Involution::CUTOFF_A_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_a_atten", 11.935f, 76.931f),
            module, Involution::RESONANCE_A_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_b_atten", 81.305f, 41.042f),
            module, Involution::CUTOFF_B_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_b_atten", 78.397f, 76.931f),
            module, Involution::RESONANCE_B_ATTEN_PARAM));

        // Character Controls - using SVG parser with fallbacks
        // Highpass is now static at 12Hz - no control needed
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(
            centerPx("filter_morph", 45.166f, 101.401f),
            module, Involution::FILTER_MORPH_PARAM));
        
        // Drive and chaos controls - using SVG parser with updated coordinates
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("chaos_amount", 15.910f, 94.088f), module, Involution::CHAOS_AMOUNT_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("chaos_rate", 74.422f, 94.088f), module, Involution::CHAOS_RATE_PARAM));
        
        // Chaos Visualizer - using SVG parser for automatic positioning
        ChaosVisualizer* chaosViz = new ChaosVisualizer(module);
        Vec screenCenter = centerPx("oscope_screen",
                                    std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN());
        if (!std::isfinite(screenCenter.x) || !std::isfinite(screenCenter.y)) {
            screenCenter = centerPx("resonance_a_cv-1", 45.166f, 57.750f);
        }
        chaosViz->box.pos = Vec(screenCenter.x - 86.5, screenCenter.y - 69); // Center the 173x138 screen
        addChild(chaosViz);
        
        // Chaos light - using SVG parser and custom JewelLED
        addChild(createLightCentered<ChaosJewelLED>(centerPx("chaos_light", 29.559f, 103.546f), module, Involution::CHAOS_LIGHT));

        // CV inputs - using SVG parser with updated coordinates
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("cutoff_a_cv", 24.027f, 44.322f), module, Involution::CUTOFF_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("resonance_a_cv", 24.027f, 68.931f), module, Involution::RESONANCE_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("cutoff_b_cv", 66.305f, 44.322f), module, Involution::CUTOFF_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("resonance_b_cv", 66.305f, 68.931f), module, Involution::RESONANCE_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("chaos_amount_cv", 29.409f, 84.630f), module, Involution::CHAOS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("chaos_lfo_cv", 60.922f, 84.630f), module, Involution::CHAOS_RATE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("filter-morph-cv", 45.166f, 119.245f), module, Involution::FILTER_MORPH_CV_INPUT));

        // Audio I/O - direct millimeter coordinates
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio_a_input", 10.276f, 118.977f), module, Involution::AUDIO_A_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio_b_input", 27.721f, 119.245f), module, Involution::AUDIO_B_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio_a_output", 63.436f, 119.347f), module, Involution::AUDIO_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio_b_output", 81.706f, 119.347f), module, Involution::AUDIO_B_OUTPUT));
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
};

Model* modelInvolution = createModel<Involution, InvolutionWidget>("Involution");
