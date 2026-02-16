#include "plugin.hpp"
#include "random.hpp"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

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

    // Bjorklund algorithm
    std::vector<int> counts(steps, 0);
    std::vector<int> remainders(steps, 0);

    int divisor = steps - hits;
    remainders[0] = hits;

    int level = 0;
    while (remainders[level] > 1) {
        counts[level] = divisor / remainders[level];
        remainders[level + 1] = divisor % remainders[level];
        divisor = remainders[level];
        level++;
    }
    counts[level] = divisor;

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
        if (chaos > 0.01f) {
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
        if (chaos > 0.01f) {
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
    MutatingPattern rings[kNumRings];
    int currentStep[kNumRings] = {0, 0, 0};
    int clockDivCounter[kNumRings] = {0, 0, 0};
    float ringHitLevel[kNumRings] = {0.f, 0.f, 0.f};

    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger freezeTrigger;
    dsp::SchmittTrigger resetPatternTrigger;

    bool frozen = false;
    RhythmMode rhythmMode = EUCLIDEAN_MODE;
    bool bipolarOutputs = false;

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

    Fatebinder() {
        config(PARAMS_LEN, INPUTS_LEN, PARAMS_LEN_OUTPUT, LIGHTS_LEN);

        // Rhythm parameters
        configParam(STEPS_PARAM, 3.f, 32.f, 8.f, "Steps");
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

        // Outputs
        configOutput(MAIN_CV_OUTPUT, "Main Mix CV");
        configOutput(RING_1_OUTPUT, "Ring 1 CV");
        configOutput(RING_2_OUTPUT, "Ring 2 CV");
        configOutput(RING_3_OUTPUT, "Ring 3 CV");
        configOutput(GATE_OUTPUT, "Composite Gate");
        configOutput(ACCENT_OUTPUT, "Accent");

        envelopes.resize(kMaxEnvelopes);
        particles.reserve(256);

        onReset();

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void onReset() override {
        for (int i = 0; i < kNumRings; i++) {
            currentStep[i] = 0;
            clockDivCounter[i] = 0;
            ringHitLevel[i] = 0.f;
        }
        internalClock = 0.f;
        frozen = false;

        int steps = (int)params[STEPS_PARAM].getValue();
        int hits = (int)params[HITS_PARAM].getValue();
        int rotation = (int)params[ROTATION_PARAM].getValue();

        for (int i = 0; i < kNumRings; i++) {
            rings[i].initialize(steps, hits, rotation, rhythmMode);
        }

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
        int ring2Div = (int)params[RING_2_DIV_PARAM].getValue();
        int ring3Div = (int)params[RING_3_DIV_PARAM].getValue();

        float probability = params[PROBABILITY_PARAM].getValue();
        if (inputs[PROBABILITY_CV_INPUT].isConnected()) {
            probability += inputs[PROBABILITY_CV_INPUT].getVoltage() * 0.1f;
        }
        probability = rack::math::clamp(probability, 0.f, 1.f);

        float chaos = params[CHAOS_PARAM].getValue();
        if (inputs[CHAOS_CV_INPUT].isConnected()) {
            chaos += inputs[CHAOS_CV_INPUT].getVoltage() * 0.1f;
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
        static int lastSteps = steps;
        static int lastHits = hits;
        static int lastRotation = rotation;

        if (steps != lastSteps || hits != lastHits || rotation != lastRotation) {
            for (int i = 0; i < kNumRings; i++) {
                rings[i].initialize(steps, hits, rotation, rhythmMode);
            }
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
            clockTicksSinceChange = 3;
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
            for (int i = 0; i < kNumRings; i++) {
                currentStep[i] = 0;
                clockDivCounter[i] = 0;
            }
        }

        // Process clock tick for each ring
        bool anyGate = false;
        bool anyAccent = false;
        float ringCV[kNumRings] = {0.f, 0.f, 0.f};

        if (clockTick) {
            // Ring 1: Every clock
            clockDivCounter[0]++;
            if (clockDivCounter[0] >= 1) {
                clockDivCounter[0] = 0;
                processRingStep(0, steps, probability, density, chaos, attack, decay, curve, shape, anyGate, anyAccent, ringCV[0]);
            }

            // Ring 2: Divided clock
            clockDivCounter[1]++;
            if (clockDivCounter[1] >= ring2Div) {
                clockDivCounter[1] = 0;
                processRingStep(1, steps, probability, density, chaos, attack, decay, curve, shape, anyGate, anyAccent, ringCV[1]);
            }

            // Ring 3: Divided clock
            clockDivCounter[2]++;
            if (clockDivCounter[2] >= ring3Div) {
                clockDivCounter[2] = 0;
                processRingStep(2, steps, probability, density, chaos, attack, decay, curve, shape, anyGate, anyAccent, ringCV[2]);
            }
        }

        // Process all envelopes
        float mainCV = 0.f;
        int overlapMode = (int)params[OVERLAP_MODE_PARAM].getValue();

        for (int ring = 0; ring < kNumRings; ring++) {
            ringCV[ring] = 0.f;
        }

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
                    mainCV = mainCV * 0.5f + value * 0.5f + (mainCV * value) * 0.3f;
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
        float ringOut = rack::math::clamp(ringCV[0], 0.f, 1.f);
        float ringOut2 = rack::math::clamp(ringCV[1], 0.f, 1.f);
        float ringOut3 = rack::math::clamp(ringCV[2], 0.f, 1.f);

        // Check bipolar switch parameter
        bool useBipolar = params[BIPOLAR_PARAM].getValue() > 0.5f;

        if (useBipolar) {
            // Bipolar mode: -5V to +5V
            outputs[MAIN_CV_OUTPUT].setVoltage(mainOut * 10.f - 5.f);
            outputs[RING_1_OUTPUT].setVoltage(ringOut * 10.f - 5.f);
            outputs[RING_2_OUTPUT].setVoltage(ringOut2 * 10.f - 5.f);
            outputs[RING_3_OUTPUT].setVoltage(ringOut3 * 10.f - 5.f);
        } else {
            // Unipolar mode: 0V to +10V
            outputs[MAIN_CV_OUTPUT].setVoltage(mainOut * 10.f);
            outputs[RING_1_OUTPUT].setVoltage(ringOut * 10.f);
            outputs[RING_2_OUTPUT].setVoltage(ringOut2 * 10.f);
            outputs[RING_3_OUTPUT].setVoltage(ringOut3 * 10.f);
        }
        outputs[GATE_OUTPUT].setVoltage(anyGate ? 10.f : 0.f);
        outputs[ACCENT_OUTPUT].setVoltage(anyAccent ? 10.f : 0.f);
    }

    void processRingStep(int ring, int steps, float probability, float density, float chaos,
                         float attack, float decay, float curve, float shape,
                         bool& anyGate, bool& anyAccent, float& ringCV) {
        currentStep[ring] = (currentStep[ring] + 1) % steps;

        bool isPatternHit = rings[ring].getStep(currentStep[ring]);

        // Probability check
        bool shouldTrigger = false;
        if (chaos < 0.01f) {
            // Deterministic probability
            shouldTrigger = isPatternHit && (rack::random::uniform() < probability);
        } else {
            // Chaotic mode: pattern is a suggestion
            float hitChance = isPatternHit ? probability : (density * chaos * 0.5f);
            shouldTrigger = rack::random::uniform() < hitChance;
        }

        if (shouldTrigger) {
            ringHitLevel[ring] = 1.f;
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
            // Re-initialize patterns with new mode
            int steps = (int)params[STEPS_PARAM].getValue();
            int hits = (int)params[HITS_PARAM].getValue();
            int rotation = (int)params[ROTATION_PARAM].getValue();
            for (int i = 0; i < kNumRings; i++) {
                rings[i].initialize(steps, hits, rotation, rhythmMode);
            }
        }

        json_t* bipolarJ = json_object_get(rootJ, "bipolarOutputs");
        if (bipolarJ) {
            bipolarOutputs = json_boolean_value(bipolarJ);
        }
    }
};

// ============================================================================
// UNIFIED DISPLAY WIDGET (Radar + Terminal with Bezel)
// ============================================================================

struct UnifiedDisplayWidget : TransparentWidget {
    Fatebinder* module = nullptr;

    // Radar colors
    NVGcolor ringColors[3] = {
        nvgRGB(0x45, 0xec, 0xff),  // Teal
        nvgRGB(0xb0, 0x6b, 0xff),  // Violet
        nvgRGB(0x58, 0x9c, 0xff)   // Azure
    };

    NVGcolor ringDim[3] = {
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
        // BEZEL - Vintage CRT monitor frame
        // ================================================================

        // Outer bezel (raised edge)
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        NVGcolor bezelLight = nvgRGB(0x6b, 0x42, 0x28);
        NVGcolor bezelDark = nvgRGB(0x1c, 0x11, 0x08);

        // Outer bezel with gentle curvature
        float outerRadius = 8.f;

        // Panel recess shading to seat the bezel flush with the panel
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -2.0f, -2.0f, box.size.x + 4.0f, box.size.y + 4.0f, outerRadius + 2.0f);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, outerRadius);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint topInsetShadow = nvgLinearGradient(args.vg,
            0.f, -2.0f, 0.f, 6.0f,
            nvgRGBA(0, 0, 0, 90), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(args.vg, topInsetShadow);
        nvgFill(args.vg);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -2.0f, -2.0f, box.size.x + 4.0f, box.size.y + 4.0f, outerRadius + 2.0f);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, outerRadius);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint panelGlow = nvgLinearGradient(args.vg,
            0.f, box.size.y - 6.0f, 0.f, box.size.y + 2.0f,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(0xc8, 0x8f, 0x55, 60));
        nvgFillPaint(args.vg, panelGlow);
        nvgFill(args.vg);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -0.5f, -0.5f, box.size.x + 1.0f, box.size.y + 1.0f, outerRadius + 0.5f);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, outerRadius);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint bezelShadow = nvgBoxGradient(args.vg,
            -0.5f, -0.5f, box.size.x + 1.0f, box.size.y + 1.0f,
            outerRadius + 0.5f, 4.0f,
            nvgRGBA(0, 0, 0, 70), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(args.vg, bezelShadow);
        nvgFill(args.vg);

        // Bezel body with warm metallic gradient
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, outerRadius);
        NVGpaint bezelOuter = nvgLinearGradient(args.vg, 0.f, 0.f, box.size.x * 0.25f, box.size.y * 1.05f,
            bezelDark, bezelLight);
        nvgFillPaint(args.vg, bezelOuter);
        nvgFill(args.vg);

        // Polished edge rendered as gradient chamfer
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.9f, 0.9f, box.size.x - 1.8f, box.size.y - 1.8f, outerRadius - 0.9f);
        nvgRoundedRect(args.vg, 2.8f, 2.8f, box.size.x - 5.6f, box.size.y - 5.6f, outerRadius - 2.8f);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint bezelEdgePaint = nvgLinearGradient(args.vg, 0.f, 0.f, box.size.x, box.size.y,
            nvgRGBA(0xe0, 0xb0, 0x6a, 200), nvgRGBA(0x18, 0x0d, 0x06, 230));
        nvgFillPaint(args.vg, bezelEdgePaint);
        nvgFill(args.vg);

        // Subtle top highlight to catch ambient light
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.8f, 1.8f, box.size.x - 3.6f, box.size.y - 3.6f, outerRadius - 1.8f);
        nvgRoundedRect(args.vg, 4.4f, 4.4f, box.size.x - 8.8f, box.size.y - 8.8f, outerRadius - 4.4f);
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint bezelSheen = nvgLinearGradient(args.vg,
            0.f, box.size.y * 0.35f, 0.f, box.size.y,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(255, 220, 150, 35));
        nvgFillPaint(args.vg, bezelSheen);
        nvgFill(args.vg);

        // Inner bezel inset
        float bezelWidth = 6.f;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, bezelWidth, bezelWidth, box.size.x - bezelWidth * 2, box.size.y - bezelWidth * 2, outerRadius - 2.5f);
        NVGpaint bezelInner = nvgLinearGradient(args.vg, bezelWidth, bezelWidth, box.size.x - bezelWidth, box.size.y - bezelWidth,
            nvgRGB(0x15, 0x0d, 0x08), nvgRGB(0x04, 0x02, 0x01));
        nvgFillPaint(args.vg, bezelInner);
        nvgFill(args.vg);

        // Screen area with slight inset
        float screenMargin = bezelWidth + 2.2f;
        float screenX = screenMargin;
        float screenY = screenMargin;
        float screenW = box.size.x - screenMargin * 2;
        float screenH = box.size.y - screenMargin * 2;

        // Embedded screen appearance - inset shadow for depth
        float insetSize = 3.f;

        // Dark shadow on top and left edges (recessed look)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX - insetSize, screenY - insetSize,
                       screenW + insetSize * 2, screenH + insetSize * 2, 6.f);
        NVGpaint topShadow = nvgLinearGradient(args.vg,
            screenX - insetSize, screenY - insetSize,
            screenX + insetSize * 2, screenY + insetSize * 2,
            nvgRGBA(0, 0, 0, 140), nvgRGBA(0, 0, 0, 0));
        nvgFillPaint(args.vg, topShadow);
        nvgFill(args.vg);

        // Light highlight on bottom and right edges (depth)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX - insetSize, screenY - insetSize,
                       screenW + insetSize * 2, screenH + insetSize * 2, 6.f);
        NVGpaint bottomHighlight = nvgLinearGradient(args.vg,
            screenX + screenW - insetSize * 2, screenY + screenH - insetSize * 2,
            screenX + screenW + insetSize, screenY + screenH + insetSize,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(90, 70, 50, 50));
        nvgFillPaint(args.vg, bottomHighlight);
        nvgFill(args.vg);

        // Dark screen background with amber tint
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, 6.f);
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
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, 6.f);
        nvgFillPaint(args.vg, glassHighlight);
        nvgFill(args.vg);

        // ================================================================
        // RADAR SECTION (Left side of screen)
        // ================================================================

        float radarSize = std::min(screenH * 0.9f, screenW * 0.55f);
        float radarX = screenX + radarSize * 0.5f;
        float radarY = screenY + screenH * 0.5f;
        float radius = radarSize * 0.45f;

        // Draw radar (simplified from ParticleDisplayWidget)
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
        float ringRadii[3] = {
            radius * 0.35f,  // Ring 1 (innermost)
            radius * 0.60f,  // Ring 2 (middle)
            radius * 0.85f   // Ring 3 (outermost)
        };

        for (int lay = 0; lay < 3; lay++) {
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

        // Particles
        for (const auto& p : module->particles) {
            float px = cx + p.x * radius;
            float py = cy + p.y * radius;
            float brightness = p.brightness;

            if (brightness > 0.01f && p.ringIndex >= 0 && p.ringIndex < 3) {
                NVGcolor color = ringColors[p.ringIndex];
                nvgBeginPath(vg);
                nvgCircle(vg, px, py, 2.5f * brightness);
                nvgFillColor(vg, nvgRGBAf(color.r * brightness, color.g * brightness, color.b * brightness, brightness));
                nvgFill(vg);

                NVGpaint glow = nvgRadialGradient(vg, px, py, 0, 10.f * brightness,
                    nvgRGBAf(color.r * brightness * 0.8f, color.g * brightness * 0.8f, color.b * brightness * 0.8f, brightness * 0.6f),
                    nvgRGBAf(0.f, 0.f, 0.f, 0.f));
                nvgBeginPath(vg);
                nvgCircle(vg, px, py, 10.f * brightness);
                nvgFillPaint(vg, glow);
                nvgFill(vg);
            }
        }

        // Current step indicators - draw as rings to clearly show hits vs empty
        if (steps > 0) {
            for (int lay = 0; lay < 3; lay++) {
                float ringRadius = ringRadii[lay];
                int currentStep = module->currentStep[lay];
                float angle = (float)currentStep / (float)steps * 2.f * M_PI - M_PI * 0.5f;
                float tipX = cx + std::cos(angle) * ringRadius;
                float tipY = cy + std::sin(angle) * ringRadius;
                NVGcolor color = ringColors[lay];

                bool isHit = module->rings[lay].getStep(currentStep);

                // Sweep line from center
                nvgBeginPath(vg);
                nvgMoveTo(vg, cx, cy);
                nvgLineTo(vg, tipX, tipY);
                nvgStrokeColor(vg, nvgRGBAf(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 0.3f));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);

                if (isHit) {
                    // Hit: crosshair indicator
                    float crossSize = 3.f;
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, tipX - crossSize, tipY);
                    nvgLineTo(vg, tipX + crossSize, tipY);
                    nvgMoveTo(vg, tipX, tipY - crossSize);
                    nvgLineTo(vg, tipX, tipY + crossSize);
                    nvgStrokeColor(vg, color);
                    nvgStrokeWidth(vg, 0.9f);
                    nvgStroke(vg);
                } else {
                    // No hit: bright dot
                    nvgBeginPath(vg);
                    nvgCircle(vg, tipX, tipY, 2.f);
                    nvgFillColor(vg, color);
                    nvgFill(vg);
                }
            }
        }

        // ================================================================
        // RING NUMBER LABELS
        // ================================================================

        std::shared_ptr<Font> labelFont = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (labelFont) {
            nvgFontFaceId(vg, labelFont->handle);
            nvgFontSize(vg, 8.f);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

            const char* ringLabels[] = {"1", "2", "3"};

            for (int i = 0; i < 3; i++) {
                float ringRadius = ringRadii[i];
                // Position labels at the top of each ring
                float labelX = cx;
                float labelY = cy - ringRadius - 6.f;

                // Draw label with ring color
                NVGcolor labelColor = ringColors[i];
                nvgFillColor(vg, labelColor);
                nvgText(vg, labelX, labelY, ringLabels[i], NULL);
            }
        }
    }

    void drawTerminal(NVGcontext* vg, float x, float y, float w, float h) {
        std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;

        nvgFontFaceId(vg, font->handle);
        nvgFontSize(vg, 7.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

        float lineHeight = 8.4f;
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
                nvgFontSize(vg, 7.5f);
            } else {
                nvgFontFaceId(vg, font->handle);
                nvgFontSize(vg, 7.5f);
            }
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
            nvgFillColor(vg, nvgRGB(0x45, 0xec, 0xff));
            nvgText(vg, x + w - 1.5f, headerY - 1.f, "shape", NULL);
            nvgFillColor(vg, nvgRGB(0xb0, 0x6b, 0xff));
            nvgText(vg, x + w - 1.5f, headerY + 6.5f, "taker", NULL);

            nvgFontFaceId(vg, font->handle);
            nvgFontSize(vg, 7.0f);
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
        int overlapMode = (int)module->params[Fatebinder::OVERLAP_MODE_PARAM].getValue();

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

        float shape = module->params[Fatebinder::SHAPE_PARAM].getValue();
        const char* shapeNames[] = {"SINE", "TRI", "SAW", "SQR"};
        int shapeIndex = rack::math::clamp((int)std::round(shape * 3.f), 0, 3);
        snprintf(buf, sizeof(buf), "WAV:%s", shapeNames[shapeIndex]);
        drawLine(col2ParamsX, col2Y, buf, terminalPurple);

        col2Y += lineHeight * 0.2f;

        const char* modeNames[] = {"ADD", "MAX", "RING"};
        snprintf(buf, sizeof(buf), "BLD:%s", modeNames[overlapMode % 3]);
        drawLine(col2ParamsX, col2Y, buf, terminalPurple);

        // Activity indicators for each ring
        col2Y += lineHeight * 0.4f;
        nvgFillColor(vg, terminalDim);
        float activityLabelY = col2Y;
        nvgText(vg, col2ActivityX, activityLabelY, "ACT", NULL);
        if (module) {
            NVGcolor activityColors[3] = {
                nvgRGB(0x45, 0xec, 0xff),
                nvgRGB(0xb0, 0x6b, 0xff),
                nvgRGB(0x58, 0x9c, 0xff)
            };

            float dotX = col2ActivityX + 16.f;
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
        if (module && module->clockTicksSinceChange < 3 && !module->useInternalClock) {
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
// OLD TERMINAL INFO DISPLAY WIDGET (kept for reference, can be removed)
// ============================================================================

struct TerminalDisplayWidget : TransparentWidget {
    Fatebinder* module = nullptr;

    NVGcolor terminalGreen = nvgRGB(0x45, 0xec, 0xff);
    NVGcolor terminalDim = nvgRGB(0x60, 0xc5, 0xd8);
    NVGcolor terminalPurple = nvgRGB(0xc8, 0x84, 0xff);
    NVGcolor bgColor = nvgRGBA(0x00, 0x00, 0x00, 0xff);

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;

        nvgSave(args.vg);

        // Dark terminal background
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, bgColor);
        nvgFill(args.vg);

        // Get monospace font
        std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) {
            nvgRestore(args.vg);
            return;
        }

        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, 8.f);
        nvgFillColor(args.vg, terminalGreen);
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

        float lineHeight = 9.5f;
        char buf[64];

        // Column 1 (left side)
        float col1X = 4.f;
        float col1Y = 3.f;

        // Terminal header (spans both columns)
        nvgFillColor(args.vg, terminalGreen);
        nvgText(args.vg, col1X, col1Y, "FATEBINDER v1.0", NULL);
        col1Y += lineHeight;
        nvgFillColor(args.vg, terminalDim);
        nvgText(args.vg, col1X, col1Y, "RHYTHM COMPUTER", NULL);
        col1Y += lineHeight * 1.3f;

        if (!module) {
            nvgFillColor(args.vg, terminalGreen);
            nvgText(args.vg, col1X, col1Y, "> STANDBY", NULL);
            nvgRestore(args.vg);
            return;
        }

        // Column 1 content
        nvgFillColor(args.vg, terminalDim);
        nvgText(args.vg, col1X, col1Y, "STATUS:", NULL);
        col1Y += lineHeight;

        int steps = (int)module->params[Fatebinder::STEPS_PARAM].getValue();
        int hits = (int)module->params[Fatebinder::HITS_PARAM].getValue();

        nvgFillColor(args.vg, terminalGreen);
        snprintf(buf, sizeof(buf), "STEPS: %02d", steps);
        nvgText(args.vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        snprintf(buf, sizeof(buf), "HITS:  %02d", hits);
        nvgText(args.vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight * 1.3f;

        nvgFillColor(args.vg, terminalDim);
        nvgText(args.vg, col1X, col1Y, "RINGS:", NULL);
        col1Y += lineHeight;

        int div2 = (int)module->params[Fatebinder::RING_2_DIV_PARAM].getValue();
        int div3 = (int)module->params[Fatebinder::RING_3_DIV_PARAM].getValue();

        nvgFillColor(args.vg, nvgRGB(0xff, 0x55, 0x00));
        snprintf(buf, sizeof(buf), "L1: 1/%d", 1);
        nvgText(args.vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        nvgFillColor(args.vg, nvgRGB(0xff, 0xaa, 0x00));
        snprintf(buf, sizeof(buf), "L2: 1/%d", div2);
        nvgText(args.vg, col1X, col1Y, buf, NULL);
        col1Y += lineHeight;

        nvgFillColor(args.vg, nvgRGB(0xff, 0x11, 0x00));
        snprintf(buf, sizeof(buf), "L3: 1/%d", div3);
        nvgText(args.vg, col1X, col1Y, buf, NULL);

        // Column 2 (right side)
        float col2X = box.size.x * 0.52f;  // Start halfway across
        float col2Y = 3.f + lineHeight * 2.3f;  // Align with STATUS

        nvgFillColor(args.vg, terminalDim);
        nvgText(args.vg, col2X, col2Y, "MUTATION:", NULL);
        col2Y += lineHeight;

        float chaos = module->params[Fatebinder::CHAOS_PARAM].getValue();
        float mutation = module->params[Fatebinder::MUTATION_RATE_PARAM].getValue();

        nvgFillColor(args.vg, chaos > 0.7f ? nvgRGB(0xff, 0x22, 0x00) : terminalGreen);
        snprintf(buf, sizeof(buf), "CHAOS: %3d%%", (int)(chaos * 100));
        nvgText(args.vg, col2X, col2Y, buf, NULL);
        col2Y += lineHeight;

        nvgFillColor(args.vg, terminalGreen);
        snprintf(buf, sizeof(buf), "RATE:  %3d%%", (int)(mutation * 100));
        nvgText(args.vg, col2X, col2Y, buf, NULL);
        col2Y += lineHeight * 1.3f;

        // Status at bottom of column 2
        if (module->frozen) {
            nvgFillColor(args.vg, nvgRGB(0x00, 0xcc, 0xff));
            nvgText(args.vg, col2X, col2Y, "> FROZEN", NULL);
        } else {
            nvgFillColor(args.vg, terminalDim);
            nvgText(args.vg, col2X, col2Y, "> ACTIVE", NULL);
        }

        // Scanlines
        for (float sy = 0; sy < box.size.y; sy += 2.f) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, sy, box.size.x, 1.f);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 60));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }
};

// ============================================================================
// PARTICLE DISPLAY WIDGET
// ============================================================================

struct ParticleDisplayWidget : TransparentWidget {
    Fatebinder* module = nullptr;

    // Military radar CRT phosphor colors - Orange/Amber/Red spectrum
    NVGcolor ringColors[3] = {
        nvgRGB(0xff, 0x55, 0x00),  // Ring 1: Orange - Primary targets
        nvgRGB(0xff, 0xaa, 0x00),  // Ring 2: Amber - Harmonic signals
        nvgRGB(0xff, 0x11, 0x00)   // Ring 3: Deep Red - Counter rhythm
    };

    NVGcolor ringDim[3] = {
        nvgRGB(0x99, 0x33, 0x00),  // Dim orange
        nvgRGB(0x99, 0x66, 0x00),  // Dim amber
        nvgRGB(0x88, 0x0a, 0x00)   // Dim red
    };

    NVGcolor gridColor = nvgRGBA(0x66, 0x33, 0x00, 0x40);  // Dark orange grid
    NVGcolor bgGlowColor = nvgRGBA(0x22, 0x11, 0x00, 0xff); // Dark military green/black

    float scanlinePhase = -1.f;
    float sweepAngle = 0.f;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        if (!module) return;

        nvgSave(args.vg);

        // Center coordinates
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float radius = std::min(cx, cy) * 0.85f;

        // ================================================================
        // MILITARY RADAR SCREEN BACKGROUND
        // ================================================================

        // Dark background with subtle radial gradient (like a CRT tube)
        nvgBeginPath(args.vg);
        NVGpaint bgPaint = nvgRadialGradient(args.vg, cx, cy, 0, radius * 1.1f,
            nvgRGBA(0x18, 0x0c, 0x00, 0xff),  // Dark center
            nvgRGBA(0x00, 0x00, 0x00, 0xff)); // Black edges
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillPaint(args.vg, bgPaint);
        nvgFill(args.vg);

        // Subtle phosphor glow around screen
        nvgBeginPath(args.vg);
        NVGpaint screenGlow = nvgRadialGradient(args.vg, cx, cy, radius * 0.95f, radius * 1.05f,
            nvgRGBA(0x66, 0x22, 0x00, 0x30),
            nvgRGBA(0x00, 0x00, 0x00, 0x00));
        nvgCircle(args.vg, cx, cy, radius * 1.05f);
        nvgFillPaint(args.vg, screenGlow);
        nvgFill(args.vg);

        // ================================================================
        // RANGE RINGS (like radar concentric circles)
        // ================================================================

        int steps = (int)module->params[Fatebinder::STEPS_PARAM].getValue();

        // Draw 4 concentric range rings
        for (int ring = 1; ring <= 4; ring++) {
            float ringRadius = radius * (ring / 4.0f);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx, cy, ringRadius);
            nvgStrokeColor(args.vg, nvgRGBA(0x44, 0x22, 0x00, 0x60));
            nvgStrokeWidth(args.vg, 0.75f);
            nvgStroke(args.vg);
        }

        // Draw radial spokes (like compass bearings)
        for (int i = 0; i < steps; i++) {
            float angle = (float)i / (float)steps * 2.f * M_PI - M_PI * 0.5f;
            float x1 = cx;
            float y1 = cy;
            float x2 = cx + std::cos(angle) * radius;
            float y2 = cy + std::sin(angle) * radius;

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeColor(args.vg, nvgRGBA(0x33, 0x18, 0x00, 0x30));
            nvgStrokeWidth(args.vg, 0.5f);
            nvgStroke(args.vg);
        }

        // ================================================================
        // EUCLIDEAN PATTERN VISUALIZATION (3 concentric rings)
        // ================================================================

        // Draw each ring's euclidean pattern as a colored ring
        for (int lay = 0; lay < 3; lay++) {
            float ringRadius = radius * (0.65f + lay * 0.15f); // Ring 1: 65%, Ring 2: 80%, Ring 3: 95%
            NVGcolor ringColor = ringColors[lay];
            NVGcolor dimColor = ringDim[lay];

            for (int i = 0; i < steps; i++) {
                float angle = (float)i / (float)steps * 2.f * M_PI - M_PI * 0.5f;
                float x = cx + std::cos(angle) * ringRadius;
                float y = cy + std::sin(angle) * ringRadius;

                if (module->rings[lay].getStep(i)) {
                    // Hit - bright dot
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x, y, 3.5f);
                    nvgFillColor(args.vg, nvgRGBA(dimColor.r * 255, dimColor.g * 255, dimColor.b * 255, 220));
                    nvgFill(args.vg);

                    // Inner glow
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x, y, 2.f);
                    nvgFillColor(args.vg, ringColor);
                    nvgFill(args.vg);
                } else {
                    // No hit - very dim dot
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x, y, 1.5f);
                    nvgFillColor(args.vg, nvgRGBA(dimColor.r * 255, dimColor.g * 255, dimColor.b * 255, 60));
                    nvgFill(args.vg);
                }
            }

            // Draw connecting arc between hits for visual clarity
            nvgBeginPath(args.vg);
            for (int i = 0; i < steps; i++) {
                float angle = (float)i / (float)steps * 2.f * M_PI - M_PI * 0.5f;
                float x = cx + std::cos(angle) * ringRadius;
                float y = cy + std::sin(angle) * ringRadius;

                if (i == 0) {
                    nvgMoveTo(args.vg, x, y);
                } else {
                    nvgLineTo(args.vg, x, y);
                }
            }
            nvgClosePath(args.vg);
            nvgStrokeColor(args.vg, nvgRGBA(dimColor.r * 255, dimColor.g * 255, dimColor.b * 255, 40));
            nvgStrokeWidth(args.vg, 0.75f);
            nvgStroke(args.vg);
        }

        // ================================================================
        // PARTICLES - Military radar "blips"
        // ================================================================

        for (const auto& p : module->particles) {
            float px = cx + p.x * radius;
            float py = cy + p.y * radius;
            float brightness = p.brightness;

            if (brightness > 0.01f && p.ringIndex >= 0 && p.ringIndex < 3) {
                NVGcolor color = ringColors[p.ringIndex];

                // Strong particle core (radar blip)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, px, py, 2.5f * brightness);
                nvgFillColor(args.vg, nvgRGBAf(
                    color.r * brightness,
                    color.g * brightness,
                    color.b * brightness,
                    brightness));
                nvgFill(args.vg);

                // Wide phosphor bloom (CRT glow)
                NVGpaint particleGlow = nvgRadialGradient(args.vg, px, py, 0, 12.f * brightness,
                    nvgRGBAf(color.r * brightness * 0.8f, color.g * brightness * 0.8f,
                            color.b * brightness * 0.8f, brightness * 0.6f),
                    nvgRGBAf(0.f, 0.f, 0.f, 0.f));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, px, py, 12.f * brightness);
                nvgFillPaint(args.vg, particleGlow);
                nvgFill(args.vg);
            }
        }

        // ================================================================
        // CURRENT STEP INDICATORS (bright sweeping dots on each ring)
        // ================================================================

        if (steps > 0) {
            for (int lay = 0; lay < 3; lay++) {
                float ringRadius = radius * (0.65f + lay * 0.15f); // Match pattern ring positions
                float angle = (float)module->currentStep[lay] / (float)steps * 2.f * M_PI - M_PI * 0.5f;
                float x = cx + std::cos(angle) * ringRadius;
                float y = cy + std::sin(angle) * ringRadius;

                NVGcolor color = ringColors[lay];

                // Bright indicator
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, x, y, 5.f);
                nvgFillColor(args.vg, color);
                nvgFill(args.vg);

                // Strong glow
                NVGpaint glow = nvgRadialGradient(args.vg, x, y, 0, 16.f,
                    nvgRGBAf(color.r * 0.9f, color.g * 0.9f, color.b * 0.9f, 0.8f),
                    nvgRGBAf(0.f, 0.f, 0.f, 0.f));
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, x, y, 16.f);
                nvgFillPaint(args.vg, glow);
                nvgFill(args.vg);

                // Radial line from center to current position (sweep line)
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx, cy);
                nvgLineTo(args.vg, x, y);
                nvgStrokeColor(args.vg, nvgRGBAf(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 0.3f));
                nvgStrokeWidth(args.vg, 1.5f);
                nvgStroke(args.vg);
            }
        }

        // ================================================================
        // RING NUMBER LABELS
        // ================================================================

        // Position labels at the right side of the display
        float labelX = cx + radius * 1.05f;
        const char* ringLabels[] = {"1", "2", "3"};

        nvgFontSize(args.vg, 10.f);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (int i = 0; i < 3; i++) {
            float ringRadius = radius * (0.65f + i * 0.15f);
            float labelY = cy - ringRadius;

            // Draw label background for better visibility
            nvgBeginPath(args.vg);
            float textBounds[4];
            nvgTextBounds(args.vg, labelX, labelY, ringLabels[i], NULL, textBounds);
            float padding = 2.f;
            nvgRect(args.vg, textBounds[0] - padding, textBounds[1] - padding,
                    textBounds[2] - textBounds[0] + padding * 2,
                    textBounds[3] - textBounds[1] + padding * 2);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 180));
            nvgFill(args.vg);

            // Draw label text with ring color
            nvgFillColor(args.vg, ringColors[i]);
            nvgText(args.vg, labelX, labelY, ringLabels[i], NULL);
        }

        // ================================================================
        // CRT SCANLINES
        // ================================================================

        const float scanSpacing = 1.5f;
        const float scanThickness = 0.45f;
        if (scanlinePhase < 0.f) {
            scanlinePhase = rack::random::uniform() * scanSpacing;
        }
        float localStart = scanlinePhase;
        while (localStart > 0.f) {
            localStart -= scanSpacing;
        }
        for (float y = localStart; y < box.size.y; y += scanSpacing) {
            if (y + scanThickness < 0.f) {
                continue;
            }
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, y, box.size.x, scanThickness);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 22));
            nvgFill(args.vg);
        }

        // Screen vignette (darker edges)
        nvgBeginPath(args.vg);
        NVGpaint vignette = nvgRadialGradient(args.vg, cx, cy, radius * 0.5f, radius * 1.1f,
            nvgRGBA(0, 0, 0, 0),
            nvgRGBA(0, 0, 0, 100));
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillPaint(args.vg, vignette);
        nvgFill(args.vg);

        nvgRestore(args.vg);
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
        auto resizeKnob = [&](app::ParamWidget* knob, float diameterMm) {
            if (!knob) {
                return;
            }
            Vec center = knob->box.pos.plus(knob->box.size.div(2.f));
            Vec newSize = LayoutHelper::mm2px(Vec(diameterMm, diameterMm));
            knob->box.size = newSize;
            knob->box.pos = center.minus(newSize.div(2.f));
        };

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
        addChild(unifiedDisplay);

        // ====================================================================
        // CONTROLS - 4 columns with better spacing for 20HP
        // ====================================================================
        const float col1X = 18.08f;  // Rhythm
        const float col2X = 37.08f;  // Probability
        const float col3X = 64.58f;  // Mutation
        const float col4X = 83.58f;  // Envelope

        const float row1Y = 54.0f;
        const float rowSpacing = 10.5f;

        // Column 1: Rhythm controls
        auto* stepsKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("steps-knob", col1X, row1Y), module, Fatebinder::STEPS_PARAM);
        resizeKnob(stepsKnob, 14.f);
        addKnobWithShadow(this, stepsKnob);
        auto* hitsKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("hits-knob", col1X, row1Y + rowSpacing), module, Fatebinder::HITS_PARAM);
        resizeKnob(hitsKnob, 14.f);
        addKnobWithShadow(this, hitsKnob);
        auto* rotationKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("rotation-knob", col1X, row1Y + rowSpacing * 2), module, Fatebinder::ROTATION_PARAM);
        resizeKnob(rotationKnob, 14.f);
        addKnobWithShadow(this, rotationKnob);
        auto* ring2Knob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("ring2-knob", col1X, row1Y + rowSpacing * 3), module, Fatebinder::RING_2_DIV_PARAM);
        resizeKnob(ring2Knob, 14.f);
        addKnobWithShadow(this, ring2Knob);
        auto* ring3Knob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("ring3-knob", col1X, row1Y + rowSpacing * 4), module, Fatebinder::RING_3_DIV_PARAM);
        resizeKnob(ring3Knob, 14.f);
        addKnobWithShadow(this, ring3Knob);

        // Column 2: Probability controls
        auto* probabilityKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("probability-knob", col2X, row1Y + 2.f), module, Fatebinder::PROBABILITY_PARAM);
        resizeKnob(probabilityKnob, 16.f);
        addKnobWithShadow(this, probabilityKnob);
        auto* chaosKnob = createParamCentered<ShapetakerKnobVintageMedium>(centerPx("chaos-knob", col2X, row1Y + rowSpacing + 7.f), module, Fatebinder::CHAOS_PARAM);
        resizeKnob(chaosKnob, 20.f);
        addKnobWithShadow(this, chaosKnob);
        auto* densityKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("density-knob", col2X, row1Y + rowSpacing * 2 + 12.f), module, Fatebinder::DENSITY_PARAM);
        resizeKnob(densityKnob, 14.f);
        addKnobWithShadow(this, densityKnob);

        // Column 3: Mutation controls
        auto* tempoKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("tempo-knob", col3X, row1Y), module, Fatebinder::TEMPO_PARAM);
        resizeKnob(tempoKnob, 16.f);
        addKnobWithShadow(this, tempoKnob);
        auto* mutationKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("mutation-knob", col3X, row1Y + rowSpacing), module, Fatebinder::MUTATION_RATE_PARAM);
        resizeKnob(mutationKnob, 14.f);
        addKnobWithShadow(this, mutationKnob);

        // Freeze/Reset buttons
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("freeze-btn", col3X, row1Y + rowSpacing * 2), module, Fatebinder::FREEZE_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("reset-btn", col3X, row1Y + rowSpacing * 3), module, Fatebinder::RESET_PARAM));

        // Overlap mode switch
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(centerPx("overlap-switch", col3X, row1Y + rowSpacing * 4), module, Fatebinder::OVERLAP_MODE_PARAM));

        // Column 4: Envelope controls
        auto* attackKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("attack-knob", col4X, row1Y), module, Fatebinder::ATTACK_PARAM);
        resizeKnob(attackKnob, 14.f);
        addKnobWithShadow(this, attackKnob);
        auto* decayKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("decay-knob", col4X, row1Y + rowSpacing), module, Fatebinder::DECAY_PARAM);
        resizeKnob(decayKnob, 14.f);
        addKnobWithShadow(this, decayKnob);
        auto* curveKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("curve-knob", col4X, row1Y + rowSpacing * 2), module, Fatebinder::CURVE_PARAM);
        resizeKnob(curveKnob, 14.f);
        addKnobWithShadow(this, curveKnob);
        auto* shapeKnob = createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("shape-knob", col4X, row1Y + rowSpacing * 3), module, Fatebinder::SHAPE_PARAM);
        resizeKnob(shapeKnob, 14.f);
        addKnobWithShadow(this, shapeKnob);

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

        // ====================================================================
        // OUTPUTS - Bottom row
        // ====================================================================
        const float outputY = 119.5f;

        // Bipolar/Unipolar toggle switch (above outputs)
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(centerPx("bipolar-toggle", ioStartX + ioSpacing * 5, outputY - 10.f), module, Fatebinder::BIPOLAR_PARAM));

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
                // Re-initialize patterns with new mode
                int steps = (int)module->params[Fatebinder::STEPS_PARAM].getValue();
                int hits = (int)module->params[Fatebinder::HITS_PARAM].getValue();
                int rotation = (int)module->params[Fatebinder::ROTATION_PARAM].getValue();
                for (int i = 0; i < module->kNumRings; i++) {
                    module->rings[i].initialize(steps, hits, rotation, module->rhythmMode);
                }
            }
        ));

        menu->addChild(createCheckMenuItem("L-System", "",
            [=]() { return module->rhythmMode == LSYSTEM_MODE; },
            [=]() {
                module->rhythmMode = LSYSTEM_MODE;
                // Re-initialize patterns with new mode
                int steps = (int)module->params[Fatebinder::STEPS_PARAM].getValue();
                int hits = (int)module->params[Fatebinder::HITS_PARAM].getValue();
                int rotation = (int)module->params[Fatebinder::ROTATION_PARAM].getValue();
                for (int i = 0; i < module->kNumRings; i++) {
                    module->rings[i].initialize(steps, hits, rotation, module->rhythmMode);
                }
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
    }
};

Model* modelFatebinder = createModel<Fatebinder, FatebinderWidget>("Fatebinder");
