#include "plugin.hpp"
#include "dsp/polyphony.hpp"
#include "utilities.hpp"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// ENVELOPE FOLLOWER
// ============================================================================

struct EnvelopeFollower {
    float envelope = 0.f;
    float attackCoeff = 0.f;
    float releaseCoeff = 0.f;

    void setSampleRate(float sr, float attackMs = 5.f, float releaseMs = 50.f) {
        attackCoeff = std::exp(-1.f / (sr * attackMs * 0.001f));
        releaseCoeff = std::exp(-1.f / (sr * releaseMs * 0.001f));
    }

    float process(float input) {
        float rectified = std::abs(input);
        if (rectified > envelope) {
            envelope += (rectified - envelope) * (1.f - attackCoeff);
        } else {
            envelope += (rectified - envelope) * (1.f - releaseCoeff);
        }
        return envelope;
    }

    void reset() {
        envelope = 0.f;
    }
};

// ============================================================================
// VINTAGE CHARACTER LFO CORE
// ============================================================================

struct PatinaLFOCore {
    float phase = 0.f;
    float output = 0.f;

    // Vintage character state
    float driftPhase = 0.f;
    float driftValue = 0.f;
    float driftHold = 0.f;
    float jitterAccum = 0.f;
    int noiseIndex = 0;

    // Pre-generated noise buffer for performance (like Torsion)
    static constexpr int kNoiseBufferSize = 512;
    float noiseBuffer[kNoiseBufferSize] = {};
    bool noiseInitialized = false;

    // Slew limiter state
    float slewedOutput = 0.f;

    enum Shape {
        SINE = 0,
        TRIANGLE,
        SAW,
        SQUARE,
        RANDOM,
        NUM_SHAPES
    };

    void initNoise() {
        if (!noiseInitialized) {
            for (int i = 0; i < kNoiseBufferSize; ++i) {
                noiseBuffer[i] = rack::random::uniform() * 2.f - 1.f;
            }
            noiseInitialized = true;
        }
    }

    float getNextNoise() {
        float n = noiseBuffer[noiseIndex];
        noiseIndex = (noiseIndex + 1) % kNoiseBufferSize;
        return n;
    }

    void reset() {
        phase = 0.f;
        output = 0.f;
        driftPhase = 0.f;
        driftValue = 0.f;
        driftHold = 0.f;
        jitterAccum = 0.f;
        slewedOutput = 0.f;
    }

    float process(float frequency, float sampleRate, float shapeParam,
                  float drift, float jitter, float slew, float complexity,
                  float envelopeDepth, float envelopeValue, bool useAmplitudeMode,
                  float crossModAmount = 0.f) {

        initNoise();

        // ====================================================================
        // VINTAGE DRIFT (slow random walk like analog oscillators)
        // ====================================================================
        // Based on Torsion's vintage drift implementation
        static constexpr float kDriftRate = 0.08f;
        static constexpr float kDriftHoldMax = 0.45f;

        driftPhase += kDriftRate / sampleRate;
        if (driftPhase >= 1.f) {
            driftPhase -= 1.f;
            driftHold = rack::random::uniform() * kDriftHoldMax;
        }

        if (driftPhase < driftHold) {
            // Hold phase - keep current drift value
        } else {
            // Drift phase - slowly change
            float driftSpeed = 0.0003f * drift;
            driftValue += getNextNoise() * driftSpeed;
            driftValue = rack::math::clamp(driftValue, -0.02f, 0.02f);
        }

        // ====================================================================
        // JITTER (micro-timing variations)
        // ====================================================================
        float jitterAmount = getNextNoise() * jitter * 0.001f;

        // ====================================================================
        // ENVELOPE MODULATION with cross-modulation
        // ====================================================================
        float freqModulation = 0.f;
        float amplitudeModulation = 1.f;

        // Add cross-modulation to frequency (from previous LFO in chain)
        freqModulation += crossModAmount;

        if (useAmplitudeMode) {
            // Amplitude mode: envelope controls output level (0 to 1)
            amplitudeModulation = envelopeValue * envelopeDepth + (1.f - envelopeDepth);
        } else {
            // Frequency mode: envelope modulates frequency (±2 octaves range)
            freqModulation += (envelopeValue * 2.f - 1.f) * envelopeDepth * 2.f;
        }

        // ====================================================================
        // PHASE INCREMENT with all modulations
        // ====================================================================
        float modulatedFreq = frequency * (1.f + driftValue + jitterAmount + freqModulation);
        modulatedFreq = rack::math::clamp(modulatedFreq, 0.f, sampleRate / 2.f);

        float phaseInc = modulatedFreq / sampleRate;
        phase += phaseInc;
        if (phase >= 1.f) {
            phase -= 1.f;
        }

        // ====================================================================
        // WAVEFORM GENERATION with morphing
        // ====================================================================
        float rawOutput = 0.f;

        // Continuous shape morphing (0-5 range, allows smooth transitions)
        int shapeFloor = static_cast<int>(std::floor(shapeParam));
        float shapeFrac = shapeParam - shapeFloor;

        // Generate base waveforms
        auto generateShape = [&](int s) -> float {
            switch (s) {
                case 0: // SINE
                    return std::sin(2.f * M_PI * phase);
                case 1: // TRIANGLE
                    return 4.f * std::abs(phase - 0.5f) - 1.f;
                case 2: // SAW
                    return 2.f * phase - 1.f;
                case 3: // SQUARE
                    return (phase < 0.5f) ? 1.f : -1.f;
                case 4: // RANDOM
                    if (phase < phaseInc) {
                        output = getNextNoise();
                    }
                    return output;
                default:
                    return std::sin(2.f * M_PI * phase);
            }
        };

        // Morph between adjacent shapes
        if (shapeFrac < 0.01f || shapeFloor >= 4) {
            // No morphing needed
            rawOutput = generateShape(rack::math::clamp(shapeFloor, 0, 4));
        } else {
            // Crossfade between two shapes
            float shape1 = generateShape(shapeFloor);
            float shape2 = generateShape(rack::math::clamp(shapeFloor + 1, 0, 4));
            rawOutput = rack::math::crossfade(shape1, shape2, shapeFrac);
        }

        // ====================================================================
        // COMPLEXITY (add subharmonics/noise)
        // ====================================================================
        if (complexity > 0.01f) {
            // Add octave down
            float subPhase = phase * 0.5f;
            float subharmonic = std::sin(2.f * M_PI * subPhase) * 0.3f;

            // Add noise
            float noise = getNextNoise() * 0.2f;

            rawOutput += (subharmonic + noise) * complexity;
            rawOutput = rack::math::clamp(rawOutput, -1.f, 1.f);
        }

        // ====================================================================
        // SLEW LIMITING (smoothness control)
        // ====================================================================
        float slewRate = 1.f - slew; // 0 = instant, 1 = very slow
        float maxChange = (1.f + slewRate * 100.f) / sampleRate;

        float delta = rawOutput - slewedOutput;
        if (std::abs(delta) > maxChange) {
            slewedOutput += (delta > 0.f ? maxChange : -maxChange);
        } else {
            slewedOutput = rawOutput;
        }

        output = slewedOutput;

        // Apply amplitude modulation if in amplitude mode
        return output * 5.f * amplitudeModulation; // Scale to ±5V
    }
};

// ============================================================================
// PATINA MODULE
// ============================================================================

struct Patina : Module {
    enum ParamId {
        // Global controls
        ENV_SENSITIVITY_PARAM,
        MASTER_RATE_PARAM,
        ENV_DEPTH_PARAM,
        ENV_MODE_PARAM,  // 0=Frequency, 1=Amplitude, 2=Alternating

        // Per-core controls (3x)
        RATE_1_PARAM,
        RATE_2_PARAM,
        RATE_3_PARAM,

        SHAPE_1_PARAM,
        SHAPE_2_PARAM,
        SHAPE_3_PARAM,

        DRIFT_1_PARAM,
        DRIFT_2_PARAM,
        DRIFT_3_PARAM,

        JITTER_1_PARAM,
        JITTER_2_PARAM,
        JITTER_3_PARAM,

        // Global character controls
        SLEW_PARAM,
        COMPLEXITY_PARAM,
        ALT_INTERVAL_PARAM,  // Alternating mode interval
        XMOD_PARAM,          // Cross-modulation depth
        VINTAGE_MODE_PARAM,

        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,

        RATE_1_INPUT,
        RATE_2_INPUT,
        RATE_3_INPUT,

        RESET_INPUT,

        INPUTS_LEN
    };

    enum OutputId {
        LFO_1_OUTPUT,
        LFO_2_OUTPUT,
        LFO_3_OUTPUT,

        ENV_OUTPUT, // Envelope follower output

        OUTPUTS_LEN
    };

    enum LightId {
        VINTAGE_LIGHT,

        ENUMS(LFO_1_LIGHT, 3),  // RGB for Teal
        ENUMS(LFO_2_LIGHT, 3),  // RGB for Purple
        ENUMS(LFO_3_LIGHT, 3),  // RGB for Amber

        LIGHTS_LEN
    };

    // DSP components
    EnvelopeFollower envFollower;
    PatinaLFOCore lfoCores[3];

    // Phase offsets for the 3 cores (0°, 120°, 240°)
    static constexpr float phaseOffsets[3] = {0.f, 0.333333f, 0.666667f};

    // Reset detection
    dsp::SchmittTrigger resetTrigger;

    // Alternating mode state
    float alternatingTimer = 0.f;
    bool alternatingUseAmplitude = false;

    Patina() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Global controls
        configParam(ENV_SENSITIVITY_PARAM, 0.f, 1.f, 0.5f, "Envelope Sensitivity", "%", 0.f, 100.f);
        configParam(MASTER_RATE_PARAM, -6.f, 3.f, 0.f, "Master Rate", " Hz", 2.f, 1.f);
        configParam(ENV_DEPTH_PARAM, 0.f, 1.f, 0.5f, "Envelope Depth", "%", 0.f, 100.f);
        configSwitch(ENV_MODE_PARAM, 0.f, 3.f, 2.f, "Envelope Mode", {"Frequency", "Amplitude", "Alternating", "Bipolar"});

        // Per-core rate controls
        configParam(RATE_1_PARAM, -6.f, 3.f, 0.f, "LFO 1 Rate", " Hz", 2.f, 1.f);
        configParam(RATE_2_PARAM, -6.f, 3.f, 0.f, "LFO 2 Rate", " Hz", 2.f, 1.f);
        configParam(RATE_3_PARAM, -6.f, 3.f, 0.f, "LFO 3 Rate", " Hz", 2.f, 1.f);

        // Shape selection with morphing (0-4.99 for smooth transitions)
        configParam(SHAPE_1_PARAM, 0.f, 4.99f, 0.f, "LFO 1 Shape");
        configParam(SHAPE_2_PARAM, 0.f, 4.99f, 0.f, "LFO 2 Shape");
        configParam(SHAPE_3_PARAM, 0.f, 4.99f, 0.f, "LFO 3 Shape");

        // Character controls per core
        configParam(DRIFT_1_PARAM, 0.f, 1.f, 0.3f, "LFO 1 Drift", "%", 0.f, 100.f);
        configParam(DRIFT_2_PARAM, 0.f, 1.f, 0.3f, "LFO 2 Drift", "%", 0.f, 100.f);
        configParam(DRIFT_3_PARAM, 0.f, 1.f, 0.3f, "LFO 3 Drift", "%", 0.f, 100.f);

        configParam(JITTER_1_PARAM, 0.f, 1.f, 0.2f, "LFO 1 Jitter", "%", 0.f, 100.f);
        configParam(JITTER_2_PARAM, 0.f, 1.f, 0.2f, "LFO 2 Jitter", "%", 0.f, 100.f);
        configParam(JITTER_3_PARAM, 0.f, 1.f, 0.2f, "LFO 3 Jitter", "%", 0.f, 100.f);

        // Global character controls
        configParam(SLEW_PARAM, 0.f, 1.f, 0.1f, "Slew", "%", 0.f, 100.f);
        configParam(COMPLEXITY_PARAM, 0.f, 1.f, 0.f, "Complexity", "%", 0.f, 100.f);
        configParam(ALT_INTERVAL_PARAM, 0.1f, 10.f, 2.f, "Alternating Interval", " s");
        configParam(XMOD_PARAM, 0.f, 1.f, 0.f, "Cross-Modulation", "%", 0.f, 100.f);
        configButton(VINTAGE_MODE_PARAM, "Vintage Mode");

        // Configure inputs
        configInput(AUDIO_INPUT, "Audio (for envelope follower)");
        configInput(RATE_1_INPUT, "LFO 1 Rate CV");
        configInput(RATE_2_INPUT, "LFO 2 Rate CV");
        configInput(RATE_3_INPUT, "LFO 3 Rate CV");
        configInput(RESET_INPUT, "Reset");

        // Configure outputs
        configOutput(LFO_1_OUTPUT, "LFO 1");
        configOutput(LFO_2_OUTPUT, "LFO 2");
        configOutput(LFO_3_OUTPUT, "LFO 3");
        configOutput(ENV_OUTPUT, "Envelope");

        // Initialize LFO cores with phase offsets
        for (int i = 0; i < 3; ++i) {
            lfoCores[i].phase = phaseOffsets[i];
        }

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();

        // Update envelope follower time constants
        float sensitivity = params[ENV_SENSITIVITY_PARAM].getValue();
        float attackMs = rack::math::rescale(sensitivity, 0.f, 1.f, 10.f, 1.f);
        float releaseMs = rack::math::rescale(sensitivity, 0.f, 1.f, 100.f, 20.f);
        envFollower.setSampleRate(sr, attackMs, releaseMs);
    }

    void onReset() override {
        envFollower.reset();
        for (int i = 0; i < 3; ++i) {
            lfoCores[i].reset();
            lfoCores[i].phase = phaseOffsets[i];
        }
    }

    void process(const ProcessArgs& args) override {
        // ====================================================================
        // RESET HANDLING
        // ====================================================================
        if (inputs[RESET_INPUT].isConnected()) {
            if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
                onReset();
            }
        }

        // ====================================================================
        // ENVELOPE FOLLOWER
        // ====================================================================
        float envelopeValue = 0.f;
        if (inputs[AUDIO_INPUT].isConnected()) {
            // Update envelope follower time constants based on sensitivity
            float sensitivity = params[ENV_SENSITIVITY_PARAM].getValue();
            float attackMs = rack::math::rescale(sensitivity, 0.f, 1.f, 10.f, 1.f);
            float releaseMs = rack::math::rescale(sensitivity, 0.f, 1.f, 100.f, 20.f);
            envFollower.setSampleRate(args.sampleRate, attackMs, releaseMs);

            // Process envelope
            float audioIn = inputs[AUDIO_INPUT].getVoltage();
            envelopeValue = envFollower.process(audioIn);

            // Normalize to 0-1 range (assuming ±5V audio)
            envelopeValue = rack::math::clamp(envelopeValue / 5.f, 0.f, 1.f);
        }

        // Output envelope follower value
        if (outputs[ENV_OUTPUT].isConnected()) {
            outputs[ENV_OUTPUT].setVoltage(envelopeValue * 10.f); // 0-10V
        }

        // ====================================================================
        // GLOBAL PARAMETERS
        // ====================================================================
        float masterRate = params[MASTER_RATE_PARAM].getValue();
        float envelopeDepth = params[ENV_DEPTH_PARAM].getValue();
        float slew = params[SLEW_PARAM].getValue();
        float complexity = params[COMPLEXITY_PARAM].getValue();
        bool vintageMode = params[VINTAGE_MODE_PARAM].getValue() > 0.5f;

        // Vintage mode light
        lights[VINTAGE_LIGHT].setBrightness(vintageMode ? 1.f : 0.f);

        // Apply vintage mode to character controls
        float vintageMult = vintageMode ? 2.f : 1.f;

        // ====================================================================
        // ENVELOPE MODE SELECTION
        // ====================================================================
        int envMode = (int)params[ENV_MODE_PARAM].getValue();
        bool useAmplitudeMode = false;
        bool useBipolarMode = false;

        if (envMode == 0) {
            // Frequency mode
            useAmplitudeMode = false;
        } else if (envMode == 1) {
            // Amplitude mode
            useAmplitudeMode = true;
        } else if (envMode == 2) {
            // Alternating mode
            float alternatingInterval = params[ALT_INTERVAL_PARAM].getValue();
            alternatingTimer += args.sampleTime;
            if (alternatingTimer >= alternatingInterval) {
                alternatingTimer -= alternatingInterval;
                alternatingUseAmplitude = !alternatingUseAmplitude;
            }
            useAmplitudeMode = alternatingUseAmplitude;
        } else {
            // Bipolar mode (mode 3)
            useAmplitudeMode = false;
            useBipolarMode = true;
        }

        // ====================================================================
        // PROCESS 3 LFO CORES with cross-modulation
        // ====================================================================
        float crossModDepth = params[XMOD_PARAM].getValue();
        float lfoOutputs[3] = {0.f, 0.f, 0.f};

        for (int i = 0; i < 3; ++i) {
            if (!outputs[LFO_1_OUTPUT + i].isConnected()) {
                continue;
            }

            // Get rate from parameter and CV
            float rateParam = params[RATE_1_PARAM + i].getValue();
            float rateCV = 0.f;
            if (inputs[RATE_1_INPUT + i].isConnected()) {
                rateCV = inputs[RATE_1_INPUT + i].getVoltage();
            }

            // Combine master rate, per-core rate, and CV
            // Shift everything down ~1.5 octaves so the master/rate knobs reach slower zones.
            constexpr float rangeShiftOctaves = 1.5f;
            float frequency = std::pow(2.f, masterRate + rateParam + rateCV - rangeShiftOctaves);
            frequency = rack::math::clamp(frequency, 0.005f, args.sampleRate / 2.f);

            // Get shape parameter (now supports morphing)
            float shapeParam = params[SHAPE_1_PARAM + i].getValue();

            // Get character controls
            float drift = params[DRIFT_1_PARAM + i].getValue() * vintageMult;
            float jitter = params[JITTER_1_PARAM + i].getValue() * vintageMult;

            // Calculate cross-modulation: previous LFO modulates current one
            // LFO chain: 3→1, 1→2, 2→3
            int prevIndex = (i + 2) % 3;
            float crossModAmount = lfoOutputs[prevIndex] * crossModDepth * 0.2f;

            // Handle bipolar envelope mode
            float bipolarEnvelope = envelopeValue;
            if (useBipolarMode) {
                bipolarEnvelope = envelopeValue * 2.f - 1.f; // Convert 0-1 to -1 to +1
            }

            // Process LFO
            float lfoOut = lfoCores[i].process(
                frequency,
                args.sampleRate,
                shapeParam,
                drift,
                jitter,
                slew,
                complexity,
                envelopeDepth,
                useBipolarMode ? bipolarEnvelope : envelopeValue,
                useAmplitudeMode,
                crossModAmount
            );

            // Store output for cross-modulation
            lfoOutputs[i] = lfoOut;

            // Output
            outputs[LFO_1_OUTPUT + i].setVoltage(lfoOut);

            // Update RGB lights with colored indicators
            // LFO 1: Teal (#00ffb4) = R:0, G:1, B:0.7
            // LFO 2: Purple (#b400ff) = R:0.7, G:0, B:1
            // LFO 3: Amber (#ffb400) = R:1, G:0.7, B:0
            float brightness = std::abs(lfoOut) / 5.f;
            if (i == 0) {
                // Teal
                lights[LFO_1_LIGHT + 0].setBrightness(0.f);
                lights[LFO_1_LIGHT + 1].setBrightness(brightness);
                lights[LFO_1_LIGHT + 2].setBrightness(brightness * 0.7f);
            } else if (i == 1) {
                // Purple
                lights[LFO_2_LIGHT + 0].setBrightness(brightness * 0.7f);
                lights[LFO_2_LIGHT + 1].setBrightness(0.f);
                lights[LFO_2_LIGHT + 2].setBrightness(brightness);
            } else {
                // Amber
                lights[LFO_3_LIGHT + 0].setBrightness(brightness);
                lights[LFO_3_LIGHT + 1].setBrightness(brightness * 0.7f);
                lights[LFO_3_LIGHT + 2].setBrightness(0.f);
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();

        // Save LFO phases for session continuity
        json_t* phasesJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(phasesJ, json_real(lfoCores[i].phase));
        }
        json_object_set_new(rootJ, "phases", phasesJ);

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        // Restore LFO phases
        json_t* phasesJ = json_object_get(rootJ, "phases");
        if (phasesJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* phaseJ = json_array_get(phasesJ, i);
                if (phaseJ) {
                    lfoCores[i].phase = json_real_value(phaseJ);
                }
            }
        }
    }
};

// ============================================================================
// PATINA WIDGET
// ============================================================================

struct PatinaWidget : ModuleWidget {
    PatinaWidget(Patina* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Patina.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        // Add screws
        LayoutHelper::ScrewPositions::addStandardScrews<ScrewSilver>(
            this,
            LayoutHelper::getModuleWidth(LayoutHelper::ModuleWidth::WIDTH_20HP));

        auto svgPath = asset::plugin(pluginInstance, "res/panels/Patina.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        // 20HP layout: 101.6mm wide × 128.5mm tall
        // Panel center: 50.8mm
        // Control sizes: Medium knob = 18mm (9mm radius), Small knob = 8mm (4mm radius), BNC = 8mm (4mm radius)
        // Safe margins: 2mm minimum clearance between controls

        const float centerX = 50.8f;

        // HORIZONTAL LAYOUT - Recalculated to avoid ALL overlaps
        // Left column (medium knob centered at 15mm): 6-24mm
        const float leftCol = 15.f;
        // Mid-left column (LFO 1 - medium knob needs 9mm, centered at 33mm): 24-42mm
        const float midLeftCol = 33.f;
        // Mid-right column (LFO 3 - medium knob centered at 68.6mm): 59.6-77.6mm
        const float midRightCol = 68.6f;
        // Right column (medium knob centered at 86.6mm): 77.6-95.6mm (panel is 101.6mm)
        const float rightCol = 86.6f;

        // ====================================================================
        // ENVELOPE FOLLOWER SECTION (Top)
        // ====================================================================
        const float envRow1 = 29.f;  // Main knobs (medium, 9mm radius)
        const float envRow2 = 41.f;  // Jacks (BNC, 4mm radius)
        const float envRow3 = 48.f;  // Mode knob (small, 4mm radius)

        // Three knobs: Sensitivity, Master Rate, Envelope Depth
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("patina-env-sensitivity", leftCol, envRow1), module, Patina::ENV_SENSITIVITY_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("patina-master-rate", centerX, envRow1), module, Patina::MASTER_RATE_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("patina-env-depth", rightCol, envRow1), module, Patina::ENV_DEPTH_PARAM));

        // Jacks: Audio In, Envelope Out, Reset
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-audio-input", leftCol, envRow2), module, Patina::AUDIO_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-env-output", centerX, envRow2), module, Patina::ENV_OUTPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-reset-input", rightCol, envRow2), module, Patina::RESET_INPUT));

        // Envelope mode switch (Frequency / Amplitude / Alternating / Bipolar) - using CKSS (2-position) stacked
        // Since we need 4 positions and don't have a CKSSFour, we'll use a custom approach
        // For now, use a knob that snaps to 4 positions
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-env-mode", centerX, envRow3), module, Patina::ENV_MODE_PARAM));

        // ====================================================================
        // LFO CORES SECTION (Middle - 3 columns)
        // ====================================================================
        // Vertical spacing calculation with tighter 1.5mm clearances to fit everything:
        // Row1: 56mm (rate medium knobs, 9mm radius): bottom at 65mm
        // Row2: 65+1.5+4 = 70.5mm (rate CV BNC, 4mm radius): bottom at 74.5mm
        // Row3: 74.5+1.5+4 = 80mm (shape small knobs, 4mm radius): bottom at 84mm
        // Row4: 84+1.5+4 = 89.5mm (drift small knobs, 4mm radius): bottom at 93.5mm
        // Row5: 93.5+1.5+4 = 99mm (jitter small knobs, 4mm radius): bottom at 103mm
        // Row6: 103+1.5+4 = 108.5mm (output BNC, 4mm radius): bottom at 112.5mm
        // Row7: 112.5+1.5+2 = 116mm (output lights, 2mm radius): bottom at 118mm
        const float lfoRow1 = 56.f;    // Rate knobs (medium, 9mm radius)
        const float lfoRow2 = 70.5f;   // Rate CV inputs (BNC, 4mm radius)
        const float lfoRow3 = 80.f;    // Shape knobs (small, 4mm radius)
        const float lfoRow4 = 89.5f;   // Drift knobs (small, 4mm radius)
        const float lfoRow5 = 99.f;    // Jitter knobs (small, 4mm radius)
        const float lfoRow6 = 108.5f;  // Output jacks (BNC, 4mm radius)
        const float lfoRow7 = 116.f;   // Output lights (2mm radius)

        // LFO 1 (Left - Teal)
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("patina-rate1", midLeftCol, lfoRow1), module, Patina::RATE_1_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-rate1-cv", midLeftCol, lfoRow2), module, Patina::RATE_1_INPUT));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-shape1", midLeftCol, lfoRow3), module, Patina::SHAPE_1_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-drift1", midLeftCol, lfoRow4), module, Patina::DRIFT_1_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-jitter1", midLeftCol, lfoRow5), module, Patina::JITTER_1_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-output1", midLeftCol, lfoRow6), module, Patina::LFO_1_OUTPUT));
        if (module) addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(centerPx("patina-light1", midLeftCol, lfoRow7), module, Patina::LFO_1_LIGHT));

        // LFO 2 (Center - Purple)
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("patina-rate2", centerX, lfoRow1), module, Patina::RATE_2_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-rate2-cv", centerX, lfoRow2), module, Patina::RATE_2_INPUT));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-shape2", centerX, lfoRow3), module, Patina::SHAPE_2_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-drift2", centerX, lfoRow4), module, Patina::DRIFT_2_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-jitter2", centerX, lfoRow5), module, Patina::JITTER_2_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-output2", centerX, lfoRow6), module, Patina::LFO_2_OUTPUT));
        if (module) addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(centerPx("patina-light2", centerX, lfoRow7), module, Patina::LFO_2_LIGHT));

        // LFO 3 (Right - Amber)
        addParam(createParamCentered<ShapetakerKnobAltMedium>(centerPx("patina-rate3", midRightCol, lfoRow1), module, Patina::RATE_3_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-rate3-cv", midRightCol, lfoRow2), module, Patina::RATE_3_INPUT));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-shape3", midRightCol, lfoRow3), module, Patina::SHAPE_3_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-drift3", midRightCol, lfoRow4), module, Patina::DRIFT_3_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-jitter3", midRightCol, lfoRow5), module, Patina::JITTER_3_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-output3", midRightCol, lfoRow6), module, Patina::LFO_3_OUTPUT));
        if (module) addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(centerPx("patina-light3", midRightCol, lfoRow7), module, Patina::LFO_3_LIGHT));

        // ====================================================================
        // CHARACTER SECTION (Bottom)
        // ====================================================================
        // Starts after lfoRow7 bottom (118mm), need to fit 2 rows before 128.5mm
        // CharRow1: 121mm (character knobs, 4mm radius): bottom at 125mm
        // CharRow2: Panel bottom - 3mm (button margin) = 125.5mm works
        const float charRow1 = 121.f;   // Four character knobs (small, 4mm radius)
        const float charRow2 = 125.5f;  // Vintage mode button (LEDButton, 3mm radius)

        // Four character knobs (recalculated to avoid horizontal overlaps)
        // Small knobs are 4mm radius. Need 8mm + 2mm clearance = 10mm spacing minimum
        // Available width: 101.6mm. With 4mm margins: 93.6mm usable
        // 4 knobs at 8mm width = 32mm, leaving 61.6mm for 3 gaps = ~20.5mm spacing
        const float char1X = 14.f;    // Small knob: 10-18mm
        const float char2X = 35.f;    // Small knob: 31-39mm
        const float char3X = 56.f;    // Small knob: 52-60mm
        const float char4X = 77.f;    // Small knob: 73-81mm

        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-slew", char1X, charRow1), module, Patina::SLEW_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-complexity", char2X, charRow1), module, Patina::COMPLEXITY_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-alt-interval", char3X, charRow1), module, Patina::ALT_INTERVAL_PARAM));
        addParam(createParamCentered<ShapetakerKnobAltSmall>(centerPx("patina-xmod", char4X, charRow1), module, Patina::XMOD_PARAM));

        // Vintage mode button with light
        addParam(createParamCentered<rack::componentlibrary::LEDButton>(centerPx("patina-vintage-button", centerX, charRow2), module, Patina::VINTAGE_MODE_PARAM));
        if (module) addChild(createLightCentered<MediumLight<RedLight>>(centerPx("patina-vintage-light", centerX, charRow2), module, Patina::VINTAGE_LIGHT));
    }
};

Model* modelPatina = createModel<Patina, PatinaWidget>("Patina");
