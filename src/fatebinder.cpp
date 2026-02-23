#include "plugin.hpp"
#include "random.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <string>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr int kOverlapModes = 3;
constexpr float kSmallValueEpsilon = 0.01f;
constexpr float kCvToUnitScale = 0.1f;
constexpr float kRotationCvScale = 3.1f;
constexpr float kRingModLinearBlend = 0.5f;
constexpr float kRingModProductBlend = 0.3f;
constexpr float kTriggerPulseSeconds = 1e-3f;

// ============================================================================
// RHYTHM GENERATION MODES
// ============================================================================

enum RhythmMode {
    EUCLIDEAN_MODE = 0,
    LSYSTEM_MODE = 1
};

// ============================================================================
// EUCLIDEAN RHYTHM ENGINE
// ============================================================================

// Generates euclidean rhythm pattern: distributes k hits across n steps
inline void generateEuclideanPattern(int steps, int hits, int rotation, std::vector<bool>& pattern) {
    pattern.clear();
    pattern.resize(steps, false);

    if (hits <= 0 || steps <= 0) return;
    hits = rack::math::clamp(hits, 0, steps);

    // Build pattern using iteration instead of recursion for C++11 compatibility
    std::vector<bool> temp;
    temp.reserve(steps);

    // Simple implementation: distribute hits evenly
    for (int i = 0; i < steps; i++) {
        temp.push_back((i * hits) % steps < hits);
    }

    // Apply rotation
    for (int i = 0; i < steps; i++) {
        int idx = (i + rotation) % steps;
        pattern[idx] = temp[i];
    }
}

// ============================================================================
// L-SYSTEM RHYTHM ENGINE
// ============================================================================

// L-System: Starts with axiom, applies production rules iteratively
// X = hit, - = rest, rules expand the pattern
inline void generateLSystemPattern(int steps, int hits, int rotation, std::vector<bool>& pattern) {
    pattern.clear();
    pattern.resize(steps, false);

    if (steps <= 0) return;

    // Different L-System rules based on density (hits/steps ratio)
    float density = (float)hits / (float)steps;

    std::string axiom;
    std::string ruleX;
    std::string ruleDash;

    // Select L-System rules based on density to achieve desired hit count
    if (density < 0.25f) {
        // Sparse: Low density pattern
        axiom = "X";
        ruleX = "X--X";      // X becomes X--X (1 hit, 2 rests, 1 hit)
        ruleDash = "-";       // Rest stays rest
    } else if (density < 0.5f) {
        // Medium-sparse
        axiom = "X-";
        ruleX = "X-X";        // X becomes X-X
        ruleDash = "--";      // Rest becomes double rest
    } else if (density < 0.75f) {
        // Medium-dense
        axiom = "X";
        ruleX = "XX-";        // X becomes XX-
        ruleDash = "-";       // Rest stays rest
    } else {
        // Dense: High density pattern
        axiom = "XX";
        ruleX = "XX-X";       // X becomes XX-X
        ruleDash = "-";       // Rest stays rest
    }

    // Apply L-System iterations
    std::string current = axiom;
    int maxIterations = 8;

    for (int iter = 0; iter < maxIterations && (int)current.length() < steps * 2; iter++) {
        std::string next;
        for (size_t i = 0; i < current.length(); i++) {
            if (current[i] == 'X') {
                next += ruleX;
            } else {
                next += ruleDash;
            }
        }
        current = next;
    }

    // Convert L-System string to pattern, cycling if needed
    for (int i = 0; i < steps; i++) {
        char symbol = current[i % current.length()];
        pattern[i] = (symbol == 'X');
    }

    // Apply rotation
    std::vector<bool> temp = pattern;
    for (int i = 0; i < steps; i++) {
        int idx = (i + rotation) % steps;
        pattern[idx] = temp[i];
    }
}

// ============================================================================
// PATTERN MUTATION
// ============================================================================

struct MutatingPattern {
    std::vector<bool> basePattern;    // Original pattern (euclidean or L-system)
    std::vector<bool> currentPattern; // Mutated pattern
    std::vector<float> mutationField; // Per-step mutation probability
    float mutationAccum = 0.f;

    void initialize(int steps, int hits, int rotation, RhythmMode mode) {
        if (mode == EUCLIDEAN_MODE) {
            generateEuclideanPattern(steps, hits, rotation, basePattern);
        } else {
            generateLSystemPattern(steps, hits, rotation, basePattern);
        }
        currentPattern = basePattern;
        mutationField.resize(steps, 0.f);
        for (int i = 0; i < steps; i++) {
            mutationField[i] = rack::random::uniform() * 0.3f;
        }
    }

    void update(float dt, float mutationRate, float chaos, bool frozen) {
        if (frozen || mutationRate < 0.001f) return;

        mutationAccum += dt * mutationRate * (1.f + chaos * 5.f);

        // Chance to mutate a random step
        while (mutationAccum >= 1.f) {
            mutationAccum -= 1.f;

            int step = rack::random::u32() % currentPattern.size();
            float threshold = mutationField[step] * (0.3f + chaos * 0.7f);

            if (rack::random::uniform() < threshold) {
                currentPattern[step] = !currentPattern[step];
            }
        }
    }

    void reset() {
        currentPattern = basePattern;
    }

    bool getStep(int step) const {
        if (step < 0 || step >= (int)currentPattern.size()) return false;
        return currentPattern[step];
    }
};

// ============================================================================
// ENVELOPE SYSTEM
// ============================================================================

struct Envelope {
    float phase = 0.f;      // 0 to 1
    float attack = 0.1f;
    float decay = 0.3f;
    float curve = 0.f;      // -1 to 1 (exp, linear, log)
    float shape = 0.f;      // 0 to 1 (sine to tri to saw to square)
    bool active = false;
    int ringIndex = 0;      // Which ring spawned this envelope

    void trigger(float atk, float dec, float curv, float shp, float chaos, int ring) {
        active = true;
        phase = 0.f;
        ringIndex = ring;

        // Vary parameters based on chaos
        if (chaos > kSmallValueEpsilon) {
            float variation = chaos * 0.5f;
            attack = atk * (1.f + (rack::random::uniform() - 0.5f) * variation);
            decay = dec * (1.f + (rack::random::uniform() - 0.5f) * variation);
            curve = curv + (rack::random::uniform() - 0.5f) * chaos * 0.3f;
            shape = shp + (rack::random::uniform() - 0.5f) * chaos * 0.2f;
        } else {
            attack = atk;
            decay = dec;
            curve = curv;
            shape = shp;
        }

        attack = rack::math::clamp(attack, 0.001f, 2.f);
        decay = rack::math::clamp(decay, 0.001f, 5.f);
        curve = rack::math::clamp(curve, -1.f, 1.f);
        shape = rack::math::clamp(shape, 0.f, 1.f);
    }

    float process(float dt) {
        if (!active) return 0.f;

        float totalTime = attack + decay;
        phase += dt / totalTime;

        if (phase >= 1.f) {
            active = false;
            return 0.f;
        }

        // Calculate envelope value
        float env = 0.f;
        float attackPhase = attack / totalTime;

        if (phase < attackPhase) {
            float t = phase / attackPhase;
            // Apply curve
            if (curve < 0.f) {
                t = std::pow(t, 1.f + std::fabs(curve) * 2.f); // Exponential
            } else if (curve > 0.f) {
                t = 1.f - std::pow(1.f - t, 1.f + curve * 2.f); // Logarithmic
            }
            env = t;
        } else {
            float t = (phase - attackPhase) / (1.f - attackPhase);
            if (curve < 0.f) {
                t = 1.f - std::pow(1.f - t, 1.f + std::fabs(curve) * 2.f);
            } else if (curve > 0.f) {
                t = std::pow(t, 1.f + curve * 2.f);
            }
            env = 1.f - t;
        }

        // Apply waveform shaping
        float phaseAngle = phase * 2.f * M_PI;
        float sine = std::sin(phaseAngle) * 0.5f + 0.5f;
        float tri = std::fabs((phase < 0.5f ? phase * 2.f : 2.f - phase * 2.f));
        float saw = phase;
        float sqr = (phase < 0.5f) ? 1.f : 0.f;

        float wave;
        if (shape < 0.33f) {
            float t = shape / 0.33f;
            wave = sine + t * (tri - sine);
        } else if (shape < 0.66f) {
            float t = (shape - 0.33f) / 0.33f;
            wave = tri + t * (saw - tri);
        } else {
            float t = (shape - 0.66f) / 0.34f;
            wave = saw + t * (sqr - saw);
        }

        return env * wave;
    }
};

// ============================================================================
// PARTICLE SYSTEM
// ============================================================================

struct Particle {
    float x = 0.f;
    float y = 0.f;
    float vx = 0.f;
    float vy = 0.f;
    float life = 0.f;      // 0 to 1
    float fadeRate = 1.f;
    float brightness = 1.f;
    int stepIndex = 0;
    int ringIndex = 0;     // 0=orange, 1=amber, 2=red

    void spawn(int step, int totalSteps, float chaos, int ring) {
        // Position on circular display
        float angle = (float)step / (float)totalSteps * 2.f * M_PI - M_PI * 0.5f;
        float radius = 0.6f + (rack::random::uniform() - 0.5f) * chaos * 0.2f;

        x = std::cos(angle) * radius;
        y = std::sin(angle) * radius;

        // Initial velocity (outward from center)
        float speed = 0.3f + rack::random::uniform() * 0.2f;
        vx = std::cos(angle) * speed;
        vy = std::sin(angle) * speed;

        life = 1.f;
        fadeRate = 0.8f + rack::random::uniform() * 0.4f;
        brightness = 1.f;
        stepIndex = step;
        ringIndex = ring;
    }

    void update(float dt, float chaos) {
        // Physics
        float drag = 0.95f - chaos * 0.2f;
        vx *= drag;
        vy *= drag;

        // Slight gravity toward center (creates spiral effect)
        float centerPull = 0.1f + chaos * 0.3f;
        vx -= x * centerPull * dt;
        vy -= y * centerPull * dt;

        // Chaos creates turbulence
        if (chaos > kSmallValueEpsilon) {
            vx += (rack::random::uniform() - 0.5f) * chaos * 0.5f * dt;
            vy += (rack::random::uniform() - 0.5f) * chaos * 0.5f * dt;
        }

        x += vx * dt;
        y += vy * dt;

        // Fade out
        life -= fadeRate * dt;
        brightness = rack::math::clamp(life, 0.f, 1.f);
    }

    bool isDead() const {
        return life <= 0.f;
    }
};

} // namespace

// ============================================================================
// FATEBINDER MODULE
// ============================================================================

struct Fatebinder : Module {
    enum ParamId {
        STEPS_PARAM,
        HITS_PARAM,
        ROTATION_PARAM,
        TEMPO_PARAM,
        RING_2_DIV_PARAM,
        RING_3_DIV_PARAM,
        PROBABILITY_PARAM,
        CHAOS_PARAM,
        DENSITY_PARAM,
        MUTATION_RATE_PARAM,
        FREEZE_PARAM,
        RESET_PARAM,
        ATTACK_PARAM,
        DECAY_PARAM,
        CURVE_PARAM,
        SHAPE_PARAM,
        OVERLAP_MODE_PARAM,
        BIPOLAR_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        CLOCK_INPUT,
        RESET_INPUT,
        CHAOS_CV_INPUT,
        PROBABILITY_CV_INPUT,
        ROTATION_CV_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        MAIN_CV_OUTPUT,
        RING_1_OUTPUT,
        RING_2_OUTPUT,
        RING_3_OUTPUT,
        GATE_OUTPUT,
        ACCENT_OUTPUT,
        PARAMS_LEN_OUTPUT = ACCENT_OUTPUT + 1
    };

    enum LightId {
        LIGHTS_LEN
    };

    // Ring system
    static constexpr int kNumRings = 3;
    static constexpr int kClockSettleTicks = 3;
    MutatingPattern rings[kNumRings];
    int currentStep[kNumRings] = {0, 0, 0};
    int clockDivCounter[kNumRings] = {0, 0, 0};
    float ringHitLevel[kNumRings] = {0.f, 0.f, 0.f};

    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger freezeTrigger;
    dsp::SchmittTrigger resetPatternTrigger;
    dsp::PulseGenerator ringTriggerPulse[kNumRings];

    bool frozen = false;
    RhythmMode rhythmMode = EUCLIDEAN_MODE;
    bool bipolarOutputs = false;
    int overlapModeState = 0; // 0=Add, 1=Max, 2=Ring mod

    // Envelope pool
    static constexpr int kMaxEnvelopes = 24; // More envelopes for 3 rings
    std::vector<Envelope> envelopes;

    // Particle system
    std::vector<Particle> particles;

    // Timing
    float internalClock = 0.f;
    float internalClockFreq = 2.f; // 2 Hz default
    bool useInternalClock = true;

    // BPM tracking
    float clockTimer = 0.f;
    float clockInterval = 0.5f; // Default 120 BPM
    float bpm = 120.f;
    int clockTicksSinceChange = 0; // Track settling time
    float displayTime = 0.f; // For blinking display
    int lastSteps = -1;
    int lastHits = -1;
    int lastRotation = -1;

    Fatebinder() {
        config(PARAMS_LEN, INPUTS_LEN, PARAMS_LEN_OUTPUT, LIGHTS_LEN);

        // Rhythm parameters
        configParam(STEPS_PARAM, 3.f, 32.f, 8.f, "Steps");
        if (paramQuantities[STEPS_PARAM]) {
            paramQuantities[STEPS_PARAM]->snapEnabled = true;
            paramQuantities[STEPS_PARAM]->smoothEnabled = false;
        }
        configParam(HITS_PARAM, 1.f, 32.f, 4.f, "Hits");
        configParam(ROTATION_PARAM, 0.f, 31.f, 0.f, "Rotation");
        configParam(TEMPO_PARAM, 20.f, 240.f, 120.f, "Tempo", " BPM");
        configParam(RING_2_DIV_PARAM, 1.f, 8.f, 2.f, "Ring 2 Clock Division");
        configParam(RING_3_DIV_PARAM, 1.f, 8.f, 3.f, "Ring 3 Clock Division");

        // Probability parameters
        configParam(PROBABILITY_PARAM, 0.f, 1.f, 1.f, "Probability", "%", 0.f, 100.f);
        configParam(CHAOS_PARAM, 0.f, 1.f, 0.f, "Chaos", "%", 0.f, 100.f);
        configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density", "%", 0.f, 100.f);

        // Mutation parameters
        configParam(MUTATION_RATE_PARAM, 0.f, 1.f, 0.3f, "Mutation Rate", "%", 0.f, 100.f);
        shapetaker::ParameterHelper::configButton(this, FREEZE_PARAM, "Freeze Pattern");
        shapetaker::ParameterHelper::configButton(this, RESET_PARAM, "Reset to Euclidean");

        // Envelope parameters
        configParam(ATTACK_PARAM, 0.001f, 2.f, 0.05f, "Attack", " s");
        configParam(DECAY_PARAM, 0.001f, 5.f, 0.3f, "Decay", " s");
        configParam(CURVE_PARAM, -1.f, 1.f, 0.f, "Curve");
        configParam(SHAPE_PARAM, 0.f, 1.f, 0.f, "Shape");
        configParam(OVERLAP_MODE_PARAM, 0.f, 2.f, 0.f, "Overlap Mode");
        shapetaker::ParameterHelper::configToggle(this, BIPOLAR_PARAM, "Output Range: Unipolar (0-10V) / Bipolar (-5-5V)");

        // Inputs
        configInput(CLOCK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset");
        configInput(CHAOS_CV_INPUT, "Chaos CV");
        configInput(PROBABILITY_CV_INPUT, "Probability CV");
        configInput(ROTATION_CV_INPUT, "Rotation CV");

        // Outputs
        configOutput(MAIN_CV_OUTPUT, "Main Mix CV");
        configOutput(RING_1_OUTPUT, "Ring 1 Trigger");
        configOutput(RING_2_OUTPUT, "Ring 2 Trigger");
        configOutput(RING_3_OUTPUT, "Ring 3 Trigger");
        configOutput(GATE_OUTPUT, "Composite Gate");
        configOutput(ACCENT_OUTPUT, "Accent");

        envelopes.resize(kMaxEnvelopes);
        particles.reserve(256);

        onReset();

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void resetSequencerState() {
        for (int i = 0; i < kNumRings; i++) {
            currentStep[i] = 0;
            clockDivCounter[i] = 0;
            ringHitLevel[i] = 0.f;
        }
    }

    void initializeRings(int steps, int hits, int rotation) {
        for (int i = 0; i < kNumRings; i++) {
            rings[i].initialize(steps, hits, rotation, rhythmMode);
        }
    }

    void reinitializeRingsFromCurrentParams() {
        int steps = (int)params[STEPS_PARAM].getValue();
        int hits = (int)params[HITS_PARAM].getValue();
        int rotation = (int)params[ROTATION_PARAM].getValue();
        initializeRings(steps, hits, rotation);
    }

    void onReset() override {
        resetSequencerState();
        internalClock = 0.f;
        frozen = false;
        lastSteps = -1;
        lastHits = -1;
        lastRotation = -1;
        reinitializeRingsFromCurrentParams();

        for (auto& env : envelopes) {
            env.active = false;
        }
        particles.clear();
    }

    void process(const ProcessArgs& args) override {
        float dt = args.sampleTime;

        // Get parameters
        int steps = (int)params[STEPS_PARAM].getValue();
        int hits = (int)params[HITS_PARAM].getValue();
        int rotation = (int)params[ROTATION_PARAM].getValue();
        if (inputs[ROTATION_CV_INPUT].isConnected()) {
            rotation += (int)(inputs[ROTATION_CV_INPUT].getVoltage() * kRotationCvScale);
            rotation = ((rotation % steps) + steps) % steps; // wrap to valid range
        }
        int ring2Div = (int)params[RING_2_DIV_PARAM].getValue();
        int ring3Div = (int)params[RING_3_DIV_PARAM].getValue();

        float probability = params[PROBABILITY_PARAM].getValue();
        if (inputs[PROBABILITY_CV_INPUT].isConnected()) {
            probability += inputs[PROBABILITY_CV_INPUT].getVoltage() * kCvToUnitScale;
        }
        probability = rack::math::clamp(probability, 0.f, 1.f);

        float chaos = params[CHAOS_PARAM].getValue();
        if (inputs[CHAOS_CV_INPUT].isConnected()) {
            chaos += inputs[CHAOS_CV_INPUT].getVoltage() * kCvToUnitScale;
        }
        chaos = rack::math::clamp(chaos, 0.f, 1.f);

        float tempoBpm = params[TEMPO_PARAM].getValue();

        float density = params[DENSITY_PARAM].getValue();
        float mutationRate = params[MUTATION_RATE_PARAM].getValue();
        float attack = params[ATTACK_PARAM].getValue();
        float decay = params[DECAY_PARAM].getValue();
        float curve = params[CURVE_PARAM].getValue();
        float shape = params[SHAPE_PARAM].getValue();

        // Freeze/Reset buttons
        if (freezeTrigger.process(params[FREEZE_PARAM].getValue())) {
            frozen = !frozen;
        }

        if (resetPatternTrigger.process(params[RESET_PARAM].getValue())) {
            for (int i = 0; i < kNumRings; i++) {
                rings[i].reset();
            }
        }

        // Regenerate patterns if parameters changed
        if (steps != lastSteps || hits != lastHits || rotation != lastRotation) {
            initializeRings(steps, hits, rotation);
            lastSteps = steps;
            lastHits = hits;
            lastRotation = rotation;
        }

        // Update display time for blinking
        displayTime += dt;
        for (int i = 0; i < kNumRings; i++) {
            ringHitLevel[i] = std::max(0.f, ringHitLevel[i] - dt * 3.f);
        }

        // Update pattern mutations
        for (int i = 0; i < kNumRings; i++) {
            rings[i].update(dt, mutationRate, chaos, frozen);
        }

        // Clock handling
        bool clockTick = false;
        useInternalClock = !inputs[CLOCK_INPUT].isConnected();

        if (useInternalClock) {
            internalClockFreq = tempoBpm / 60.f;
            internalClock += dt * internalClockFreq;
            if (internalClock >= 1.f) {
                internalClock -= 1.f;
                clockTick = true;
            }
            // Calculate BPM from internal clock
            bpm = tempoBpm;
            clockTicksSinceChange = kClockSettleTicks;
        } else {
            // Accumulate time for BPM calculation
            clockTimer += dt;

            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
                clockTick = true;
                // Calculate BPM from external clock interval
                if (clockTimer > 0.001f) {
                    // Very fast settling with minimal smoothing
                    float newInterval = clockInterval * 0.3f + clockTimer * 0.7f;

                    // Check if tempo changed significantly (more than 2%)
                    if (std::abs(newInterval - clockInterval) > clockInterval * 0.02f) {
                        clockTicksSinceChange = 0; // Reset tracking counter
                    } else {
                        clockTicksSinceChange++;
                    }

                    clockInterval = newInterval;
                    bpm = 60.f / clockInterval;
                    bpm = rack::math::clamp(bpm, 1.f, 999.f); // Reasonable BPM range
                }
                clockTimer = 0.f; // Reset timer on each clock tick
            }
        }

        // Reset handling
        if (inputs[RESET_INPUT].isConnected() && resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            resetSequencerState();
        }

        // Process clock tick for each ring
        bool anyGate = false;
        bool anyAccent = false;
        std::array<float, kNumRings> ringCV = {{0.f, 0.f, 0.f}};

        if (clockTick) {
            std::array<int, kNumRings> ringDivisions = {{1, ring2Div, ring3Div}};
            for (int ring = 0; ring < kNumRings; ++ring) {
                clockDivCounter[ring]++;
                if (clockDivCounter[ring] >= ringDivisions[ring]) {
                    clockDivCounter[ring] = 0;
                    processRingStep(ring, steps, probability, density, chaos, attack, decay, curve, shape, anyGate, anyAccent);
                }
            }
        }

        // Process all envelopes
        float mainCV = 0.f;
        int overlapMode = rack::math::clamp(overlapModeState, 0, kOverlapModes - 1);

        for (auto& env : envelopes) {
            if (!env.active) continue;

            float value = env.process(dt);

            // Add to ring output
            if (env.ringIndex >= 0 && env.ringIndex < kNumRings) {
                ringCV[env.ringIndex] += value;
            }

            // Add to main mix
            switch (overlapMode) {
                case 0: // Add
                    mainCV += value;
                    break;
                case 1: // Max
                    mainCV = std::max(mainCV, value);
                    break;
                case 2: // Ring mod
                    mainCV = mainCV * kRingModLinearBlend + value * kRingModLinearBlend + (mainCV * value) * kRingModProductBlend;
                    break;
            }
        }

        mainCV = rack::math::clamp(mainCV, 0.f, 1.f);

        // Update particles
        for (auto& p : particles) {
            p.update(dt, chaos);
        }

        // Remove dead particles
        particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p) { return p.isDead(); }),
            particles.end()
        );

        // Outputs
        float mainOut = mainCV;
        // Check bipolar state (context menu setting)
        bool useBipolar = bipolarOutputs;

        if (useBipolar) {
            // Bipolar mode: -5V to +5V
            outputs[MAIN_CV_OUTPUT].setVoltage(mainOut * 10.f - 5.f);
        } else {
            // Unipolar mode: 0V to +10V
            outputs[MAIN_CV_OUTPUT].setVoltage(mainOut * 10.f);
        }
        for (int ring = 0; ring < kNumRings; ++ring) {
            outputs[RING_1_OUTPUT + ring].setVoltage(ringTriggerPulse[ring].process(dt) ? 10.f : 0.f);
        }
        outputs[GATE_OUTPUT].setVoltage(anyGate ? 10.f : 0.f);
        outputs[ACCENT_OUTPUT].setVoltage(anyAccent ? 10.f : 0.f);
    }

    void processRingStep(int ring, int steps, float probability, float density, float chaos,
                         float attack, float decay, float curve, float shape,
                         bool& anyGate, bool& anyAccent) {
        currentStep[ring] = (currentStep[ring] + 1) % steps;

        bool isPatternHit = rings[ring].getStep(currentStep[ring]);

        // Probability check
        bool shouldTrigger = false;
        if (chaos < kSmallValueEpsilon) {
            // Deterministic probability
            shouldTrigger = isPatternHit && (rack::random::uniform() < probability);
        } else {
            // Chaotic mode: pattern is a suggestion
            float hitChance = isPatternHit ? probability : (density * chaos * 0.5f);
            shouldTrigger = rack::random::uniform() < hitChance;
        }

        if (shouldTrigger) {
            ringHitLevel[ring] = 1.f;
            ringTriggerPulse[ring].trigger(kTriggerPulseSeconds);
            // Find inactive envelope
            Envelope* env = nullptr;
            for (auto& e : envelopes) {
                if (!e.active) {
                    env = &e;
                    break;
                }
            }

            if (env) {
                env->trigger(attack, decay, curve, shape, chaos, ring);
                anyGate = true;
                anyAccent = anyAccent || isPatternHit;

                // Spawn particles
                int particleCount = 2 + (int)(chaos * 4);
                for (int i = 0; i < particleCount; i++) {
                    Particle p;
                    p.spawn(currentStep[ring], steps, chaos, ring);
                    particles.push_back(p);
                }
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        for (int i = 0; i < kNumRings; i++) {
            json_object_set_new(rootJ, ("currentStep" + std::to_string(i)).c_str(), json_integer(currentStep[i]));
        }
        json_object_set_new(rootJ, "frozen", json_boolean(frozen));
        json_object_set_new(rootJ, "rhythmMode", json_integer(rhythmMode));
        json_object_set_new(rootJ, "bipolarOutputs", json_boolean(bipolarOutputs));
        json_object_set_new(rootJ, "overlapMode", json_integer(overlapModeState));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        for (int i = 0; i < kNumRings; i++) {
            json_t* stepJ = json_object_get(rootJ, ("currentStep" + std::to_string(i)).c_str());
            if (stepJ) currentStep[i] = json_integer_value(stepJ);
        }
        json_t* frozenJ = json_object_get(rootJ, "frozen");
        if (frozenJ) frozen = json_boolean_value(frozenJ);

        json_t* modeJ = json_object_get(rootJ, "rhythmMode");
        if (modeJ) {
            rhythmMode = (RhythmMode)json_integer_value(modeJ);
            reinitializeRingsFromCurrentParams();
        }

        json_t* bipolarJ = json_object_get(rootJ, "bipolarOutputs");
        if (bipolarJ) {
            bipolarOutputs = json_boolean_value(bipolarJ);
        }
        json_t* overlapJ = json_object_get(rootJ, "overlapMode");
        if (overlapJ) {
            overlapModeState = rack::math::clamp((int)json_integer_value(overlapJ), 0, kOverlapModes - 1);
        }
    }
};

// ============================================================================
// UNIFIED DISPLAY WIDGET (Radar + Terminal with Bezel)
// ============================================================================

struct UnifiedDisplayWidget : TransparentWidget {
    Fatebinder* module = nullptr;

    // Radar colors
    NVGcolor ringColors[Fatebinder::kNumRings] = {
        nvgRGB(0x45, 0xec, 0xff),  // Teal
        nvgRGB(0xb0, 0x6b, 0xff),  // Violet
        nvgRGB(0x58, 0x9c, 0xff)   // Azure
    };

    NVGcolor ringDim[Fatebinder::kNumRings] = {
        nvgRGB(0x1d, 0x84, 0x9a),
        nvgRGB(0x5e, 0x3a, 0x8f),
        nvgRGB(0x2c, 0x59, 0x92)
    };

    // Terminal colors
    NVGcolor terminalGreen = nvgRGB(0x45, 0xec, 0xff);
    NVGcolor terminalDim = nvgRGB(0x60, 0xc5, 0xd8);
    NVGcolor terminalPurple = nvgRGB(0xc8, 0x84, 0xff);

    float scanlinePhase = -1.f;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;

        nvgSave(args.vg);

        // ================================================================
        // BEZEL - slimmer treatment to match Clairaudient/Involution style
        // ================================================================
        constexpr float outerRadius = 7.f;
        constexpr float bezelWidth = 1.85f;
        constexpr float lipWidth = 0.9f;
        constexpr float screenInset = 0.1f;
        float innerRadius = std::max(1.2f, outerRadius - bezelWidth);
        float lipRadius = std::max(0.9f, innerRadius - lipWidth);

        // Recess shadow where the bezel sits in the panel.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -1.2f, -1.2f, box.size.x + 2.4f, box.size.y + 2.4f, outerRadius + 1.2f);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, outerRadius);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint recessShadow = nvgBoxGradient(args.vg,
            -1.2f, -1.2f, box.size.x + 2.4f, box.size.y + 2.4f,
            outerRadius + 1.2f, 3.2f,
            nvgRGBA(0, 0, 0, 74), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(args.vg, recessShadow);
        nvgFill(args.vg);

        // Main bezel ring.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, outerRadius);
        nvgRoundedRect(args.vg, bezelWidth, bezelWidth, box.size.x - 2.f * bezelWidth, box.size.y - 2.f * bezelWidth, innerRadius);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint bezelBody = nvgLinearGradient(args.vg,
            0.f, 0.f, box.size.x * 0.35f, box.size.y,
            nvgRGBA(0x82, 0x62, 0x45, 240), nvgRGBA(0x1a, 0x12, 0x0d, 246));
        nvgFillPaint(args.vg, bezelBody);
        nvgFill(args.vg);

        // Thin catch-light across upper bezel edge.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, outerRadius - 0.5f);
        nvgRoundedRect(args.vg, bezelWidth + 0.15f, bezelWidth + 0.15f,
            box.size.x - 2.f * (bezelWidth + 0.15f), box.size.y - 2.f * (bezelWidth + 0.15f), innerRadius - 0.15f);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint bezelHighlight = nvgLinearGradient(args.vg,
            0.f, 0.f, 0.f, box.size.y * 0.45f,
            nvgRGBA(255, 226, 182, 56), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(args.vg, bezelHighlight);
        nvgFill(args.vg);

        // Inner lip ring.
        float lipInset = bezelWidth + lipWidth;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, bezelWidth, bezelWidth,
            box.size.x - 2.f * bezelWidth, box.size.y - 2.f * bezelWidth, innerRadius);
        nvgRoundedRect(args.vg, lipInset, lipInset,
            box.size.x - 2.f * lipInset, box.size.y - 2.f * lipInset, lipRadius);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint lipShade = nvgLinearGradient(args.vg,
            0.f, bezelWidth, box.size.x, box.size.y - bezelWidth,
            nvgRGBA(10, 10, 12, 225), nvgRGBA(76, 58, 42, 136));
        nvgFillPaint(args.vg, lipShade);
        nvgFill(args.vg);

        // Tight gasket so panel leather never peeks through.
        float screenMargin = lipInset + screenInset;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, lipInset, lipInset,
            box.size.x - 2.f * lipInset, box.size.y - 2.f * lipInset, lipRadius);
        nvgRoundedRect(args.vg, screenMargin, screenMargin,
            box.size.x - 2.f * screenMargin, box.size.y - 2.f * screenMargin, std::max(0.75f, lipRadius - screenInset));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGBA(8, 8, 10, 240));
        nvgFill(args.vg);

        // Screen area with subtle inset.
        float screenX = screenMargin;
        float screenY = screenMargin;
        float screenW = box.size.x - screenMargin * 2;
        float screenH = box.size.y - screenMargin * 2;

        // Embedded screen appearance - shallow inset for a sleeker profile.
        float insetSize = 1.4f;

        // Top-left inset shadow.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX - insetSize, screenY - insetSize,
                       screenW + insetSize * 2, screenH + insetSize * 2, 5.2f);
        NVGpaint topShadow = nvgLinearGradient(args.vg,
            screenX - insetSize, screenY - insetSize,
            screenX + insetSize * 2, screenY + insetSize * 2,
            nvgRGBA(0, 0, 0, 118), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(args.vg, topShadow);
        nvgFill(args.vg);

        // Bottom-right highlight.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX - insetSize, screenY - insetSize,
                       screenW + insetSize * 2, screenH + insetSize * 2, 5.2f);
        NVGpaint bottomHighlight = nvgLinearGradient(args.vg,
            screenX + screenW - insetSize * 2, screenY + screenH - insetSize * 2,
            screenX + screenW + insetSize, screenY + screenH + insetSize,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(115, 92, 67, 42));
        nvgFillPaint(args.vg, bottomHighlight);
        nvgFill(args.vg);

        // Dark screen background with amber tint
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, 5.2f);
        NVGpaint glassFill = nvgLinearGradient(args.vg, screenX, screenY, screenX + screenW, screenY + screenH,
            nvgRGB(0x05, 0x02, 0x01), nvgRGB(0x12, 0x08, 0x03));
        nvgFillPaint(args.vg, glassFill);
        nvgFill(args.vg);

        // Corner highlight for curved glass
        // Minimal subtle highlight to avoid color shift
        NVGpaint glassHighlight = nvgRadialGradient(args.vg,
            screenX + screenW * 0.5f, screenY + screenH * 0.15f, 5.f, screenW * 0.8f,
            nvgRGBA(0x22, 0x44, 0x55, 18), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, 5.2f);
        nvgFillPaint(args.vg, glassHighlight);
        nvgFill(args.vg);

        // ================================================================
        // RADAR SECTION (Left side of screen)
        // ================================================================

        float radarSize = std::min(screenH * 0.9f, screenW * 0.55f);
        float radarX = screenX + radarSize * 0.5f;
        float radarY = screenY + screenH * 0.5f;
        float radius = radarSize * 0.45f;

        // Draw radar
        drawRadar(args.vg, radarX, radarY, radius);

        // ================================================================
        // TERMINAL SECTION (Right side of screen)
        // ================================================================

        float termX = screenX + radarSize + 4;
        float termY = screenY - 2.f;
        float termW = screenW - radarSize - 4;
        float termH = screenH;

        nvgScissor(args.vg, termX, termY, termW, termH);
        drawTerminal(args.vg, termX, termY, termW, termH);
        nvgResetScissor(args.vg);

        // ================================================================
        // SCREEN EFFECTS (over everything)
        // ================================================================

        // Scanlines (static and softened to avoid aliasing when zoomed out)
        const float scanSpacing = 1.5f;
        const float scanThickness = 0.18f;
        if (scanlinePhase < 0.f) {
            scanlinePhase = rack::random::uniform() * scanSpacing;
        }
        float scanStartY = screenY + scanlinePhase;
        while (scanStartY > screenY) {
            scanStartY -= scanSpacing;
        }
        for (float y = scanStartY; y < screenY + screenH; y += scanSpacing) {
            if (y + scanThickness < screenY) {
                continue;
            }
            nvgBeginPath(args.vg);
            nvgRect(args.vg, screenX, y, screenW, scanThickness);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 20));
            nvgFill(args.vg);
        }

        // Phosphor separation lines (static, faint)
        const float separationSpacing = 6.f;
        const float separationThickness = 0.3f;
        const float separationOffset = separationSpacing * 0.5f;
        for (float y = screenY + separationOffset; y < screenY + screenH; y += separationSpacing) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, screenX, y, screenW, separationThickness);
            nvgFillColor(args.vg, nvgRGBA(0x45, 0xec, 0xff, 10));
            nvgFill(args.vg);
        }

        // Screen vignette
        nvgBeginPath(args.vg);
        NVGpaint vignette = nvgRadialGradient(args.vg, screenX + screenW * 0.5f, screenY + screenH * 0.5f,
            std::min(screenW, screenH) * 0.3f, std::max(screenW, screenH) * 0.7f,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 120));
        nvgRect(args.vg, screenX, screenY, screenW, screenH);
        nvgFillPaint(args.vg, vignette);
        nvgFill(args.vg);

        nvgRestore(args.vg);
    }

    void drawRadar(NVGcontext* vg, float cx, float cy, float radius) {
        if (!module) return;

        int steps = (int)module->params[Fatebinder::STEPS_PARAM].getValue();

        // Range rings
        for (int ring = 1; ring <= 4; ring++) {
            float ringRadius = radius * (ring / 4.0f);
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, ringRadius);
            nvgStrokeColor(vg, nvgRGBA(0x44, 0x1c, 0x00, 0x66));
            nvgStrokeWidth(vg, 0.65f);
            nvgStroke(vg);
        }

        // Radial spokes
        for (int i = 0; i < steps; i++) {
            float angle = (float)i / (float)steps * 2.f * M_PI - M_PI * 0.5f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx, cy);
            nvgLineTo(vg, cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
            nvgStrokeColor(vg, nvgRGBA(0x33, 0x15, 0x00, 0x4c));
            nvgStrokeWidth(vg, 0.5f);
            nvgStroke(vg);
        }

        // Pattern rings - spread out with more spacing
        std::array<float, Fatebinder::kNumRings> ringRadii = {{
            radius * 0.35f,  // Ring 1 (innermost)
            radius * 0.60f,  // Ring 2 (middle)
            radius * 0.85f   // Ring 3 (outermost)
        }};

        for (int lay = 0; lay < Fatebinder::kNumRings; lay++) {
            float ringRadius = ringRadii[lay];
            NVGcolor dimColor = ringDim[lay];

            for (int i = 0; i < steps; i++) {
                if (!module->rings[lay].getStep(i)) continue;

                float angle = (float)i / (float)steps * 2.f * M_PI - M_PI * 0.5f;
                float x = cx + std::cos(angle) * ringRadius;
                float y = cy + std::sin(angle) * ringRadius;

                // Dim dot for stored pattern hit
                nvgBeginPath(vg);
                nvgCircle(vg, x, y, 1.5f);
                nvgFillColor(vg, nvgRGBA(dimColor.r * 255, dimColor.g * 255, dimColor.b * 255, 100));
                nvgFill(vg);
            }
        }

        // Current step indicators - hit-driven glow and cross at the active step location
        if (steps > 0) {
            for (int lay = 0; lay < Fatebinder::kNumRings; lay++) {
                float ringRadius = ringRadii[lay];
                int currentStep = module->currentStep[lay];
                float angle = (float)currentStep / (float)steps * 2.f * M_PI - M_PI * 0.5f;
                float tipX = cx + std::cos(angle) * ringRadius;
                float tipY = cy + std::sin(angle) * ringRadius;
                NVGcolor color = ringColors[lay];
                bool hasPatternStep = module->rings[lay].getStep(currentStep);
                float hitLevel = rack::math::clamp(module->ringHitLevel[lay], 0.f, 1.f);
                bool hitActive = hasPatternStep && hitLevel > kSmallValueEpsilon;

                // Sweep line from center
                nvgBeginPath(vg);
                nvgMoveTo(vg, cx, cy);
                nvgLineTo(vg, tipX, tipY);
                nvgStrokeColor(vg, nvgRGBAf(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 0.3f));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);

                if (hitActive) {
                    // Hit: glow behind crosshair at the exact hit location.
                    float glowRadius = 6.f + hitLevel * 7.f;
                    NVGpaint glow = nvgRadialGradient(vg, tipX, tipY, 0.f, glowRadius,
                        nvgRGBAf(color.r, color.g, color.b, 0.18f + hitLevel * 0.42f),
                        nvgRGBAf(color.r, color.g, color.b, 0.f));
                    nvgBeginPath(vg);
                    nvgCircle(vg, tipX, tipY, glowRadius);
                    nvgFillPaint(vg, glow);
                    nvgFill(vg);

                    float crossSize = 3.0f + hitLevel * 1.2f;
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, tipX - crossSize, tipY);
                    nvgLineTo(vg, tipX + crossSize, tipY);
                    nvgMoveTo(vg, tipX, tipY - crossSize);
                    nvgLineTo(vg, tipX, tipY + crossSize);
                    nvgStrokeColor(vg, color);
                    nvgStrokeWidth(vg, 0.9f + 0.4f * hitLevel);
                    nvgStroke(vg);
                } else {
                    // No recent hit: idle cursor dot
                    nvgBeginPath(vg);
                    nvgCircle(vg, tipX, tipY, 2.f);
                    nvgFillColor(vg, color);
                    nvgFill(vg);
                }
            }
        }

    }

    void drawTerminal(NVGcontext* vg, float x, float y, float w, float h) {
        std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;

        nvgFontFaceId(vg, font->handle);
        nvgFontSize(vg, 8.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

        float lineHeight = 9.4f;
        char buf[64];

        // Header - title and subtitle
        float headerY = y + 2.f;
        nvgFillColor(vg, terminalGreen);
        nvgText(vg, x + 4.f, headerY, "FATEBINDER", NULL);
        nvgFillColor(vg, terminalDim);
        nvgText(vg, x + 49.f, headerY, "RHYTHM COMPUTER", NULL);
        nvgFillColor(vg, nvgRGB(0xb0, 0x6b, 0xff));
        nvgText(vg, x + 4.f, headerY + lineHeight * 0.9f, u8"運命結束機", NULL);
        nvgFillColor(vg, terminalDim);
        nvgText(vg, x + 49.f, headerY + lineHeight * 0.9f, u8"シェイプテイカー", NULL);

        {
            std::shared_ptr<Font> futura = APP->window->loadFont(asset::system("res/fonts/FuturaLT-Bold.ttf"));
            if (futura) {
                nvgFontFaceId(vg, futura->handle);
                nvgFontSize(vg, 8.5f);
            } else {
                nvgFontFaceId(vg, font->handle);
                nvgFontSize(vg, 8.5f);
            }
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
            nvgFillColor(vg, nvgRGB(0x45, 0xec, 0xff));
            nvgText(vg, x + w - 1.5f, headerY - 1.f, "shape", NULL);
            nvgFillColor(vg, nvgRGB(0xb0, 0x6b, 0xff));
            nvgText(vg, x + w - 1.5f, headerY + 6.5f, "taker", NULL);

            nvgFontFaceId(vg, font->handle);
            nvgFontSize(vg, 8.0f);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        }

        if (!module) {
            nvgFillColor(vg, terminalGreen);
            nvgText(vg, x + 3.f, headerY + lineHeight * 2.4f, "> STANDBY", NULL);
            return;
        }

        // 3-column layout
        float col1X = x + 2.5f;
        float col2ParamsX = x + w * 0.33f + 2.0f;
        float col2ActivityX = x + w * 0.27f + 2.0f;
        float col3X = x + w * 0.64f + 2.0f;
        float col1Y = headerY + lineHeight * 2.3f;

        int steps = (int)module->params[Fatebinder::STEPS_PARAM].getValue();
        int hits = (int)module->params[Fatebinder::HITS_PARAM].getValue();
        int rotation = (int)module->params[Fatebinder::ROTATION_PARAM].getValue();

        // Column 1: Pattern
        const char* rhythmName = (module->rhythmMode == LSYSTEM_MODE) ? "L-SYSTEM" : "EUCLIDEAN";
        nvgFillColor(vg, terminalPurple);
        snprintf(buf, sizeof(buf), "%s", rhythmName);
        nvgText(vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        nvgFillColor(vg, terminalGreen);
        snprintf(buf, sizeof(buf), "HIT:%d", hits);
        nvgText(vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        snprintf(buf, sizeof(buf), "STP:%d", steps);
        nvgText(vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        snprintf(buf, sizeof(buf), "ROT:%d", rotation);
        nvgText(vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight * 1.1f;

        // Rings
        int div2 = (int)module->params[Fatebinder::RING_2_DIV_PARAM].getValue();
        int div3 = (int)module->params[Fatebinder::RING_3_DIV_PARAM].getValue();

        NVGcolor divLabel = terminalPurple;
        divLabel.r *= 0.6f;
        divLabel.g *= 0.6f;
        divLabel.b *= 0.6f;
        nvgFillColor(vg, divLabel);
        nvgText(vg, col1X, col1Y, "DIV:", NULL);
        col1Y += lineHeight;

        nvgFillColor(vg, nvgRGB(0x45, 0xec, 0xff));
        nvgText(vg, col1X, col1Y, "1/1", NULL);
        col1Y += lineHeight;

        nvgFillColor(vg, nvgRGB(0xb0, 0x6b, 0xff));
        snprintf(buf, sizeof(buf), "1/%d", div2);
        nvgText(vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        nvgFillColor(vg, nvgRGB(0x58, 0x9c, 0xff));
        snprintf(buf, sizeof(buf), "1/%d", div3);
        nvgText(vg, col1X, col1Y, buf, NULL);

        // Column 2: Envelope & Overlap
        float col2Y = headerY + lineHeight * 2.3f;

        float attack = module->params[Fatebinder::ATTACK_PARAM].getValue();
        float decay = module->params[Fatebinder::DECAY_PARAM].getValue();
        float curve = module->params[Fatebinder::CURVE_PARAM].getValue();
        int overlapMode = module->overlapModeState;

        auto drawLine = [&](float xPos, float& yPos, const char* text,
                            NVGcolor color = nvgRGB(0x00, 0xff, 0x41)) {
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            std::string line(text ? text : "");
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string label = line.substr(0, colonPos + 1);
                std::string value = line.substr(colonPos + 1);

                NVGcolor labelColor = color;
                labelColor.r *= 0.6f;
                labelColor.g *= 0.6f;
                labelColor.b *= 0.6f;
                nvgFillColor(vg, labelColor);
                nvgText(vg, xPos, yPos, label.c_str(), NULL);

                float bounds[4];
                nvgTextBounds(vg, 0.f, 0.f, label.c_str(), NULL, bounds);
                float labelWidth = bounds[2] - bounds[0];

                NVGcolor valueColor = color;
                valueColor.r = std::min(valueColor.r * 1.1f + 0.05f, 1.f);
                valueColor.g = std::min(valueColor.g * 1.1f + 0.05f, 1.f);
                valueColor.b = std::min(valueColor.b * 1.1f + 0.05f, 1.f);
                nvgFillColor(vg, valueColor);
                nvgText(vg, xPos + labelWidth + 2.f, yPos, value.c_str(), NULL);
            } else {
                nvgFillColor(vg, color);
                nvgText(vg, xPos, yPos, line.c_str(), NULL);
            }
            yPos += lineHeight;
        };

        snprintf(buf, sizeof(buf), "ATK:%dms", (int)std::round(attack * 1000.f));
        drawLine(col2ParamsX, col2Y, buf, terminalPurple);

        snprintf(buf, sizeof(buf), "DEC:%dms", (int)std::round(decay * 1000.f));
        drawLine(col2ParamsX, col2Y, buf, terminalPurple);

        snprintf(buf, sizeof(buf), "CUR:%+d%%", (int)std::round(curve * 100.f));
        drawLine(col2ParamsX, col2Y, buf, terminalPurple);

        const char* modeNames[kOverlapModes] = {"ADD", "MAX", "RING"};
        snprintf(buf, sizeof(buf), "BLD:%s", modeNames[overlapMode % kOverlapModes]);
        drawLine(col2ParamsX, col2Y, buf, terminalPurple);

        // Activity indicators for each ring
        col2Y += lineHeight * 1.2f;
        col2Y += lineHeight * 0.4f;
        nvgFillColor(vg, terminalDim);
        float activityLabelY = col2Y;
        nvgText(vg, col2ActivityX, activityLabelY, "ACT", NULL);
        if (module) {
            NVGcolor activityColors[Fatebinder::kNumRings] = {
                nvgRGB(0x45, 0xec, 0xff),
                nvgRGB(0xb0, 0x6b, 0xff),
                nvgRGB(0x58, 0x9c, 0xff)
            };

            float dotX = col2ActivityX + 20.f;
            float dotY = activityLabelY + 3.2f;
            for (int i = 0; i < Fatebinder::kNumRings; i++) {
                float level = rack::math::clamp(module->ringHitLevel[i], 0.f, 1.f);
                float brightness = 0.4f + level * 0.6f;
                float alpha = 0.2f + level * 0.8f;

                NVGcolor fill = activityColors[i];
                fill.r = rack::math::clamp(fill.r * brightness, 0.f, 1.f);
                fill.g = rack::math::clamp(fill.g * brightness, 0.f, 1.f);
                fill.b = rack::math::clamp(fill.b * brightness, 0.f, 1.f);
                fill.a = rack::math::clamp(alpha, 0.f, 1.f);

                nvgBeginPath(vg);
                nvgCircle(vg, dotX + i * 7.f, dotY, 2.3f);
                nvgFillColor(vg, fill);
                nvgFill(vg);

                NVGcolor outline = activityColors[i];
                outline.a = 0.55f;
                nvgBeginPath(vg);
                nvgCircle(vg, dotX + i * 7.f, dotY, 2.3f);
                nvgStrokeColor(vg, outline);
                nvgStrokeWidth(vg, 0.6f);
                nvgStroke(vg);
            }
        }
        col2Y += lineHeight * 0.9f;

        // Tracking indicator placeholder in column 2
        col2Y += lineHeight * 0.4f;
        float trackingRowY = col2Y;
        if (module && module->clockTicksSinceChange < Fatebinder::kClockSettleTicks && !module->useInternalClock) {
            float timeBlink = std::fmod(module->displayTime, 1.0f);
            if (timeBlink < 0.5f) {
                nvgFillColor(vg, nvgRGB(0xff, 0xa0, 0x40));
                nvgText(vg, col2ParamsX, trackingRowY, "TRACKING", NULL);
            }
        }

        col2Y += lineHeight * 0.4f;

        // Column 3: Mutation & State
        float col3Y = headerY + lineHeight * 2.3f;

        float chaos = module->params[Fatebinder::CHAOS_PARAM].getValue();
        float mutation = module->params[Fatebinder::MUTATION_RATE_PARAM].getValue();
        float probability = module->params[Fatebinder::PROBABILITY_PARAM].getValue();
        float tempo = module->params[Fatebinder::TEMPO_PARAM].getValue();
        float density = module->params[Fatebinder::DENSITY_PARAM].getValue();

        snprintf(buf, sizeof(buf), "SET:%d BPM", (int)tempo);
        drawLine(col3X, col3Y, buf, nvgRGB(0xff, 0xa0, 0x40));

        snprintf(buf, sizeof(buf), "CLK:%d BPM", (int)module->bpm);
        drawLine(col3X, col3Y, buf, nvgRGB(0xff, 0xa0, 0x40));

        snprintf(buf, sizeof(buf), "PRB:%d%%", (int)(probability * 100.f));
        drawLine(col3X, col3Y, buf, terminalGreen);

        snprintf(buf, sizeof(buf), "CHA:%d%%", (int)(chaos * 100.f));
        drawLine(col3X, col3Y, buf, chaos > 0.7f ? nvgRGB(0xff, 0x33, 0x22) : terminalGreen);

        snprintf(buf, sizeof(buf), "DEN:%d%%", (int)(density * 100.f));
        drawLine(col3X, col3Y, buf, terminalGreen);

        snprintf(buf, sizeof(buf), "MUT:%d%%", (int)(mutation * 100.f));
        drawLine(col3X, col3Y, buf, terminalGreen);

        snprintf(buf, sizeof(buf), "RNG:%s", module->bipolarOutputs ? "-5/+5V" : "0/+10V");
        drawLine(col3X, col3Y, buf, nvgRGB(0x45, 0xec, 0xff));

        snprintf(buf, sizeof(buf), "STS:%s", module->frozen ? "FROZEN" : "ACTIVE");
        drawLine(col3X, col3Y, buf, module->frozen ? nvgRGB(0xb0, 0x6b, 0xff) : terminalGreen);

        col3Y += lineHeight * 0.2f;

        // Column 2 tracking indicator handled above
    }
};

// ============================================================================
// WIDGET
// ============================================================================

struct FatebinderWidget : ModuleWidget {
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

    FatebinderWidget(Fatebinder* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Fatebinder.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        auto mm = [](float x, float y) { return LayoutHelper::mm2px(Vec(x, y)); };
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Fatebinder.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        // Add screws
        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        // Panel width: 101.6mm (20HP), height: 128.5mm
        // ====================================================================
        // TOP DISPLAY - Unified radar + terminal with CRT bezel
        // ====================================================================

        UnifiedDisplayWidget* unifiedDisplay = new UnifiedDisplayWidget();
        unifiedDisplay->module = module;
        Rect displayRect = parser.rectMm("unified-display", 10.08f, 12.f, 81.44f, 35.f);
        unifiedDisplay->box.pos = mm(displayRect.pos.x, displayRect.pos.y);
        unifiedDisplay->box.size = mm(displayRect.size.x, displayRect.size.y);  // Full width minus margins
        constexpr float kDisplayScale = 1.10f;
        Vec displayCenter = unifiedDisplay->box.pos.plus(unifiedDisplay->box.size.div(2.f));
        unifiedDisplay->box.size = unifiedDisplay->box.size.mult(kDisplayScale);
        unifiedDisplay->box.pos = displayCenter.minus(unifiedDisplay->box.size.div(2.f));
        addChild(unifiedDisplay);

        // ====================================================================
        // CONTROLS - 4 columns, redistributed after panel simplification
        // ====================================================================
        // Col 1: Rhythm (Steps 20mm, Hits 14mm, Rotation 14mm)
        // Col 2: Probability + Ring dividers (Probability 16mm, Chaos 14mm, Ring2 14mm, Ring3 14mm)
        // Col 3: Mutation (Tempo 16mm, Mutation 14mm, Freeze btn, Reset btn)
        // Col 4: Envelope (Attack 14mm, Decay 14mm, Shape 14mm)
        const float col1X = 16.f;
        const float col2X = 38.f;
        const float col3X = 65.f;
        const float col4X = 86.f;

        const float topY = 56.f;

        // Column 1: Rhythm - Steps is the hero knob (20mm), rest are 14mm
        auto* stepsKnob = createParamCentered<ShapetakerKnobVintageMedium>(centerPx("steps-knob", col1X, topY), module, Fatebinder::STEPS_PARAM);
        addKnobWithShadow(this, stepsKnob);
        auto* hitsKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("hits-knob", col1X, 73.f), module, Fatebinder::HITS_PARAM);
        addKnobWithShadow(this, hitsKnob);
        auto* rotationKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("rotation-knob", col1X, 87.f), module, Fatebinder::ROTATION_PARAM);
        addKnobWithShadow(this, rotationKnob);

        // Column 2: Probability + Ring dividers
        auto* probabilityKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("probability-knob", col2X, topY), module, Fatebinder::PROBABILITY_PARAM);
        addKnobWithShadow(this, probabilityKnob);
        auto* chaosKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("chaos-knob", col2X, 71.f), module, Fatebinder::CHAOS_PARAM);
        addKnobWithShadow(this, chaosKnob);
        auto* ring2Knob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("ring2-knob", col2X, 85.f), module, Fatebinder::RING_2_DIV_PARAM);
        addKnobWithShadow(this, ring2Knob);
        auto* ring3Knob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("ring3-knob", col2X, 99.f), module, Fatebinder::RING_3_DIV_PARAM);
        addKnobWithShadow(this, ring3Knob);

        // Column 3: Mutation
        auto* tempoKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("tempo-knob", col3X, topY), module, Fatebinder::TEMPO_PARAM);
        addKnobWithShadow(this, tempoKnob);
        auto* mutationKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("mutation-knob", col3X, 71.f), module, Fatebinder::MUTATION_RATE_PARAM);
        addKnobWithShadow(this, mutationKnob);
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("freeze-btn", col3X, 84.f), module, Fatebinder::FREEZE_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("reset-btn", col3X, 95.f), module, Fatebinder::RESET_PARAM));

        // Column 4: Envelope
        auto* attackKnob = createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("attack-knob", col4X, topY), module, Fatebinder::ATTACK_PARAM);
        addKnobWithShadow(this, attackKnob);
        auto* decayKnob = createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("decay-knob", col4X, 70.f), module, Fatebinder::DECAY_PARAM);
        addKnobWithShadow(this, decayKnob);

        // ====================================================================
        // CV INPUTS - Spread across bottom
        // ====================================================================
        const float inputY = 107.0f;
        const float ioSpacing = 11.5f;
        const float ioStartX = 18.08f;

        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("clock-input", ioStartX, inputY), module, Fatebinder::CLOCK_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("reset-input", ioStartX + ioSpacing, inputY), module, Fatebinder::RESET_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("chaos-cv-input", ioStartX + ioSpacing * 2, inputY), module, Fatebinder::CHAOS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("probability-cv-input", ioStartX + ioSpacing * 3, inputY), module, Fatebinder::PROBABILITY_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("rotation-cv", ioStartX + ioSpacing * 4, inputY), module, Fatebinder::ROTATION_CV_INPUT));

        // ====================================================================
        // OUTPUTS - Bottom row
        // ====================================================================
        const float outputY = 119.5f;

        // Bipolar toggle removed from panel - available in context menu

        // Ring outputs
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("ring1-output", ioStartX, outputY), module, Fatebinder::RING_1_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("ring2-output", ioStartX + ioSpacing, outputY), module, Fatebinder::RING_2_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("ring3-output", ioStartX + ioSpacing * 2, outputY), module, Fatebinder::RING_3_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("gate-output", ioStartX + ioSpacing * 3, outputY), module, Fatebinder::GATE_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("accent-output", ioStartX + ioSpacing * 4, outputY), module, Fatebinder::ACCENT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("main-output", ioStartX + ioSpacing * 5, outputY), module, Fatebinder::MAIN_CV_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Fatebinder* module = dynamic_cast<Fatebinder*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Rhythm Generation Mode"));

        menu->addChild(createCheckMenuItem("Euclidean", "",
            [=]() { return module->rhythmMode == EUCLIDEAN_MODE; },
            [=]() {
                module->rhythmMode = EUCLIDEAN_MODE;
                module->reinitializeRingsFromCurrentParams();
            }
        ));

        menu->addChild(createCheckMenuItem("L-System", "",
            [=]() { return module->rhythmMode == LSYSTEM_MODE; },
            [=]() {
                module->rhythmMode = LSYSTEM_MODE;
                module->reinitializeRingsFromCurrentParams();
            }
        ));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Envelope Output Range"));

        menu->addChild(createCheckMenuItem("0V to +10V", "",
            [=]() { return !module->bipolarOutputs; },
            [=]() { module->bipolarOutputs = false; }
        ));

        menu->addChild(createCheckMenuItem("-5V to +5V", "",
            [=]() { return module->bipolarOutputs; },
            [=]() { module->bipolarOutputs = true; }
        ));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Overlap Mode"));

        menu->addChild(createCheckMenuItem("Add", "",
            [=]() { return module->overlapModeState == 0; },
            [=]() { module->overlapModeState = 0; }
        ));
        menu->addChild(createCheckMenuItem("Max", "",
            [=]() { return module->overlapModeState == 1; },
            [=]() { module->overlapModeState = 1; }
        ));
        menu->addChild(createCheckMenuItem("Ring Mod", "",
            [=]() { return module->overlapModeState == 2; },
            [=]() { module->overlapModeState = 2; }
        ));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Envelope Curve"));

        struct CurveSlider : ui::Slider {
            struct CurveQuantity : Quantity {
                Fatebinder* module;
                float getValue() override { return module->params[Fatebinder::CURVE_PARAM].getValue(); }
                void setValue(float value) override { module->params[Fatebinder::CURVE_PARAM].setValue(rack::math::clamp(value, -1.f, 1.f)); }
                float getDefaultValue() override { return 0.f; }
                float getMinValue() override { return -1.f; }
                float getMaxValue() override { return 1.f; }
                std::string getLabel() override { return "Curve"; }
                std::string getUnit() override { return ""; }
                int getDisplayPrecision() override { return 2; }
            };
            CurveSlider(Fatebinder* module) {
                box.size.x = 200.f;
                quantity = new CurveQuantity();
                static_cast<CurveQuantity*>(quantity)->module = module;
            }
            ~CurveSlider() { delete quantity; }
        };
        menu->addChild(new CurveSlider(module));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("L-System Density"));

        struct DensitySlider : ui::Slider {
            struct DensityQuantity : Quantity {
                Fatebinder* module;
                float getValue() override { return module->params[Fatebinder::DENSITY_PARAM].getValue(); }
                void setValue(float value) override { module->params[Fatebinder::DENSITY_PARAM].setValue(rack::math::clamp(value, 0.f, 1.f)); }
                float getDefaultValue() override { return 0.5f; }
                float getMinValue() override { return 0.f; }
                float getMaxValue() override { return 1.f; }
                std::string getLabel() override { return "Density"; }
                std::string getUnit() override { return "%"; }
                float getDisplayMultiplier() { return 100.f; }
                int getDisplayPrecision() override { return 0; }
            };
            DensitySlider(Fatebinder* module) {
                box.size.x = 200.f;
                quantity = new DensityQuantity();
                static_cast<DensityQuantity*>(quantity)->module = module;
            }
            ~DensitySlider() { delete quantity; }
        };
        menu->addChild(new DensitySlider(module));
    }
};

Model* modelFatebinder = createModel<Fatebinder, FatebinderWidget>("Fatebinder");
