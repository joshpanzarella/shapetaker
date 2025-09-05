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
#include "voice/PolyOut.hpp"

using stx::transmutation::ChordPack;
using stx::transmutation::ChordData;

// Forward declarations
struct Transmutation;

// Custom Shapetaker Widgets: use shared declarations from plugin.hpp / shapetakerWidgets.hpp


// Custom colored jewel LEDs moved to shared widgets (plugin.hpp)

// Use the jewel LEDs from plugin.hpp - they're already properly defined there
// No need to redefine them here since they're in the global header

#include "transmutation/chords.hpp"
#include "transmutation/engine.hpp"

// HighResMatrixWidget moved to src/transmutation/ui.{hpp,cpp}

// (Legacy Matrix8x8Widget removed)


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
    
    // Chord pack system
    ChordPack currentChordPack;
    std::array<int, st::SymbolCount> symbolToChordMapping; // Mapping for all symbols
    std::array<int, 12> buttonToSymbolMapping; // Maps button positions 0-11 to symbol IDs 0-39
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
    }
    
    void process(const ProcessArgs& args) override {
        engineTimeSec += args.sampleTime;
        // Mirror param to internal flag for UI drawing
        spookyTvMode = params[SCREEN_STYLE_PARAM].getValue() > 0.5f;

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
            INFO("Loaded: '%s' (%d chords)", currentChordPack.name.c_str(), (int)currentChordPack.chords.size());
            // Keep placed symbols as-is; only chord mappings change
            randomizeSymbolAssignment(false);
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
    
    void loadDefaultChordPack() {
        stx::transmutation::loadDefaultChordPack(currentChordPack);
        // Keep placed symbols as-is; only chord mappings change
        randomizeSymbolAssignment(false);
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
        auto fits = [&](const std::pair<int,int>& p){ return p.first <= gridSteps && p.second <= gridSteps; };
        std::vector<std::pair<int,int>> curatedFit;
        for (auto& p : curated) if (fits(p)) curatedFit.push_back(p);

        auto gcd = [](int a, int b){ a = std::abs(a); b = std::abs(b); while (b) { int t = a % b; a = b; b = t; } return a; };
        auto pickFrom = [&](const std::vector<int>& p) -> int {
            if (p.empty()) return std::max(1, std::min(gridSteps, 8));
            uint32_t r = rack::random::u32();
            return p[r % p.size()];
        };

        // Weighted pool with bias toward 4..16 and select A
        std::vector<int> pool; pool.reserve(vals.size() * 3);
        for (int v : vals) {
            int w = (v >= 4 && v <= 16) ? 3 : ((v == 24 || v == 28 || v == 32) ? 2 : 1);
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
            int idx = (int)(rack::random::u32() % 4); // 0..3
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
    }

    void randomizePackSafe() {
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
    bool randomAllBpm = false;
    bool randomAllMultiplier = false;
    bool  randomUsePreferredVoices = true; // prefer chord.preferredVoices for chord steps

    // Randomize a sequence's content (steps and voice counts)
    void randomizeSequence(Sequence& seq) {
        auto symbols = getValidSymbols();
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
        randomAllBpm = false;
        randomAllMultiplier = false;
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

        // 7) Reassert poly handshakes for downstream modules
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
        
        // Randomize symbol mappings after loading chord pack; keep placed symbols unchanged
        if (!currentChordPack.chords.empty()) {
            randomizeSymbolAssignment(false);
        }
    }
};


// HighResMatrixWidget draw moved to ui.cpp
#if 0 // Legacy implementation superseded by src/transmutation/ui.cpp
void HighResMatrixWidget::drawMatrix(const DrawArgs& args) {
    float time = APP->engine->getFrame() * 0.0009f;
    float waveA = sinf(time * 0.30f) * 0.10f + sinf(time * 0.50f) * 0.06f;
        float waveB = cosf(time * 0.25f) * 0.08f + cosf(time * 0.45f) * 0.05f;
        float tapeWarp = sinf(time * 0.15f) * 0.04f + cosf(time * 0.22f) * 0.025f;
        float deepWarp = sinf(time * 0.09f) * 0.06f;
        
        // Dark VHS horror movie background with slight blue tint and warping - rounded corners
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8.0f); // Match LED matrix corner radius
        int bgBrightness = 8 + (int)(waveA * 12); // Slower brightness variation
        nvgFillColor(args.vg, nvgRGBA(bgBrightness, bgBrightness * 0.8f, bgBrightness * 1.3f, 255));
        nvgFill(args.vg);
        
        // Set up clipping to keep all effects within the rounded rectangle
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, 0, 0, box.size.x, box.size.y);
        // Use shared spooky overlay helpers
        st::VhsState s { time, waveA, waveB, tapeWarp, deepWarp };
        st::drawSpookyScanlines(args, box.size.x, box.size.y, s, /*smallVariant*/ false);
        st::drawSpookyDistortionBands(args, box.size.x, box.size.y, s, /*smallVariant*/ false);
        
        nvgRestore(args.vg); // End clipping
        
        // Draw the symbol with Shapetaker colors and VHS warping
        nvgSave(args.vg);
        float shakeX = sinf(time * 0.55f) * 0.5f + tapeWarp * 1.8f + deepWarp * 1.4f; // even slower, smoother wobble
        float shakeY = cosf(time * 0.40f) * 0.4f + waveA * 1.2f + waveB * 0.8f; // slower, layered
        nvgTranslate(args.vg, box.size.x / 2 + shakeX, box.size.y * 0.40f + shakeY); // More centered vertically
        nvgScale(args.vg, 5.0f, 5.0f); // Much bigger and more pixelated
        
        // Cycle through Shapetaker colors with ultra slow VHS color shift
        float colorCycle = sin(time * 0.3f) * 0.5f + 0.5f; // Even slower color transitions like old tape
        int symbolR, symbolG, symbolB;
        
        if (colorCycle < 0.25f) {
            // Teal with VHS warping
            symbolR = 0;
            symbolG = 180 + (int)(waveA * 50);
            symbolB = 180 + (int)(waveB * 50);
        } else if (colorCycle < 0.5f) {
            // Purple with VHS warping  
            symbolR = 180 + (int)(waveA * 50);
            symbolG = 0;
            symbolB = 255;
        } else if (colorCycle < 0.75f) {
            // Muted green with VHS warping
            symbolR = 60 + (int)(waveB * 30);
            symbolG = 120 + (int)(waveA * 40);
            symbolB = 80 + (int)(tapeWarp * 80);
        } else {
            // Gray with VHS warping
            symbolR = 140 + (int)(waveA * 40);
            symbolG = 140 + (int)(waveB * 40);
            symbolB = 150 + (int)(tapeWarp * 60);
        }
        
        // Draw symbol with intense glow layers
        for (int glow = 0; glow < 6; glow++) {
            float glowAlpha = 80 / (glow + 1);
            float glowOffset = glow * 0.8f;
            
            // Create blur effect with multiple offset draws
            for (int blur = 0; blur < 2; blur++) {
                float blurOffset = blur * 0.3f;
                st::drawAlchemicalSymbol(args, Vec(glowOffset + blurOffset, glowOffset + blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                st::drawAlchemicalSymbol(args, Vec(-glowOffset + blurOffset, glowOffset - blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                st::drawAlchemicalSymbol(args, Vec(glowOffset - blurOffset, -glowOffset + blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                st::drawAlchemicalSymbol(args, Vec(-glowOffset - blurOffset, -glowOffset - blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
            }
        }
        
        // Main symbol with bright core (positioned to align with glow effects at center)
        st::drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGBA(symbolR, symbolG, symbolB, 255));
        
        // Extra bright white core for VHS warping intensity
        st::drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGBA(255, 255, 255, 50 + (int)(waveA * 30)));
        nvgRestore(args.vg);
        
        // Set up vintage text for chord name (warm ink + soft shadow)
        nvgFontSize(args.vg, 32.0f);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        
        // Calculate maximum text width (80% of matrix width for padding)
        float maxTextWidth = box.size.x * 0.8f;
        
        // Wrap text to fit within matrix bounds
        std::vector<std::string> textLines = wrapText(module->displayChordName, maxTextWidth, args.vg);
        
        // Calculate starting position for multi-line text
        float lineHeight = 36.0f; // Space between lines
        float totalHeight = textLines.size() * lineHeight;
        float startY = (box.size.y * 0.75f) - (totalHeight / 2.0f);
        
        // Text position with slow, intense VHS warping movement, closer to symbol
        float baseTextX = box.size.x / 2 + sin(time * 1.1f) * 0.6f + tapeWarp * 2.5f + deepWarp * 2.0f;
        
        // Text color is a consistent warm ink; remove unused cycling variables
        
        // Render each line of text with all effects
        for (size_t i = 0; i < textLines.size(); i++) {
            float textY = startY + (i * lineHeight) + cos(time * 0.9f) * 0.4f + waveB * 1.2f + waveA * 0.8f;
            
            // Soft single shadow
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 140));
            nvgText(args.vg, baseTextX + 1.5f, textY + 1.5f, textLines[i].c_str(), NULL);
            // Fuzzy halo (low-alpha offsets) for a softer, vintage CRT look
            NVGcolor inkFuzz = nvgRGBA(232, 224, 200, 110);
            const float fx[8] = {  0.9f, -0.9f,  0.9f, -0.9f,  0.6f, -0.6f,  0.0f,  0.0f };
            const float fy[8] = {  0.9f,  0.9f, -0.9f, -0.9f,  0.0f,  0.0f,  0.6f, -0.6f };
            for (int k = 0; k < 8; ++k) {
                nvgFillColor(args.vg, inkFuzz);
                nvgText(args.vg, baseTextX + fx[k], textY + fy[k], textLines[i].c_str(), NULL);
            }
            // Warm ink main fill
            nvgFillColor(args.vg, nvgRGBA(232, 224, 200, 240));
            nvgText(args.vg, baseTextX, textY, textLines[i].c_str(), NULL);
        }
        
        nvgRestore(args.vg);
        return;
        } else {
            // Clean chord name display mode
            nvgSave(args.vg);
            
            // Clean, simple background
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8.0f);
            nvgFillColor(args.vg, nvgRGBA(20, 20, 25, 240)); // Dark subtle background
            nvgFill(args.vg);
            
            // Simple symbol display centered
            nvgSave(args.vg);
            nvgTranslate(args.vg, box.size.x / 2, box.size.y * 0.35f);
            nvgScale(args.vg, 4.0f, 4.0f); // Larger but clean
            st::drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGB(0, 255, 180), 6.5f, 1.5f); // Teal color
            nvgRestore(args.vg);
            
            // Clean text display
            nvgFontSize(args.vg, 24.0f);
            nvgFontFaceId(args.vg, APP->window->uiFont->handle);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            
            // Calculate maximum text width
            float maxTextWidth = box.size.x * 0.9f;
            std::vector<std::string> textLines = wrapText(module->displayChordName, maxTextWidth, args.vg);
            
            // Position text below symbol
            float lineHeight = 26.0f;
            float startY = box.size.y * 0.70f - ((textLines.size() - 1) * lineHeight / 2.0f);
            
            // Simple white text
            nvgFillColor(args.vg, nvgRGB(240, 240, 240));
            for (size_t i = 0; i < textLines.size(); i++) {
                float textY = startY + (i * lineHeight);
                nvgText(args.vg, box.size.x / 2, textY, textLines[i].c_str(), NULL);
            }
            
            nvgRestore(args.vg);
            return;
        }
    }

    // High-resolution matrix rendering
    nvgSave(args.vg);
    
    // Set up high-quality rendering
    nvgShapeAntiAlias(args.vg, true);
    
    // Draw matrix background with subtle gradient
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8);
    NVGpaint bg = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, nvgRGBA(15, 15, 20, 255), nvgRGBA(5, 5, 10, 255));
    nvgFillPaint(args.vg, bg);
    nvgFill(args.vg);

    // Bezel: subtle inner shadow and outer stroke to give depth
    {
        float rOuter = 8.0f;
        float inset = 2.0f;
        float rInner = std::max(0.0f, rOuter - 2.0f);

        // Outer border
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, rOuter);
        nvgStrokeColor(args.vg, nvgRGBA(10, 10, 14, 220));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);

        // Inner shadow ring using box gradient
        NVGpaint innerShadow = nvgBoxGradient(
            args.vg,
            inset, inset,
            box.size.x - inset * 2.0f,
            box.size.y - inset * 2.0f,
            rInner, 6.0f,
            nvgRGBA(0, 0, 0, 60),
            nvgRGBA(0, 0, 0, 0)
        );
        nvgBeginPath(args.vg);
        // Outer path of ring
        nvgRoundedRect(args.vg, inset - 1.0f, inset - 1.0f, box.size.x - (inset - 1.0f) * 2.0f, box.size.y - (inset - 1.0f) * 2.0f, rInner + 1.0f);
        // Inner hole
        nvgRoundedRect(args.vg, inset + 1.0f, inset + 1.0f, box.size.x - (inset + 1.0f) * 2.0f, box.size.y - (inset + 1.0f) * 2.0f, std::max(0.0f, rInner - 1.0f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, innerShadow);
        nvgFill(args.vg);

        // Top highlight fade
        nvgSave(args.vg);
        nvgScissor(args.vg, 0, 0, box.size.x, std::min(10.0f, box.size.y));
        NVGpaint topHi = nvgLinearGradient(args.vg, 0, 0, 0, 10.0f, nvgRGBA(255, 255, 255, 24), nvgRGBA(255, 255, 255, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset + 0.5f, inset + 0.5f, box.size.x - (inset + 1.0f), 8.0f, rInner);
        nvgFillPaint(args.vg, topHi);
        nvgFill(args.vg);
        nvgRestore(args.vg);

        // Bottom shadow fade for symmetry
        nvgSave(args.vg);
        nvgScissor(args.vg, 0, box.size.y - std::min(10.0f, box.size.y), box.size.x, std::min(10.0f, box.size.y));
        NVGpaint bottomShadow = nvgLinearGradient(args.vg, 0, box.size.y, 0, box.size.y - 10.0f, nvgRGBA(0, 0, 0, 40), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset + 0.5f, box.size.y - 8.5f, box.size.x - (inset + 1.0f), 8.0f, rInner);
        nvgFillPaint(args.vg, bottomShadow);
        nvgFill(args.vg);
        nvgRestore(args.vg);

        // Inner lip stroke to simulate metallic edge
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset + 1.0f, inset + 1.0f, box.size.x - (inset + 1.0f) * 2.0f, box.size.y - (inset + 1.0f) * 2.0f, std::max(0.0f, rInner - 1.0f));
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 30));
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);

        // Side speculars: subtle left and right inner highlights for a metallic feel
        // Left edge inner highlight
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 1.0f, inset - 1.0f, box.size.x - (inset - 1.0f) * 2.0f, box.size.y - (inset - 1.0f) * 2.0f, rInner + 1.0f);
        nvgRoundedRect(args.vg, inset + 1.0f, inset + 1.0f, box.size.x - (inset + 1.0f) * 2.0f, box.size.y - (inset + 1.0f) * 2.0f, std::max(0.0f, rInner - 1.0f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint leftHi = nvgLinearGradient(args.vg, inset - 1.0f, 0, inset + 8.0f, 0, nvgRGBA(255, 255, 255, 22), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, leftHi);
        nvgFill(args.vg);
        // Right edge inner highlight
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 1.0f, inset - 1.0f, box.size.x - (inset - 1.0f) * 2.0f, box.size.y - (inset - 1.0f) * 2.0f, rInner + 1.0f);
        nvgRoundedRect(args.vg, inset + 1.0f, inset + 1.0f, box.size.x - (inset + 1.0f) * 2.0f, box.size.y - (inset + 1.0f) * 2.0f, std::max(0.0f, rInner - 1.0f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint rightHi = nvgLinearGradient(args.vg, box.size.x - (inset - 1.0f), 0, box.size.x - (inset + 8.0f), 0, nvgRGBA(255, 255, 255, 14), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, rightHi);
        nvgFill(args.vg);
    }
    
    // Determine grid based on module setting
    int cols = 8;
    int rows = 8;
    if (module) {
        if (module->gridSteps == 16) { cols = rows = 4; }
        else if (module->gridSteps == 32) { cols = rows = 6; }
        else { cols = rows = 8; }
    }

    // Add a slight inner padding so edge circles aren't crowded by the bezel
    float pad = std::max(2.0f, std::min(box.size.x, box.size.y) * 0.02f);
    float innerW = box.size.x - pad * 2.0f;
    float innerH = box.size.y - pad * 2.0f;
    
    // Calculate cell size in widget coordinates within the padded area
    float cellWidth = innerW / cols;
    float cellHeight = innerH / rows;
    
    // Draw each matrix cell at high resolution
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            // Map grid cell to step index; for 32‑step 6x6, center the last 2 steps
            int stepIndex = -1;
            if (module) {
                if (module->gridSteps == 16) {
                    stepIndex = y * 4 + x;
                } else if (module->gridSteps == 32) {
                    if (y < 5) stepIndex = y * 6 + x;
                    else if (y == 5 && x >= 2 && x <= 3) stepIndex = 30 + (x - 2);
                } else {
                    stepIndex = y * cols + x;
                }
            } else {
                stepIndex = y * cols + x;
            }
            
            // Calculate cell position and center
            Vec cellPos = Vec(pad + x * cellWidth, pad + y * cellHeight);
            Vec cellCenter = Vec(cellPos.x + cellWidth/2, cellPos.y + cellHeight/2);
            
            // Get step data from both sequences (content presence)
            bool hasA = false, hasB = false;
            bool playheadA = false, playheadB = false;
            int aSymbolId = -999;
            int bSymbolId = -999;
            int aVoices = 0;
            int bVoices = 0;
            bool inA = false, inB = false; // within sequence length regardless of content
            
            if (module && stepIndex >= 0) {
                int totalSteps = module->gridSteps;
                if (stepIndex < totalSteps) {
                inA = (stepIndex < module->sequenceA.length);
                inB = (stepIndex < module->sequenceB.length);

                // Sequence A content
                if (inA && module->sequenceA.steps[stepIndex].chordIndex >= -2) {
                    hasA = true;
                    aSymbolId = module->sequenceA.steps[stepIndex].alchemySymbolId;
                    aVoices = module->sequenceA.steps[stepIndex].voiceCount;
                }
                // Sequence B content
                if (inB && module->sequenceB.steps[stepIndex].chordIndex >= -2) {
                    hasB = true;
                    bSymbolId = module->sequenceB.steps[stepIndex].alchemySymbolId;
                    bVoices = module->sequenceB.steps[stepIndex].voiceCount;
                }
                
                // Check for playhead or edit-cursor position
                bool editCursorA = (module->editModeA && inA && module->sequenceA.currentStep == stepIndex);
                bool editCursorB = (module->editModeB && inB && module->sequenceB.currentStep == stepIndex);
                playheadA = ((module->sequenceA.running && module->sequenceA.currentStep == stepIndex) || editCursorA);
                playheadB = ((module->sequenceB.running && module->sequenceB.currentStep == stepIndex) || editCursorB);
                // Do not gate playheads by edit mode; running playheads always light
                }
            }
            
            // Draw high-resolution cell background
            nvgBeginPath(args.vg);
            
            // Use larger, smoother circles; adjust per grid density
            // For equal on-screen gap: tune radius per grid (8:0.42, 6:0.44, 4:0.40)
            float radiusFactor = 0.42f;
            if (module) {
                if (module->gridSteps == 32) radiusFactor = 0.44f;
                else if (module->gridSteps == 16) radiusFactor = 0.40f;
            }
            float cellRadius = std::min(cellWidth, cellHeight) * radiusFactor;
            nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
            
            // LED color logic with smooth gradients
            
            // Check for edit mode highlighting
            bool editModeHighlight = false;
            if (module && ((module->editModeA && hasA) || (module->editModeB && hasB))) {
                editModeHighlight = true;
            }
            
            if (playheadA && playheadB) {
                // Both playheads - create gradient mix
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(120, 160, 255, 255), nvgRGBA(60, 80, 200, 255));
                nvgFillPaint(args.vg, paint);
            } else if (playheadA) {
                // Sequence A - teal gradient
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(0, 255, 180, 255), nvgRGBA(0, 180, 120, 255));
                nvgFillPaint(args.vg, paint);
            } else if (playheadB) {
                // Sequence B - purple gradient  
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(180, 0, 255, 255), nvgRGBA(120, 0, 180, 255));
                nvgFillPaint(args.vg, paint);
            } else if (editModeHighlight) {
                // Edit mode - subtle glow
                if (hasA && module->editModeA) {
                    NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                      nvgRGBA(0, 150, 120, 200), nvgRGBA(0, 80, 60, 200));
                    nvgFillPaint(args.vg, paint);
                } else if (hasB && module->editModeB) {
                    NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                      nvgRGBA(120, 0, 150, 200), nvgRGBA(60, 0, 80, 200));
                    nvgFillPaint(args.vg, paint);
                }
            } else if (inA && inB) {
                // Both sequences - subtle mix
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(60, 80, 120, 255), nvgRGBA(30, 40, 60, 255));
                nvgFillPaint(args.vg, paint);
            } else if (inA) {
                // Sequence A only
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(0, 100, 70, 255), nvgRGBA(0, 50, 35, 255));
                nvgFillPaint(args.vg, paint);
            } else if (inB) {
                // Sequence B only
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(70, 0, 100, 255), nvgRGBA(35, 0, 50, 255));
                nvgFillPaint(args.vg, paint);
            } else {
                // Empty - subtle gradient
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(25, 25, 30, 255), nvgRGBA(15, 15, 20, 255));
                nvgFillPaint(args.vg, paint);
            }
            
            // Add edit mode glow for unused steps, but never override a playhead highlight
            if (module && (module->editModeA || module->editModeB)) {
                // A edit: within A length, no A content at this step, and not the active playhead
                if (module->editModeA && inA && !hasA && !playheadA) {
                    NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                      nvgRGBA(0, 180, 140, 120), nvgRGBA(0, 80, 60, 60));
                    nvgFillPaint(args.vg, paint);
                }
                // B edit: within B length, no B content at this step, and not the active playhead
                if (module->editModeB && inB && !hasB && !playheadB) {
                    NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                      nvgRGBA(140, 0, 180, 120), nvgRGBA(60, 0, 80, 60));
                    nvgFillPaint(args.vg, paint);
                }
            }
            
            nvgFill(args.vg);
            
            // Draw alchemical symbols for both sequences when present
            if (hasA && hasB) {
                // Emphasis based on edit mode (visual handled by placement/scale)
                NVGcolor ink = nvgRGBA(232, 224, 200, 255);
                // Nudge minis toward center to avoid hugging the circle edge
                float off = std::min(cellWidth, cellHeight) * 0.14f;
                float scale = 0.85f; // make dual-occupancy symbols larger
                // A (upper-left)
                nvgSave(args.vg);
                nvgTranslate(args.vg, cellCenter.x - off, cellCenter.y - off);
                nvgScale(args.vg, scale, scale);
                if (st::isValidSymbolId(aSymbolId)) st::drawAlchemicalSymbol(args, Vec(0, 0), aSymbolId, ink, 6.5f, 1.0f);
                else if (aSymbolId == -1) st::drawRestSymbol(args, Vec(0, 0));
                else if (aSymbolId == -2) st::drawTieSymbol(args, Vec(0, 0));
                nvgRestore(args.vg);
                // B (lower-right)
                nvgSave(args.vg);
                nvgTranslate(args.vg, cellCenter.x + off, cellCenter.y + off);
                nvgScale(args.vg, scale, scale);
                if (st::isValidSymbolId(bSymbolId)) st::drawAlchemicalSymbol(args, Vec(0, 0), bSymbolId, ink, 6.5f, 1.0f);
                else if (bSymbolId == -1) st::drawRestSymbol(args, Vec(0, 0));
                else if (bSymbolId == -2) st::drawTieSymbol(args, Vec(0, 0));
                nvgRestore(args.vg);
                // Dual-color ring accent (keep within cell bounds)
                float r = std::max(cellRadius - 1.2f, cellRadius * 0.9f);
                nvgBeginPath(args.vg);
                nvgArc(args.vg, cellCenter.x, cellCenter.y, r, -M_PI/2, M_PI/2, NVG_CW);
                nvgStrokeColor(args.vg, nvgRGBA(0, 255, 180, 180));
                nvgStrokeWidth(args.vg, 1.2f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgArc(args.vg, cellCenter.x, cellCenter.y, r, M_PI/2, M_PI/2 + M_PI, NVG_CW);
                nvgStrokeColor(args.vg, nvgRGBA(180, 0, 255, 180));
                nvgStrokeWidth(args.vg, 1.2f);
                nvgStroke(args.vg);
            } else {
                // Single occupancy fallback (centered)
                int symbolId = hasA ? aSymbolId : (hasB ? bSymbolId : -999);
                if (st::isValidSymbolId(symbolId)) {
                    NVGcolor ink = nvgRGBA(232, 224, 200, 255);
                    // Larger single-occupancy symbol for clarity
                    st::drawAlchemicalSymbol(args, cellCenter, symbolId, ink, 8.2f, 1.0f);
                } else if (symbolId == -1) {
                    st::drawRestSymbol(args, cellCenter);
                } else if (symbolId == -2) {
                    st::drawTieSymbol(args, cellCenter);
                }
            }
            
            // Draw voice count indicators near the edge of the circle
            if (module && (hasA || hasB) && stepIndex < module->gridSteps) {
                if (hasA && hasB) {
                    // Dual occupancy: show voice dots in a 2x3 grid placed to the side of each symbol (no overlap)
                    auto drawDotsGrid = [&](Vec center, int count) {
                        int dots = std::min(count, 6);
                        if (dots <= 0) return;
                        float pitch = std::min(cellWidth, cellHeight) * 0.09f; // spacing between dots
                        for (int i = 0; i < dots; i++) {
                            int row = i / 3;      // 0..1
                            int col = i % 3;      // 0..2
                            float dx = (col - 1) * pitch;       // -pitch, 0, +pitch
                            float dy = (row - 0.5f) * pitch;    // -0.5p, +0.5p
                            nvgBeginPath(args.vg);
                            nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 230));
                            nvgCircle(args.vg, center.x + dx, center.y + dy, 1.3f);
                            nvgFill(args.vg);
                        }
                    };
                    // Compute symbol centers (same offsets used when drawing minis)
                    float off = std::min(cellWidth, cellHeight) * 0.18f;
                    Vec symA = Vec(cellCenter.x - off, cellCenter.y - off); // top symbol
                    Vec symB = Vec(cellCenter.x + off, cellCenter.y + off); // bottom symbol
                    // Place dot grids to the side of each symbol (right of top, left of bottom), near circle edge
                    float pitch = std::min(cellWidth, cellHeight) * 0.09f;
                    float gridHalfW = pitch;       // half width of 3 columns
                    float ringR = std::max(0.0f, cellRadius - 3.0f);
                    float sideDist = std::max(0.0f, ringR - gridHalfW - 1.4f);
                    // Initial desired centers: right of top symbol, left of bottom symbol
                    Vec gridCenterA = Vec(cellCenter.x + sideDist, symA.y);
                    Vec gridCenterB = Vec(cellCenter.x - sideDist, symB.y);
                    // Clamp horizontally so dots stay inside the circle at their row extremes
                    auto clampGridXInsideCircle = [&](Vec center) -> float {
                        float margin = 4.0f; // keep dots well inside border
                        float yTop = center.y - 0.5f * pitch;
                        float yBot = center.y + 0.5f * pitch;
                        float dyAbs = std::max(fabsf(yTop - cellCenter.y), fabsf(yBot - cellCenter.y));
                        float xMax = sqrtf(std::max(0.0f, cellRadius * cellRadius - dyAbs * dyAbs)) - margin;
                        return xMax;
                    };
                    float xMaxA = clampGridXInsideCircle(gridCenterA);
                    float xMaxB = clampGridXInsideCircle(gridCenterB);
                    // Right side clamp for A (ensure rightmost column inside)
                    gridCenterA.x = std::min(gridCenterA.x, cellCenter.x + (xMaxA - gridHalfW - 1.0f));
                    // Left side clamp for B (ensure leftmost column inside)
                    gridCenterB.x = std::max(gridCenterB.x, cellCenter.x - (xMaxB - gridHalfW - 1.0f));
                    drawDotsGrid(gridCenterA, aVoices);
                    drawDotsGrid(gridCenterB, bVoices);
                } else {
                    // Single occupancy: use a full ring near the circle edge
                    int voiceCount = hasA ? aVoices : bVoices;
                    NVGcolor dotColor = (playheadA || playheadB) ? nvgRGBA(0, 0, 0, 255) : nvgRGBA(255, 255, 255, 255);
                    float ringR = std::max(0.0f, cellRadius - 1.4f);
                    int dots = std::min(voiceCount, 6);
                    for (int i = 0; i < dots; i++) {
                        float angle = (float)i / 6.0f * 2.0f * M_PI - M_PI/2;
                        float dotX = cellCenter.x + cosf(angle) * ringR;
                        float dotY = cellCenter.y + sinf(angle) * ringR;
                        nvgBeginPath(args.vg);
                        nvgFillColor(args.vg, dotColor);
                        nvgCircle(args.vg, dotX, dotY, 1.2f);
                        nvgFill(args.vg);
                    }
                }
            }
            
            // Draw playhead ring overlay to guarantee visibility even if fills change
            if (playheadA || playheadB) {
                float ringR = cellRadius + 1.0f;
                if (playheadA && playheadB) {
                    // Split ring: top half teal, bottom half purple
                    nvgBeginPath(args.vg);
                    nvgArc(args.vg, cellCenter.x, cellCenter.y, ringR, -M_PI, 0, NVG_CW);
                    nvgStrokeColor(args.vg, nvgRGBA(0, 255, 180, 220));
                    nvgStrokeWidth(args.vg, 2.0f);
                    nvgStroke(args.vg);
                    nvgBeginPath(args.vg);
                    nvgArc(args.vg, cellCenter.x, cellCenter.y, ringR, 0, M_PI, NVG_CW);
                    nvgStrokeColor(args.vg, nvgRGBA(180, 0, 255, 220));
                    nvgStrokeWidth(args.vg, 2.0f);
                    nvgStroke(args.vg);
                } else if (playheadA) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, cellCenter.x, cellCenter.y, ringR);
                    nvgStrokeColor(args.vg, nvgRGBA(0, 255, 180, 220));
                    nvgStrokeWidth(args.vg, 2.0f);
                    nvgStroke(args.vg);
                } else if (playheadB) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, cellCenter.x, cellCenter.y, ringR);
                    nvgStrokeColor(args.vg, nvgRGBA(180, 0, 255, 220));
                    nvgStrokeWidth(args.vg, 2.0f);
                    nvgStroke(args.vg);
                }
            }

            // Draw subtle cell border for definition beneath overlay
            nvgStrokeColor(args.vg, nvgRGBA(60, 60, 70, 100));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        }
    }
    
    // Draw edit mode matrix border glow at high resolution
    if (module && (module->editModeA || module->editModeB)) {
        nvgSave(args.vg);
        
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        
        float time = system::getTime();
        float pulse = 0.4f + 0.3f * sin(time * 3.0f);
        
        NVGcolor glowColor;
        if (module->editModeA) {
            glowColor = nvgRGBA(0, 255, 180, pulse * 150);
        } else {
            glowColor = nvgRGBA(180, 0, 255, pulse * 150);
        }
        
        // Tighter, inner-edge glow (hug the screen bounds)
        nvgBeginPath(args.vg);
        // Inset by 0.5px so the glow sits right at the inner edge
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, 7.5f);
        nvgStrokeColor(args.vg, glowColor);
        nvgStrokeWidth(args.vg, 1.25f);
        nvgStroke(args.vg);
        
        nvgRestore(args.vg);
    }

    // Vintage screen overlay: vignette + patina + faint micro-scratches (subtle, on top of bezel)
    {
        nvgSave(args.vg);
        st::drawVignettePatinaScratches(
            args,
            0, 0, box.size.x, box.size.y, 8.0f,
            /*vignetteAlpha*/ 26,
            /*patinaStart*/ nvgRGBA(24, 30, 20, 10),
            /*patinaEnd*/   nvgRGBA(50, 40, 22, 12),
            /*scratchesAlpha*/ 8,
            /*scratchStroke*/ 0.5f,
            /*scratchCount*/ 3,
            /*seed*/ 73321u
        );
        nvgRestore(args.vg);
    }

    nvgRestore(args.vg);
}
#endif // legacy drawMatrix

#if 0 // Legacy helper superseded by src/transmutation/ui.cpp
void HighResMatrixWidget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color) {
    // Delegate to shared utility to avoid duplicate switch logic
    st::drawAlchemicalSymbol(args, pos, symbolId, color, 6.5f, 1.0f);
}
#endif

/* removed legacy switch */
/*
    switch (symbolId) {
        case 0: // Sol (Sun) - Circle with center dot (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.3f);
            nvgFill(args.vg);
            break;
            
        case 1: // Luna (Moon) - Crescent (match original)
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 2: // Mercury - Circle with horns and cross (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
            nvgStroke(args.vg);
            // Horns
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 3: // Venus - Circle with cross below (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 4: // Mars - Circle with arrow up-right (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.2f, -size * 0.2f);
            nvgLineTo(args.vg, size * 0.7f, -size * 0.7f);
            nvgLineTo(args.vg, size * 0.4f, -size * 0.7f);
            nvgMoveTo(args.vg, size * 0.7f, -size * 0.7f);
            nvgLineTo(args.vg, size * 0.7f, -size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        case 5: // Jupiter
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, 0);
            nvgLineTo(args.vg, size * 0.2f, 0);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 0.5f, M_PI * 1.5f, NVG_CCW);
            nvgStroke(args.vg);
            break;
            
        case 6: // Saturn
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.2f, 0);
            nvgLineTo(args.vg, size * 0.6f, 0);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, -size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 1.5f, M_PI * 0.5f, NVG_CCW);
            nvgStroke(args.vg);
            break;
            
        case 7: // Fire
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 8: // Water
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 9: // Air - Triangle up with line through (match original)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 10: // Earth - Triangle down with line through (match original)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 11: // Quintessence - Interwoven circles (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        // Additional Occult Symbols
        case 12: // Pentagram - Five-pointed star
            nvgBeginPath(args.vg);
            // Draw pentagram by connecting every second point
            for (int i = 0; i < 5; i++) {
                int pointIndex = (i * 2) % 5;
                float angle = pointIndex * 2.0f * M_PI / 5.0f - M_PI / 2.0f;
                float x = cosf(angle) * size;
                float y = sinf(angle) * size;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 13: // Hexagram - Six-pointed star (Star of David)
            nvgBeginPath(args.vg);
            // First triangle (up)
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.866f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.866f, size * 0.5f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            // Second triangle (down)  
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.866f, -size * 0.5f);
            nvgLineTo(args.vg, size * 0.866f, -size * 0.5f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 14: // Ankh - Egyptian symbol of life
            nvgBeginPath(args.vg);
            // Cross vertical line
            nvgMoveTo(args.vg, 0, -size * 0.2f);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            // Cross horizontal line
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.5f, size * 0.2f);
            nvgLineTo(args.vg, size * 0.5f, size * 0.2f);
            nvgStroke(args.vg);
            // Loop at top
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, -size * 0.4f, size * 0.3f, 0, M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 15: // Eye of Horus - Egyptian protection symbol
            nvgBeginPath(args.vg);
            // Eye outline
            nvgMoveTo(args.vg, -size * 0.8f, 0);
            nvgBezierTo(args.vg, -size * 0.8f, -size * 0.5f, size * 0.8f, -size * 0.5f, size * 0.8f, 0);
            nvgBezierTo(args.vg, size * 0.8f, size * 0.5f, -size * 0.8f, size * 0.5f, -size * 0.8f, 0);
            nvgStroke(args.vg);
            // Pupil
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.2f);
            nvgFill(args.vg);
            // Tear line
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.2f);
            nvgLineTo(args.vg, -size * 0.3f, size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 16: // Ouroboros - Snake eating its tail
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.8f);
            nvgStroke(args.vg);
            // Snake head
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.8f, 0, size * 0.15f);
            nvgStroke(args.vg);
            // Tail into mouth
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.65f, 0);
            nvgLineTo(args.vg, size * 0.5f, 0);
            nvgStroke(args.vg);
            break;
            
        case 17: // Triskele - Triple spiral
            nvgBeginPath(args.vg);
            for (int i = 0; i < 3; i++) {
                float angle = i * 2.0f * M_PI / 3.0f;
                nvgMoveTo(args.vg, 0, 0);
                for (int j = 1; j <= 8; j++) {
                    float r = (j / 8.0f) * size;
                    float a = angle + (j / 8.0f) * M_PI;
                    nvgLineTo(args.vg, cosf(a) * r, sinf(a) * r);
                }
            }
            nvgStroke(args.vg);
            break;
            
        case 18: // Caduceus - Staff of Hermes
            nvgBeginPath(args.vg);
            // Central staff
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            // Left spiral
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgBezierTo(args.vg, -size * 0.4f, -size * 0.2f, -size * 0.4f, size * 0.2f, 0, size * 0.6f);
            nvgStroke(args.vg);
            // Right spiral
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgBezierTo(args.vg, size * 0.4f, -size * 0.2f, size * 0.4f, size * 0.2f, 0, size * 0.6f);
            nvgStroke(args.vg);
            // Wings
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, -size * 0.8f);
            nvgLineTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.3f, -size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 19: // Yin Yang - Balance symbol
            nvgBeginPath(args.vg);
            // Outer circle
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            // Dividing line
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, -size * 0.5f, size * 0.5f, 0, M_PI, NVG_CW);
            nvgArc(args.vg, 0, size * 0.5f, size * 0.5f, M_PI, 2 * M_PI, NVG_CCW);
            nvgStroke(args.vg);
            // Small circles
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.5f, size * 0.15f);
            nvgFill(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, size * 0.5f, size * 0.15f);
            nvgStroke(args.vg);
            break;
            
        // Even More Occult/Alchemical Symbols
        case 20: // Seal of Solomon - Hexagram with circle
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            // Star of David inside
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.7f);
            nvgLineTo(args.vg, -size * 0.6f, size * 0.35f);
            nvgLineTo(args.vg, size * 0.6f, size * 0.35f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.7f);
            nvgLineTo(args.vg, -size * 0.6f, -size * 0.35f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.35f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 21: // Sulfur - Triangle over cross
            nvgBeginPath(args.vg);
            // Triangle
            nvgMoveTo(args.vg, 0, -size * 0.5f);
            nvgLineTo(args.vg, -size * 0.6f, size * 0.1f);
            nvgLineTo(args.vg, size * 0.6f, size * 0.1f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.1f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.45f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.45f);
            nvgStroke(args.vg);
            break;
            
        case 22: // Salt - Circle with horizontal line
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.8f, 0);
            nvgLineTo(args.vg, size * 0.8f, 0);
            nvgStroke(args.vg);
            break;
            
        case 23: // Antimony - Circle with cross below
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.1f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.45f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.45f);
            nvgStroke(args.vg);
            break;
            
        case 24: // Philosopher's Stone - Square with inscribed circle and dot
            nvgBeginPath(args.vg);
            nvgRect(args.vg, -size * 0.7f, -size * 0.7f, size * 1.4f, size * 1.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.5f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.1f);
            nvgFill(args.vg);
            break;
            
        case 25: // Arsenic - Stylized As
            nvgBeginPath(args.vg);
            // Zigzag pattern
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgStroke(args.vg);
            // Horizontal line through middle
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 26: // Copper - Venus symbol variant
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.2f, size * 0.4f);
            nvgStroke(args.vg);
            // Cross with curved arms
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, -size * 0.2f, size * 0.5f, size * 0.2f, 0, M_PI, NVG_CW);
            nvgArc(args.vg, size * 0.2f, size * 0.5f, size * 0.2f, M_PI, 2 * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 27: // Iron - Mars symbol variant with double arrow
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.3f);
            nvgStroke(args.vg);
            // Double arrow
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.1f, -size * 0.1f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.8f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgStroke(args.vg);
            break;
            
        case 28: // Lead - Saturn symbol with additional cross
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.3f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.3f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.8f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, size * 0.2f, size * 0.3f, 0, M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 29: // Silver - Crescent moon
            nvgBeginPath(args.vg);
            nvgArc(args.vg, size * 0.2f, 0, size * 0.8f, M_PI * 0.6f, M_PI * 1.4f, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 30: // Zinc - Jupiter symbol with line through
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, 0);
            nvgLineTo(args.vg, size * 0.6f, 0);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, -size * 0.4f);
            nvgLineTo(args.vg, size * 0.4f, size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        case 31: // Tin - Jupiter variant with double cross
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.2f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.2f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, size * 0.2f);
            nvgLineTo(args.vg, size * 0.6f, size * 0.2f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.8f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 32: // Bismuth - Stylized Bi with flourishes
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.3f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, 0);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.4f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 33: // Magnesium - Alchemical Mg symbol
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, -size * 0.6f, size * 0.8f);
            nvgLineTo(args.vg, size * 0.6f, size * 0.8f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, 0);
            nvgLineTo(args.vg, size * 0.6f, 0);
            nvgStroke(args.vg);
            break;
            
        case 34: // Platinum - Crossed circle with dots
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.8f, -size * 0.8f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.8f, -size * 0.8f);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.2f);
            nvgFill(args.vg);
            break;
            
        case 35: // Aether - Upward pointing triangle with horizontal line above
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.8f);
            nvgLineTo(args.vg, -size * 0.7f, size * 0.4f);
            nvgLineTo(args.vg, size * 0.7f, size * 0.4f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.8f, -size * 0.9f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.9f);
            nvgStroke(args.vg);
            break;
            
        case 36: // Void - Empty circle with cross through
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.7f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size, 0);
            nvgLineTo(args.vg, size, 0);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            break;
            
        case 37: // Chaos Star - Eight-pointed star
            nvgBeginPath(args.vg);
            for (int i = 0; i < 8; i++) {
                float angle = i * M_PI / 4.0f;
                nvgMoveTo(args.vg, 0, 0);
                nvgLineTo(args.vg, cosf(angle) * size, sinf(angle) * size);
            }
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.2f);
            nvgFill(args.vg);
            break;
            
        case 38: // Tree of Life - Stylized Kabbalah tree
            nvgBeginPath(args.vg);
            // Central pillar
            nvgMoveTo(args.vg, 0, -size * 0.9f);
            nvgLineTo(args.vg, 0, size * 0.9f);
            nvgStroke(args.vg);
            // Sephirot (circles)
            for (int i = 0; i < 3; i++) {
                float y = -size * 0.6f + i * size * 0.6f;
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, y, size * 0.15f);
                nvgStroke(args.vg);
            }
            // Side connections
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.3f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.3f);
            nvgStroke(args.vg);
            break;
            
        case 39: // Leviathan Cross - Cross of Satan with infinity
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.8f);
            nvgLineTo(args.vg, 0, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.2f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.2f);
            nvgStroke(args.vg);
            // Double cross
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, -size * 0.5f);
            nvgLineTo(args.vg, size * 0.4f, -size * 0.5f);
            nvgStroke(args.vg);
            // Infinity at bottom
            nvgBeginPath(args.vg);
            for (float t = 0; t < 2 * M_PI; t += 0.1f) {
                float x = size * 0.4f * sinf(t) / (1 + cosf(t) * cosf(t));
                float y = size * 0.6f + size * 0.2f * sinf(t) * cosf(t) / (1 + cosf(t) * cosf(t));
                if (t == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            nvgStroke(args.vg);
            break;
        }
        
        nvgRestore(args.vg);
    }
*/

#if 0 // Legacy implementation superseded by src/transmutation/ui.cpp
void HighResMatrixWidget::drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor) {
    nvgSave(args.vg);
    // Compute ring radius based on current grid size
    int cols = 8;
    int rows = 8;
    if (module) {
        if (module->gridSteps == 16) { cols = rows = 4; }
        else if (module->gridSteps == 32) { cols = rows = 6; }
    }
    float cellWidth = box.size.x / cols;
    float cellHeight = box.size.y / rows;
    float radiusFactor = 0.42f;
    if (module) {
        if (module->gridSteps == 32) radiusFactor = 0.44f;
        else if (module->gridSteps == 16) radiusFactor = 0.40f;
    }
    float circleR = std::min(cellWidth, cellHeight) * radiusFactor;
    float ringR = std::max(0.0f, circleR - 1.4f);
    st::drawVoiceCountDots(args, pos, voiceCount, ringR, 1.2f, dotColor);
    nvgRestore(args.vg);
}
#endif // legacy drawVoiceCount

 

 

 

 

 

 

// (Legacy Matrix8x8Widget removed)


// AlchemicalSymbolWidget moved to src/transmutation/ui.{hpp,cpp}


// TransmutationDisplayWidget moved to src/transmutation/ui.{hpp,cpp}

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
        
        // Edit mode buttons (above matrix) - from SVG circles edit_a_btn/edit_b_btn (cx, cy)
        {
            std::string tA = findTagForId("edit_a_btn");
            std::string tB = findTagForId("edit_b_btn");
            float ax = getAttr(tA, "cx", 55.973103f);
            float ay = getAttr(tA, "cy", 16.805513f);
            float bx = getAttr(tB, "cx", 74.402115f);
            float by = getAttr(tB, "cy", 16.678213f);
            addParam(createParamCentered<ShapetakerVintageMomentary>(mm2px(Vec(ax, ay)), module, Transmutation::EDIT_A_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(mm2px(Vec(bx, by)), module, Transmutation::EDIT_B_PARAM));
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
            addParam(createParamCentered<ShapetakerVintageMomentary>(pos("a_play_btn", 22.586929f, 67.512939f), module, Transmutation::START_A_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(pos("a_stop_btn", 22.784245f, 75.573959f), module, Transmutation::STOP_A_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(pos("a_reset_btn", 22.784245f, 83.509323f), module, Transmutation::RESET_A_PARAM));
            // Sequence B
            addParam(createParamCentered<ShapetakerKnobMedium>(pos("seq_b_length", 115.02555f, 37.849998f), module, Transmutation::LENGTH_B_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(pos("b_play_btn", 108.43727f, 67.450111f), module, Transmutation::START_B_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(pos("b_stop_btn", 108.43727f, 75.511131f), module, Transmutation::STOP_B_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(pos("b_reset_btn", 108.43728f, 83.446495f), module, Transmutation::RESET_B_PARAM));
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
            addParam(createParamCentered<ShapetakerVintageMomentary>(mm2px(Vec(rx, ry)), module, Transmutation::REST_PARAM));
            addParam(createParamCentered<ShapetakerVintageMomentary>(mm2px(Vec(tx, ty)), module, Transmutation::TIE_PARAM));
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
