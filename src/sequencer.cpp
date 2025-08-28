#include "plugin.hpp"
#include <vector>
#include <array>
#include <string>
#include <random>
#include <fstream>

// Forward declarations
struct Transmutation;

struct ChordData {
    std::string name;
    std::vector<float> intervals;
    int preferredVoices;
    std::string category;
};

struct ChordPack {
    std::string name;
    std::string key;
    std::vector<ChordData> chords;
    std::string description;
};

struct SequenceStep {
    int chordIndex;
    int voiceCount;
    int alchemySymbolId;
    
    SequenceStep() : chordIndex(-1), voiceCount(1), alchemySymbolId(-1) {}
};

struct Sequence {
    std::array<SequenceStep, 64> steps;
    int length;
    int currentStep;
    bool running;
    float clockPhase;
    
    Sequence() : length(16), currentStep(0), running(false), clockPhase(0.0f) {}
};

// Matrix8x8Widget - declaration only, implementation after Transmutation class
struct Matrix8x8Widget : Widget {
    Transmutation* module;
    static constexpr int MATRIX_SIZE = 8;
    static constexpr float LED_SIZE = 10.0f;
    static constexpr float LED_SPACING = 14.0f;
    
    Matrix8x8Widget(Transmutation* module);
    void onButton(const event::Button& e) override;
    void onMatrixClick(int x, int y);
    void onMatrixRightClick(int x, int y);
    void programStep(Sequence& seq, int stepIndex);
    void drawLayer(const DrawArgs& args, int layer) override;
    void drawMatrix(const DrawArgs& args);
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId);
    void drawRestSymbol(const DrawArgs& args, Vec pos);
    void drawTieSymbol(const DrawArgs& args, Vec pos);
    void drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount);
};

struct Transmutation : Module {
    enum ParamId {
        // Edit mode controls
        EDIT_A_PARAM,
        EDIT_B_PARAM,
        
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
        
        // Sequence B mode
        SEQ_B_MODE_PARAM,
        
        // Chord pack selection
        CHORD_PACK_PARAM,
        
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
        
        PARAMS_LEN
    };
    
    enum InputId {
        CLOCK_A_INPUT,
        CLOCK_B_INPUT,
        RESET_A_INPUT,
        RESET_B_INPUT,
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
        EDIT_A_LIGHT,
        EDIT_B_LIGHT,
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
    
    // Chord pack system
    ChordPack currentChordPack;
    std::array<int, 12> symbolToChordMapping;
    
    // Clock system
    float internalClock = 0.0f;
    float clockRate = 120.0f; // BPM
    
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
    
    Transmutation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Edit mode buttons
        configParam(EDIT_A_PARAM, 0.f, 1.f, 0.f, "Edit Transmutation A");
        configParam(EDIT_B_PARAM, 0.f, 1.f, 0.f, "Edit Transmutation B");
        
        // Transmutation controls
        configParam(LENGTH_A_PARAM, 1.f, 64.f, 16.f, "Transmutation A Length");
        configParam(LENGTH_B_PARAM, 1.f, 64.f, 16.f, "Transmutation B Length");
        configParam(START_A_PARAM, 0.f, 1.f, 0.f, "Start Transmutation A");
        configParam(STOP_A_PARAM, 0.f, 1.f, 0.f, "Stop Transmutation A");
        configParam(RESET_A_PARAM, 0.f, 1.f, 0.f, "Reset Transmutation A");
        configParam(START_B_PARAM, 0.f, 1.f, 0.f, "Start Transmutation B");
        configParam(STOP_B_PARAM, 0.f, 1.f, 0.f, "Stop Transmutation B");
        configParam(RESET_B_PARAM, 0.f, 1.f, 0.f, "Reset Transmutation B");
        
        // Clock control
        configParam(INTERNAL_CLOCK_PARAM, 60.f, 200.f, 120.f, "Internal Clock", " BPM");
        
        // Transmutation B mode (0=Independent, 1=Harmony, 2=Lock)
        configSwitch(SEQ_B_MODE_PARAM, 0.f, 2.f, 0.f, "Transmutation B Mode", {"Independent", "Harmony", "Lock"});
        
        // Chord pack selection
        configParam(CHORD_PACK_PARAM, 0.f, 1.f, 0.f, "Load Chord Pack");
        
        // Alchemical symbol buttons
        for (int i = 0; i < 12; i++) {
            configParam(SYMBOL_1_PARAM + i, 0.f, 1.f, 0.f, "Alchemical Symbol " + std::to_string(i + 1));
        }
        
        // Rest and tie
        configParam(REST_PARAM, 0.f, 1.f, 0.f, "Rest");
        configParam(TIE_PARAM, 0.f, 1.f, 0.f, "Tie");
        
        // Inputs
        configInput(CLOCK_A_INPUT, "Clock A");
        configInput(CLOCK_B_INPUT, "Clock B");
        configInput(RESET_A_INPUT, "Reset A");
        configInput(RESET_B_INPUT, "Reset B");
        
        // Outputs
        configOutput(CV_A_OUTPUT, "CV A (Polyphonic)");
        configOutput(GATE_A_OUTPUT, "Gate A (Polyphonic)");
        configOutput(CV_B_OUTPUT, "CV B (Polyphonic)");
        configOutput(GATE_B_OUTPUT, "Gate B (Polyphonic)");
        
        // Initialize symbol mapping to -1 (no chord assigned)
        symbolToChordMapping.fill(-1);
        
        // Load default chord pack
        loadDefaultChordPack();
    }
    
    void process(const ProcessArgs& args) override {
        // Handle edit mode toggles
        if (editATrigger.process(params[EDIT_A_PARAM].getValue())) {
            editModeA = !editModeA;
            if (editModeA) editModeB = false; // Only one edit mode at a time
        }
        
        if (editBTrigger.process(params[EDIT_B_PARAM].getValue())) {
            editModeB = !editModeB;
            if (editModeB) editModeA = false;
        }
        
        // Update sequence lengths from parameters
        sequenceA.length = (int)params[LENGTH_A_PARAM].getValue();
        sequenceB.length = (int)params[LENGTH_B_PARAM].getValue();
        
        // Handle sequence controls
        if (startATrigger.process(params[START_A_PARAM].getValue())) {
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
        
        // Handle chord pack loading
        if (params[CHORD_PACK_PARAM].getValue() > 0.5f) {
            params[CHORD_PACK_PARAM].setValue(0.0f); // Reset button
            // File browser will be handled by widget
        }
        
        // Handle symbol button presses
        for (int i = 0; i < 12; i++) {
            if (symbolTriggers[i].process(params[SYMBOL_1_PARAM + i].getValue())) {
                onSymbolPressed(i);
            }
        }
        
        // Handle rest/tie buttons
        if (restTrigger.process(params[REST_PARAM].getValue())) {
            selectedSymbol = -1; // Rest symbol
        }
        if (tieTrigger.process(params[TIE_PARAM].getValue())) {
            selectedSymbol = -2; // Tie symbol
        }
        
        // Update internal clock
        clockRate = params[INTERNAL_CLOCK_PARAM].getValue();
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
        
        // Update lights
        lights[EDIT_A_LIGHT].setBrightness(editModeA ? 1.0f : 0.0f);
        lights[EDIT_B_LIGHT].setBrightness(editModeB ? 1.0f : 0.0f);
        lights[RUNNING_A_LIGHT].setBrightness(sequenceA.running ? 1.0f : 0.0f);
        lights[RUNNING_B_LIGHT].setBrightness(sequenceB.running ? 1.0f : 0.0f);
        
        // Update symbol lights with color coding for sequences
        for (int i = 0; i < 12; i++) {
            bool symbolActiveA = false;
            bool symbolActiveB = false;
            
            if (sequenceA.running && getCurrentChordIndex(sequenceA) == i) {
                symbolActiveA = true;
            }
            if (sequenceB.running && getCurrentChordIndex(sequenceB) == i) {
                symbolActiveB = true;
            }
            
            // RGB Light indices: Red=0, Green=1, Blue=2
            int lightIndex = SYMBOL_1_LIGHT + i * 3;
            
            if (symbolActiveA && symbolActiveB) {
                // Both sequences - mix teal and purple = cyan-magenta
                lights[lightIndex + 0].setBrightness(0.5f); // Red
                lights[lightIndex + 1].setBrightness(1.0f); // Green (for teal component)
                lights[lightIndex + 2].setBrightness(1.0f); // Blue (for both teal and purple)
            } else if (symbolActiveA) {
                // Sequence A - Teal color (#00ffb4)
                lights[lightIndex + 0].setBrightness(0.0f); // Red
                lights[lightIndex + 1].setBrightness(1.0f); // Green
                lights[lightIndex + 2].setBrightness(0.7f); // Blue
            } else if (symbolActiveB) {
                // Sequence B - Purple color (#b400ff)
                lights[lightIndex + 0].setBrightness(0.7f); // Red
                lights[lightIndex + 1].setBrightness(0.0f); // Green
                lights[lightIndex + 2].setBrightness(1.0f); // Blue
            } else {
                // No sequence active - off
                lights[lightIndex + 0].setBrightness(0.0f);
                lights[lightIndex + 1].setBrightness(0.0f);
                lights[lightIndex + 2].setBrightness(0.0f);
            }
        }
    }
    
    void processSequence(Sequence& seq, int clockInputId, int cvOutputId, int gateOutputId, const ProcessArgs& args, bool internalClockTrigger) {
        if (!seq.running) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
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
        
        // Advance sequence on clock
        if (clockTrigger) {
            seq.currentStep = (seq.currentStep + 1) % seq.length;
        }
        
        // Get current step data
        const SequenceStep& currentStep = seq.steps[seq.currentStep];
        
        // Output CV and gates based on current step
        if (currentStep.chordIndex >= 0 && currentStep.chordIndex < 12) {
            outputChord(currentStep, cvOutputId, gateOutputId);
        } else {
            // Rest or tie
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
        }
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
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
            return;
        }
        
        // In harmony mode, sequence B follows A's timing and chord but plays harmony notes
        if (!sequenceA.running) {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
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
        
        // Advance sequence B step
        if (clockTrigger) {
            sequenceB.currentStep = (sequenceB.currentStep + 1) % sequenceB.length;
        }
        
        // Get current chord from sequence A
        const SequenceStep& stepA = sequenceA.steps[sequenceA.currentStep];
        const SequenceStep& stepB = sequenceB.steps[sequenceB.currentStep];
        
        if (stepA.chordIndex >= 0 && stepA.chordIndex < 12) {
            // Generate harmony based on sequence A's chord
            outputHarmony(stepA, stepB, CV_B_OUTPUT, GATE_B_OUTPUT);
        } else {
            // If A is resting, B rests too
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
        }
    }
    
    void processSequenceBLock(const ProcessArgs& args, bool internalClockTrigger) {
        if (!sequenceB.running) {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
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
        
        // Advance sequence B
        if (clockTrigger) {
            sequenceB.currentStep = (sequenceB.currentStep + 1) % sequenceB.length;
        }
        
        // Output sequence B's programmed progression using same chord pack as A
        const SequenceStep& currentStep = sequenceB.steps[sequenceB.currentStep];
        if (currentStep.chordIndex >= 0 && currentStep.chordIndex < 12) {
            outputChord(currentStep, CV_B_OUTPUT, GATE_B_OUTPUT);
        } else {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
        }
    }
    
    void outputHarmony(const SequenceStep& stepA, const SequenceStep& stepB, int cvOutputId, int gateOutputId) {
        if (stepA.chordIndex < 0 || stepA.chordIndex >= (int)currentChordPack.chords.size()) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
            return;
        }
        
        const ChordData& chordA = currentChordPack.chords[symbolToChordMapping[stepA.chordIndex]];
        int voiceCount = std::min(stepB.voiceCount, 6);
        
        // Set up polyphonic outputs
        outputs[cvOutputId].setChannels(voiceCount);
        outputs[gateOutputId].setChannels(voiceCount);
        
        // Generate harmony notes based on the root chord
        // This creates harmony by using upper chord tones and inversions
        float rootNote = 0.0f; // C4 = 0V as standard
        
        for (int voice = 0; voice < voiceCount; voice++) {
            float harmonyInterval = 0.0f;
            
            if (voice < (int)chordA.intervals.size()) {
                // Use chord tones but transpose up an octave for harmony
                harmonyInterval = chordA.intervals[voice] + 12.0f; // +1 octave
                
                // Add some variation based on voice number for richer harmony
                if (voice % 2 == 1) {
                    harmonyInterval += 7.0f; // Add fifth for more harmonic interest
                }
            } else {
                // If more voices requested, cycle through intervals with additional octaves
                int intervalIndex = voice % chordA.intervals.size();
                int octaveOffset = (voice / chordA.intervals.size()) + 1; // Start at +1 octave for harmony
                harmonyInterval = chordA.intervals[intervalIndex] + octaveOffset * 12.0f;
                
                // Add fifth variation for odd voices
                if (voice % 2 == 1) {
                    harmonyInterval += 7.0f;
                }
            }
            
            float noteCV = rootNote + harmonyInterval / 12.0f; // Convert semitones to V/oct
            outputs[cvOutputId].setVoltage(noteCV, voice);
            outputs[gateOutputId].setVoltage(10.0f, voice); // Standard gate voltage
        }
    }
    
    void outputChord(const SequenceStep& step, int cvOutputId, int gateOutputId) {
        if (step.chordIndex < 0 || step.chordIndex >= (int)currentChordPack.chords.size()) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
            return;
        }
        
        const ChordData& chord = currentChordPack.chords[symbolToChordMapping[step.chordIndex]];
        int voiceCount = std::min(step.voiceCount, 6);
        
        // Set up polyphonic outputs
        outputs[cvOutputId].setChannels(voiceCount);
        outputs[gateOutputId].setChannels(voiceCount);
        
        // Calculate root note (C4 = 0V as standard in VCV Rack)
        float rootNote = 0.0f; // C4 = 0V, will be configurable later
        
        // Output chord tones with proper voice allocation
        for (int voice = 0; voice < voiceCount; voice++) {
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
        }
    }
    
    int getCurrentChordIndex(const Sequence& seq) {
        return seq.steps[seq.currentStep].chordIndex;
    }
    
    void onSymbolPressed(int symbolIndex) {
        selectedSymbol = symbolIndex;
        
        // Audition the chord if we're in edit mode
        if ((editModeA || editModeB) && symbolIndex >= 0 && symbolIndex < 12) {
            auditionChord(symbolIndex);
        }
    }
    
    void auditionChord(int symbolIndex) {
        if (symbolIndex < 0 || symbolIndex >= 12 || 
            symbolToChordMapping[symbolIndex] >= (int)currentChordPack.chords.size()) {
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
        int voiceCount = std::min(chord.preferredVoices, 6);
        
        // Set up polyphonic outputs
        outputs[cvOutputId].setChannels(voiceCount);
        outputs[gateOutputId].setChannels(voiceCount);
        
        // Calculate root note (C4 = 0V as standard in VCV Rack)
        float rootNote = 0.0f; // C4 = 0V
        
        // Output chord tones with proper voice allocation (same as outputChord)
        for (int voice = 0; voice < voiceCount; voice++) {
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
        }
        
        // Set a timer to turn off the audition after a short time
        // (This would need a proper gate generator for timing, keeping simple for now)
    }
    
    bool loadChordPackFromFile(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) return false;
            
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            
            json_error_t error;
            json_t* rootJ = json_loads(content.c_str(), 0, &error);
            if (!rootJ) return false;
            
            // Parse chord pack
            json_t* nameJ = json_object_get(rootJ, "name");
            json_t* keyJ = json_object_get(rootJ, "key");
            json_t* descJ = json_object_get(rootJ, "description");
            json_t* chordsJ = json_object_get(rootJ, "chords");
            
            if (!nameJ || !keyJ || !chordsJ) {
                json_decref(rootJ);
                return false;
            }
            
            currentChordPack.name = json_string_value(nameJ);
            currentChordPack.key = json_string_value(keyJ);
            currentChordPack.description = descJ ? json_string_value(descJ) : "";
            currentChordPack.chords.clear();
            
            // Parse chords array
            size_t chordIndex;
            json_t* chordJ;
            json_array_foreach(chordsJ, chordIndex, chordJ) {
                json_t* chordNameJ = json_object_get(chordJ, "name");
                json_t* intervalsJ = json_object_get(chordJ, "intervals");
                json_t* voicesJ = json_object_get(chordJ, "preferredVoices");
                json_t* categoryJ = json_object_get(chordJ, "category");
                
                if (!chordNameJ || !intervalsJ) continue;
                
                ChordData chord;
                chord.name = json_string_value(chordNameJ);
                chord.preferredVoices = voicesJ ? (int)json_integer_value(voicesJ) : 3;
                chord.category = categoryJ ? json_string_value(categoryJ) : "unknown";
                
                // Parse intervals array
                size_t intervalIndex;
                json_t* intervalJ;
                json_array_foreach(intervalsJ, intervalIndex, intervalJ) {
                    chord.intervals.push_back((float)json_real_value(intervalJ));
                }
                
                currentChordPack.chords.push_back(chord);
            }
            
            json_decref(rootJ);
            randomizeSymbolAssignment();
            return true;
            
        } catch (...) {
            return false;
        }
    }
    
    void randomizeSymbolAssignment() {
        if (currentChordPack.chords.empty()) return;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, currentChordPack.chords.size() - 1);
        
        for (int i = 0; i < 12; i++) {
            symbolToChordMapping[i] = dis(gen);
        }
    }
    
    void loadDefaultChordPack() {
        currentChordPack.name = "Basic Major";
        currentChordPack.key = "C";
        currentChordPack.description = "Basic major chord progressions";
        
        // Add some basic chords
        ChordData cmaj = {"Cmaj", {0, 4, 7}, 3, "major"};
        ChordData dmin = {"Dmin", {2, 5, 9}, 3, "minor"};
        ChordData emin = {"Emin", {4, 7, 11}, 3, "minor"};
        ChordData fmaj = {"Fmaj", {5, 9, 0}, 3, "major"};
        ChordData gmaj = {"Gmaj", {7, 11, 2}, 3, "major"};
        ChordData amin = {"Amin", {9, 0, 4}, 3, "minor"};
        ChordData gmaj7 = {"Gmaj7", {7, 11, 2, 5}, 4, "major7"};
        ChordData fmaj7 = {"Fmaj7", {5, 9, 0, 4}, 4, "major7"};
        ChordData dmin7 = {"Dmin7", {2, 5, 9, 0}, 4, "minor7"};
        ChordData cmaj7 = {"Cmaj7", {0, 4, 7, 11}, 4, "major7"};
        ChordData amin7 = {"Amin7", {9, 0, 4, 7}, 4, "minor7"};
        ChordData emin7 = {"Emin7", {4, 7, 11, 2}, 4, "minor7"};
        
        currentChordPack.chords = {cmaj, dmin, emin, fmaj, gmaj, amin, gmaj7, fmaj7, dmin7, cmaj7, amin7, emin7};
        
        randomizeSymbolAssignment();
    }
};

// Matrix8x8Widget Implementation
Matrix8x8Widget::Matrix8x8Widget(Transmutation* module) : module(module) {
    box.size = Vec(LED_SPACING * MATRIX_SIZE, LED_SPACING * MATRIX_SIZE);
}

void Matrix8x8Widget::onButton(const event::Button& e) {
    if (e.action == GLFW_PRESS) {
        Vec pos = e.pos;
        int x = (int)(pos.x / LED_SPACING);
        int y = (int)(pos.y / LED_SPACING);
        
        if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE) {
            if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
                onMatrixClick(x, y);
            } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
                onMatrixRightClick(x, y);
            }
            e.consume(this);
        }
    }
    Widget::onButton(e);
}

void Matrix8x8Widget::onMatrixClick(int x, int y) {
    if (!module) return;
    
    int stepIndex = y * MATRIX_SIZE + x; // Convert 2D to 1D
    if (stepIndex >= 64) return;
    
    // Only allow programming in edit mode
    if (module->editModeA) {
        programStep(module->sequenceA, stepIndex);
    } else if (module->editModeB) {
        programStep(module->sequenceB, stepIndex);
    }
}

void Matrix8x8Widget::onMatrixRightClick(int x, int y) {
    if (!module) return;
    
    int stepIndex = y * MATRIX_SIZE + x;
    if (stepIndex >= 64) return;
    
    // Only allow voice count editing in edit mode and on steps with chords
    if (module->editModeA) {
        SequenceStep& step = module->sequenceA.steps[stepIndex];
        if (step.chordIndex >= 0) {
            // Cycle through voice counts 1-6
            step.voiceCount = (step.voiceCount % 6) + 1;
        }
    } else if (module->editModeB) {
        SequenceStep& step = module->sequenceB.steps[stepIndex];
        if (step.chordIndex >= 0) {
            // Cycle through voice counts 1-6
            step.voiceCount = (step.voiceCount % 6) + 1;
        }
    }
}

void Matrix8x8Widget::programStep(Sequence& seq, int stepIndex) {
    if (!module || stepIndex >= 64) return;
    
    SequenceStep& step = seq.steps[stepIndex];
    
    if (module->selectedSymbol >= 0 && module->selectedSymbol < 12) {
        // Assign chord to step
        step.chordIndex = module->selectedSymbol;
        step.alchemySymbolId = module->selectedSymbol;
        // Set default voice count based on chord's preferred voices
        if (module->symbolToChordMapping[module->selectedSymbol] < (int)module->currentChordPack.chords.size()) {
            const ChordData& chord = module->currentChordPack.chords[module->symbolToChordMapping[module->selectedSymbol]];
            step.voiceCount = std::min(chord.preferredVoices, 6);
        }
    } else if (module->selectedSymbol == -1) {
        // Rest
        step.chordIndex = -1;
        step.alchemySymbolId = -1;
        step.voiceCount = 1;
    } else if (module->selectedSymbol == -2) {
        // Tie
        step.chordIndex = -2;
        step.alchemySymbolId = -2;
        step.voiceCount = 1;
    }
}

void Matrix8x8Widget::drawLayer(const DrawArgs& args, int layer) {
    if (layer == 1) {
        drawMatrix(args);
    }
    Widget::drawLayer(args, layer);
}

void Matrix8x8Widget::drawMatrix(const DrawArgs& args) {
    for (int x = 0; x < MATRIX_SIZE; x++) {
        for (int y = 0; y < MATRIX_SIZE; y++) {
            Vec ledPos = Vec(x * LED_SPACING + LED_SPACING/2, y * LED_SPACING + LED_SPACING/2);
            int stepIndex = y * MATRIX_SIZE + x;
            
            // Get step data from both sequences
            bool hasA = false, hasB = false;
            bool playheadA = false, playheadB = false;
            int symbolId = -1;
            
            if (module && stepIndex < 64) {
                // Check if this step has data in either sequence
                if (stepIndex < module->sequenceA.length && module->sequenceA.steps[stepIndex].chordIndex >= -2) {
                    hasA = true;
                    if (module->sequenceA.steps[stepIndex].alchemySymbolId >= 0) {
                        symbolId = module->sequenceA.steps[stepIndex].alchemySymbolId;
                    }
                }
                
                if (stepIndex < module->sequenceB.length && module->sequenceB.steps[stepIndex].chordIndex >= -2) {
                    hasB = true;
                    if (symbolId < 0 && module->sequenceB.steps[stepIndex].alchemySymbolId >= 0) {
                        symbolId = module->sequenceB.steps[stepIndex].alchemySymbolId;
                    }
                }
                
                // Check for playhead position
                playheadA = (module->sequenceA.running && module->sequenceA.currentStep == stepIndex);
                playheadB = (module->sequenceB.running && module->sequenceB.currentStep == stepIndex);
            }
            
            // Draw LED background
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, ledPos.x, ledPos.y, LED_SIZE/2);
            
            // LED color logic with edit mode feedback
            NVGcolor ledColor = nvgRGBA(20, 20, 20, 255); // Default off
            
            // Check for edit mode highlighting
            bool editModeHighlight = false;
            if (module && ((module->editModeA && hasA) || (module->editModeB && hasB))) {
                editModeHighlight = true;
            }
            
            if (playheadA && playheadB) {
                // Both playheads - mix colors
                ledColor = nvgRGBA(90, 127, 217, 255); // Teal + Purple mix
            } else if (playheadA) {
                ledColor = nvgRGBA(0, 255, 180, 255); // Bright teal
            } else if (playheadB) {
                ledColor = nvgRGBA(180, 0, 255, 255); // Bright purple
            } else if (editModeHighlight) {
                // Edit mode - make editable steps more visible
                if (hasA && module->editModeA) {
                    ledColor = nvgRGBA(0, 200, 140, 200); // Brighter teal for editing
                } else if (hasB && module->editModeB) {
                    ledColor = nvgRGBA(140, 0, 200, 200); // Brighter purple for editing
                }
            } else if (hasA && hasB) {
                // Both sequences have data - dim mix
                ledColor = nvgRGBA(45, 63, 108, 255); // Dim mix
            } else if (hasA) {
                ledColor = nvgRGBA(0, 127, 90, 255); // Dim teal
            } else if (hasB) {
                ledColor = nvgRGBA(90, 0, 127, 255); // Dim purple
            }
            
            // Add edit mode matrix border glow for empty steps
            if (module && (module->editModeA || module->editModeB)) {
                // Subtle edit mode indication on empty steps
                if (!hasA && !hasB) {
                    ledColor = nvgRGBA(40, 40, 60, 100); // Subtle highlight for programmable steps
                }
            }
            
            nvgFillColor(args.vg, ledColor);
            nvgFill(args.vg);
            
            // Draw alchemical symbol if assigned
            if (symbolId >= 0 && symbolId < 12) {
                drawAlchemicalSymbol(args, ledPos, symbolId);
            } else if (symbolId == -1) {
                // Draw rest symbol
                drawRestSymbol(args, ledPos);
            } else if (symbolId == -2) {
                // Draw tie symbol
                drawTieSymbol(args, ledPos);
            }
            
            // Draw voice count indicators
            if (module && stepIndex < 64 && (hasA || hasB)) {
                int voiceCount = 1;
                if (hasA && stepIndex < module->sequenceA.length) {
                    voiceCount = module->sequenceA.steps[stepIndex].voiceCount;
                } else if (hasB && stepIndex < module->sequenceB.length) {
                    voiceCount = module->sequenceB.steps[stepIndex].voiceCount;
                }
                
                if (voiceCount > 1) {
                    drawVoiceCount(args, ledPos, voiceCount);
                }
            }
            
            // LED border
            nvgStrokeColor(args.vg, nvgRGBA(80, 80, 80, 255));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        }
    }
    
    // Draw edit mode matrix border glow
    if (module && (module->editModeA || module->editModeB)) {
        nvgSave(args.vg);
        
        // Set up glow effect
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        
        // Draw animated border glow
        float time = system::getTime();
        float pulse = 0.3f + 0.2f * sin(time * 3.0f);
        
        NVGcolor glowColor;
        if (module->editModeA) {
            glowColor = nvgRGBA(0, 255, 180, pulse * 100);
        } else {
            glowColor = nvgRGBA(180, 0, 255, pulse * 100);
        }
        
        // Draw outer glow border
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -4, -4, 
                      MATRIX_SIZE * LED_SPACING + 8, 
                      MATRIX_SIZE * LED_SPACING + 8, 6);
        nvgStrokeColor(args.vg, glowColor);
        nvgStrokeWidth(args.vg, 3.0f);
        nvgStroke(args.vg);
        
        nvgRestore(args.vg);
    }
}

void Matrix8x8Widget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId) {
    nvgSave(args.vg);
    nvgTranslate(args.vg, pos.x, pos.y);
    
    // Set drawing properties
    nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 220));
    nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 180));
    nvgStrokeWidth(args.vg, 0.8f);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);
    
    float size = 2.5f; // Symbol size
    
    switch (symbolId) {
        case 0: // Sol (Sun) - Circle with center dot
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.3f);
            nvgFill(args.vg);
            break;
            
        case 1: // Luna (Moon) - Crescent
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 2: // Mercury - Circle with horns and cross
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
            
        case 3: // Venus - Circle with cross below
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
            
        case 4: // Mars - Circle with arrow up-right
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
            
        case 5: // Jupiter - Cross with curved line
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
            
        case 6: // Saturn - Cross with curved line (flipped)
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
            
        case 7: // Fire - Triangle pointing up
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 8: // Water - Triangle pointing down  
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 9: // Air - Triangle up with line through
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
            
        case 10: // Earth - Triangle down with line through
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
            
        case 11: // Quintessence - Interwoven circles
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            break;
    }
    
    nvgRestore(args.vg);
}

void Matrix8x8Widget::drawRestSymbol(const DrawArgs& args, Vec pos) {
    nvgStrokeColor(args.vg, nvgRGBA(150, 150, 150, 255));
    nvgStrokeWidth(args.vg, 1.5f);
    
    // Draw rest symbol (small horizontal line)
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - 3, pos.y);
    nvgLineTo(args.vg, pos.x + 3, pos.y);
    nvgStroke(args.vg);
}

void Matrix8x8Widget::drawTieSymbol(const DrawArgs& args, Vec pos) {
    nvgStrokeColor(args.vg, nvgRGBA(255, 200, 100, 255));
    nvgStrokeWidth(args.vg, 1.5f);
    
    // Draw tie symbol (curved line)
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - 3, pos.y);
    nvgBezierTo(args.vg, pos.x - 1, pos.y - 3, pos.x + 1, pos.y - 3, pos.x + 3, pos.y);
    nvgStroke(args.vg);
}

void Matrix8x8Widget::drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount) {
    if (voiceCount <= 1) return;
    
    // Draw small dots around the LED to indicate voice count
    nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 180));
    
    float radius = LED_SIZE/2 + 2.0f; // Just outside the LED
    
    for (int i = 0; i < std::min(voiceCount, 6); i++) {
        float angle = (float)i / 6.0f * 2.0f * M_PI - M_PI/2; // Start from top
        float dotX = pos.x + cos(angle) * radius;
        float dotY = pos.y + sin(angle) * radius;
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, dotX, dotY, 0.8f);
        nvgFill(args.vg);
    }
}

// Alchemical Symbol Button Widget
struct AlchemicalSymbolWidget : Widget {
    Transmutation* module;
    int symbolId;
    
    AlchemicalSymbolWidget(Transmutation* module, int symbolId) : module(module), symbolId(symbolId) {
        box.size = Vec(20, 20);
    }
    
    void draw(const DrawArgs& args) override {
        bool isSelected = module && module->selectedSymbol == symbolId;
        bool inEditMode = module && (module->editModeA || module->editModeB);
        bool isCurrentlyPlaying = false;
        
        // Check if this symbol's chord is currently playing
        if (module) {
            int currentChordA = module->getCurrentChordIndex(module->sequenceA);
            int currentChordB = module->getCurrentChordIndex(module->sequenceB);
            
            // Check if this symbol maps to a currently playing chord
            for (int i = 0; i < 12; i++) {
                if (module->symbolToChordMapping[i] == symbolId) {
                    if ((module->sequenceA.running && currentChordA == i) ||
                        (module->sequenceB.running && currentChordB == i)) {
                        isCurrentlyPlaying = true;
                        break;
                    }
                }
            }
        }
        
        // Draw button background with enhanced states
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        
        if (isCurrentlyPlaying) {
            // Bright glow when chord is playing
            nvgFillColor(args.vg, nvgRGBA(255, 255, 100, 180));
            nvgFill(args.vg);
            
            // Animated glow effect
            float time = system::getTime();
            float pulse = 0.7f + 0.3f * sin(time * 8.0f);
            nvgStrokeColor(args.vg, nvgRGBA(255, 255, 0, pulse * 255));
            nvgStrokeWidth(args.vg, 2.0f);
            nvgStroke(args.vg);
        } else if (isSelected && inEditMode) {
            // Selected for editing - bright blue
            nvgFillColor(args.vg, nvgRGBA(0, 200, 255, 150));
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, nvgRGBA(0, 255, 255, 255));
            nvgStrokeWidth(args.vg, 2.0f);
            nvgStroke(args.vg);
        } else if (inEditMode) {
            // In edit mode but not selected - subtle highlight
            nvgFillColor(args.vg, nvgRGBA(60, 60, 80, 120));
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, nvgRGBA(140, 140, 160, 200));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        } else {
            // Normal state
            nvgFillColor(args.vg, nvgRGBA(40, 40, 40, 100));
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 150));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        }
        
        // Draw the alchemical symbol
        drawAlchemicalSymbol(args, Vec(box.size.x/2, box.size.y/2), symbolId);
    }
    
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId) {
        nvgSave(args.vg);
        nvgTranslate(args.vg, pos.x, pos.y);
        
        // Set drawing properties for button symbols (larger)
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 255));
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(args.vg, 1.2f);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgLineJoin(args.vg, NVG_ROUND);
        
        float size = 6.0f; // Larger symbols for buttons
        
        switch (symbolId) {
            case 0: // Sol (Sun)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size * 0.3f);
                nvgFill(args.vg);
                break;
                
            case 1: // Luna (Moon)
                nvgBeginPath(args.vg);
                nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
                nvgStroke(args.vg);
                break;
                
            case 2: // Mercury
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
                nvgLineTo(args.vg, 0, -size * 0.6f);
                nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, size * 0.2f);
                nvgLineTo(args.vg, 0, size * 0.8f);
                nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
                nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
                nvgStroke(args.vg);
                break;
                
            case 3: // Venus
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
                
            case 4: // Mars
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
                
            case 9: // Air
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
                
            case 10: // Earth
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
                
            case 11: // Quintessence
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f);
                nvgStroke(args.vg);
                break;
        }
        
        nvgRestore(args.vg);
    }
    
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            module->selectedSymbol = symbolId;
            e.consume(this);
        }
        Widget::onButton(e);
    }
};

struct ChordPackButton : Widget {
    Transmutation* module;
    
    ChordPackButton(Transmutation* module) : module(module) {
        box.size = Vec(30, 15);
    }
    
    void draw(const DrawArgs& args) override {
        // Draw button background
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(60, 60, 80, 180));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(120, 120, 140, 255));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
        
        // Draw text
        nvgFontSize(args.vg, 8);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 255));
        nvgText(args.vg, box.size.x/2, box.size.y/2, "LOAD", NULL);
        
        // Show chord pack name below button if loaded
        if (module && !module->currentChordPack.name.empty()) {
            nvgFontSize(args.vg, 6);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(args.vg, nvgRGBA(200, 200, 255, 200));
            nvgText(args.vg, box.size.x/2, box.size.y + 2, module->currentChordPack.name.c_str(), NULL);
        }
    }
    
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            // For now, cycle through example chord packs
            static int packIndex = 0;
            std::vector<std::string> packFiles = {
                asset::plugin(pluginInstance, "chord_packs/80s_pop_d_sharp.json"),
                asset::plugin(pluginInstance, "chord_packs/jazz_standards_bb.json")
            };
            
            if (module->loadChordPackFromFile(packFiles[packIndex])) {
                packIndex = (packIndex + 1) % packFiles.size();
            }
            e.consume(this);
        }
        Widget::onButton(e);
    }
};

// Custom Display Widget - shows BPM, sequence status, steps, and mode information
struct TransmutationDisplayWidget : TransparentWidget {
    Transmutation* module;
    std::shared_ptr<Font> font;
    
    TransmutationDisplayWidget(Transmutation* module) {
        this->module = module;
        box.size = mm2px(Vec(40, 20)); // 40mm x 20mm display area
        font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) {
            // Fallback to a default font if ShareTech isn't available
            font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (!module) return;
        if (!font) return; // Safety check
        
        // Additional safety check for module initialization
        try {
        
        // Background
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(20, 25, 30, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(80, 90, 100, 150));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
        
        // Set up text properties
        nvgFontSize(args.vg, 10);
        if (font && font->handle >= 0) {
            nvgFontFaceId(args.vg, font->handle);
        }
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        
        float y = 5;
        
        // BPM Display
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 200));
        std::string bpmText = "BPM: " + std::to_string((int)module->clockRate);
        nvgText(args.vg, 5, y, bpmText.c_str(), NULL);
        y += 12;
        
        // Sequence A Status
        nvgFillColor(args.vg, nvgRGBA(0, 255, 200, 255)); // Teal for A
        std::string statusA = std::string("A: ") + (module->sequenceA.running ? "RUN" : "STOP") + 
                              " [" + std::to_string(module->sequenceA.currentStep + 1) + 
                              "/" + std::to_string(module->sequenceA.length) + "]";
        nvgText(args.vg, 5, y, statusA.c_str(), NULL);
        y += 12;
        
        // Sequence B Status with Mode
        nvgFillColor(args.vg, nvgRGBA(200, 100, 255, 255)); // Purple for B
        int bMode = (int)module->params[Transmutation::SEQ_B_MODE_PARAM].getValue();
        std::string modeNames[] = {"IND", "HAR", "LOK"}; // Independent, Harmony, Lock
        std::string statusB = std::string("B: ") + (module->sequenceB.running ? "RUN" : "STOP") + 
                              " [" + std::to_string(module->sequenceB.currentStep + 1) + 
                              "/" + std::to_string(module->sequenceB.length) + "] " + 
                              modeNames[bMode];
        nvgText(args.vg, 5, y, statusB.c_str(), NULL);
        y += 12;
        
        // Edit Mode Status
        nvgFillColor(args.vg, nvgRGBA(255, 255, 100, 255)); // Yellow for edit mode
        std::string editStatus = "EDIT: ";
        if (module->editModeA) {
            editStatus += "A";
        } else if (module->editModeB) {
            editStatus += "B";
        } else {
            editStatus += "OFF";
        }
        nvgText(args.vg, 5, y, editStatus.c_str(), NULL);
        
        // Clock source indicators (small icons on the right)
        float rightX = box.size.x - 25;
        nvgFillColor(args.vg, nvgRGBA(150, 150, 150, 200));
        nvgFontSize(args.vg, 8);
        nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        
        // Clock A source
        std::string clockAText = module->inputs[Transmutation::CLOCK_A_INPUT].isConnected() ? "EXT" : "INT";
        nvgText(args.vg, rightX, 17, clockAText.c_str(), NULL);
        
        // Clock B source  
        std::string clockBText = module->inputs[Transmutation::CLOCK_B_INPUT].isConnected() ? "EXT" : "INT";
        nvgText(args.vg, rightX, 29, clockBText.c_str(), NULL);
        
        nvgRestore(args.vg);
        
        } catch (...) {
            // Silently handle any drawing errors to prevent crashes
            if (args.vg) {
                nvgRestore(args.vg);
            }
        }
    }
};

struct TransmutationWidget : ModuleWidget {
    Matrix8x8Widget* matrix;
    
    TransmutationWidget(Transmutation* module) {
        setModule(module);
        
        // 26HP = 131.318mm width
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Transmutation.svg")));
        
        // Add screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // 8x8 LED Matrix (center of panel) - properly spaced
        matrix = new Matrix8x8Widget(module);
        matrix->box.pos = Vec(mm2px(50), mm2px(52)); // Match new SVG matrix position
        addChild(matrix);
        
        // Edit mode buttons (above matrix) - proper spacing
        addParam(createParamCentered<VCVButton>(mm2px(Vec(56.659, 32)), module, Transmutation::EDIT_A_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(74.659, 32)), module, Transmutation::EDIT_B_PARAM));
        
        // Edit mode lights
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(56.659, 32)), module, Transmutation::EDIT_A_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(74.659, 32)), module, Transmutation::EDIT_B_LIGHT));
        
        // Left side controls - Sequence A (match SVG layout)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20, 56)), module, Transmutation::LENGTH_A_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(12, 84)), module, Transmutation::START_A_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(20, 84)), module, Transmutation::STOP_A_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(28, 84)), module, Transmutation::RESET_A_PARAM));
        
        // Right side controls - Sequence B (match SVG layout)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(111.318, 56)), module, Transmutation::LENGTH_B_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(103.318, 84)), module, Transmutation::START_B_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(111.318, 84)), module, Transmutation::STOP_B_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(119.318, 84)), module, Transmutation::RESET_B_PARAM));
        
        // Sequence B mode switch (right side)
        addParam(createParamCentered<CKSSThree>(mm2px(Vec(121.318, 100)), module, Transmutation::SEQ_B_MODE_PARAM));
        
        // Clock controls (center bottom)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(65.659, 105)), module, Transmutation::INTERNAL_CLOCK_PARAM));
        
        // Custom Display Widget - temporarily commented out for debugging
        // TransmutationDisplayWidget* display = new TransmutationDisplayWidget(module);
        // display->box.pos = mm2px(Vec(10, 115)); // Position in lower left area
        // addChild(display);
        
        // Chord pack loader button (custom widget) - centered on new panel
        ChordPackButton* chordPackButton = new ChordPackButton(module);
        chordPackButton->box.pos = mm2px(Vec(48.659, 12)); // Match SVG position
        addChild(chordPackButton);
        
        // Left side I/O - Sequence A (match SVG positions)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 100)), module, Transmutation::CLOCK_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 108)), module, Transmutation::RESET_A_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8, 116)), module, Transmutation::CV_A_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8, 124)), module, Transmutation::GATE_A_OUTPUT));
        
        // Right side I/O - Sequence B (match SVG positions)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(123.318, 100)), module, Transmutation::CLOCK_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(123.318, 108)), module, Transmutation::RESET_B_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(123.318, 116)), module, Transmutation::CV_B_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(123.318, 124)), module, Transmutation::GATE_B_OUTPUT));
        
        // Alchemical Symbol Buttons - properly spaced around matrix
        // Top row of symbols (above matrix) - match new panel positions
        float topSymbolPositions[] = {43, 51, 59, 72, 80, 88}; // Match SVG positions
        for (int i = 0; i < 6; i++) {
            AlchemicalSymbolWidget* symbolWidget = new AlchemicalSymbolWidget(module, i);
            symbolWidget->box.pos = mm2px(Vec(topSymbolPositions[i] - 3, 42 - 3)); // Center 6mm widget
            addChild(symbolWidget);
            addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(topSymbolPositions[i], 39)), module, Transmutation::SYMBOL_1_LIGHT + i * 3));
        }
        
        // Bottom row of symbols (below matrix) - match new panel positions
        float bottomSymbolPositions[] = {43, 51, 59, 72, 80, 88}; // Same X positions
        for (int i = 6; i < 12; i++) {
            AlchemicalSymbolWidget* symbolWidget = new AlchemicalSymbolWidget(module, i);
            symbolWidget->box.pos = mm2px(Vec(bottomSymbolPositions[i - 6] - 3, 87 - 3)); // Center 6mm widget
            addChild(symbolWidget);
            addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(bottomSymbolPositions[i - 6], 96)), module, Transmutation::SYMBOL_1_LIGHT + i * 3));
        }
        
        // Rest and Tie buttons - positioned with sequence controls
        addParam(createParamCentered<VCVButton>(mm2px(Vec(20, 70)), module, Transmutation::REST_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(111.318, 70)), module, Transmutation::TIE_PARAM));
        
        // Running lights - positioned with sequence controls
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(20, 95)), module, Transmutation::RUNNING_A_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(111.318, 95)), module, Transmutation::RUNNING_B_LIGHT));
    }
};

Model* modelTransmutation = createModel<Transmutation, TransmutationWidget>("Transmutation");