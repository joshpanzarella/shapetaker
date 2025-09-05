#include "plugin.hpp"
#include "transmutation/view.hpp"
#include "transmutation/ui.hpp"
#include "transmutation/chords.hpp"
#include "transmutation/engine.hpp"
#include "transmutation/widgets.hpp"
#include <vector>
#include <array>
#include <string>
#include <random>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include "voice/PolyOut.hpp"

using stx::transmutation::ChordPack;
using stx::transmutation::ChordData;

// Forward declarations
struct Transmutation;

// Custom Shapetaker Widgets: use shared declarations from plugin.hpp / shapetakerWidgets.hpp

struct Transmutation : Module,
    public stx::transmutation::TransmutationView,
    public stx::transmutation::TransmutationController {
    enum ParamId {
        // Edit mode controls
        EDIT_A_PARAM,
        EDIT_B_PARAM,

        // Display style
        SCREEN_STYLE_PARAM,

        // Sequence controls
        LENGTH_A_PARAM,
        LENGTH_B_PARAM,
        START_A_PARAM,
        STOP_A_PARAM,
        RESET_A_PARAM,
        START_B_PARAM,
        STOP_B_PARAM,
        RESET_B_PARAM,

        // Clock controls
        INTERNAL_CLOCK_PARAM,
        BPM_MULTIPLIER_PARAM,

        // Sequence B mode
        SEQ_B_MODE_PARAM,

        // Alchemical symbol buttons (12)
        SYMBOL_1_PARAM,
        SYMBOL_2_PARAM,
        SYMBOL_3_PARAM,
        SYMBOL_4_PARAM,
        SYMBOL_5_PARAM,
        SYMBOL_6_PARAM,
        SYMBOL_7_PARAM,
        SYMBOL_8_PARAM,
        SYMBOL_9_PARAM,
        SYMBOL_10_PARAM,
        SYMBOL_11_PARAM,
        SYMBOL_12_PARAM,

        // Rest and tie buttons
        REST_PARAM,
        TIE_PARAM,

        // Context-only sliders for randomization probabilities
        CHORD_DENSITY_PARAM,
        REST_PROB_PARAM,
        TIE_PROB_PARAM,

        PARAMS_LEN
    };
    // Grid steps (visual density): 16, 32, or 64
    int gridSteps = 32;
    enum InputId {
        CLOCK_A_INPUT,
        CLOCK_B_INPUT,
        RESET_A_INPUT,
        RESET_B_INPUT,
        START_A_INPUT,
        STOP_A_INPUT,
        START_B_INPUT,
        STOP_B_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        // Sequence A polyphonic outputs (6 voices)
        CV_A_OUTPUT,
        GATE_A_OUTPUT,

        // Sequence B polyphonic outputs (6 voices)  
        CV_B_OUTPUT,
        GATE_B_OUTPUT,

        OUTPUTS_LEN
    };

    enum LightId {
        RUNNING_A_LIGHT,
        RUNNING_B_LIGHT,

        // Alchemical symbol lights (12 symbols × 3 colors each = 36 lights)
        SYMBOL_1_LIGHT,
        SYMBOL_1_LIGHT_GREEN = SYMBOL_1_LIGHT + 1,
        SYMBOL_1_LIGHT_BLUE = SYMBOL_1_LIGHT + 2,
        SYMBOL_2_LIGHT,
        SYMBOL_2_LIGHT_GREEN = SYMBOL_2_LIGHT + 1,
        SYMBOL_2_LIGHT_BLUE = SYMBOL_2_LIGHT + 2,
        SYMBOL_3_LIGHT,
        SYMBOL_3_LIGHT_GREEN = SYMBOL_3_LIGHT + 1,
        SYMBOL_3_LIGHT_BLUE = SYMBOL_3_LIGHT + 2,
        SYMBOL_4_LIGHT,
        SYMBOL_4_LIGHT_GREEN = SYMBOL_4_LIGHT + 1,
        SYMBOL_4_LIGHT_BLUE = SYMBOL_4_LIGHT + 2,
        SYMBOL_5_LIGHT,
        SYMBOL_5_LIGHT_GREEN = SYMBOL_5_LIGHT + 1,
        SYMBOL_5_LIGHT_BLUE = SYMBOL_5_LIGHT + 2,
        SYMBOL_6_LIGHT,
        SYMBOL_6_LIGHT_GREEN = SYMBOL_6_LIGHT + 1,
        SYMBOL_6_LIGHT_BLUE = SYMBOL_6_LIGHT + 2,
        SYMBOL_7_LIGHT,
        SYMBOL_7_LIGHT_GREEN = SYMBOL_7_LIGHT + 1,
        SYMBOL_7_LIGHT_BLUE = SYMBOL_7_LIGHT + 2,
        SYMBOL_8_LIGHT,
        SYMBOL_8_LIGHT_GREEN = SYMBOL_8_LIGHT + 1,
        SYMBOL_8_LIGHT_BLUE = SYMBOL_8_LIGHT + 2,
        SYMBOL_9_LIGHT,
        SYMBOL_9_LIGHT_GREEN = SYMBOL_9_LIGHT + 1,
        SYMBOL_9_LIGHT_BLUE = SYMBOL_9_LIGHT + 2,
        SYMBOL_10_LIGHT,
        SYMBOL_10_LIGHT_GREEN = SYMBOL_10_LIGHT + 1,
        SYMBOL_10_LIGHT_BLUE = SYMBOL_10_LIGHT + 2,
        SYMBOL_11_LIGHT,
        SYMBOL_11_LIGHT_GREEN = SYMBOL_11_LIGHT + 1,
        SYMBOL_11_LIGHT_BLUE = SYMBOL_11_LIGHT + 2,
        SYMBOL_12_LIGHT,
        SYMBOL_12_LIGHT_GREEN = SYMBOL_12_LIGHT + 1,
        SYMBOL_12_LIGHT_BLUE = SYMBOL_12_LIGHT + 2,

        LIGHTS_LEN
    };

    // Sequencer state
    Sequence sequenceA;
    Sequence sequenceB;

    // Edit mode state
    bool editModeA = false;
    bool editModeB = false;
    int selectedSymbol = -1;

    // Symbol preview display system (8-bit retro style)
    std::string displayChordName = "";
    int displaySymbolId = -999; // -999 means no symbol display
    float symbolPreviewTimer = 0.0f;
    static constexpr float SYMBOL_PREVIEW_DURATION = 0.50f; // Show for 500ms
    bool spookyTvMode = true; // Toggle for spooky TV effect vs clean display (backed by SCREEN_STYLE_PARAM)
    bool doubleOccupancyMode = false; // Visual split mode for step circles
    // Sticky UI params that must not be altered by randomization
    int stickySeqBMode = 0;
    float stickyScreenStyle = 1.f;

    // Chord pack system
    ChordPack currentChordPack;
    std::array<int, st::SymbolCount> symbolToChordMapping; // Mapping for all symbols
    std::array<int, 12> buttonToSymbolMapping; // Maps button positions 0-11 to symbol IDs 0..(st::SymbolCount-1)
    std::array<float, 12> buttonPressAnim;     // 1.0 on press, decays to 0 for animation

    // Clock system
    float internalClock = 0.0f;
    float clockRate = 120.0f; // BPM
    double engineTimeSec = 0.0; // running wallclock for scheduling

    // Output shaping / gate driving
    // Small CV slew per voice (optional)
    dsp::SlewLimiter cvSlewA[stx::transmutation::MAX_VOICES];
    dsp::SlewLimiter cvSlewB[stx::transmutation::MAX_VOICES];

    // Tunables
    bool enableCvSlew = false; // Disabled by default - slewing is bad for polyphonic chords
    float cvSlewMs = 3.0f;        // per-step pitch slew to soften discontinuities
    bool stablePolyChannels = true; // Keep poly channel count stable across steps to avoid gate/channel drops

    // Groove engine
    bool grooveEnabled = false;
    float grooveAmount = 0.0f; // 0..1
    enum GroovePreset { GROOVE_NONE = 0, GROOVE_SWING8 = 1, GROOVE_SWING16 = 2, GROOVE_SHUFFLE16 = 3, GROOVE_REGGAETON = 4 };
    GroovePreset groovePreset = GROOVE_NONE;

    // Built-in 16-step offset tables (fraction of step period, clamped >= 0)
    // Values in [0..0.5) recommended. Amount scales these.
    const float* getGrooveTable(GroovePreset p) const {
        static float none[16] = {0};
        static float swing8[16];
        static float swing16[16];
        static float shuffle16[16];
        static float dembow[16];
        static bool inited = false;
        if (!inited) {
            // Swing 8ths: delay off-beats (steps 2,4,6,8,10,12,14,16) by 33%
            for (int i = 0; i < 16; ++i) {
                swing8[i] = ((i % 2) == 1) ? 0.33f : 0.f; // 0-based: 1,3,5,... are the "&" of the beat
            }
            // Swing 16ths: delay 2 and 4 in each group of 4 (0-based indices 1 and 3)
            for (int i = 0; i < 16; ++i) {
                int pos4 = i % 4;
                swing16[i] = (pos4 == 1 || pos4 == 3) ? 0.20f : 0.f;
            }
            // Shuffle16: slightly delay step 2 in each group of 2 16ths (i odd)
            for (int i = 0; i < 16; ++i) {
                shuffle16[i] = ((i % 2) == 1) ? 0.12f : 0.f;
            }
            // Reggaeton (dembow-inspired) microdelay emphasis
            // Slight delays on characteristic backbeats: 4, 7-8, 12-13 (0-based)
            for (int i = 0; i < 16; ++i) dembow[i] = 0.f;
            dembow[3] = 0.10f; // beat 2 backbeat
            dembow[6] = 0.16f; dembow[7] = 0.10f; // syncopation
            dembow[11] = 0.14f; dembow[12] = 0.08f; // late hits
            inited = true;
        }
        switch (p) {
            case GROOVE_SWING8: return swing8;
            case GROOVE_SWING16: return swing16;
            case GROOVE_SHUFFLE16: return shuffle16;
            case GROOVE_REGGAETON: return dembow;
            default: return none;
        }
    }

    // =========================
    // Chord pack normalization
    // - ensure no duplicate chords per pack (prefer inversions to differentiate)
    // - sanitize pack name to avoid duplicate words (e.g., repeated "neon")
    // =========================

    static std::string sanitizeNameWords(const std::string& name) {
        std::stringstream ss(name);
        std::string word;
        std::unordered_set<std::string> seen;
        std::vector<std::string> kept;
        kept.reserve(8);
        while (ss >> word) {
            std::string low = word;
            std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            if (seen.insert(low).second) kept.push_back(word);
        }
        if (kept.empty()) return name;
        std::string out;
        for (size_t i = 0; i < kept.size(); ++i) {
            if (i) out.push_back(' ');
            out += kept[i];
        }
        return out;
    }

    static std::string canonicalKey(const std::vector<float>& intervals) {
        if (intervals.empty()) return "";
        std::vector<int> v; v.reserve(intervals.size());
        for (float f : intervals) v.push_back((int)std::round(f));
        std::sort(v.begin(), v.end());
        int base = v.front();
        for (int& x : v) x -= base;
        // Build key
        std::string key;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) key.push_back(',');
            key += std::to_string(v[i]);
        }
        return key;
    }

    static std::vector<float> invertIntervals(const std::vector<float>& in, int inversionIndex) {
        // Convert to ints for stable operations
        if (in.empty()) return in;
        std::vector<int> v; v.reserve(in.size());
        for (float f : in) v.push_back((int)std::round(f));
        std::sort(v.begin(), v.end());
        int n = (int)v.size();
        if (n <= 1) return in;
        int k = inversionIndex % n;
        if (k <= 0) return in;
        // Build rotated list: [v[k..n-1], v[0]+12, ..., v[k-1]+12]
        std::vector<int> u; u.reserve(n);
        int base = v[k];
        for (int i = k; i < n; ++i) u.push_back(v[i]);
        for (int i = 0; i < k; ++i) u.push_back(v[i] + 12);
        // Renormalize to start at 0
        for (int& x : u) x -= base;
        std::vector<float> out; out.reserve(n);
        for (int x : u) out.push_back((float)x);
        return out;
    }

    static std::vector<float> octaveSpreadVariant(const std::vector<float>& in) {
        if (in.empty()) return in;
        std::vector<int> v; v.reserve(in.size());
        for (float f : in) v.push_back((int)std::round(f));
        std::sort(v.begin(), v.end());
        // Raise the top interval by an octave to create a distinct voicing
        v.back() += 12;
        // Keep absolute values; canonical check will normalize
        std::vector<float> out; out.reserve(v.size());
        for (int x : v) out.push_back((float)x);
        return out;
    }

    void normalizeChordPack(ChordPack& pack) {
        // Sanitize pack name (remove duplicate words)
        pack.name = sanitizeNameWords(pack.name);

        std::unordered_set<std::string> seen;
        for (auto& chord : pack.chords) {
            std::string key0 = canonicalKey(chord.intervals);
            if (key0.empty()) continue;
            if (seen.insert(key0).second) continue; // unique as-is

            // Try inversions to make it unique
            bool madeUnique = false;
            int nI = (int)std::max<size_t>(1, chord.intervals.size()) - 1;
            for (int k = 1; k <= nI; ++k) {
                auto inv = invertIntervals(chord.intervals, k);
                std::string keyInv = canonicalKey(inv);
                if (!keyInv.empty() && seen.find(keyInv) == seen.end()) {
                    chord.intervals = inv;
                    seen.insert(keyInv);
                    // Tag name to indicate inversion applied
                    chord.name += " (Inv " + std::to_string(k) + ")";
                    madeUnique = true;
                    break;
                }
            }
            if (madeUnique) continue;

            // Fallback: create an octave-spread variant
            auto var = octaveSpreadVariant(chord.intervals);
            std::string keyV = canonicalKey(var);
            if (!keyV.empty() && seen.find(keyV) == seen.end()) {
                chord.intervals = var;
                seen.insert(keyV);
                chord.name += " (Oct+)";
            } else {
                // As a last resort, keep but don't duplicate in 'seen' so we avoid cascading changes
                // Alternatively, we could drop it; but preserve user data.
            }
        }
    }

    // Compute per-step micro-delay (seconds), clamped to [0 .. 0.45 * stepPeriod]
    float computeGrooveDelaySec(const Sequence& seq, int nextIndex, float stepPeriodSec) const {
        if (!grooveEnabled || grooveAmount <= 0.f) return 0.f;
        const float* table = getGrooveTable(groovePreset);
        int idx = (nextIndex % 16 + 16) % 16;
        float frac = table[idx];
        float delay = grooveAmount * frac * stepPeriodSec;
        float maxDelay = 0.45f * stepPeriodSec; // keep within half step to avoid overlap
        if (delay < 0.f) delay = 0.f;
        if (delay > maxDelay) delay = maxDelay;
        return delay;
    }

    // Force chord updates when parameters change during playback
    bool forceChordUpdateA = false;
    bool forceChordUpdateB = false;

    enum GateMode { GATE_SUSTAIN = 0, GATE_PULSE = 1 };
    GateMode gateMode = GATE_SUSTAIN;
    float gatePulseMs = 8.0f;     // pulse width when in pulse mode
    dsp::PulseGenerator gatePulsesA[stx::transmutation::MAX_VOICES];
    dsp::PulseGenerator gatePulsesB[stx::transmutation::MAX_VOICES];

    // Placement / voicing
    bool oneVoiceRandomNote = false;     // when a step is 1-voice, select a random chord tone instead of the first
    bool randomizeChordVoicing = false;  // when multi-voice, randomize target notes ordering per placement
    bool harmonyLimitVoices = true;      // in Harmony mode, limit B to 1–2 voices (sparser counterpoint)
    int lastStepA = -1;
    int lastStepB = -1;

    // Polyphony policy
    bool forceSixPoly = false;    // when true, always output 6 channels (duplicates/extends chord tones)
    // Force a one-shot reassert of poly channel count to downstream modules
    bool reassertPolyA = false;
    bool reassertPolyB = false;
    // One-shot exact-channel emission (ignore stablePolyChannels for one frame)
    bool oneShotExactPolyA = false;
    bool oneShotExactPolyB = false;
    // Poly test (one frame override)
    bool polyTestA = false;
    bool polyTestB = false;

    // Helper: decide if two resolved steps represent a change that should retrigger
    bool isStepChanged(const SequenceStep* prev, const SequenceStep* curr) const {
        return stx::transmutation::isStepChanged(prev, curr);
    }

    float lastCvA[stx::transmutation::MAX_VOICES] = {0.f,0.f,0.f,0.f,0.f,0.f};
    float lastCvB[stx::transmutation::MAX_VOICES] = {0.f,0.f,0.f,0.f,0.f,0.f};

    // Helper: resolve a step to an effective chord step (follow TIEs backward).
    // Returns nullptr if no playable chord is found.
    const SequenceStep* resolveEffectiveStep(const Sequence& seq, int idx) const {
        return stx::transmutation::resolveEffectiveStep(seq, idx, symbolToChordMapping, currentChordPack);
    }

    // Helper: clear gates but HOLD last CV so releases don't pitch-jump to 0V
    void stableClearOutputs(int cvOutputId, int gateOutputId) {
        // Keep current channel count (or fall back to MAX_VOICES) so downstream modules
        // see a stable number of channels during envelope release tails.
        int ch = outputs[cvOutputId].getChannels();
        if (ch <= 0) ch = 1; // minimal safe fallback
        outputs[cvOutputId].setChannels(ch);
        outputs[gateOutputId].setChannels(ch);

        for (int v = 0; v < ch && v < stx::transmutation::MAX_VOICES; ++v) {
            float last = (cvOutputId == CV_A_OUTPUT) ? lastCvA[v] : lastCvB[v];
            outputs[cvOutputId].setVoltage(last, v);   // hold pitch
            outputs[gateOutputId].setVoltage(0.f, v);  // gate low
        }
    }

    // Apply gate policy for current step
    void applyGates(const ProcessArgs& args, int gateOutputId, dsp::PulseGenerator pulses[stx::transmutation::MAX_VOICES], int activeVoices, bool stepChanged) {
        bool exact = (gateOutputId == GATE_A_OUTPUT) ? oneShotExactPolyA : oneShotExactPolyB;
        int totalChannels = (stablePolyChannels && !exact) ? stx::transmutation::MAX_VOICES : rack::math::clamp(activeVoices, 1, stx::transmutation::MAX_VOICES);
        stx::transmutation::applyGates(args, outputs.data(), gateOutputId, pulses, activeVoices,
            gateMode == GATE_SUSTAIN ? stx::transmutation::GATE_SUSTAIN : stx::transmutation::GATE_PULSE,
            gatePulseMs, stepChanged, totalChannels);
        if (exact) {
            if (gateOutputId == GATE_A_OUTPUT) oneShotExactPolyA = false; else oneShotExactPolyB = false;
        }
    }

    // Triggers
    dsp::SchmittTrigger editATrigger;
    dsp::SchmittTrigger editBTrigger;
    dsp::SchmittTrigger startATrigger;
    dsp::SchmittTrigger stopATrigger;
    dsp::SchmittTrigger resetATrigger;
    dsp::SchmittTrigger startBTrigger;
    dsp::SchmittTrigger stopBTrigger;
    dsp::SchmittTrigger resetBTrigger;
    dsp::SchmittTrigger symbolTriggers[12];
    dsp::SchmittTrigger restTrigger;
    dsp::SchmittTrigger tieTrigger;
    dsp::SchmittTrigger clockATrigger;
    dsp::SchmittTrigger clockBTrigger;
    dsp::SchmittTrigger resetAInputTrigger;
    dsp::SchmittTrigger resetBInputTrigger;
    dsp::SchmittTrigger startAInputTrigger;
    dsp::SchmittTrigger stopAInputTrigger;
    dsp::SchmittTrigger startBInputTrigger;
    dsp::SchmittTrigger stopBInputTrigger;

    Transmutation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Edit mode buttons
        configParam(EDIT_A_PARAM, 0.f, 1.f, 0.f, "Edit Transmutation A");
        configParam(EDIT_B_PARAM, 0.f, 1.f, 0.f, "Edit Transmutation B");

        // Screen style (0 = Clean, 1 = Spooky)
        configSwitch(SCREEN_STYLE_PARAM, 0.f, 1.f, 1.f, "Screen Style", {"Clean", "Spooky"});

        // Transmutation controls
        configParam(LENGTH_A_PARAM, 1.f, 64.f, 16.f, "Transmutation A Length");
        paramQuantities[LENGTH_A_PARAM]->snapEnabled = true;
        configParam(LENGTH_B_PARAM, 1.f, 64.f, 16.f, "Transmutation B Length");
        paramQuantities[LENGTH_B_PARAM]->snapEnabled = true;
        configParam(START_A_PARAM, 0.f, 1.f, 0.f, "Start Transmutation A");
        configParam(STOP_A_PARAM, 0.f, 1.f, 0.f, "Stop Transmutation A");
        configParam(RESET_A_PARAM, 0.f, 1.f, 0.f, "Reset Transmutation A");
        configParam(START_B_PARAM, 0.f, 1.f, 0.f, "Start Transmutation B");
        configParam(STOP_B_PARAM, 0.f, 1.f, 0.f, "Stop Transmutation B");
        configParam(RESET_B_PARAM, 0.f, 1.f, 0.f, "Reset Transmutation B");

        // Clock control
        configParam(INTERNAL_CLOCK_PARAM, 20.f, 200.f, 120.f, "Internal Clock", " BPM");
        paramQuantities[INTERNAL_CLOCK_PARAM]->snapEnabled = true;

        // BPM Multiplier
        configParam(BPM_MULTIPLIER_PARAM, 0.f, 3.f, 0.f, "BPM Multiplier");
        paramQuantities[BPM_MULTIPLIER_PARAM]->snapEnabled = true;

        // Transmutation B mode (0=Independent, 1=Harmony, 2=Lock)
        configSwitch(SEQ_B_MODE_PARAM, 0.f, 2.f, 0.f, "Transmutation B Mode", {"Independent", "Harmony", "Lock"});

        // Ensure Spooky TV effect defaults to ON
        params[SCREEN_STYLE_PARAM].setValue(1.f);

        // Alchemical symbol buttons
        for (int i = 0; i < 12; i++) {
            configParam(SYMBOL_1_PARAM + i, 0.f, 1.f, 0.f, "Alchemical Symbol " + std::to_string(i + 1));
        }

        // Rest and tie
        configParam(REST_PARAM, 0.f, 1.f, 0.f, "Rest");
        configParam(TIE_PARAM, 0.f, 1.f, 0.f, "Tie");

        // Context-only sliders (0..1) for randomization probabilities
        configParam(CHORD_DENSITY_PARAM, 0.f, 1.f, 0.60f, "Chord Density");
        configParam(REST_PROB_PARAM, 0.f, 1.f, 0.12f, "Rest Probability");
        configParam(TIE_PROB_PARAM, 0.f, 1.f, 0.10f, "Tie Probability");

        // Inputs
        configInput(CLOCK_A_INPUT, "Clock A");
        configInput(CLOCK_B_INPUT, "Clock B");
        configInput(RESET_A_INPUT, "Reset A");
        configInput(RESET_B_INPUT, "Reset B");
        configInput(START_A_INPUT, "Start A Trigger");
        configInput(STOP_A_INPUT, "Stop A Trigger");
        configInput(START_B_INPUT, "Start B Trigger");
        configInput(STOP_B_INPUT, "Stop B Trigger");

        // Outputs
        configOutput(CV_A_OUTPUT, "CV A (Polyphonic)");
        configOutput(GATE_A_OUTPUT, "Gate A (Polyphonic)");
        configOutput(CV_B_OUTPUT, "CV B (Polyphonic)");
        configOutput(GATE_B_OUTPUT, "Gate B (Polyphonic)");

        // Initialize symbol mapping to -1 (no chord assigned)
        symbolToChordMapping.fill(-1);

        // Initialize button mapping to default 0-11 sequence
        for (int i = 0; i < 12; i++) {
            buttonToSymbolMapping[i] = i;
        }

        // Load default chord pack
        loadDefaultChordPack();
        // Default grid to 32 for legibility
        gridSteps = 32;
        // Init button press animations
        for (int i = 0; i < 12; ++i) buttonPressAnim[i] = 0.f;

        // Initialize sticky (non-randomizable) params
        stickySeqBMode = (int)params[SEQ_B_MODE_PARAM].getValue();
        stickyScreenStyle = params[SCREEN_STYLE_PARAM].getValue();
    }

    void process(const ProcessArgs& args) override {
        engineTimeSec += args.sampleTime;
        // Mirror param to internal flag for UI drawing
        spookyTvMode = params[SCREEN_STYLE_PARAM].getValue() > 0.5f;

        // Keep sticky copies of non-randomizable params (for restoring after global randomize)
        stickySeqBMode = (int)params[SEQ_B_MODE_PARAM].getValue();
        stickyScreenStyle = params[SCREEN_STYLE_PARAM].getValue();

        // Randomization floats are controlled directly by context menu sliders

        // Configure slew limiters with current sample rate
        if (enableCvSlew) {
            for (int i = 0; i < 6; ++i) {
                cvSlewA[i].setRiseFall(cvSlewMs / 1000.f, cvSlewMs / 1000.f);
                cvSlewB[i].setRiseFall(cvSlewMs / 1000.f, cvSlewMs / 1000.f);
            }
        }
        // Keep sequence length knobs bounded by current grid size
        {
            float maxLen = (float)gridSteps;
            if (paramQuantities[LENGTH_A_PARAM]->maxValue != maxLen)
                paramQuantities[LENGTH_A_PARAM]->maxValue = maxLen;
            if (paramQuantities[LENGTH_B_PARAM]->maxValue != maxLen)
                paramQuantities[LENGTH_B_PARAM]->maxValue = maxLen;
            // Clamp current values if grid shrank
            if (params[LENGTH_A_PARAM].getValue() > maxLen)
                params[LENGTH_A_PARAM].setValue(maxLen);
            if (params[LENGTH_B_PARAM].getValue() > maxLen)
                params[LENGTH_B_PARAM].setValue(maxLen);
        }
        // Decay symbol button press animations
        for (int i = 0; i < 12; ++i) {
            if (buttonPressAnim[i] > 0.f) {
                buttonPressAnim[i] = std::max(0.f, buttonPressAnim[i] - (float)(args.sampleTime * 6.0));
            }
        }
        // Handle edit mode toggles - ensure one mode is always active
        if (editATrigger.process(params[EDIT_A_PARAM].getValue())) {
            if (!editModeA) {
                // Switching to Edit A mode
                editModeA = true;
                editModeB = false;
            } else {
                // Leaving Edit A mode - enter play mode
                editModeA = false;
                editModeB = false;
            }
        }

        if (editBTrigger.process(params[EDIT_B_PARAM].getValue())) {
            if (!editModeB) {
                // Switching to Edit B mode
                editModeB = true;
                editModeA = false;
            } else {
                // Leaving Edit B mode - enter play mode
                editModeB = false;
                editModeA = false;
            }
        }

        // Ensure at least one mode is active (default to Edit A if somehow none are active)
        if (!editModeA && !editModeB) {
            // This is play mode - explicitly do nothing as this is a valid state
        }

        // Update sequence lengths from parameters (clamped to gridSteps)
        sequenceA.length = clamp((int)params[LENGTH_A_PARAM].getValue(), 1, gridSteps);
        sequenceB.length = clamp((int)params[LENGTH_B_PARAM].getValue(), 1, gridSteps);

        // Handle sequence controls
        if (startATrigger.process(params[START_A_PARAM].getValue())) {
            // Always start from first step
            sequenceA.currentStep = 0;
            sequenceA.clockPhase = 0.0f;
            sequenceA.running = true;
        }
        if (stopATrigger.process(params[STOP_A_PARAM].getValue())) {
            sequenceA.running = false;
        }
        if (resetATrigger.process(params[RESET_A_PARAM].getValue())) {
            sequenceA.currentStep = 0;
            sequenceA.clockPhase = 0.0f;
        }

        if (startBTrigger.process(params[START_B_PARAM].getValue())) {
            // Always start from first step
            sequenceB.currentStep = 0;
            sequenceB.clockPhase = 0.0f;
            sequenceB.running = true;
        }
        if (stopBTrigger.process(params[STOP_B_PARAM].getValue())) {
            sequenceB.running = false;
        }
        if (resetBTrigger.process(params[RESET_B_PARAM].getValue())) {
            sequenceB.currentStep = 0;
            sequenceB.clockPhase = 0.0f;
        }

        // Handle external reset inputs
        if (inputs[RESET_A_INPUT].isConnected()) {
            if (resetAInputTrigger.process(inputs[RESET_A_INPUT].getVoltage())) {
                sequenceA.currentStep = 0;
                sequenceA.clockPhase = 0.0f;
            }
        }

        if (inputs[RESET_B_INPUT].isConnected()) {
            if (resetBInputTrigger.process(inputs[RESET_B_INPUT].getVoltage())) {
                sequenceB.currentStep = 0;
                sequenceB.clockPhase = 0.0f;
            }
        }

        // Handle external start/stop trigger inputs
        if (inputs[START_A_INPUT].isConnected()) {
            if (startAInputTrigger.process(inputs[START_A_INPUT].getVoltage())) {
                // Always start from first step
                sequenceA.currentStep = 0;
                sequenceA.clockPhase = 0.0f;
                sequenceA.running = true;
            }
        }

        if (inputs[STOP_A_INPUT].isConnected()) {
            if (stopAInputTrigger.process(inputs[STOP_A_INPUT].getVoltage())) {
                sequenceA.running = false;
            }
        }

        if (inputs[START_B_INPUT].isConnected()) {
            if (startBInputTrigger.process(inputs[START_B_INPUT].getVoltage())) {
                // Always start from first step
                sequenceB.currentStep = 0;
                sequenceB.clockPhase = 0.0f;
                sequenceB.running = true;
            }
        }

        if (inputs[STOP_B_INPUT].isConnected()) {
            if (stopBInputTrigger.process(inputs[STOP_B_INPUT].getVoltage())) {
                sequenceB.running = false;
            }
        }

        // Handle symbol button presses
        for (int i = 0; i < 12; i++) {
            if (symbolTriggers[i].process(params[SYMBOL_1_PARAM + i].getValue())) {
                // Map button position to actual symbol ID
                int symbolId = buttonToSymbolMapping[i];
                onSymbolPressed(symbolId);
            }
        }

        // Handle rest/tie buttons
        if (restTrigger.process(params[REST_PARAM].getValue())) {
            onSymbolPressed(-1); // Rest symbol
        }
        if (tieTrigger.process(params[TIE_PARAM].getValue())) {
            onSymbolPressed(-2); // Tie symbol
        }

        // Update internal clock
        float baseBPM = params[INTERNAL_CLOCK_PARAM].getValue();
        int multiplierIndex = (int)params[BPM_MULTIPLIER_PARAM].getValue();
        float multipliers[] = {1.0f, 2.0f, 4.0f, 8.0f};
        float multiplier = multipliers[multiplierIndex];
        clockRate = baseBPM * multiplier;
        float clockFreq = clockRate / 60.0f; // Convert BPM to Hz
        internalClock += args.sampleTime * clockFreq;

        // Generate internal clock pulse when needed
        bool internalClockTrigger = false;
        if (internalClock >= 1.0f) {
            internalClock -= 1.0f;
            internalClockTrigger = true;
        }

        // Process sequences
        processSequence(sequenceA, CLOCK_A_INPUT, CV_A_OUTPUT, GATE_A_OUTPUT, args, internalClockTrigger);
        processSequenceB(args, internalClockTrigger);

        // Update chord name display timer
        // Update symbol preview timer
        if (symbolPreviewTimer > 0.0f) {
            symbolPreviewTimer -= args.sampleTime;
            if (symbolPreviewTimer <= 0.0f) {
                displayChordName = "";
                displaySymbolId = -999;
                symbolPreviewTimer = 0.0f;
            }
        }

        // Optional: Poly test override (one frame), applied after sequence processing
        if (polyTestA) {
            const int n = stx::transmutation::MAX_VOICES;
            outputs[CV_A_OUTPUT].setChannels(n);
            outputs[GATE_A_OUTPUT].setChannels(n);
            for (int v = 0; v < n; ++v) {
                float cv = 0.2f * v; // 0, 0.2, 0.4, ...
                outputs[CV_A_OUTPUT].setVoltage(cv, v);
                outputs[GATE_A_OUTPUT].setVoltage(10.f, v);
                lastCvA[v] = cv;
            }
            polyTestA = false;
        }
        if (polyTestB) {
            const int n = stx::transmutation::MAX_VOICES;
            outputs[CV_B_OUTPUT].setChannels(n);
            outputs[GATE_B_OUTPUT].setChannels(n);
            for (int v = 0; v < n; ++v) {
                float cv = 0.2f * v;
                outputs[CV_B_OUTPUT].setVoltage(cv, v);
                outputs[GATE_B_OUTPUT].setVoltage(10.f, v);
                lastCvB[v] = cv;
            }
            polyTestB = false;
        }

        // Update lights
        // Dimmer run lights for a subtler always-on indicator
        lights[RUNNING_A_LIGHT].setBrightness(sequenceA.running ? 0.15f : 0.0f);
        lights[RUNNING_B_LIGHT].setBrightness(sequenceB.running ? 0.15f : 0.0f);

        // Determine effective symbols at current steps (follow ties)
        int effSymA = -1;
        int effSymB = -1;
        if (sequenceA.running) {
            if (const SequenceStep* eff = resolveEffectiveStep(sequenceA, sequenceA.currentStep)) effSymA = eff->chordIndex;
        }
        if (sequenceB.running) {
            if (const SequenceStep* eff = resolveEffectiveStep(sequenceB, sequenceB.currentStep)) effSymB = eff->chordIndex;
        }

        // Trigger a short pulse on the corresponding button each time the step advances
        auto pulseForSymbol = [&](int sym){
            if (!st::isValidSymbolId(sym)) return;
            for (int i = 0; i < 12; ++i) if (buttonToSymbolMapping[i] == sym) { buttonPressAnim[i] = 1.0f; break; }
        };
        if (sequenceA.running && sequenceA.currentStep != lastStepA) { pulseForSymbol(effSymA); lastStepA = sequenceA.currentStep; }
        if (sequenceB.running && sequenceB.currentStep != lastStepB) { pulseForSymbol(effSymB); lastStepB = sequenceB.currentStep; }

        // Update symbol lights with color coding and pulse intensity
        for (int i = 0; i < 12; i++) {
            bool symbolActiveA = sequenceA.running && (effSymA >= 0) && (buttonToSymbolMapping[i] == effSymA);
            bool symbolActiveB = sequenceB.running && (effSymB >= 0) && (buttonToSymbolMapping[i] == effSymB);
            float pulse = rack::math::clamp(buttonPressAnim[i], 0.f, 1.f);

            // RGB Light indices: Red=0, Green=1, Blue=2
            int lightIndex = SYMBOL_1_LIGHT + i * 3;

            float r = 0.f, g = 0.f, b = 0.f;
            if (symbolActiveA) {
                // Teal flash (A): mix G+B
                float intensity = 0.25f + 0.75f * pulse;
                g = std::max(g, intensity);
                b = std::max(b, intensity * 0.7f);
            }
            if (symbolActiveB) {
                // Purple flash (B): mix R+B
                float intensity = 0.25f + 0.75f * pulse;
                r = std::max(r, intensity * 0.7f);
                b = std::max(b, intensity);
            }
            lights[lightIndex + 0].setBrightness(r);
            lights[lightIndex + 1].setBrightness(g);
            lights[lightIndex + 2].setBrightness(b);
        }
    }

    // TransmutationView implementation
    float getInternalClockBpm() override { return params[INTERNAL_CLOCK_PARAM].getValue(); }
    int getBpmMultiplier() override { return (int)params[BPM_MULTIPLIER_PARAM].getValue(); }
    bool isSeqARunning() const override { return sequenceA.running; }
    bool isSeqBRunning() const override { return sequenceB.running; }
    int getSeqACurrentStep() const override { return sequenceA.currentStep; }
    int getSeqALength() const override { return sequenceA.length; }
    int getSeqBCurrentStep() const override { return sequenceB.currentStep; }
    int getSeqBLength() const override { return sequenceB.length; }
    bool isClockAConnected() override { return inputs[CLOCK_A_INPUT].isConnected(); }
    bool isClockBConnected() override { return inputs[CLOCK_B_INPUT].isConnected(); }
    int getSeqBMode() override { return (int)params[SEQ_B_MODE_PARAM].getValue(); }
    bool isEditModeA() const override { return editModeA; }
    bool isEditModeB() const override { return editModeB; }
    int getGridSteps() const override { return gridSteps; }
    int getButtonSymbol(int pos) const override { return (pos >=0 && pos < 12) ? buttonToSymbolMapping[pos] : -999; }
    int getSymbolToChord(int symbolId) const override { return st::isValidSymbolId(symbolId) ? symbolToChordMapping[symbolId] : -1; }
    stx::transmutation::StepInfo getStepA(int idx) const override {
        stx::transmutation::StepInfo s{};
        int i = (idx % sequenceA.length + sequenceA.length) % sequenceA.length;
        s.chordIndex = sequenceA.steps[i].chordIndex;
        s.voiceCount = sequenceA.steps[i].voiceCount;
        s.symbolId   = sequenceA.steps[i].alchemySymbolId;
        return s;
    }
    stx::transmutation::StepInfo getStepB(int idx) const override {
        stx::transmutation::StepInfo s{};
        int i = (idx % sequenceB.length + sequenceB.length) % sequenceB.length;
        s.chordIndex = sequenceB.steps[i].chordIndex;
        s.voiceCount = sequenceB.steps[i].voiceCount;
        s.symbolId   = sequenceB.steps[i].alchemySymbolId;
        return s;
    }
    int getDisplaySymbolId() const override { return displaySymbolId; }
    std::string getDisplayChordName() const override { return displayChordName; }
    float getSymbolPreviewTimer() const override { return symbolPreviewTimer; }
    bool getSpookyTvMode() const override { return spookyTvMode; }
    bool isDoubleOccupancy() const override { return doubleOccupancyMode; }

    // Symbol button support
    int getSelectedSymbol() const override { return selectedSymbol; }
    float getButtonPressAnim(int buttonPos) const override { 
        return (buttonPos >= 0 && buttonPos < 12) ? buttonPressAnim[buttonPos] : 0.0f; 
    }
    int getCurrentChordIndex(bool seqA) const override { 
        return seqA ? getCurrentChordIndex(sequenceA) : getCurrentChordIndex(sequenceB); 
    }

    // Controller API (used by matrix programming)
    void programStepA(int stepIndex) override {
        if (stepIndex < 0 || stepIndex >= sequenceA.length) return;
        SequenceStep& step = sequenceA.steps[stepIndex];
        if (st::isValidSymbolId(selectedSymbol) && symbolToChordMapping[selectedSymbol] >= 0) {
            step.chordIndex = selectedSymbol;
            step.alchemySymbolId = selectedSymbol;
            int ci = symbolToChordMapping[selectedSymbol];
            if (ci >= 0 && ci < (int)currentChordPack.chords.size())
                step.voiceCount = std::min(currentChordPack.chords[ci].preferredVoices, 6);
        } else if (selectedSymbol == -1) {
            step.chordIndex = -1; step.alchemySymbolId = -1; step.voiceCount = 1;
        } else if (selectedSymbol == -2) {
            step.chordIndex = -2; step.alchemySymbolId = -2; step.voiceCount = 1;
        }
    }
    void programStepB(int stepIndex) override {
        if (stepIndex < 0 || stepIndex >= sequenceB.length) return;
        SequenceStep& step = sequenceB.steps[stepIndex];
        if (st::isValidSymbolId(selectedSymbol) && symbolToChordMapping[selectedSymbol] >= 0) {
            step.chordIndex = selectedSymbol;
            step.alchemySymbolId = selectedSymbol;
            int ci = symbolToChordMapping[selectedSymbol];
            if (ci >= 0 && ci < (int)currentChordPack.chords.size())
                step.voiceCount = std::min(currentChordPack.chords[ci].preferredVoices, 6);
        } else if (selectedSymbol == -1) {
            step.chordIndex = -1; step.alchemySymbolId = -1; step.voiceCount = 1;
        } else if (selectedSymbol == -2) {
            step.chordIndex = -2; step.alchemySymbolId = -2; step.voiceCount = 1;
        }
    }
    void cycleVoiceCountA(int idx) override {
        if (idx < 0 || idx >= sequenceA.length) return;
        SequenceStep& s = sequenceA.steps[idx];
        if (st::isValidSymbolId(s.chordIndex) && symbolToChordMapping[s.chordIndex] >= 0) {
            s.voiceCount = (s.voiceCount % stx::transmutation::MAX_VOICES) + 1;
            // Force immediate chord update if this is the current playing step
            if (sequenceA.running && idx == sequenceA.currentStep) {
                forceChordUpdateA = true;
            }
        }
    }
    void cycleVoiceCountB(int idx) override {
        if (idx < 0 || idx >= sequenceB.length) return;
        SequenceStep& s = sequenceB.steps[idx];
        if (st::isValidSymbolId(s.chordIndex) && symbolToChordMapping[s.chordIndex] >= 0) {
            s.voiceCount = (s.voiceCount % stx::transmutation::MAX_VOICES) + 1;
            // Force immediate chord update if this is the current playing step
            if (sequenceB.running && idx == sequenceB.currentStep) {
                forceChordUpdateB = true;
            }
        }
    }
    void setEditCursorA(int idx) override {
        if (idx < 0 || idx >= sequenceA.length) return;
        // Do not move the playhead while running; allow cursor move only when stopped
        if (!sequenceA.running) sequenceA.currentStep = idx;
    }
    void setEditCursorB(int idx) override {
        if (idx < 0 || idx >= sequenceB.length) return;
        // Do not move the playhead while running; allow cursor move only when stopped
        if (!sequenceB.running) sequenceB.currentStep = idx;
    }

    // Write CV preview for a step while stopped (gates low)
    void writeCvPreview(const ProcessArgs& args, const SequenceStep& step, int cvOutputId, int gateOutputId) {
        if (!st::isValidSymbolId(step.chordIndex)) {
            stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }
        int mappedIndex = symbolToChordMapping[step.chordIndex];
        if (mappedIndex < 0 || mappedIndex >= (int)currentChordPack.chords.size()) {
            stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }
        const ChordData& chord = currentChordPack.chords[mappedIndex];
        int voiceCount = forceSixPoly ? stx::transmutation::MAX_VOICES : std::min(step.voiceCount, stx::transmutation::MAX_VOICES);

        std::vector<float> targetNotes;
        stx::poly::buildTargetsFromIntervals(chord.intervals, voiceCount, /*harmony*/ false, targetNotes);
        if (randomizeChordVoicing && voiceCount > 1) {
            std::mt19937 rng(rack::random::u32());
            std::shuffle(targetNotes.begin(), targetNotes.end(), rng);
        }
        std::vector<float> assigned(6, 0.0f);
        for (int v = 0; v < voiceCount && v < 6; ++v) assigned[v] = targetNotes[v % targetNotes.size()];

        bool exact = (cvOutputId == CV_A_OUTPUT) ? oneShotExactPolyA : oneShotExactPolyB;
        const int totalCh = (stablePolyChannels && !exact) ? stx::transmutation::MAX_VOICES : voiceCount;
        outputs[cvOutputId].setChannels(totalCh);
        outputs[gateOutputId].setChannels(totalCh);
        for (int v = 0; v < totalCh; ++v) {
            if (v < voiceCount) {
                float noteCV = assigned[v];
                float smoothed = enableCvSlew ? ((cvOutputId == CV_A_OUTPUT) ? cvSlewA[v].process(args.sampleTime, noteCV)
                                                                               : cvSlewB[v].process(args.sampleTime, noteCV))
                                              : noteCV;
                outputs[cvOutputId].setVoltage(smoothed, v);
                if (cvOutputId == CV_A_OUTPUT) lastCvA[v] = smoothed; else lastCvB[v] = smoothed;
            } else {
                float held = (cvOutputId == CV_A_OUTPUT) ? lastCvA[v] : lastCvB[v];
                outputs[cvOutputId].setVoltage(held, v);
            }
            outputs[gateOutputId].setVoltage(0.f, v);
        }
        // Do not clear oneShotExactPoly here; let the first running step use exact channels for gate handshake
    }

    void processSequence(Sequence& seq, int clockInputId, int cvOutputId, int gateOutputId, const ProcessArgs& args, bool internalClockTrigger) {
        // If requested, bump channels to 0 then rebuild this frame so downstream modules refresh poly mode
        bool& reassertPoly = (cvOutputId == CV_A_OUTPUT) ? reassertPolyA : reassertPolyB;
        if (reassertPoly) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
            reassertPoly = false;
        }
        if (!seq.running) {
            // While stopped: preview current step CV so users see/hear a chord immediately, gates remain low
            if (const SequenceStep* eff = resolveEffectiveStep(seq, seq.currentStep))
                writeCvPreview(args, *eff, cvOutputId, gateOutputId);
            else
                stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }

        // Get clock source (external overrides internal)
        bool useExternalClock = inputs[clockInputId].isConnected();
        bool clockTrigger = false;

        if (useExternalClock) {
            if (clockInputId == CLOCK_A_INPUT) {
                clockTrigger = clockATrigger.process(inputs[clockInputId].getVoltage());
            } else {
                clockTrigger = clockBTrigger.process(inputs[clockInputId].getVoltage());
            }
        } else {
            // Use the global internal clock trigger
            clockTrigger = internalClockTrigger;
        }

        // Measure period on external clock
        if (useExternalClock && clockTrigger) {
            float period = (float)(engineTimeSec - seq.lastClockTime);
            if (period > 1e-4f && period < 5.0f) {
                seq.estPeriod = 0.8f * seq.estPeriod + 0.2f * period;
            }
            seq.lastClockTime = engineTimeSec;
        }

        // Schedule/advance with groove micro-delay
        float basePeriod = useExternalClock ? (seq.estPeriod > 1e-4f ? seq.estPeriod : 0.5f)
                                            : (60.f / std::max(1.f, clockRate));
        if (clockTrigger) {
            // If a previous advance is pending but hasn't fired, force it now to avoid backlog
            if (seq.groovePending && seq.grooveDelay > 0.f) {
                seq.grooveDelay = 0.f; // will advance this frame
            }
            int nextIndex = (seq.currentStep + 1) % seq.length;
            seq.grooveDelay = computeGrooveDelaySec(seq, nextIndex, basePeriod);
            seq.groovePending = true;
        }

        bool stepChanged = false;
        if (seq.groovePending) {
            seq.grooveDelay -= args.sampleTime;
            if (seq.grooveDelay <= 0.f) {
                int prevIndex = seq.currentStep;
                int nextIndex = (seq.currentStep + 1) % seq.length;
                const SequenceStep* prevEff = resolveEffectiveStep(seq, prevIndex);
                const SequenceStep* nextEff = resolveEffectiveStep(seq, nextIndex);
                stepChanged = isStepChanged(prevEff, nextEff);
                seq.currentStep = nextIndex;
                seq.groovePending = false;
            }
        }

        // Check for forced updates from parameter changes
        if (cvOutputId == CV_A_OUTPUT && forceChordUpdateA) {
            stepChanged = true;
            forceChordUpdateA = false;
        }
        if (cvOutputId == CV_B_OUTPUT && forceChordUpdateB) {
            stepChanged = true;
            forceChordUpdateB = false;
        }

        // Resolve effective step (follows TIEs). Output or clear.
        const SequenceStep* eff = resolveEffectiveStep(seq, seq.currentStep);
        if (eff) outputChord(args, *eff, cvOutputId, gateOutputId, stepChanged);
        else stableClearOutputs(cvOutputId, gateOutputId);
    }

    void processSequenceB(const ProcessArgs& args, bool internalClockTrigger) {
        int bMode = (int)params[SEQ_B_MODE_PARAM].getValue();

        switch (bMode) {
            case 0: // Independent mode
                processSequence(sequenceB, CLOCK_B_INPUT, CV_B_OUTPUT, GATE_B_OUTPUT, args, internalClockTrigger);
                break;

            case 1: // Harmony mode
                processSequenceBHarmony(args, internalClockTrigger);
                break;

            case 2: // Lock mode  
                processSequenceBLock(args, internalClockTrigger);
                break;
        }
    }

    void processSequenceBHarmony(const ProcessArgs& args, bool internalClockTrigger) {
        if (!sequenceB.running) {
            stableClearOutputs(CV_B_OUTPUT, GATE_B_OUTPUT);
            return;
        }

        // In harmony mode, sequence B follows A's timing and chord but plays harmony notes
        if (!sequenceA.running) {
            stableClearOutputs(CV_B_OUTPUT, GATE_B_OUTPUT);
            return;
        }

        // Get clock from sequence A or external B clock
        bool useExternalClock = inputs[CLOCK_B_INPUT].isConnected();
        bool clockTrigger = false;

        if (useExternalClock) {
            clockTrigger = clockBTrigger.process(inputs[CLOCK_B_INPUT].getVoltage());
        } else {
            // Follow sequence A's timing using internal clock
            clockTrigger = internalClockTrigger && sequenceA.running;
        }
        // Measure B external period
        if (useExternalClock && clockTrigger) {
            float period = (float)(engineTimeSec - sequenceB.lastClockTime);
            if (period > 1e-4f && period < 5.0f) {
                sequenceB.estPeriod = 0.8f * sequenceB.estPeriod + 0.2f * period;
            }
            sequenceB.lastClockTime = engineTimeSec;
        }
        // Schedule/advance with groove (for Harmony, use same base period as A's internal clock)
        float basePeriod = useExternalClock ? (sequenceB.estPeriod > 1e-4f ? sequenceB.estPeriod : 0.5f)
                                            : (60.f / std::max(1.f, clockRate));
        if (clockTrigger) {
            if (sequenceB.groovePending && sequenceB.grooveDelay > 0.f) {
                sequenceB.grooveDelay = 0.f;
            }
            int nextB = (sequenceB.currentStep + 1) % sequenceB.length;
            sequenceB.grooveDelay = computeGrooveDelaySec(sequenceB, nextB, basePeriod);
            sequenceB.groovePending = true;
        }
        bool stepChanged = false;
        if (sequenceB.groovePending) {
            sequenceB.grooveDelay -= args.sampleTime;
            if (sequenceB.grooveDelay <= 0.f) {
                int prevB = sequenceB.currentStep;
                int nextB = (sequenceB.currentStep + 1) % sequenceB.length;
                const SequenceStep* prevEffB = resolveEffectiveStep(sequenceB, prevB);
                const SequenceStep* nextEffB = resolveEffectiveStep(sequenceB, nextB);
                const SequenceStep* prevEffA = resolveEffectiveStep(sequenceA, (sequenceA.currentStep - 1 + sequenceA.length) % sequenceA.length);
                const SequenceStep* currEffA = resolveEffectiveStep(sequenceA, sequenceA.currentStep);
                bool changedB = isStepChanged(prevEffB, nextEffB);
                bool changedA = isStepChanged(prevEffA, currEffA);
                stepChanged = changedA || changedB;
                sequenceB.currentStep = nextB;
                sequenceB.groovePending = false;
            }
        }

        // Resolve effective A/B steps
        const SequenceStep* effA = resolveEffectiveStep(sequenceA, sequenceA.currentStep);
        const SequenceStep* effB = resolveEffectiveStep(sequenceB, sequenceB.currentStep);
        if (effA) {
            // Generate harmony based on sequence A's chord
            // If B is null (rest), still use A's chord but a default voiceCount of 1
            SequenceStep bTmp;
            if (!effB) { bTmp.chordIndex = -1; bTmp.voiceCount = 1; bTmp.alchemySymbolId = -1; }
            outputHarmony(args, *effA, effB ? *effB : bTmp, CV_B_OUTPUT, GATE_B_OUTPUT, stepChanged);
        } else {
            // If A has no effective chord, silence B (stable frame)
            stableClearOutputs(CV_B_OUTPUT, GATE_B_OUTPUT);
        }
    }

    void processSequenceBLock(const ProcessArgs& args, bool internalClockTrigger) {
        if (!sequenceB.running) {
            stableClearOutputs(CV_B_OUTPUT, GATE_B_OUTPUT);
            return;
        }

        // In lock mode, sequence B uses the same chord pack as A but has independent timing/progression
        // Standard clock handling for B
        bool useExternalClock = inputs[CLOCK_B_INPUT].isConnected();
        bool clockTrigger = false;

        if (useExternalClock) {
            clockTrigger = clockBTrigger.process(inputs[CLOCK_B_INPUT].getVoltage());
        } else {
            // Use the global internal clock trigger
            clockTrigger = internalClockTrigger;
        }
        // Measure external period
        if (useExternalClock && clockTrigger) {
            float period = (float)(engineTimeSec - sequenceB.lastClockTime);
            if (period > 1e-4f && period < 5.0f) {
                sequenceB.estPeriod = 0.8f * sequenceB.estPeriod + 0.2f * period;
            }
            sequenceB.lastClockTime = engineTimeSec;
        }
        // Schedule/advance with groove
        float basePeriod = useExternalClock ? (sequenceB.estPeriod > 1e-4f ? sequenceB.estPeriod : 0.5f)
                                            : (60.f / std::max(1.f, clockRate));
        if (clockTrigger) {
            if (sequenceB.groovePending && sequenceB.grooveDelay > 0.f) {
                sequenceB.grooveDelay = 0.f;
            }
            int nextB = (sequenceB.currentStep + 1) % sequenceB.length;
            sequenceB.grooveDelay = computeGrooveDelaySec(sequenceB, nextB, basePeriod);
            sequenceB.groovePending = true;
        }
        bool stepChanged = false;
        if (sequenceB.groovePending) {
            sequenceB.grooveDelay -= args.sampleTime;
            if (sequenceB.grooveDelay <= 0.f) {
                int prevB = sequenceB.currentStep;
                int nextB = (sequenceB.currentStep + 1) % sequenceB.length;
                const SequenceStep* prevEff = resolveEffectiveStep(sequenceB, prevB);
                const SequenceStep* nextEff = resolveEffectiveStep(sequenceB, nextB);
                stepChanged = isStepChanged(prevEff, nextEff);
                sequenceB.currentStep = nextB;
                sequenceB.groovePending = false;
            }
        }

        // Output sequence B's programmed progression using same chord pack as A
        if (const SequenceStep* eff = resolveEffectiveStep(sequenceB, sequenceB.currentStep))
            outputChord(args, *eff, CV_B_OUTPUT, GATE_B_OUTPUT, stepChanged);
        else
            stableClearOutputs(CV_B_OUTPUT, GATE_B_OUTPUT);
    }

    void outputHarmony(const ProcessArgs& args, const SequenceStep& stepA, const SequenceStep& stepB, int cvOutputId, int gateOutputId, bool stepChanged) {
        // Validate symbol ID and its chord mapping
        if (!st::isValidSymbolId(stepA.chordIndex)) {
            stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }
        int mappedIndexA = symbolToChordMapping[stepA.chordIndex];
        if (mappedIndexA < 0 || mappedIndexA >= (int)currentChordPack.chords.size()) {
            stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }

        const ChordData& chordA = currentChordPack.chords[mappedIndexA];
        int reqVoices = std::min(stepB.voiceCount, stx::transmutation::MAX_VOICES);
        if (harmonyLimitVoices) reqVoices = rack::math::clamp(reqVoices, 1, 2);
        int voiceCount = forceSixPoly ? stx::transmutation::MAX_VOICES : reqVoices;

        // Build targets and assign
        std::vector<float> targetNotes;
        int baseVoices = stepB.voiceCount; // requested on B side
        if (baseVoices == 1 && oneVoiceRandomNote) {
            // pick a random chord tone
            if (!chordA.intervals.empty()) {
                int idx = (int)std::floor(rack::random::uniform() * chordA.intervals.size());
                idx = rack::clamp(idx, 0, (int)chordA.intervals.size() - 1);
                stx::poly::buildTargetsFromIntervals({ chordA.intervals[idx] }, 1, /*harmony*/ true, targetNotes);
            } else {
                stx::poly::buildTargetsFromIntervals(chordA.intervals, voiceCount, /*harmony*/ true, targetNotes);
            }
        } else {
            stx::poly::buildTargetsFromIntervals(chordA.intervals, voiceCount, /*harmony*/ true, targetNotes);
            if (randomizeChordVoicing && voiceCount > 1) {
                std::mt19937 rng(rack::random::u32());
                std::shuffle(targetNotes.begin(), targetNotes.end(), rng);
            }
        }
        // SIMPLE DIRECT ASSIGNMENT - NO MORE assignNearest!
        std::vector<float> assigned(6, 0.0f);  // Always 6 elements, but only fill voiceCount
        if (!targetNotes.empty()) {
            // Calculate root note (C4 = 0V as standard in VCV Rack)
            float rootNote = 0.0f; // C4 = 0V - can be modified for transposition

            for (int v = 0; v < voiceCount && v < 6; v++) {
                // Cycle through all target notes for the requested voice count
                int targetIdx = v % targetNotes.size();
                assigned[v] = rootNote + targetNotes[targetIdx]; // Add root note offset!
            }
            // Voices beyond voiceCount remain at 0.0f
        }

        // Set outputs to total channel count (stable if enabled)
        bool exact = (cvOutputId == CV_A_OUTPUT) ? oneShotExactPolyA : oneShotExactPolyB;
        const int totalCh = (stablePolyChannels && !exact) ? stx::transmutation::MAX_VOICES : voiceCount;
        outputs[cvOutputId].setChannels(totalCh);
        for (int voice = 0; voice < totalCh; voice++) {
            if (voice < voiceCount) {
                float noteCV = assigned[voice];
                float smoothed = noteCV;
                if (enableCvSlew) {
                    smoothed = (cvOutputId == CV_A_OUTPUT) ? cvSlewA[voice].process(args.sampleTime, noteCV)
                                                           : cvSlewB[voice].process(args.sampleTime, noteCV);
                }
                outputs[cvOutputId].setVoltage(smoothed, voice);
                if (cvOutputId == CV_A_OUTPUT) lastCvA[voice] = smoothed; else lastCvB[voice] = smoothed;
            } else {
                // Hold last CV for inactive channels to avoid pitch jumps
                float held = (cvOutputId == CV_A_OUTPUT) ? lastCvA[voice] : lastCvB[voice];
                outputs[cvOutputId].setVoltage(held, voice);
            }
        }
        // Apply gate policy
        applyGates(args, gateOutputId, cvOutputId == CV_A_OUTPUT ? gatePulsesA : gatePulsesB, voiceCount, stepChanged);
        if (exact) {
            if (cvOutputId == CV_A_OUTPUT) oneShotExactPolyA = false; else oneShotExactPolyB = false;
        }

        // DEBUG: Removed gate voltage logging to save log space
    }

    void outputChord(const ProcessArgs& args, const SequenceStep& step, int cvOutputId, int gateOutputId, bool stepChanged) {
        // Validate symbol ID and its chord mapping
        if (!st::isValidSymbolId(step.chordIndex)) {
            stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }
        int mappedIndex = symbolToChordMapping[step.chordIndex];
        if (mappedIndex < 0 || mappedIndex >= (int)currentChordPack.chords.size()) {
            stableClearOutputs(cvOutputId, gateOutputId);
            return;
        }

        const ChordData& chord = currentChordPack.chords[mappedIndex];
        int voiceCount = forceSixPoly ? stx::transmutation::MAX_VOICES : std::min(step.voiceCount, stx::transmutation::MAX_VOICES);

        // Build targets and assign
        std::vector<float> targetNotes;
        int baseVoices = step.voiceCount; // requested on the step
        if (baseVoices == 1 && oneVoiceRandomNote) {
            // pick a random chord tone
            if (!chord.intervals.empty()) {
                int idx = (int)std::floor(rack::random::uniform() * chord.intervals.size());
                idx = rack::clamp(idx, 0, (int)chord.intervals.size() - 1);
                stx::poly::buildTargetsFromIntervals({ chord.intervals[idx] }, 1, /*harmony*/ false, targetNotes);
            } else {
                stx::poly::buildTargetsFromIntervals(chord.intervals, voiceCount, /*harmony*/ false, targetNotes);
            }
        } else {
            stx::poly::buildTargetsFromIntervals(chord.intervals, voiceCount, /*harmony*/ false, targetNotes);
            if (randomizeChordVoicing && voiceCount > 1) {
                std::mt19937 rng(rack::random::u32());
                std::shuffle(targetNotes.begin(), targetNotes.end(), rng);
            }
        }
        // SIMPLE DIRECT ASSIGNMENT - NO MORE assignNearest!
        std::vector<float> assigned(6, 0.0f);  // Always 6 elements, but only fill voiceCount
        if (!targetNotes.empty()) {
            // Calculate root note (C4 = 0V as standard in VCV Rack)
            float rootNote = 0.0f; // C4 = 0V - can be modified for transposition

            for (int v = 0; v < voiceCount && v < 6; v++) {
                // Cycle through all target notes for the requested voice count
                int targetIdx = v % targetNotes.size();
                assigned[v] = rootNote + targetNotes[targetIdx]; // Add root note offset!
            }
            // Voices beyond voiceCount remain at 0.0f
        }

        // Set outputs to total channel count (stable if enabled)
        bool exact2 = (cvOutputId == CV_A_OUTPUT) ? oneShotExactPolyA : oneShotExactPolyB;
        const int totalCh2 = (stablePolyChannels && !exact2) ? stx::transmutation::MAX_VOICES : voiceCount;
        outputs[cvOutputId].setChannels(totalCh2);
        outputs[gateOutputId].setChannels(totalCh2);
        for (int voice = 0; voice < totalCh2; voice++) {
            if (voice < voiceCount) {
                float noteCV = assigned[voice];
                float smoothed = noteCV;
                if (enableCvSlew) {
                    smoothed = (cvOutputId == CV_A_OUTPUT) ? cvSlewA[voice].process(args.sampleTime, noteCV)
                                                           : cvSlewB[voice].process(args.sampleTime, noteCV);
                }
                outputs[cvOutputId].setVoltage(smoothed, voice);
                if (cvOutputId == CV_A_OUTPUT) lastCvA[voice] = smoothed; else lastCvB[voice] = smoothed;
            } else {
                float held = (cvOutputId == CV_A_OUTPUT) ? lastCvA[voice] : lastCvB[voice];
                outputs[cvOutputId].setVoltage(held, voice);
            }
        }

        // DEBUG: Removed voltage logging to save log space

        // Apply gate policy
        applyGates(args, gateOutputId, cvOutputId == CV_A_OUTPUT ? gatePulsesA : gatePulsesB, voiceCount, stepChanged);
        if (exact2) {
            if (cvOutputId == CV_A_OUTPUT) oneShotExactPolyA = false; else oneShotExactPolyB = false;
        }

        // DEBUG: Removed gate voltage logging to save log space
    }

    int getCurrentChordIndex(const Sequence& seq) const {
        return seq.steps[seq.currentStep].chordIndex;
    }

    // Pattern operations --------------------------------------------------
    void clampCursorToLength(Sequence& seq) {
        if (seq.length < 1) seq.length = 1;
        if (seq.length > 64) seq.length = 64;
        if (seq.currentStep >= seq.length) seq.currentStep = std::max(0, seq.length - 1);
        if (seq.currentStep < 0) seq.currentStep = 0;
    }

    void clearSequence(Sequence& seq) {
        for (int i = 0; i < seq.length; ++i) seq.steps[i] = SequenceStep();
        clampCursorToLength(seq);
    }

    void initializeSequences() {
        // Reset both sequences to default empty 8-step state
        sequenceA = Sequence();
        sequenceB = Sequence();
        sequenceA.length = 8;
        sequenceB.length = 8;
        sequenceA.running = false;
        sequenceB.running = false;
        clearSequence(sequenceA);
        clearSequence(sequenceB);
        sequenceA.currentStep = 0;
        sequenceB.currentStep = 0;
        // Sync parameters
        params[LENGTH_A_PARAM].setValue(8.0f);
        params[LENGTH_B_PARAM].setValue(8.0f);
    }

    void shiftSequence(Sequence& seq, int dir) {
        // dir: -1 left, +1 right
        if (seq.length <= 1) return;
        if (dir < 0) {
            SequenceStep first = seq.steps[0];
            for (int i = 0; i < seq.length - 1; ++i) seq.steps[i] = seq.steps[i + 1];
            seq.steps[seq.length - 1] = first;
        } else if (dir > 0) {
            SequenceStep last = seq.steps[seq.length - 1];
            for (int i = seq.length - 1; i > 0; --i) seq.steps[i] = seq.steps[i - 1];
            seq.steps[0] = last;
        }
        clampCursorToLength(seq);
    }

    void copySequence(const Sequence& from, Sequence& to, bool copyLength = true) {
        int len = copyLength ? from.length : std::min(from.length, to.length);
        for (int i = 0; i < len; ++i) to.steps[i] = from.steps[i];
        if (copyLength) {
            to.length = from.length;
            clampCursorToLength((Sequence&)to);
        }
    }

    void swapSequencesContent(Sequence& a, Sequence& b) {
        Sequence tmp;
        tmp.length = a.length;
        for (int i = 0; i < a.length; ++i) tmp.steps[i] = a.steps[i];
        // copy b -> a
        a.length = b.length;
        for (int i = 0; i < b.length; ++i) a.steps[i] = b.steps[i];
        // copy tmp -> b
        b.length = tmp.length;
        for (int i = 0; i < tmp.length; ++i) b.steps[i] = tmp.steps[i];
        clampCursorToLength(a);
        clampCursorToLength(b);
    }

    void onSymbolPressed(int symbolIndex) override {
        selectedSymbol = symbolIndex;

        // Debug output
        if (st::isValidSymbolId(symbolIndex)) {
            int chordIndex = symbolToChordMapping[symbolIndex];
            INFO("Symbol pressed: %d -> Chord index: %d (of %d chords)", symbolIndex, chordIndex, (int)currentChordPack.chords.size());
        }

        // Display chord name on LED matrix
        // Trigger 8-bit symbol preview when selecting a symbol
        if (st::isValidSymbolId(symbolIndex) &&
            symbolToChordMapping[symbolIndex] >= 0 && symbolToChordMapping[symbolIndex] < (int)currentChordPack.chords.size()) {
            const ChordData& chord = currentChordPack.chords[symbolToChordMapping[symbolIndex]];
            displayChordName = chord.name;
            displaySymbolId = symbolIndex;
            symbolPreviewTimer = SYMBOL_PREVIEW_DURATION;
        } else if (symbolIndex == -1) {
            displayChordName = "REST";
            displaySymbolId = -1;
            symbolPreviewTimer = SYMBOL_PREVIEW_DURATION;
        } else if (symbolIndex == -2) {
            displayChordName = "TIE";
            displaySymbolId = -2;
            symbolPreviewTimer = SYMBOL_PREVIEW_DURATION;
        }

        // Audition the chord if we're in edit mode
        if ((editModeA || editModeB) && st::isValidSymbolId(symbolIndex)) {
            auditionChord(symbolIndex);
        }

        // Trigger press animation on the corresponding button slot
        if (st::isValidSymbolId(symbolIndex)) {
            for (int i = 0; i < 12; ++i) {
                if (buttonToSymbolMapping[i] == symbolIndex) {
                    buttonPressAnim[i] = 1.0f;
                    break;
                }
            }
        }
    }

    void auditionChord(int symbolIndex) {
        if (!st::isValidSymbolId(symbolIndex) ||
            symbolToChordMapping[symbolIndex] < 0 || symbolToChordMapping[symbolIndex] >= (int)currentChordPack.chords.size()) {
            return;
        }

        const ChordData& chord = currentChordPack.chords[symbolToChordMapping[symbolIndex]];

        // Trigger a brief chord preview on the appropriate output
        if (editModeA) {
            outputChordAudition(chord, CV_A_OUTPUT, GATE_A_OUTPUT);
        } else if (editModeB) {
            outputChordAudition(chord, CV_B_OUTPUT, GATE_B_OUTPUT);
        }
    }

    void outputChordAudition(const ChordData& chord, int cvOutputId, int gateOutputId) {
        int voiceCount = std::min(chord.preferredVoices, stx::transmutation::MAX_VOICES);

        // Set up polyphonic outputs to actual voice count
        const int chCount = voiceCount;
        outputs[cvOutputId].setChannels(chCount);
        outputs[gateOutputId].setChannels(chCount);

        // Calculate root note (C4 = 0V as standard in VCV Rack)
        float rootNote = 0.0f; // C4 = 0V

        // Output chord tones with proper voice allocation (same as outputChord)
        for (int voice = 0; voice < chCount; voice++) {
            if (voice < voiceCount) {
                float noteCV = rootNote;

                if (voice < (int)chord.intervals.size()) {
                    // Use chord intervals directly
                    noteCV = rootNote + chord.intervals[voice] / 12.0f; // Convert semitones to V/oct
                } else {
                    // If more voices requested than chord intervals, cycle through intervals in higher octaves
                    int intervalIndex = voice % chord.intervals.size();
                    int octaveOffset = voice / chord.intervals.size();
                    noteCV = rootNote + (chord.intervals[intervalIndex] + octaveOffset * 12.0f) / 12.0f;
                }

                outputs[cvOutputId].setVoltage(noteCV, voice);
                outputs[gateOutputId].setVoltage(10.0f, voice); // Standard gate voltage
            } else {
                // Clear unused voices
                outputs[cvOutputId].setVoltage(0.0f, voice);
                outputs[gateOutputId].setVoltage(0.0f, voice);
            }
        }

        // Set a timer to turn off the audition after a short time
        // (This would need a proper gate generator for timing, keeping simple for now)
    }

    bool loadChordPackFromFile(const std::string& filepath) {
        if (stx::transmutation::loadChordPackFromFile(filepath, currentChordPack)) {
            // Normalize pack content to avoid duplicates and redundant words
            normalizeChordPack(currentChordPack);
            INFO("Loaded: '%s' (%d chords)", currentChordPack.name.c_str(), (int)currentChordPack.chords.size());
            // Remap placed steps to preserve button positions with the new symbol set
            randomizeSymbolAssignment(true);
            // Normalize existing sequences to new pack (ensure playable voices)
            auto normalize = [&](Sequence& seq){
                for (int i = 0; i < seq.length; ++i) {
                    SequenceStep& st = seq.steps[i];
                    if (st.chordIndex >= 0 && st.chordIndex < st::SymbolCount) {
                        int mapped = symbolToChordMapping[st.chordIndex];
                        if (mapped >= 0 && mapped < (int)currentChordPack.chords.size()) {
                            int pv = currentChordPack.chords[mapped].preferredVoices;
                            st.voiceCount = clamp(pv, 1, stx::transmutation::MAX_VOICES);
                            // Ensure alchemySymbolId matches chordIndex for UI
                            st.alchemySymbolId = st.chordIndex;
                        }
                    }
                }
            };
            normalize(sequenceA);
            normalize(sequenceB);
            // Force immediate refresh
            forceChordUpdateA = true; forceChordUpdateB = true;
            reassertPolyA = true; reassertPolyB = true;
            oneShotExactPolyA = true; oneShotExactPolyB = true;
            return true;
        }
        INFO("FAILED to load: %s", system::getFilename(filepath).c_str());
        return false;
    }

    void remapPlacedSymbols(const std::array<int, 12>& oldButtons, const std::array<int, 12>& newButtons) {
        auto remapSeq = [&](Sequence& seq) {
            for (int i = 0; i < seq.length; ++i) {
                SequenceStep& st = seq.steps[i];
                if (st.chordIndex >= 0) {
                    for (int b = 0; b < 12; ++b) {
                        int fromSym = oldButtons[b];
                        int toSym   = newButtons[b];
                        if (st.chordIndex == fromSym) {
                            st.chordIndex = toSym;
                            st.alchemySymbolId = toSym;
                            break;
                        }
                    }
                }
            }
        };
        remapSeq(sequenceA);
        remapSeq(sequenceB);
    }

    void randomizeSymbolAssignment(bool remapPlacedSteps = false) {
        std::array<int, 12> oldButtons = buttonToSymbolMapping;
        stx::transmutation::randomizeSymbolAssignment(currentChordPack, symbolToChordMapping, buttonToSymbolMapping);
        if (remapPlacedSteps) {
            remapPlacedSymbols(oldButtons, buttonToSymbolMapping);
        }
    }

    // Derive the 12 button symbols from existing sequences so UI matches placed steps on load
    void deriveButtonsFromSequences() {
        std::vector<int> found;
        found.reserve(12);
        auto addIf = [&](int sym){
            if (!st::isValidSymbolId(sym)) return;
            if (std::find(found.begin(), found.end(), sym) == found.end()) found.push_back(sym);
        };
        // Scan A then B in order
        for (int i = 0; i < sequenceA.length && (int)found.size() < 12; ++i) addIf(sequenceA.steps[i].chordIndex);
        for (int i = 0; i < sequenceB.length && (int)found.size() < 12; ++i) addIf(sequenceB.steps[i].chordIndex);
        // If none found, keep current mapping
        if (found.empty()) return;
        // Fill remaining with any valid symbols not already chosen
        for (int s = 0; s < st::SymbolCount && (int)found.size() < 12; ++s) {
            int mapped = symbolToChordMapping[s];
            if (mapped >= 0 && mapped < (int)currentChordPack.chords.size()) {
                if (std::find(found.begin(), found.end(), s) == found.end()) found.push_back(s);
            }
        }
        // Write into buttonToSymbolMapping
        for (int i = 0; i < 12; ++i) buttonToSymbolMapping[i] = (i < (int)found.size()) ? found[i] : i;
    }

    void loadDefaultChordPack() {
        stx::transmutation::loadDefaultChordPack(currentChordPack);
        normalizeChordPack(currentChordPack);
        // Remap placed steps to preserve button positions with the new symbol set
        randomizeSymbolAssignment(true);
    }

    // Randomize both sequence lengths with improved variety and musicality
    void randomizeSequenceLengths() {
        // 1) Build candidate values within grid
        int valsAll[] = {3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,20,21,22,24,28,30,32,36,40,42,48,56,64};
        std::vector<int> vals;
        for (int v : valsAll) if (v <= gridSteps) vals.push_back(v);
        if (vals.empty()) {
            for (int v = 1; v <= gridSteps; ++v) vals.push_back(v);
        }

        // 2) Optionally sample from curated, musical pairs for extra spice
        std::vector<std::pair<int,int>> curated = {
            {7,8},{5,7},{3,4},{4,5},{6,7},{7,9},
            {12,16},{10,12},{12,15},{9,16},{14,16},{8,12},
            {15,16},{10,16},{6,10},{5,8},{5,9}
        };
        // Enrich curated set for larger grids so we can land >16 more often
        if (gridSteps >= 32) {
            std::vector<std::pair<int,int>> c32 = {
                {12,24},{16,24},{8,24},{10,20},{15,30},{12,18},
                {18,24},{20,24},{24,28},{24,30},{24,32},{14,28}
            };
            curated.insert(curated.end(), c32.begin(), c32.end());
        }
        if (gridSteps >= 64) {
            std::vector<std::pair<int,int>> c64 = {
                {16,32},{24,48},{32,48},{28,42},{21,28},{30,45},
                {36,40},{42,56},{32,64},{24,36},{18,36},{20,40}
            };
            curated.insert(curated.end(), c64.begin(), c64.end());
        }
        auto fits = [&](const std::pair<int,int>& p){ return p.first <= gridSteps && p.second <= gridSteps; };
        std::vector<std::pair<int,int>> curatedFit;
        for (auto& p : curated) if (fits(p)) curatedFit.push_back(p);

        auto gcd = [](int a, int b){ a = std::abs(a); b = std::abs(b); while (b) { int t = a % b; a = b; b = t; } return a; };
        auto pickFrom = [&](const std::vector<int>& p) -> int {
            if (p.empty()) return std::max(1, std::min(gridSteps, 8));
            uint32_t r = rack::random::u32();
            return p[r % p.size()];
        };

        // Weighted pool that scales with grid size so higher steps appear in 32/64 grids
        std::vector<int> pool; pool.reserve(vals.size() * 6);
        auto isNice = [](int v){
            switch (v) {
                case 8: case 10: case 12: case 14: case 15: case 16:
                case 18: case 20: case 24: case 28: case 30: case 32:
                case 36: case 40: case 42: case 48: case 56: case 64:
                    return true;
                default: return false;
            }
        };
        for (int v : vals) {
            int w = 1;
            if (gridSteps <= 16) {
                // Small grid: bias toward 4..16
                if (v >= 4 && v <= 16) w = 3; else w = 1;
            } else if (gridSteps <= 32) {
                // 32 grid: encourage 18..32 noticeably
                if (v <= 8) w = 2; else if (v <= 16) w = 3; else if (v <= 24) w = 4; else w = 3;
            } else { // 64 grid
                // 64 grid: encourage mid/high ranges
                if (v <= 8) w = 1; else if (v <= 16) w = 2; else if (v <= 32) w = 4; else w = 3;
            }
            if (isNice(v)) w += (gridSteps >= 64) ? 2 : 1; // boost musically nice sizes
            for (int i = 0; i < w; ++i) pool.push_back(v);
        }
        int a = pickFrom(pool);

        // With some probability, use curated pairs
        int b = a;
        if (!curatedFit.empty() && (rack::random::uniform() < 0.35f)) {
            auto pr = curatedFit[rack::random::u32() % curatedFit.size()];
            // Randomly assign orientation
            if (rack::random::uniform() < 0.5f) { a = pr.first; b = pr.second; }
            else { a = pr.second; b = pr.first; }
        } else {
            // Else, search randomly for a good partner using a scoring function
            float bestScore = -1e9f; int bestB = a;
            for (int tries = 0; tries < 24; ++tries) {
                int cand = pickFrom(pool);
                // Score
                float s = 0.f;
                if (cand != a) s += 1.0f; else s -= 1.5f;       // discourage equality
                int g = gcd(a, cand);
                if (g == 1) s += 2.0f; else if (g == 2) s += 1.0f; else s -= 0.25f * g; // favor coprime/small gcd
                float ratio = (float)std::max(a,cand) / (float)std::min(a,cand);
                float nearInt = std::fabs(ratio - std::round(ratio));
                if (nearInt < 0.02f) s -= 1.0f;                  // avoid exact multiples
                s += std::fabs(a - cand) / (float)std::max(16, gridSteps); // encourage different sizes
                if (cand >= 4 && cand <= 16) s += 0.25f;         // slight bias toward usable lengths
                if (s > bestScore) { bestScore = s; bestB = cand; }
            }
            b = bestB;
        }

        // Apply
        params[LENGTH_A_PARAM].setValue((float)a);
        params[LENGTH_B_PARAM].setValue((float)b);
        sequenceA.length = a;
        sequenceB.length = b;
        clampCursorToLength(sequenceA);
        clampCursorToLength(sequenceB);
    }

    // Discover all chord pack files under chord_packs/*/*.json
    std::vector<std::string> listAllChordPackFiles() {
        std::vector<std::string> packs;
        std::string chordPackDir = asset::plugin(pluginInstance, "chord_packs");
        if (!system::isDirectory(chordPackDir)) return packs;
        for (const std::string& entry : system::getEntries(chordPackDir)) {
            std::string fullPath = entry; // system::getEntries returns full paths
            if (!system::isDirectory(fullPath)) continue;
            for (const std::string& fileEntry : system::getEntries(fullPath)) {
                if (system::getExtension(fileEntry) == ".json") packs.push_back(fileEntry);
            }
        }
        return packs;
    }

    // Randomly choose and load a chord pack; returns true if loaded
    bool randomizeChordPack() {
        auto packs = listAllChordPackFiles();
        if (packs.empty()) return false;
        std::mt19937 rng(rack::random::u32());
        const std::string& path = packs[rng() % packs.size()];
        bool ok = loadChordPackFromFile(path);
        if (ok) {
            // Brief on-screen preview using proper chord pack name
            displayChordName = currentChordPack.name;
            displaySymbolId = -999;
            symbolPreviewTimer = 1.0f;
        }
        return ok;
    }

    void randomizeEverything() {
        // Preserve non-randomizable param values across this routine
        int keepModeB = (int)params[SEQ_B_MODE_PARAM].getValue();
        float keepScreen = params[SCREEN_STYLE_PARAM].getValue();
        // Try to pick a random pack; if none, keep current/default
        if (randomAllPack) randomizeChordPack();
        // Pick complementary lengths
        if (randomAllLengths) randomizeSequenceLengths();
        // Fill content for both sequences
        if (randomAllSteps) {
            randomizeSequence(sequenceA);
            randomizeSequence(sequenceB);
        }
        // Randomize clock settings if enabled
        if (randomAllBpm) {
            std::mt19937 rng(rack::random::u32());
            std::uniform_real_distribution<float> bpm(60.f, 160.f); // sensible musical range
            params[INTERNAL_CLOCK_PARAM].setValue(bpm(rng));
        }
        if (randomAllMultiplier) {
            // Exclude the fastest (8x) from randomization
            int idx = (int)(rack::random::u32() % 3); // 0..2 -> 1x,2x,4x
            params[BPM_MULTIPLIER_PARAM].setValue((float)idx);
        }
        // Restart both sequences at step 0 and force immediate update
        sequenceA.currentStep = 0;
        sequenceB.currentStep = 0;
        forceChordUpdateA = true;
        forceChordUpdateB = true;
        // Reassert poly to downstream
        reassertPolyA = true;
        reassertPolyB = true;
        // For one frame, emit exact voiceCount channels to guarantee downstream re-latch
        oneShotExactPolyA = true;
        oneShotExactPolyB = true;

        // Restore non-randomizable params
        params[SEQ_B_MODE_PARAM].setValue((float)keepModeB);
        params[SCREEN_STYLE_PARAM].setValue(keepScreen);
    }

    void randomizePackSafe() {
        int keepModeB = (int)params[SEQ_B_MODE_PARAM].getValue();
        float keepScreen = params[SCREEN_STYLE_PARAM].getValue();
        // Randomize pack + steps + lengths with hard poly handshake
        randomizeChordPack();
        randomizeSequenceLengths();
        randomizeSequence(sequenceA);
        randomizeSequence(sequenceB);
        sequenceA.currentStep = 0;
        sequenceB.currentStep = 0;
        forceChordUpdateA = true;
        forceChordUpdateB = true;
        // Force re-latch
        reassertPolyA = true; reassertPolyB = true;
        oneShotExactPolyA = true; oneShotExactPolyB = true;

        // Restore non-randomizable params
        params[SEQ_B_MODE_PARAM].setValue((float)keepModeB);
        params[SCREEN_STYLE_PARAM].setValue(keepScreen);
    }

    // Collect valid symbol IDs that are mapped to a chord in the current pack
    std::vector<int> getValidSymbols() const {
        std::vector<int> ids;
        ids.reserve(st::SymbolCount);
        for (int s = 0; s < st::SymbolCount; ++s) {
            int mapped = symbolToChordMapping[s];
            if (mapped >= 0 && mapped < (int)currentChordPack.chords.size()) ids.push_back(s);
        }
        if (ids.empty()) {
            // Fallback: all 0..11 if somehow no mapping yet
            for (int s = 0; s < 12; ++s) ids.push_back(s);
        }
        return ids;
    }

    // Randomization controls
    float randomRestProb = 0.12f; // 12% base weight for rests
    float randomTieProb  = 0.10f; // 10% base weight for ties
    float randomChordProb = 0.60f; // direct chord density (0..1); non-chord split by rest/tie weights
    // Randomize Everything options
    bool randomAllPack = true;
    bool randomAllLengths = true;
    bool randomAllSteps = true;
    bool randomAllBpm = true;
    bool randomAllMultiplier = true;
    bool  randomUsePreferredVoices = true; // prefer chord.preferredVoices for chord steps

    // Randomize a sequence's content (steps and voice counts)
    void randomizeSequence(Sequence& seq) {
        // Prefer the 12 visible button symbols so steps match button icons; fallback to all valid symbols
        std::vector<int> symbols;
        for (int i = 0; i < 12; ++i) {
            int sym = buttonToSymbolMapping[i];
            if (st::isValidSymbolId(sym)) {
                int mapped = symbolToChordMapping[sym];
                if (mapped >= 0 && mapped < (int)currentChordPack.chords.size()) symbols.push_back(sym);
            }
        }
        if (symbols.empty()) symbols = getValidSymbols();
        if (symbols.empty()) return;
        std::mt19937 rng(rack::random::u32());
        std::uniform_int_distribution<int> voiceDist(1, stx::transmutation::MAX_VOICES);
        std::uniform_real_distribution<float> pick(0.f, 1.f);

        int len = clamp(seq.length, 1, gridSteps);
        // Compute final probabilities: chord density wins; remaining split by rest/tie weights
        float chordP = clamp(randomChordProb, 0.f, 1.f);
        float rtWeight = clamp(randomRestProb, 0.f, 1.f) + clamp(randomTieProb, 0.f, 1.f);
        float remaining = 1.f - chordP;
        if (remaining < 0.f) remaining = 0.f;
        float restP = 0.f, tieP = 0.f;
        if (rtWeight <= 1e-6f) {
            restP = remaining; tieP = 0.f;
        } else {
            restP = remaining * (randomRestProb / rtWeight);
            tieP  = remaining * (randomTieProb  / rtWeight);
        }
        float tieThreshold = restP + tieP;
        bool anyChord = false;
        for (int i = 0; i < len; ++i) {
            float r = pick(rng);
            SequenceStep stp;
            if (r < restP) {
                // REST
                stp.chordIndex = -1;
                stp.alchemySymbolId = -1;
                stp.voiceCount = 1;
            } else if (r < tieThreshold) {
                // TIE (follows previous)
                stp.chordIndex = -2;
                stp.alchemySymbolId = -2;
                stp.voiceCount = 1;
            } else {
                // Chord step
                int sidx = symbols[rng() % symbols.size()];
                stp.chordIndex = sidx;
                stp.alchemySymbolId = sidx;
                // Choose voices: prefer chord's suggested size if available
                if (randomUsePreferredVoices) {
                    int mapped = symbolToChordMapping[sidx];
                    if (mapped >= 0 && mapped < (int)currentChordPack.chords.size()) {
                        int pv = currentChordPack.chords[mapped].preferredVoices;
                        stp.voiceCount = clamp(pv, 1, stx::transmutation::MAX_VOICES);
                    } else {
                        stp.voiceCount = voiceDist(rng);
                    }
                } else {
                    stp.voiceCount = voiceDist(rng);
                }
                // If this is sequence B and we are in Harmony mode, optionally limit voices to 1–2
                if (&seq == &sequenceB && ((int)params[SEQ_B_MODE_PARAM].getValue()) == 1 && harmonyLimitVoices) {
                    stp.voiceCount = 1 + (rng() % 2); // 1 or 2
                }
                anyChord = true;
            }
            seq.steps[i] = stp;
        }
        // Ensure at least one playable chord and step 0 is chord for immediate output
        if (!anyChord || seq.steps[0].chordIndex < 0) {
            int sidx = symbols[rng() % symbols.size()];
            seq.steps[0].chordIndex = sidx;
            seq.steps[0].alchemySymbolId = sidx;
            int mapped = symbolToChordMapping[sidx];
            int pv = (mapped >= 0 && mapped < (int)currentChordPack.chords.size()) ? currentChordPack.chords[mapped].preferredVoices : 3;
            seq.steps[0].voiceCount = clamp(pv, 1, stx::transmutation::MAX_VOICES);
        }
        // Keep playhead in bounds
        if (seq.currentStep >= len) seq.currentStep = 0;
    }

    // Integrate with Rack's default "Randomize" menu item
    void onRandomize() override {
        // Guard against host randomizing UI params: restore sticky values
        params[SEQ_B_MODE_PARAM].setValue((float)stickySeqBMode);
        params[SCREEN_STYLE_PARAM].setValue(stickyScreenStyle);
        randomizeSequence(sequenceA);
        randomizeSequence(sequenceB);
    }

    void onReset() override {
        // Called by the "Initialize" context menu button. Restore module to default state.
        // 1) Clear and reset sequences
        initializeSequences();

        // 2) Reset UI/edit state
        editModeA = false;
        editModeB = false;
        selectedSymbol = -1;
        displayChordName.clear();
        displaySymbolId = -999;
        symbolPreviewTimer = 0.f;
        for (int i = 0; i < 12; ++i) buttonPressAnim[i] = 0.f;

        // 3) Reset engine/state flags
        sequenceA.running = false;
        sequenceB.running = false;
        enableCvSlew = false;
        cvSlewMs = 3.0f;
        stablePolyChannels = true;
        forceSixPoly = false;
        gateMode = GATE_SUSTAIN;
        gatePulseMs = 8.0f;
        oneVoiceRandomNote = false;
        randomizeChordVoicing = false;
        grooveEnabled = false;
        grooveAmount = 0.0f;
        groovePreset = GROOVE_NONE;
        gridSteps = 32;

        // 4) Reset randomization options
        randomAllPack = true;
        randomAllLengths = true;
        randomAllSteps = true;
        randomAllBpm = true;
        randomAllMultiplier = true;
        randomUsePreferredVoices = true;
        randomRestProb = 0.12f;
        randomTieProb = 0.10f;
        randomChordProb = 0.60f;

        // 5) Reset parameters to defaults (mirrors constructor defaults)
        params[INTERNAL_CLOCK_PARAM].setValue(120.f);
        params[BPM_MULTIPLIER_PARAM].setValue(0.f);
        params[SEQ_B_MODE_PARAM].setValue(0.f);
        params[SCREEN_STYLE_PARAM].setValue(1.f); // Spooky on by default
        params[CHORD_DENSITY_PARAM].setValue(0.60f);
        params[REST_PROB_PARAM].setValue(0.12f);
        params[TIE_PROB_PARAM].setValue(0.10f);

        // 6) Reset chord pack and symbol mappings
        symbolToChordMapping.fill(-1);
        for (int i = 0; i < 12; ++i) buttonToSymbolMapping[i] = i;
        loadDefaultChordPack();

        // 7) Set matrix grid to 16 and reassert poly handshakes
        gridSteps = 16;
        reassertPolyA = true; reassertPolyB = true;
        oneShotExactPolyA = true; oneShotExactPolyB = true;
        forceChordUpdateA = true; forceChordUpdateB = true;
    }

    // Persist settings
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "gridSteps", json_integer(gridSteps));
        json_object_set_new(rootJ, "enableCvSlew", json_boolean(enableCvSlew));
        json_object_set_new(rootJ, "cvSlewMs", json_real(cvSlewMs));
        json_object_set_new(rootJ, "stablePolyChannels", json_boolean(stablePolyChannels));
        json_object_set_new(rootJ, "grooveEnabled", json_boolean(grooveEnabled));
        json_object_set_new(rootJ, "grooveAmount", json_real(grooveAmount));
        json_object_set_new(rootJ, "groovePreset", json_integer((int)groovePreset));
        json_object_set_new(rootJ, "randomRestProb", json_real(randomRestProb));
        json_object_set_new(rootJ, "randomTieProb", json_real(randomTieProb));
        json_object_set_new(rootJ, "randomUsePreferredVoices", json_boolean(randomUsePreferredVoices));
        json_object_set_new(rootJ, "randomChordProb", json_real(randomChordProb));
        json_object_set_new(rootJ, "randomAllPack", json_boolean(randomAllPack));
        json_object_set_new(rootJ, "randomAllLengths", json_boolean(randomAllLengths));
        json_object_set_new(rootJ, "randomAllSteps", json_boolean(randomAllSteps));
        json_object_set_new(rootJ, "randomAllBpm", json_boolean(randomAllBpm));
        json_object_set_new(rootJ, "randomAllMultiplier", json_boolean(randomAllMultiplier));
        json_object_set_new(rootJ, "forceSixPoly", json_boolean(forceSixPoly));
        json_object_set_new(rootJ, "gateMode", json_integer((int)gateMode));
        json_object_set_new(rootJ, "gatePulseMs", json_real(gatePulseMs));
        json_object_set_new(rootJ, "oneVoiceRandomNote", json_boolean(oneVoiceRandomNote));
        json_object_set_new(rootJ, "randomizeChordVoicing", json_boolean(randomizeChordVoicing));
        json_object_set_new(rootJ, "harmonyLimitVoices", json_boolean(harmonyLimitVoices));

        // Save display options
        json_object_set_new(rootJ, "doubleOccupancyMode", json_boolean(doubleOccupancyMode));

        // Save current chord pack
        json_t* chordPackJ = json_object();
        json_object_set_new(chordPackJ, "name", json_string(currentChordPack.name.c_str()));
        json_object_set_new(chordPackJ, "key", json_string(currentChordPack.key.c_str()));
        json_object_set_new(chordPackJ, "description", json_string(currentChordPack.description.c_str()));
        json_t* chordsJ = json_array();
        for (const auto& chord : currentChordPack.chords) {
            json_t* chordJ = json_object();
            json_object_set_new(chordJ, "name", json_string(chord.name.c_str()));
            json_object_set_new(chordJ, "preferredVoices", json_integer(chord.preferredVoices));
            json_object_set_new(chordJ, "category", json_string(chord.category.c_str()));
            json_t* intervalsJ = json_array();
            for (float interval : chord.intervals) {
                json_array_append_new(intervalsJ, json_real(interval));
            }
            json_object_set_new(chordJ, "intervals", intervalsJ);
            json_array_append_new(chordsJ, chordJ);
        }
        json_object_set_new(chordPackJ, "chords", chordsJ);
        json_object_set_new(rootJ, "currentChordPack", chordPackJ);

        // Save sequence A content
        json_t* seqAJ = json_object();
        json_object_set_new(seqAJ, "length", json_integer(sequenceA.length));
        json_object_set_new(seqAJ, "currentStep", json_integer(sequenceA.currentStep));
        json_object_set_new(seqAJ, "running", json_boolean(sequenceA.running));
        json_t* stepsAJ = json_array();
        for (int i = 0; i < sequenceA.length; i++) {
            json_t* stepJ = json_object();
            json_object_set_new(stepJ, "chordIndex", json_integer(sequenceA.steps[i].chordIndex));
            json_object_set_new(stepJ, "voiceCount", json_integer(sequenceA.steps[i].voiceCount));
            json_object_set_new(stepJ, "alchemySymbolId", json_integer(sequenceA.steps[i].alchemySymbolId));
            json_array_append_new(stepsAJ, stepJ);
        }
        json_object_set_new(seqAJ, "steps", stepsAJ);
        json_object_set_new(rootJ, "sequenceA", seqAJ);

        // Save sequence B content
        json_t* seqBJ = json_object();
        json_object_set_new(seqBJ, "length", json_integer(sequenceB.length));
        json_object_set_new(seqBJ, "currentStep", json_integer(sequenceB.currentStep));
        json_object_set_new(seqBJ, "running", json_boolean(sequenceB.running));
        json_t* stepsBJ = json_array();
        for (int i = 0; i < sequenceB.length; i++) {
            json_t* stepJ = json_object();
            json_object_set_new(stepJ, "chordIndex", json_integer(sequenceB.steps[i].chordIndex));
            json_object_set_new(stepJ, "voiceCount", json_integer(sequenceB.steps[i].voiceCount));
            json_object_set_new(stepJ, "alchemySymbolId", json_integer(sequenceB.steps[i].alchemySymbolId));
            json_array_append_new(stepsBJ, stepJ);
        }
        json_object_set_new(seqBJ, "steps", stepsBJ);
        json_object_set_new(rootJ, "sequenceB", seqBJ);

        // Note: Symbol mappings are NOT saved - they're randomized on each load

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* gJ = json_object_get(rootJ, "gridSteps");
        if (gJ && json_is_integer(gJ)) {
            int v = (int)json_integer_value(gJ);
            if (v == 16 || v == 32 || v == 64) gridSteps = v;
        }
        if (json_t* eSlew = json_object_get(rootJ, "enableCvSlew")) {
            enableCvSlew = json_boolean_value(eSlew);
        }
        if (json_t* vSlew = json_object_get(rootJ, "cvSlewMs")) {
            if (json_is_number(vSlew)) cvSlewMs = (float)json_number_value(vSlew);
        }
        if (json_t* spc = json_object_get(rootJ, "stablePolyChannels")) {
            stablePolyChannels = json_boolean_value(spc);
        }
        if (json_t* ge = json_object_get(rootJ, "grooveEnabled")) {
            grooveEnabled = json_boolean_value(ge);
        }
        if (json_t* ga = json_object_get(rootJ, "grooveAmount")) {
            if (json_is_number(ga)) grooveAmount = (float)json_number_value(ga);
        }
        if (json_t* gp = json_object_get(rootJ, "groovePreset")) {
            if (json_is_integer(gp)) groovePreset = (GroovePreset)json_integer_value(gp);
        }
        if (json_t* rrp = json_object_get(rootJ, "randomRestProb")) {
            if (json_is_number(rrp)) randomRestProb = (float)json_number_value(rrp);
            params[REST_PROB_PARAM].setValue(rack::math::clamp(randomRestProb, 0.f, 1.f));
        }
        if (json_t* rtp = json_object_get(rootJ, "randomTieProb")) {
            if (json_is_number(rtp)) randomTieProb = (float)json_number_value(rtp);
            params[TIE_PROB_PARAM].setValue(rack::math::clamp(randomTieProb, 0.f, 1.f));
        }
        if (json_t* rpv = json_object_get(rootJ, "randomUsePreferredVoices")) {
            randomUsePreferredVoices = json_boolean_value(rpv);
        }
        if (json_t* rcp = json_object_get(rootJ, "randomChordProb")) {
            if (json_is_number(rcp)) randomChordProb = (float)json_number_value(rcp);
            params[CHORD_DENSITY_PARAM].setValue(rack::math::clamp(randomChordProb, 0.f, 1.f));
        }
        if (json_t* rap = json_object_get(rootJ, "randomAllPack")) { randomAllPack = json_boolean_value(rap); }
        if (json_t* ral = json_object_get(rootJ, "randomAllLengths")) { randomAllLengths = json_boolean_value(ral); }
        if (json_t* ras = json_object_get(rootJ, "randomAllSteps")) { randomAllSteps = json_boolean_value(ras); }
        if (json_t* rab = json_object_get(rootJ, "randomAllBpm")) { randomAllBpm = json_boolean_value(rab); }
        if (json_t* ram = json_object_get(rootJ, "randomAllMultiplier")) { randomAllMultiplier = json_boolean_value(ram); }
        if (json_t* f6 = json_object_get(rootJ, "forceSixPoly")) {
            forceSixPoly = json_boolean_value(f6);
        }
        if (json_t* gm = json_object_get(rootJ, "gateMode")) {
            if (json_is_integer(gm)) gateMode = (GateMode)json_integer_value(gm);
        }
        if (json_t* gp = json_object_get(rootJ, "gatePulseMs")) {
            if (json_is_number(gp)) gatePulseMs = (float)json_number_value(gp);
        }
        if (json_t* ov = json_object_get(rootJ, "oneVoiceRandomNote")) {
            oneVoiceRandomNote = json_boolean_value(ov);
        }
        if (json_t* rv = json_object_get(rootJ, "randomizeChordVoicing")) {
            randomizeChordVoicing = json_boolean_value(rv);
        }
        if (json_t* hlv = json_object_get(rootJ, "harmonyLimitVoices")) {
            harmonyLimitVoices = json_boolean_value(hlv);
        }

        // Load display options
        if (json_t* dom = json_object_get(rootJ, "doubleOccupancyMode")) {
            doubleOccupancyMode = json_boolean_value(dom);
        }

        // Load chord pack
        if (json_t* cpJ = json_object_get(rootJ, "currentChordPack")) {
            currentChordPack.chords.clear();
            if (json_t* nameJ = json_object_get(cpJ, "name")) {
                if (json_is_string(nameJ)) currentChordPack.name = json_string_value(nameJ);
            }
            if (json_t* keyJ = json_object_get(cpJ, "key")) {
                if (json_is_string(keyJ)) currentChordPack.key = json_string_value(keyJ);
            }
            if (json_t* descJ = json_object_get(cpJ, "description")) {
                if (json_is_string(descJ)) currentChordPack.description = json_string_value(descJ);
            }
            if (json_t* chordsJ = json_object_get(cpJ, "chords")) {
                if (json_is_array(chordsJ)) {
                    size_t index;
                    json_t* chordJ;
                    json_array_foreach(chordsJ, index, chordJ) {
                        ChordData chord{};
                        if (json_t* cnJ = json_object_get(chordJ, "name")) {
                            if (json_is_string(cnJ)) chord.name = json_string_value(cnJ);
                        }
                        if (json_t* pvJ = json_object_get(chordJ, "preferredVoices")) {
                            if (json_is_integer(pvJ)) chord.preferredVoices = (int)json_integer_value(pvJ);
                        }
                        if (json_t* catJ = json_object_get(chordJ, "category")) {
                            if (json_is_string(catJ)) chord.category = json_string_value(catJ);
                        }
                        if (json_t* intJ = json_object_get(chordJ, "intervals")) {
                            if (json_is_array(intJ)) {
                                size_t intIndex;
                                json_t* intervalJ;
                                json_array_foreach(intJ, intIndex, intervalJ) {
                                    if (json_is_number(intervalJ)) {
                                        chord.intervals.push_back((float)json_number_value(intervalJ));
                                    }
                                }
                            }
                        }
                        currentChordPack.chords.push_back(chord);
                    }
                }
            }
            // Apply normalization after loading from saved patch JSON
            normalizeChordPack(currentChordPack);
        }

        // Load sequence A
        if (json_t* seqAJ = json_object_get(rootJ, "sequenceA")) {
            if (json_t* lenJ = json_object_get(seqAJ, "length")) {
                if (json_is_integer(lenJ)) {
                    int len = (int)json_integer_value(lenJ);
                    if (len >= 1 && len <= 64) sequenceA.length = len;
                }
            }
            if (json_t* curJ = json_object_get(seqAJ, "currentStep")) {
                if (json_is_integer(curJ)) sequenceA.currentStep = (int)json_integer_value(curJ);
            }
            if (json_t* runJ = json_object_get(seqAJ, "running")) {
                sequenceA.running = json_boolean_value(runJ);
            }
            if (json_t* stepsJ = json_object_get(seqAJ, "steps")) {
                if (json_is_array(stepsJ)) {
                    size_t index;
                    json_t* stepJ;
                    json_array_foreach(stepsJ, index, stepJ) {
                        if (index >= 64) break;
                        if (json_t* ciJ = json_object_get(stepJ, "chordIndex")) {
                            if (json_is_integer(ciJ)) sequenceA.steps[index].chordIndex = (int)json_integer_value(ciJ);
                        }
                        if (json_t* vcJ = json_object_get(stepJ, "voiceCount")) {
                            if (json_is_integer(vcJ)) sequenceA.steps[index].voiceCount = (int)json_integer_value(vcJ);
                        }
                        if (json_t* asiJ = json_object_get(stepJ, "alchemySymbolId")) {
                            if (json_is_integer(asiJ)) sequenceA.steps[index].alchemySymbolId = (int)json_integer_value(asiJ);
                        }
                    }
                }
            }
        }

        // Load sequence B
        if (json_t* seqBJ = json_object_get(rootJ, "sequenceB")) {
            if (json_t* lenJ = json_object_get(seqBJ, "length")) {
                if (json_is_integer(lenJ)) {
                    int len = (int)json_integer_value(lenJ);
                    if (len >= 1 && len <= 64) sequenceB.length = len;
                }
            }
            if (json_t* curJ = json_object_get(seqBJ, "currentStep")) {
                if (json_is_integer(curJ)) sequenceB.currentStep = (int)json_integer_value(curJ);
            }
            if (json_t* runJ = json_object_get(seqBJ, "running")) {
                sequenceB.running = json_boolean_value(runJ);
            }
            if (json_t* stepsJ = json_object_get(seqBJ, "steps")) {
                if (json_is_array(stepsJ)) {
                    size_t index;
                    json_t* stepJ;
                    json_array_foreach(stepsJ, index, stepJ) {
                        if (index >= 64) break;
                        if (json_t* ciJ = json_object_get(stepJ, "chordIndex")) {
                            if (json_is_integer(ciJ)) sequenceB.steps[index].chordIndex = (int)json_integer_value(ciJ);
                        }
                        if (json_t* vcJ = json_object_get(stepJ, "voiceCount")) {
                            if (json_is_integer(vcJ)) sequenceB.steps[index].voiceCount = (int)json_integer_value(vcJ);
                        }
                        if (json_t* asiJ = json_object_get(stepJ, "alchemySymbolId")) {
                            if (json_is_integer(asiJ)) sequenceB.steps[index].alchemySymbolId = (int)json_integer_value(asiJ);
                        }
                    }
                }
            }
        }

        // On load, populate symbol mappings but align button symbols to existing steps
        if (!currentChordPack.chords.empty()) {
            randomizeSymbolAssignment(false); // fill symbolToChordMapping
            deriveButtonsFromSequences();     // make buttons reflect placed symbols
        }
    }
};

struct TransmutationWidget : ModuleWidget {
    HighResMatrixWidget* matrix;
    // Draw background image behind panel and widgets without holding persistent window resources
    void draw(const DrawArgs& args) override {
        // Draw panel background texture first
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/vcv-panel-background.png"));
        if (bg) {
            NVGpaint paint = nvgImagePattern(args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, bg->handle, 1.0f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        }
        // Then draw the panel SVG and all child widgets/params/ports
        ModuleWidget::draw(args);
    }

    void appendContextMenu(Menu* menu) override {
        Transmutation* module = dynamic_cast<Transmutation*>(this->module);
        if (!module) return;

        auto check = [](bool on){ return on ? "✓" : ""; };

        // Steps Grid submenu
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Steps Grid", "", [module, check](Menu* sub) {
            sub->addChild(createMenuItem("16 steps", check(module->gridSteps == 16), [module]() {
                module->gridSteps = 16;
            }));
            sub->addChild(createMenuItem("32 steps", check(module->gridSteps == 32), [module]() {
                module->gridSteps = 32;
            }));
            sub->addChild(createMenuItem("64 steps", check(module->gridSteps == 64), [module]() {
                module->gridSteps = 64;
            }));
        }));

        // Display submenu
        menu->addChild(createSubmenuItem("Display", "", [module, check](Menu* sub) {
            sub->addChild(createMenuLabel("Display Mode"));
            sub->addChild(createMenuItem("Spooky TV Effect", check(module->params[Transmutation::SCREEN_STYLE_PARAM].getValue() > 0.5f), [module]() {
                float v = module->params[Transmutation::SCREEN_STYLE_PARAM].getValue();
                module->params[Transmutation::SCREEN_STYLE_PARAM].setValue(v > 0.5f ? 0.f : 1.f);
            }));
            sub->addChild(new MenuSeparator);
            sub->addChild(createMenuLabel("Step Occupancy"));
            sub->addChild(createMenuItem("Single (blended)", check(!module->doubleOccupancyMode), [module]() {
                module->doubleOccupancyMode = false;
            }));
            sub->addChild(createMenuItem("Double (split)", check(module->doubleOccupancyMode), [module]() {
                module->doubleOccupancyMode = true;
            }));
        }));

        // Pattern operations submenu
        menu->addChild(createSubmenuItem("Pattern Ops", "", [module](Menu* sub){
            sub->addChild(createSubmenuItem("Clear", "", [module](Menu* clearSub){
                clearSub->addChild(createMenuItem("Clear A", "", [module]() { module->clearSequence(module->sequenceA); }));
                clearSub->addChild(createMenuItem("Clear B", "", [module]() { module->clearSequence(module->sequenceB); }));
                clearSub->addChild(createMenuItem("Clear All", "", [module]() { 
                    module->clearSequence(module->sequenceA); 
                    module->clearSequence(module->sequenceB); 
                }));
            }));
            sub->addChild(createSubmenuItem("Shift A", "", [module](Menu* shiftSub){
                shiftSub->addChild(createMenuItem("Left",  "", [module]() { module->shiftSequence(module->sequenceA, -1); }));
                shiftSub->addChild(createMenuItem("Right", "", [module]() { module->shiftSequence(module->sequenceA, +1); }));
            }));
            sub->addChild(createSubmenuItem("Shift B", "", [module](Menu* shiftSub){
                shiftSub->addChild(createMenuItem("Left",  "", [module]() { module->shiftSequence(module->sequenceB, -1); }));
                shiftSub->addChild(createMenuItem("Right", "", [module]() { module->shiftSequence(module->sequenceB, +1); }));
            }));
            sub->addChild(createSubmenuItem("Copy / Swap", "", [module](Menu* copySub){
                copySub->addChild(createMenuItem("Copy A → B (with length)", "", [module]() { module->copySequence(module->sequenceA, module->sequenceB, true); }));
                copySub->addChild(createMenuItem("Copy B → A (with length)", "", [module]() { module->copySequence(module->sequenceB, module->sequenceA, true); }));
                copySub->addChild(createMenuItem("Swap A ↔ B (contents)", "", [module]() { module->swapSequencesContent(module->sequenceA, module->sequenceB); }));
            }));
        }));

        // Output shaping submenu
        menu->addChild(createSubmenuItem("Output Shaping", "", [module, check](Menu* sub) {
            sub->addChild(createMenuItem("CV Slew", check(module->enableCvSlew), [module]() {
                module->enableCvSlew = !module->enableCvSlew;
            }));
            sub->addChild(createMenuItem("Stable Poly Channels", check(module->stablePolyChannels), [module]() {
                module->stablePolyChannels = !module->stablePolyChannels;
            }));
            sub->addChild(createMenuItem("Force 6-voice Polyphony", check(module->forceSixPoly), [module]() {
                module->forceSixPoly = !module->forceSixPoly;
            }));
            sub->addChild(createSubmenuItem("Gate Mode", "", [module, check](Menu* gateSub){
                gateSub->addChild(createMenuItem("Sustain", check(module->gateMode == Transmutation::GATE_SUSTAIN), [module]() { module->gateMode = Transmutation::GATE_SUSTAIN; }));
                gateSub->addChild(createMenuItem("Pulse", check(module->gateMode == Transmutation::GATE_PULSE), [module]() { module->gateMode = Transmutation::GATE_PULSE; }));
            }));
        }));

        // Placement / Voicing submenu
        menu->addChild(createSubmenuItem("Placement / Voicing", "", [module, check](Menu* sub) {
            sub->addChild(createSubmenuItem("1-Voice Placement", "", [module, check](Menu* voiceSub){
                voiceSub->addChild(createMenuItem("First chord tone", check(!module->oneVoiceRandomNote), [module]() {
                    module->oneVoiceRandomNote = false;
                }));
                voiceSub->addChild(createMenuItem("Random chord tone", check(module->oneVoiceRandomNote), [module]() {
                    module->oneVoiceRandomNote = true;
                }));
            }));
            sub->addChild(createMenuItem("Randomize multi-voice voicing", check(module->randomizeChordVoicing), [module]() {
                module->randomizeChordVoicing = !module->randomizeChordVoicing;
            }));
            sub->addChild(createMenuItem("Harmony: limit to 1–2 voices", check(module->harmonyLimitVoices), [module]() {
                module->harmonyLimitVoices = !module->harmonyLimitVoices;
            }));
        }));

        // Advanced submenu
        menu->addChild(createSubmenuItem("Advanced", "", [module, check](Menu* adv){
            adv->addChild(createSubmenuItem("Pulse Width (ms)", "", [module, check](Menu* pulseSub){
                const float opts[] = {2.f, 5.f, 8.f, 10.f, 20.f, 50.f};
                for (float v : opts) {
                    std::string label = std::to_string((int)v);
                    pulseSub->addChild(createMenuItem(label, check(fabsf(module->gatePulseMs - v) < 0.5f), [module, v]() {
                        module->gatePulseMs = v;
                    }));
                }
            }));
            adv->addChild(createSubmenuItem("CV Slew (ms)", "", [module, check](Menu* slewSub){
                const float opts[] = {0.f, 1.f, 2.f, 3.f, 5.f, 10.f};
                for (float v : opts) {
                    std::string label = std::to_string((int)v);
                    slewSub->addChild(createMenuItem(label, check(fabsf(module->cvSlewMs - v) < 0.5f), [module, v]() {
                        module->cvSlewMs = v;
                    }));
                }
            }));
        }));

        // Randomization submenu
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Randomize Everything", "", [module, check](Menu* randMenu) {
            randMenu->addChild(createMenuLabel("Randomization Options"));
            randMenu->addChild(createMenuItem("Pack", check(module->randomAllPack), [module]() {
                module->randomAllPack = !module->randomAllPack;
            }));
            randMenu->addChild(createMenuItem("Sequence Lengths", check(module->randomAllLengths), [module]() {
                module->randomAllLengths = !module->randomAllLengths;
            }));
            randMenu->addChild(createMenuItem("Step Content", check(module->randomAllSteps), [module]() {
                module->randomAllSteps = !module->randomAllSteps;
            }));
            randMenu->addChild(createMenuItem("BPM", check(module->randomAllBpm), [module]() {
                module->randomAllBpm = !module->randomAllBpm;
            }));
            randMenu->addChild(createMenuItem("Clock Multiplier", check(module->randomAllMultiplier), [module]() {
                module->randomAllMultiplier = !module->randomAllMultiplier;
            }));
            randMenu->addChild(new MenuSeparator);
            randMenu->addChild(createMenuItem("Use Preferred Voice Counts", check(module->randomUsePreferredVoices), [module]() {
                module->randomUsePreferredVoices = !module->randomUsePreferredVoices;
            }));
            randMenu->addChild(createMenuLabel("Step Symbol Source"));
            randMenu->addChild(createMenuItem("Use 12 Button Symbols", "✓", [module]() {
                // Always use 12 visible symbols on randomization (requested behavior)
            }));

            // Sliders for chord density, rest, and tie probabilities (Impromptu pattern)
            randMenu->addChild(new MenuSeparator);
            struct ProbQuantity : Quantity {
                float* value = nullptr;
                float def = 0.f;
                std::string label;
                ProbQuantity(float* ref, const char* lab, float d) { value = ref; label = lab; def = d; }
                void setValue(float v) override { *value = rack::math::clamp(v, 0.f, 1.f); }
                float getValue() override { return *value; }
                float getMinValue() override { return 0.f; }
                float getMaxValue() override { return 1.f; }
                float getDefaultValue() override { return def; }
                float getDisplayValue() override { return getValue() * 100.f; }
                void setDisplayValue(float v) override { setValue(v / 100.f); }
                std::string getLabel() override { return label; }
                std::string getUnit() override { return "%"; }
            };
            struct ProbSlider : ui::Slider {
                ProbSlider(float* ref, const char* label, float def) {
                    quantity = new ProbQuantity(ref, label, def);
                }
                ~ProbSlider() override { delete quantity; }
            };
            auto addProbSlider = [&](Menu* m, const char* label, float& ref, float def) {
                m->addChild(createMenuLabel(label));
                auto* s = new ProbSlider(&ref, label, def);
                s->box.size.x = 200.0f;
                m->addChild(s);
            };
            addProbSlider(randMenu, "Chord Density", module->randomChordProb, 0.60f);
            addProbSlider(randMenu, "Rest Probability", module->randomRestProb, 0.12f);
            addProbSlider(randMenu, "Tie Probability", module->randomTieProb, 0.10f);

            randMenu->addChild(new MenuSeparator);
            randMenu->addChild(createMenuItem("⚡ Randomize Now!", "", [module]() {
                module->randomizeEverything();
            }));
        }));

        // Chord packs submenu (reverted to key-based organization)
        menu->addChild(createSubmenuItem("Chord Packs", "", [module](Menu* chordMenu) {
            // Helper to get a friendly display name from a JSON pack, falling back to filename stem
            auto packDisplayName = [](const std::string& packPath, const std::string& fallbackStem) {
                std::string display = fallbackStem;
                try {
                    std::string content;
                    if (system::exists(packPath)) {
                        std::string data;
                        std::ifstream f(packPath);
                        if (f) {
                            std::stringstream ss; ss << f.rdbuf();
                            content = ss.str();
                        }
                    }
                    if (!content.empty()) {
                        json_error_t error;
                        json_t* rootJ = json_loads(content.c_str(), 0, &error);
                        if (rootJ) {
                            json_t* nameJ = json_object_get(rootJ, "name");
                            if (nameJ && json_is_string(nameJ)) {
                                display = json_string_value(nameJ);
                            }
                            json_decref(rootJ);
                        }
                    }
                } catch (...) {}
                return display;
            };

            // Root chord pack directory inside plugin assets
            std::string chordPackDir = asset::plugin(pluginInstance, "chord_packs");

            // Default pack at top
            std::string rightText = (module->currentChordPack.name == "Basic Major") ? "✓" : "";
            chordMenu->addChild(createMenuItem("Basic Major", rightText, [module]() {
                module->loadDefaultChordPack();
                module->displayChordName = "Basic Major";
                module->displaySymbolId = -999; // text-only preview
                module->symbolPreviewTimer = 1.0f;
            }));

            // Random chord pack helpers
            chordMenu->addChild(createMenuItem("Random Pack (Safe)", "", [module]() {
                module->randomizePackSafe();
            }));
            chordMenu->addChild(createMenuItem("Random Pack", "", [module]() {
                if (!module->randomizeChordPack()) {
                    // Fallback to default if none found
                    module->loadDefaultChordPack();
                }
            }));

            if (!system::isDirectory(chordPackDir)) {
                chordMenu->addChild(createMenuLabel("No chord_packs directory found"));
                return;
            }

            // Helpers for clean labels
            auto basename = [](const std::string& path) {
                size_t pos = path.find_last_of("/\\");
                return (pos == std::string::npos) ? path : path.substr(pos + 1);
            };
            auto stem = [](const std::string& filename) {
                size_t slash = filename.find_last_of("/\\");
                std::string name = (slash == std::string::npos) ? filename : filename.substr(slash + 1);
                size_t dot = name.find_last_of('.');
                return (dot == std::string::npos) ? name : name.substr(0, dot);
            };

            // Collect key directories as absolute paths, sort by base name
            std::vector<std::string> keyDirs;
            for (const std::string& entry : system::getEntries(chordPackDir)) {
                std::string fullPath = entry;
                if (system::isDirectory(fullPath)) keyDirs.push_back(fullPath);
            }
            std::sort(keyDirs.begin(), keyDirs.end(), [&](const std::string& a, const std::string& b) {
                return basename(a) < basename(b);
            });

            if (!keyDirs.empty()) chordMenu->addChild(new MenuSeparator);

            // Build submenus per key with friendly names, no absolute paths
            for (const std::string& keyPath : keyDirs) {
                if (!system::isDirectory(keyPath)) continue;
                std::string keyLabel = basename(keyPath);

                // Gather pack files
                std::vector<std::string> packFiles;
                for (const std::string& fileEntry : system::getEntries(keyPath)) {
                    if (system::getExtension(fileEntry) == ".json") packFiles.push_back(fileEntry);
                }
                std::sort(packFiles.begin(), packFiles.end(), [&](const std::string& a, const std::string& b){
                    return stem(a) < stem(b);
                });
                if (packFiles.empty()) continue;

                chordMenu->addChild(createSubmenuItem(keyLabel, "", [module, keyPath, keyLabel, packFiles, packDisplayName, stem](Menu* keySubmenu) {
                    keySubmenu->addChild(createMenuLabel("Key: " + keyLabel));
                    keySubmenu->addChild(new MenuSeparator);

                    for (const std::string& packFile : packFiles) {
                        std::string packPath = packFile; // absolute path
                        std::string packStem = stem(packFile);
                        std::string displayName = packDisplayName(packPath, packStem);

                        std::string check = "";
                        if (module) {
                            if (module->currentChordPack.name == displayName ||
                                module->currentChordPack.name.find(packStem) != std::string::npos) {
                                check = "✓";
                            }
                        }

                        keySubmenu->addChild(createMenuItem(displayName, check, [module, packPath, displayName]() {
                            if (!module) return;
                        if (module->loadChordPackFromFile(packPath)) {
                            module->displayChordName = displayName;
                            module->displaySymbolId = -999; // text-only preview
                            module->symbolPreviewTimer = 1.0f;
                            INFO("Loaded chord pack: %s", displayName.c_str());
                            } else {
                                module->displayChordName = "LOAD ERROR";
                                module->displaySymbolId = -999;
                                module->symbolPreviewTimer = 1.0f;
                                WARN("Failed to load chord pack: %s", packPath.c_str());
                            }
                        }));
                    }
                }));
            }
        }));
    }

    TransmutationWidget(Transmutation* module) {
        setModule(module);

        // 26HP = 131.318mm width
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Transmutation.svg")));

        // Add screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Background drawn in draw()

        // Read positions from SVG by id (simple attribute parser)
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Transmutation.svg");
        std::string svg;
        {
            std::ifstream f(svgPath);
            if (f) {
                std::stringstream ss; ss << f.rdbuf();
                svg = ss.str();
            }
        }
        auto findTagForId = [&](const std::string& id) -> std::string {
            if (svg.empty()) return "";
            std::string needle = "id=\"" + id + "\"";
            size_t pos = svg.find(needle);
            if (pos == std::string::npos) return "";
            // Find start of tag
            size_t start = svg.rfind('<', pos);
            size_t end = svg.find('>', pos);
            if (start == std::string::npos || end == std::string::npos || end <= start) return "";
            return svg.substr(start, end - start + 1);
        };
        auto getAttr = [&](const std::string& tag, const std::string& key, float defVal) -> float {
            if (tag.empty()) return defVal;
            std::string k = key + "=\"";
            size_t p = tag.find(k);
            if (p == std::string::npos) return defVal;
            p += k.size();
            size_t q = tag.find('"', p);
            if (q == std::string::npos) return defVal;
            try {
                return std::stof(tag.substr(p, q - p));
            } catch (...) { return defVal; }
        };

        // High-Resolution 8x8 Matrix positioned from <rect id="main_screen" x y width height>
        matrix = new HighResMatrixWidget(
            static_cast<stx::transmutation::TransmutationView*>(module), 
            static_cast<stx::transmutation::TransmutationController*>(module)
        );
        {
            std::string tag = findTagForId("main_screen");
            float mx = getAttr(tag, "x", 27.143473f);
            float my = getAttr(tag, "y", 34.0f);
            float mw = getAttr(tag, "width", 77.0f);
            float mh = getAttr(tag, "height", 77.0f);
            matrix->box.pos = Vec(mm2px(mx), mm2px(my));
            matrix->box.size = Vec(mm2px(mw), mm2px(mh));
        }
        addChild(matrix);

        // Helper to add a 10% upsized vintage momentary button centered at position
        auto addMomentaryScaled = [&](Vec pixelPos, int paramId) {
            auto* w = createParamCentered<ShapetakerVintageMomentary>(pixelPos, module, paramId);
            // Preserve center while scaling size by 1.10x
            Vec center = w->box.pos.plus(w->box.size.div(2.0f));
            // Adjusted to 1.05x per user feedback
            w->box.size = w->box.size.mult(1.05f);
            w->box.pos = center.minus(w->box.size.div(2.0f));
            addParam(w);
        };

        // Edit mode buttons (above matrix) - from SVG circles edit_a_btn/edit_b_btn (cx, cy)
        {
            std::string tA = findTagForId("edit_a_btn");
            std::string tB = findTagForId("edit_b_btn");
            float ax = getAttr(tA, "cx", 55.973103f);
            float ay = getAttr(tA, "cy", 16.805513f);
            float bx = getAttr(tB, "cx", 74.402115f);
            float by = getAttr(tB, "cy", 16.678213f);
            addMomentaryScaled(mm2px(Vec(ax, ay)), Transmutation::EDIT_A_PARAM);
            addMomentaryScaled(mm2px(Vec(bx, by)), Transmutation::EDIT_B_PARAM);
        }

        // Edit mode lights removed

        // Left/Right controls - read from panel IDs to stay in sync with SVG
        {
            auto pos = [&](const std::string& id, float defx, float defy) {
                std::string tag = findTagForId(id);
                float cx = getAttr(tag, "cx", defx);
                float cy = getAttr(tag, "cy", defy);
                if (tag.find("<rect") != std::string::npos) {
                    float rx = getAttr(tag, "x", defx);
                    float ry = getAttr(tag, "y", defy);
                    float rw = getAttr(tag, "width", 0.0f);
                    float rh = getAttr(tag, "height", 0.0f);
                    cx = rx + rw * 0.5f;
                    cy = ry + rh * 0.5f;
                }
                return mm2px(Vec(cx, cy));
            };
            // Sequence A
            addParam(createParamCentered<ShapetakerKnobMedium>(pos("seq_a_length", 15.950587f, 37.849998f), module, Transmutation::LENGTH_A_PARAM));
            addParam(createParamCentered<ShapetakerKnobMedium>(pos("main_bpm", 15.950588f, 18.322521f), module, Transmutation::INTERNAL_CLOCK_PARAM));
            addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(pos("clk_mult_select", 34.340317f, 18.322521f), module, Transmutation::BPM_MULTIPLIER_PARAM));
            addMomentaryScaled(pos("a_play_btn", 22.586929f, 67.512939f), Transmutation::START_A_PARAM);
            addMomentaryScaled(pos("a_stop_btn", 22.784245f, 75.573959f), Transmutation::STOP_A_PARAM);
            addMomentaryScaled(pos("a_reset_btn", 22.784245f, 83.509323f), Transmutation::RESET_A_PARAM);
            // Sequence B
            addParam(createParamCentered<ShapetakerKnobMedium>(pos("seq_b_length", 115.02555f, 37.849998f), module, Transmutation::LENGTH_B_PARAM));
            addMomentaryScaled(pos("b_play_btn", 108.43727f, 67.450111f), Transmutation::START_B_PARAM);
            addMomentaryScaled(pos("b_stop_btn", 108.43727f, 75.511131f), Transmutation::STOP_B_PARAM);
            addMomentaryScaled(pos("b_reset_btn", 108.43728f, 83.446495f), Transmutation::RESET_B_PARAM);
            addParam(createParamCentered<ShapetakerVintageSelector>(pos("mode_switch", 110.08858f, 19.271444f), module, Transmutation::SEQ_B_MODE_PARAM));
        }

        // Status display widget removed per design cleanup

        // I/O - read from panel IDs to stay in sync
        {
            auto cpos = [&](const std::string& id, float defx, float defy) {
                std::string tag = findTagForId(id);
                float cx = getAttr(tag, "cx", defx);
                float cy = getAttr(tag, "cy", defy);
                return mm2px(Vec(cx, cy));
            };
            // A side
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("a_clk_cv", 15.950586f, 95.834518f), module, Transmutation::CLOCK_A_INPUT));
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("a_reset_cv", 7.5470452f, 83.509323f), module, Transmutation::RESET_A_INPUT));
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("a_play_cv", 7.5470452f, 67.512939f), module, Transmutation::START_A_INPUT));
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("a_stop_cv", 7.5470452f, 75.511131f), module, Transmutation::STOP_A_INPUT));
            addOutput(createOutputCentered<ShapetakerBNCPort>(cpos("a_cv_out", 15.950586f, 105.7832f), module, Transmutation::CV_A_OUTPUT));
            addOutput(createOutputCentered<ShapetakerBNCPort>(cpos("a_gate_out", 15.950586f, 115.73187f), module, Transmutation::GATE_A_OUTPUT));
            // B side
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("b_clk_cv", 115.02555f, 95.834518f), module, Transmutation::CLOCK_B_INPUT));
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("b_reset_cv", 123.6797f, 83.509323f), module, Transmutation::RESET_B_INPUT));
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("b_play_cv", 123.6797f, 67.512939f), module, Transmutation::START_B_INPUT));
            addInput(createInputCentered<ShapetakerBNCPort>(cpos("b_stop_cv", 123.6797f, 75.511131f), module, Transmutation::STOP_B_INPUT));
            addOutput(createOutputCentered<ShapetakerBNCPort>(cpos("b_cv_out", 115.02555f, 105.7832f), module, Transmutation::CV_B_OUTPUT));
            addOutput(createOutputCentered<ShapetakerBNCPort>(cpos("b_gate_out", 115.02555f, 115.73187f), module, Transmutation::GATE_B_OUTPUT));
        }

        // Alchemical Symbol Buttons from SVG rects alchem_1..alchem_12 (x,y are top-left)
        for (int i = 0; i < 12; i++) {
            std::string id = std::string("alchem_") + std::to_string(i + 1);
            std::string tag = findTagForId(id);
            float x = getAttr(tag, "x", (i < 6 ? (36.0f + 10.65f * i) : (36.0f + 10.65f * (i - 6))));
            float y = getAttr(tag, "y", (i < 6 ? 110.0f : 117.56f)); // Both rows at bottom of matrix
            float wRect = getAttr(tag, "width", 6.0f);
            float hRect = getAttr(tag, "height", 6.0f);
            // Enlarge buttons but keep center aligned in their reserved rect
            float scale = 1.22f; // ~22% larger while keeping separation
            float w = wRect * scale;
            float h = hRect * scale;
            float cx = x + wRect * 0.5f;
            float cy = y + hRect * 0.5f;
            float xPos = cx - w * 0.5f;
            float yPos = cy - h * 0.5f;
            AlchemicalSymbolWidget* symbolWidget = new AlchemicalSymbolWidget(
                static_cast<stx::transmutation::TransmutationView*>(module), 
                static_cast<stx::transmutation::TransmutationController*>(module), 
                i
            );
            symbolWidget->box.pos = mm2px(Vec(xPos, yPos));
            symbolWidget->box.size = mm2px(Vec(w, h));
            addChild(symbolWidget);
        }

        // Rest and Tie buttons from SVG ids rest_btn/tie_btn (cx, cy)
        {
            std::string tr = findTagForId("rest_btn");
            std::string tt = findTagForId("tie_btn");
            float rx = getAttr(tr, "cx", 15.950587f);
            float ry = getAttr(tr, "cy", 53.27956f);
            float tx = getAttr(tt, "cx", 115.02555f);
            float ty = getAttr(tt, "cy", 53.27956f);
            addMomentaryScaled(mm2px(Vec(rx, ry)), Transmutation::REST_PARAM);
            addMomentaryScaled(mm2px(Vec(tx, ty)), Transmutation::TIE_PARAM);
        }

        // Running lights from SVG ids seq_a_led/seq_b_led (cx, cy)
        {
            std::string la = findTagForId("seq_a_led");
            std::string lb = findTagForId("seq_b_led");
            float ax = getAttr(la, "cx", 29.029953f);
            float ay = getAttr(la, "cy", 33.132351f);
            float bx = getAttr(lb, "cx", 102.28805f);
            float by = getAttr(lb, "cy", 33.5513f);
            addChild(createLightCentered<shapetaker::transmutation::TealJewelLEDMedium>(mm2px(Vec(ax, ay)), module, Transmutation::RUNNING_A_LIGHT));
            addChild(createLightCentered<shapetaker::transmutation::PurpleJewelLEDMedium>(mm2px(Vec(bx, by)), module, Transmutation::RUNNING_B_LIGHT));
        }

        // Panel-wide patina overlay for cohesive vintage appearance (added last so it sits on top subtly)
        auto overlay = new PanelPatinaOverlay();
        overlay->box = Rect(Vec(0, 0), box.size);
        addChild(overlay);
    }
};

Model* modelTransmutation = createModel<Transmutation, TransmutationWidget>("Transmutation");
