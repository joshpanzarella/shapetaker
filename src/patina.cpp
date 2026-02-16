#include "plugin.hpp"
#include "dsp/polyphony.hpp"
#include "utilities.hpp"
#include <algorithm>
#include <array>
#include <string>
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

    // DC offset tracking — running sum over one cycle
    float dcAccum = 0.f;
    float dcOffset = 0.f;
    int dcSampleCount = 0;
    float prevPhase = 0.f;

    // Random waveform sample-and-hold state
    float randomSH = 0.f;

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
        dcAccum = 0.f;
        dcOffset = 0.f;
        dcSampleCount = 0;
        prevPhase = 0.f;
        randomSH = 0.f;
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
                case 4: // RANDOM (sample-and-hold)
                    if (phase < phaseInc) {
                        randomSH = getNextNoise();
                    }
                    return randomSH;
                default:
                    return std::sin(2.f * M_PI * phase);
            }
        };

        // Morph between adjacent shapes using equal-power crossfade
        if (shapeFrac < 0.01f || shapeFloor >= 4) {
            rawOutput = generateShape(rack::math::clamp(shapeFloor, 0, 4));
        } else {
            float shape1 = generateShape(shapeFloor);
            float shape2 = generateShape(rack::math::clamp(shapeFloor + 1, 0, 4));
            // Equal-power crossfade: cos/sin curves maintain constant energy
            float angle = shapeFrac * 0.5f * M_PI;
            rawOutput = shape1 * std::cos(angle) + shape2 * std::sin(angle);
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
        // SLEW LIMITING (smoothness control) - frequency-aware
        // ====================================================================
        // Make slew rate proportional to frequency so it maintains consistent
        // smoothing across the entire frequency range without amplitude collapse

        // Scale maxChange by frequency to prevent amplitude collapse at high rates
        // At low slew, allow instant changes. At high slew, limit based on frequency
        float cyclesPerSample = frequency / sampleRate;
        float baseMaxChange = 4.f * cyclesPerSample; // Allow 4x the phase increment for smooth waveforms
        float maxChange = baseMaxChange + (1.f - slew) * 100.f / sampleRate;

        float delta = rawOutput - slewedOutput;
        if (std::abs(delta) > maxChange) {
            slewedOutput += (delta > 0.f ? maxChange : -maxChange);
        } else {
            slewedOutput = rawOutput;
        }

        output = slewedOutput;

        // DC offset removal — compute running average over each cycle
        // and subtract it. Updates once per cycle wrap to stay LFO-friendly.
        dcAccum += output;
        dcSampleCount++;
        if (phase < prevPhase) {
            // Phase wrapped — one full cycle completed
            if (dcSampleCount > 0) {
                dcOffset = dcAccum / dcSampleCount;
            }
            dcAccum = 0.f;
            dcSampleCount = 0;
        }
        prevPhase = phase;
        output -= dcOffset;

        // Apply amplitude modulation if in amplitude mode
        return output * 5.f * amplitudeModulation; // Scale to ±5V
    }
};

// ============================================================================
// PATINA MODULE
// ============================================================================

// Clock subdivision ratios (file scope to avoid C++11 linking issues)
static constexpr std::array<float, 11> kClockSubdivisionRatios = {
    0.125f, // /8
    0.166667f, // /6
    0.25f,  // /4
    0.333333f, // /3
    0.5f,   // /2
    1.f,    // 1x
    2.f,    // 2x
    3.f,    // 3x
    4.f,    // 4x
    6.f,    // 6x
    8.f     // 8x
};

struct Patina : Module {
    enum ParamId {
        // Global controls
        MASTER_RATE_PARAM,
        ENV_DEPTH_PARAM,

        // Per-core controls (3x)
        RATE_1_PARAM,
        RATE_2_PARAM,
        RATE_3_PARAM,

        SHAPE_1_PARAM,
        SHAPE_2_PARAM,
        SHAPE_3_PARAM,

        // Global character controls
        DRIFT_PARAM,            // Global drift (affects all 3 LFOs)
        JITTER_PARAM,           // Global jitter (affects all 3 LFOs)
        GRAVITY_PARAM,          // Orbital phase coupling strength
        LOCK_MODE_PARAM,        // Harmonic lock mode button

        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,

        CLOCK_INPUT,

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

        STEREO_L_OUTPUT, // Stereo field L (phase-based mix)
        STEREO_R_OUTPUT, // Stereo field R (phase-based mix)

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
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger lockModeTrigger;

    // External clock tracking
    float clockElapsed = 0.f;
    float clockInterval = 0.f;
    float clockFrequency = 0.f;
    bool clockLocked = false;
    bool clockPrimed = false;

    // Envelope smoothing to prevent pops
    float slewedEnvelope = 0.f;

    // Harmonic lock mode state
    bool harmonicLockEnabled = false;

    // Context menu settings
    bool unipolarMode = false;     // Output voltage range: false = bipolar (-5V to +5V), true = unipolar (0-10V)
    int envelopeMode = 0;          // 0=Frequency, 1=Amplitude
    bool bipolarEnvelope = false;  // Bipolar envelope conversion
    bool lfoClockModes[3] = {false, false, false}; // false = free, true = clock subdivisions

    float getClockSubdivision(float rateControl) const {
        float normalized = rack::math::clamp(rateControl, -6.f, 3.f);
        float scaled = rack::math::rescale(normalized, -6.f, 3.f, 0.f, static_cast<float>(kClockSubdivisionRatios.size() - 1));
        int idx = static_cast<int>(std::round(scaled));
        idx = rack::math::clamp(idx, 0, static_cast<int>(kClockSubdivisionRatios.size() - 1));
        return kClockSubdivisionRatios[idx];
    }

    Patina() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Global controls
        configParam(MASTER_RATE_PARAM, -6.f, 3.f, 0.f, "Master Rate", " Hz", 2.f, 1.f);
        configParam(ENV_DEPTH_PARAM, 0.f, 1.f, 0.5f, "Envelope Depth", "%", 0.f, 100.f);

        // Per-LFO rate controls
        configParam(RATE_1_PARAM, -6.f, 3.f, 0.f, "LFO 1 Rate", " Hz", 2.f, 1.f);
        configParam(RATE_2_PARAM, -6.f, 3.f, 0.f, "LFO 2 Rate", " Hz", 2.f, 1.f);
        configParam(RATE_3_PARAM, -6.f, 3.f, 0.f, "LFO 3 Rate", " Hz", 2.f, 1.f);

        // Shape selection with morphing (0-4.99 for smooth transitions)
        configParam(SHAPE_1_PARAM, 0.f, 4.99f, 0.f, "LFO 1 Shape");
        configParam(SHAPE_2_PARAM, 0.f, 4.99f, 0.f, "LFO 2 Shape");
        configParam(SHAPE_3_PARAM, 0.f, 4.99f, 0.f, "LFO 3 Shape");

        // Global character controls
        configParam(DRIFT_PARAM, 0.f, 1.f, 0.3f, "Drift", "%", 0.f, 100.f);
        configParam(JITTER_PARAM, 0.f, 1.f, 0.2f, "Jitter", "%", 0.f, 100.f);
        configParam(GRAVITY_PARAM, 0.f, 1.f, 0.f, "Gravity (Orbital Coupling)", "%", 0.f, 100.f);
        configButton(LOCK_MODE_PARAM, "Harmonic Lock Mode");

        // Configure inputs
        configInput(AUDIO_INPUT, "Audio (for envelope follower)");
        configInput(CLOCK_INPUT, "External clock (positive edge)");
        configInput(RATE_1_INPUT, "LFO 1 Rate CV");
        configInput(RATE_2_INPUT, "LFO 2 Rate CV");
        configInput(RATE_3_INPUT, "LFO 3 Rate CV");
        configInput(RESET_INPUT, "Reset");

        // Configure outputs
        configOutput(LFO_1_OUTPUT, "LFO 1");
        configOutput(LFO_2_OUTPUT, "LFO 2");
        configOutput(LFO_3_OUTPUT, "LFO 3");
        configOutput(ENV_OUTPUT, "Envelope");
        configOutput(STEREO_L_OUTPUT, "Stereo Field L");
        configOutput(STEREO_R_OUTPUT, "Stereo Field R");

        // Initialize LFO cores with phase offsets
        for (int i = 0; i < 3; ++i) {
            lfoCores[i].phase = phaseOffsets[i];
        }

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();

        // Update envelope follower time constants with good defaults
        float attackMs = 5.f;   // Fast attack
        float releaseMs = 50.f; // Medium release
        envFollower.setSampleRate(sr, attackMs, releaseMs);
    }

    void onReset() override {
        envFollower.reset();
        for (int i = 0; i < 3; ++i) {
            lfoCores[i].reset();
            lfoCores[i].phase = phaseOffsets[i];
        }
        clockTrigger.reset();
        clockElapsed = 0.f;
        clockInterval = 0.f;
        clockFrequency = 0.f;
        clockLocked = false;
        clockPrimed = false;
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
            // Process envelope with fixed time constants
            float audioIn = inputs[AUDIO_INPUT].getVoltage();
            envelopeValue = envFollower.process(audioIn);

            // Normalize to 0-1 range (assuming ±5V audio)
            envelopeValue = rack::math::clamp(envelopeValue / 5.f, 0.f, 1.f);
        }

        // Apply bipolar conversion if enabled (from context menu)
        if (bipolarEnvelope) {
            envelopeValue = envelopeValue * 2.f - 1.f; // Convert 0-1 to -1 to +1
        }

        // Apply slew limiting to envelope to prevent pops from rapid changes
        // Use a fast slew rate (10ms time constant) for smooth transitions
        float slewCoeff = std::exp(-1.f / (args.sampleRate * 0.01f)); // 10ms slew
        slewedEnvelope += (envelopeValue - slewedEnvelope) * (1.f - slewCoeff);

        // Output envelope follower value (0-10V or -10V to +10V if bipolar)
        if (outputs[ENV_OUTPUT].isConnected()) {
            outputs[ENV_OUTPUT].setVoltage(envelopeValue * 10.f);
        }

        // ====================================================================
        // EXTERNAL CLOCK (global)
        // ====================================================================
        clockElapsed += args.sampleTime;
        bool clockConnected = inputs[CLOCK_INPUT].isConnected();
        bool wasClockLocked = clockLocked;

        if (clockConnected && clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
            // Ignore ultra-fast double triggers; expect musical clocks (sub-100 Hz)
            constexpr float kMinInterval = 0.0025f;
            constexpr float kMaxInterval = 12.f;
            if (clockPrimed && clockElapsed >= kMinInterval) {
                float newInterval = rack::math::clamp(clockElapsed, kMinInterval, kMaxInterval);
                if (clockInterval <= 0.f) {
                    clockInterval = newInterval;
                } else {
                    // Light smoothing to avoid drastic jitter on the detected tempo
                    clockInterval = rack::math::crossfade(clockInterval, newInterval, 0.2f);
                }
                clockFrequency = 1.f / clockInterval;
                clockLocked = true;
            }
            clockElapsed = 0.f;
            clockPrimed = true;
        }

        // Drop lock if the clock disappears for a few beats
        float timeout = (clockInterval > 0.f) ? std::max(clockInterval * 4.f, 0.5f) : 2.f;
        if (!clockConnected || clockElapsed > timeout) {
            clockLocked = false;
            clockInterval = 0.f;
            clockFrequency = 0.f;
            clockPrimed = false;
        }

        bool clockActive = clockConnected && clockLocked && clockFrequency > 0.f;
        bool clockJustLocked = clockActive && !wasClockLocked;
        float clockBaseHz = clockActive ? rack::math::clamp(clockFrequency, 0.01f, args.sampleRate * 0.25f) : 0.f;

        if (clockJustLocked) {
            // Align LFOs to their intended offsets on the first valid clock edge
            for (int i = 0; i < 3; ++i) {
                lfoCores[i].phase = phaseOffsets[i];
            }
        }

        // ====================================================================
        // GLOBAL PARAMETERS
        // ====================================================================
        float masterRate = params[MASTER_RATE_PARAM].getValue();
        float envelopeDepth = params[ENV_DEPTH_PARAM].getValue();
        float drift = params[DRIFT_PARAM].getValue();
        float jitter = params[JITTER_PARAM].getValue();

        // ====================================================================
        // ENVELOPE MODE SELECTION (from context menu)
        // ====================================================================
        bool useAmplitudeMode = (envelopeMode == 1);

        // ====================================================================
        // HARMONIC LOCK MODE (toggle button)
        // ====================================================================
        if (lockModeTrigger.process(params[LOCK_MODE_PARAM].getValue())) {
            harmonicLockEnabled = !harmonicLockEnabled;
        }

        // ====================================================================
        // ORBITAL PHASE COUPLING (gravity parameter)
        // ====================================================================
        float gravity = params[GRAVITY_PARAM].getValue();

        // Apply gravitational phase coupling between the three LFOs
        // Each LFO is attracted to the others based on gravity strength
        if (gravity > 0.01f) {
            // Calculate phase differences and apply attractive forces
            for (int i = 0; i < 3; ++i) {
                float attraction = 0.f;

                // Calculate attraction from other two LFOs
                for (int j = 0; j < 3; ++j) {
                    if (i != j) {
                        // Calculate shortest phase distance (wrapping around 0-1)
                        float phaseDiff = lfoCores[j].phase - lfoCores[i].phase;
                        if (phaseDiff > 0.5f) phaseDiff -= 1.f;
                        if (phaseDiff < -0.5f) phaseDiff += 1.f;

                        // Attraction strength falls off with distance (inverse square-ish)
                        float distance = std::abs(phaseDiff);
                        float strength = 1.f / (1.f + distance * distance * 20.f);

                        attraction += phaseDiff * strength * 0.5f;
                    }
                }

                // Apply orbital coupling as phase nudge
                // Scale by gravity and sample time for frame-rate independence
                lfoCores[i].phase += attraction * gravity * 0.02f * args.sampleTime;

                // Wrap phase
                if (lfoCores[i].phase >= 1.f) lfoCores[i].phase -= 1.f;
                if (lfoCores[i].phase < 0.f) lfoCores[i].phase += 1.f;
            }
        }

        // ====================================================================
        // CALCULATE FREQUENCIES FOR ALL 3 LFOs
        // ====================================================================
        float frequencies[3];
        for (int i = 0; i < 3; ++i) {
            // Get rate from parameter and CV
            float rateParam = params[RATE_1_PARAM + i].getValue();
            float rateCV = inputs[RATE_1_INPUT + i].isConnected() ? inputs[RATE_1_INPUT + i].getVoltage() : 0.f;
            float rateControl = rateParam + rateCV;

            // Combine master rate, per-core rate, and CV
            // Shift everything down ~1.5 octaves so the master/rate knobs reach slower zones.
            constexpr float rangeShiftOctaves = 1.5f;
            float frequency = std::pow(2.f, masterRate + rateControl - rangeShiftOctaves);
            frequency = rack::math::clamp(frequency, 0.005f, args.sampleRate / 2.f);

            // Optionally override with external clock subdivisions per LFO
            if (clockActive && lfoClockModes[i]) {
                float subdivision = getClockSubdivision(rateControl);
                frequency = rack::math::clamp(clockBaseHz * subdivision, 0.005f, args.sampleRate / 2.f);
            }

            frequencies[i] = frequency;
        }

        // ====================================================================
        // HARMONIC LOCK MODE (quantize frequency ratios to musical intervals)
        // ====================================================================
        if (harmonicLockEnabled) {
            // Musical ratios: 1:1, 1:2, 2:3, 3:4, 3:5, 4:5, 5:6, etc.
            // Find the slowest LFO as the fundamental
            float fundamental = std::min({frequencies[0], frequencies[1], frequencies[2]});

            // Quantize each frequency to nearest musical ratio
            for (int i = 0; i < 3; ++i) {
                if (fundamental < 0.01f) break; // Avoid division by zero

                float ratio = frequencies[i] / fundamental;

                // Common musical ratios (sorted)
                static const float musicalRatios[] = {
                    1.0f,      // 1:1 (unison)
                    1.125f,    // 9:8 (major second)
                    1.2f,      // 6:5 (minor third)
                    1.25f,     // 5:4 (major third)
                    1.333f,    // 4:3 (perfect fourth)
                    1.5f,      // 3:2 (perfect fifth)
                    1.6f,      // 8:5 (minor sixth)
                    1.667f,    // 5:3 (major sixth)
                    1.75f,     // 7:4 (harmonic seventh)
                    2.0f,      // 2:1 (octave)
                    2.5f,      // 5:2 (octave + major third)
                    3.0f,      // 3:1 (octave + fifth)
                    4.0f,      // 4:1 (two octaves)
                    5.0f,      // 5:1
                    6.0f,      // 6:1
                    8.0f       // 8:1 (three octaves)
                };

                // Find nearest musical ratio
                float nearestRatio = musicalRatios[0];
                float nearestDist = std::abs(ratio - nearestRatio);

                for (float mr : musicalRatios) {
                    float dist = std::abs(ratio - mr);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearestRatio = mr;
                    }
                }

                // Apply quantized ratio
                frequencies[i] = fundamental * nearestRatio;
                frequencies[i] = rack::math::clamp(frequencies[i], 0.005f, args.sampleRate / 2.f);
            }
        }

        // ====================================================================
        // PROCESS 3 LFO CORES
        // ====================================================================
        float lfoOutputs[3] = {0.f, 0.f, 0.f};

        for (int i = 0; i < 3; ++i) {
            if (!outputs[LFO_1_OUTPUT + i].isConnected()) {
                continue;
            }

            float frequency = frequencies[i];

            // Get shape parameter (supports morphing)
            float shapeParam = params[SHAPE_1_PARAM + i].getValue();

            // Process LFO with global drift and jitter
            // Use slewed envelope to prevent pops from rapid envelope changes
            float lfoOut = lfoCores[i].process(
                frequency,
                args.sampleRate,
                shapeParam,
                drift,             // Global drift
                jitter,            // Global jitter
                0.f,               // No slew (removed)
                0.f,               // No complexity (removed)
                envelopeDepth,
                slewedEnvelope,    // Use slewed envelope for smooth modulation
                useAmplitudeMode,
                0.f                // No cross-modulation (removed)
            );

            // Store output for stereo field generation
            lfoOutputs[i] = lfoOut;

            // Apply voltage range conversion for output
            float finalOutput = lfoOut;
            if (unipolarMode) {
                // Convert from ±5V to 0-10V
                finalOutput = (lfoOut + 5.f) * 1.f;  // Shift and scale to 0-10V
            }

            // Output with proper normalization
            outputs[LFO_1_OUTPUT + i].setVoltage(finalOutput);

            // Update RGB lights with colored indicators (based on bipolar output)
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

        // ====================================================================
        // STEREO FIELD GENERATION (phase-based panning)
        // ====================================================================
        if (outputs[STEREO_L_OUTPUT].isConnected() || outputs[STEREO_R_OUTPUT].isConnected()) {
            float stereoL = 0.f;
            float stereoR = 0.f;

            // Pan each LFO based on its phase position in the cycle
            // Phase 0 = center, phase 0.25 = right, phase 0.5 = center, phase 0.75 = left
            for (int i = 0; i < 3; ++i) {
                float phase = lfoCores[i].phase;

                // Convert phase to stereo position using sine/cosine
                // This creates a smooth circular panning motion
                float pan = std::sin(2.f * M_PI * phase); // -1 (left) to +1 (right)

                // Equal-power panning law
                float panRight = (pan + 1.f) * 0.5f; // 0 to 1
                float panLeft = 1.f - panRight;

                // Use square root for equal-power panning
                panRight = std::sqrt(panRight);
                panLeft = std::sqrt(panLeft);

                // Mix each LFO into stereo field
                stereoL += lfoOutputs[i] * panLeft;
                stereoR += lfoOutputs[i] * panRight;
            }

            // Average the three LFOs (divide by 3) to prevent clipping
            stereoL *= 0.333f;
            stereoR *= 0.333f;

            // Apply voltage range conversion if needed
            if (unipolarMode) {
                stereoL = (stereoL + 5.f) * 1.f;
                stereoR = (stereoR + 5.f) * 1.f;
            }

            // Output stereo field
            outputs[STEREO_L_OUTPUT].setVoltage(stereoL);
            outputs[STEREO_R_OUTPUT].setVoltage(stereoR);
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

        // Save context menu settings
        json_object_set_new(rootJ, "unipolarMode", json_boolean(unipolarMode));
        json_object_set_new(rootJ, "envelopeMode", json_integer(envelopeMode));
        json_object_set_new(rootJ, "bipolarEnvelope", json_boolean(bipolarEnvelope));
        json_object_set_new(rootJ, "harmonicLockEnabled", json_boolean(harmonicLockEnabled));
        json_t* clockModesJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(clockModesJ, json_boolean(lfoClockModes[i]));
        }
        json_object_set_new(rootJ, "lfoClockModes", clockModesJ);

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

        // Restore context menu settings
        json_t* unipolarJ = json_object_get(rootJ, "unipolarMode");
        if (unipolarJ) {
            unipolarMode = json_boolean_value(unipolarJ);
        }

        json_t* envModeJ = json_object_get(rootJ, "envelopeMode");
        if (envModeJ) {
            envelopeMode = json_integer_value(envModeJ);
        }

        json_t* bipolarEnvJ = json_object_get(rootJ, "bipolarEnvelope");
        if (bipolarEnvJ) {
            bipolarEnvelope = json_boolean_value(bipolarEnvJ);
        }

        json_t* harmonicLockJ = json_object_get(rootJ, "harmonicLockEnabled");
        if (harmonicLockJ) {
            harmonicLockEnabled = json_boolean_value(harmonicLockJ);
        }

        json_t* clockModesJ = json_object_get(rootJ, "lfoClockModes");
        if (clockModesJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* modeJ = json_array_get(clockModesJ, i);
                if (modeJ) {
                    lfoClockModes[i] = json_boolean_value(modeJ);
                }
            }
        }
    }
};

// ============================================================================
// PATINA WIDGET
// ============================================================================

struct PatinaWidget : ModuleWidget {
    // Match the uniform Clairaudient/Tessellation/Transmutation/Torsion leather treatment
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            // Keep leather grain density consistent across panel widths via fixed-height tiling.
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;

            nvgSave(args.vg);

            // Base tile pass
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            // Offset low-opacity pass to soften seam visibility
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            // Slight darkening to match existing module tone
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

    PatinaWidget(Patina* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Patina.svg")));

        // Add screws (jet black, all four corners)
        addChild(createWidget<ScrewJetBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewJetBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewJetBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewJetBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        using LayoutHelper = shapetaker::ui::LayoutHelper;

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
        // ENVELOPE FOLLOWER SECTION (Top) - Simplified
        // ====================================================================
        const float envRow1 = 29.f;  // Main knobs (medium, 9mm radius)
        const float envRow2 = 41.f;  // Jacks (BNC, 4mm radius)

        // Two knobs: Master Rate, Envelope Depth
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("patina-master-rate", leftCol, envRow1), module, Patina::MASTER_RATE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("patina-env-depth", rightCol, envRow1), module, Patina::ENV_DEPTH_PARAM));

        // Jacks: Audio In, Envelope Out, Clock, Reset
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-audio-input", leftCol, envRow2), module, Patina::AUDIO_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-env-output", centerX, envRow2), module, Patina::ENV_OUTPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-clock-input", midRightCol, envRow2), module, Patina::CLOCK_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-reset-input", rightCol, envRow2), module, Patina::RESET_INPUT));

        // ====================================================================
        // LFO CORES SECTION (Middle - 3 columns) - Simplified
        // ====================================================================
        // Simplified vertical spacing:
        // Row1: 56mm (rate medium knobs, 9mm radius)
        // Row2: 70.5mm (rate CV inputs, 4mm radius)
        // Row3: 80mm (shape small knobs, 4mm radius)
        // Row4: 95mm (output jacks, 4mm radius)
        // Row5: 106mm (output lights, 2mm radius)
        const float lfoRow1 = 56.f;    // Rate knobs (medium, 9mm radius)
        const float lfoRow2 = 70.5f;   // Rate CV inputs (BNC, 4mm radius)
        const float lfoRow3 = 80.f;    // Shape knobs (small, 4mm radius)
        const float lfoRow4 = 95.f;    // Output jacks (BNC, 4mm radius)
        const float lfoRow5 = 106.f;   // Output lights (2mm radius)

        // LFO 1 (Left - Teal)
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("patina-rate1", midLeftCol, lfoRow1), module, Patina::RATE_1_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-rate1-cv", midLeftCol, lfoRow2), module, Patina::RATE_1_INPUT));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("patina-shape1", midLeftCol, lfoRow3), module, Patina::SHAPE_1_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-output1", midLeftCol, lfoRow4), module, Patina::LFO_1_OUTPUT));
        if (module) addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(centerPx("patina-light1", midLeftCol, lfoRow5), module, Patina::LFO_1_LIGHT));

        // LFO 2 (Center - Purple)
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("patina-rate2", centerX, lfoRow1), module, Patina::RATE_2_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-rate2-cv", centerX, lfoRow2), module, Patina::RATE_2_INPUT));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("patina-shape2", centerX, lfoRow3), module, Patina::SHAPE_2_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-output2", centerX, lfoRow4), module, Patina::LFO_2_OUTPUT));
        if (module) addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(centerPx("patina-light2", centerX, lfoRow5), module, Patina::LFO_2_LIGHT));

        // LFO 3 (Right - Amber)
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("patina-rate3", midRightCol, lfoRow1), module, Patina::RATE_3_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("patina-rate3-cv", midRightCol, lfoRow2), module, Patina::RATE_3_INPUT));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("patina-shape3", midRightCol, lfoRow3), module, Patina::SHAPE_3_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-output3", midRightCol, lfoRow4), module, Patina::LFO_3_OUTPUT));
        if (module) addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(centerPx("patina-light3", midRightCol, lfoRow5), module, Patina::LFO_3_LIGHT));

        // ====================================================================
        // CHARACTER SECTION (Bottom) - Orbital/Harmonic Controls
        // ====================================================================
        // Global character controls with orbital/harmonic features
        const float charRow = 115.f;   // Character knobs row
        const float lockRow = 125.f;   // Lock button row

        // Three character knobs with even spacing
        const float char1X = 23.f;     // Drift knob (left)
        const float char2X = 50.8f;    // Gravity knob (center)
        const float char3X = 78.6f;    // Jitter knob (right)

        addKnobWithShadow(this, createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("patina-drift", char1X, charRow), module, Patina::DRIFT_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("patina-gravity", char2X, charRow), module, Patina::GRAVITY_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("patina-jitter", char3X, charRow), module, Patina::JITTER_PARAM));

        // Lock mode button (below gravity knob)
        addParam(createParamCentered<VCVButton>(centerPx("patina-lock", char2X, lockRow), module, Patina::LOCK_MODE_PARAM));

        // Stereo output jacks (bottom right)
        const float stereoRow = 118.f;
        const float stereoLX = 15.f;
        const float stereoRX = 86.6f;

        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-stereo-l", stereoLX, stereoRow), module, Patina::STEREO_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("patina-stereo-r", stereoRX, stereoRow), module, Patina::STEREO_R_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Patina* module = dynamic_cast<Patina*>(this->module);
        if (!module)
            return;

        menu->addChild(new MenuSeparator);

        // ====================================================================
        // LFO Output Range
        // ====================================================================
        menu->addChild(createMenuLabel("LFO Output Range"));

        struct UnipolarModeItem : MenuItem {
            Patina* module;
            bool unipolar;

            void onAction(const event::Action& e) override {
                module->unipolarMode = unipolar;
            }

            void step() override {
                rightText = (module->unipolarMode == unipolar) ? "✔" : "";
                MenuItem::step();
            }
        };

        UnipolarModeItem* bipolarItem = createMenuItem<UnipolarModeItem>("Bipolar (-5V to +5V)");
        bipolarItem->module = module;
        bipolarItem->unipolar = false;
        menu->addChild(bipolarItem);

        UnipolarModeItem* unipolarItem = createMenuItem<UnipolarModeItem>("Unipolar (0V to 10V)");
        unipolarItem->module = module;
        unipolarItem->unipolar = true;
        menu->addChild(unipolarItem);

        menu->addChild(new MenuSeparator);

        // ====================================================================
        // Envelope Mode
        // ====================================================================
        menu->addChild(createMenuLabel("Envelope Mode"));

        struct EnvelopeModeItem : MenuItem {
            Patina* module;
            int mode;

            void onAction(const event::Action& e) override {
                module->envelopeMode = mode;
            }

            void step() override {
                rightText = (module->envelopeMode == mode) ? "✔" : "";
                MenuItem::step();
            }
        };

        EnvelopeModeItem* freqModeItem = createMenuItem<EnvelopeModeItem>("Frequency Modulation");
        freqModeItem->module = module;
        freqModeItem->mode = 0;
        menu->addChild(freqModeItem);

        EnvelopeModeItem* ampModeItem = createMenuItem<EnvelopeModeItem>("Amplitude Modulation");
        ampModeItem->module = module;
        ampModeItem->mode = 1;
        menu->addChild(ampModeItem);

        menu->addChild(new MenuSeparator);

        // ====================================================================
        // Clocking
        // ====================================================================
        menu->addChild(createMenuLabel("Clock Modes"));

        struct LFOClockModeItem : MenuItem {
            Patina* module;
            int lfoIndex;
            bool useClock;

            void onAction(const event::Action& e) override {
                module->lfoClockModes[lfoIndex] = useClock;
            }

            void step() override {
                rightText = (module->lfoClockModes[lfoIndex] == useClock) ? "✔" : "";
                MenuItem::step();
            }
        };

        for (int i = 0; i < 3; ++i) {
            std::string labelBase = "LFO " + std::to_string(i + 1) + " ";
            auto freeItem = createMenuItem<LFOClockModeItem>(labelBase + "Free");
            freeItem->module = module;
            freeItem->lfoIndex = i;
            freeItem->useClock = false;
            menu->addChild(freeItem);

            auto clockItem = createMenuItem<LFOClockModeItem>(labelBase + "Clocked (subdiv)");
            clockItem->module = module;
            clockItem->lfoIndex = i;
            clockItem->useClock = true;
            menu->addChild(clockItem);
        }

        menu->addChild(new MenuSeparator);

        // ====================================================================
        // Envelope Settings
        // ====================================================================
        menu->addChild(createMenuLabel("Envelope Settings"));

        struct BipolarEnvelopeItem : MenuItem {
            Patina* module;

            void onAction(const event::Action& e) override {
                module->bipolarEnvelope = !module->bipolarEnvelope;
            }

            void step() override {
                rightText = module->bipolarEnvelope ? "✔" : "";
                MenuItem::step();
            }
        };

        BipolarEnvelopeItem* bipolarEnvItem = createMenuItem<BipolarEnvelopeItem>("Bipolar Envelope (-1 to +1)");
        bipolarEnvItem->module = module;
        menu->addChild(bipolarEnvItem);
    }
};

Model* modelPatina = createModel<Patina, PatinaWidget>("Patina");
