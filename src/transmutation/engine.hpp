// Transmutation: lightweight sequence structures used by the engine and UI
#pragma once
#include <array>
#include <string>
#include "chords.hpp"
// alchemySymbols constants now in utilities.hpp
#include <rack.hpp>

struct SequenceStep {
    int chordIndex;      // 0..39 for symbol IDs, -1 REST, -2 TIE, -999 uninitialized
    int voiceCount;      // requested voice count
    int alchemySymbolId; // optional symbol annotation

    SequenceStep() : chordIndex(-999), voiceCount(1), alchemySymbolId(-999) {}
};

struct Sequence {
    std::array<SequenceStep, 64> steps;
    int length;
    int currentStep;
    bool running;
    float clockPhase;

    // Groove scheduling state (microtiming delay per step)
    bool groovePending;
    float grooveDelay;        // seconds remaining until advancing to next step
    double lastClockTime;     // last incoming clock tick time (seconds)
    float estPeriod;          // estimated step period (seconds)

    Sequence() : length(16), currentStep(0), running(false), clockPhase(0.0f),
        groovePending(false), grooveDelay(0.f), lastClockTime(0.0), estPeriod(0.5f) {}
};

namespace stx { namespace transmutation {
// Default polyphony for Transmutation
constexpr int MAX_VOICES = 8;
// Compare steps for re-trigger decisions
bool isStepChanged(const SequenceStep* prev, const SequenceStep* curr);

// Follow TIEs backward and validate mapping against the chord pack
const SequenceStep* resolveEffectiveStep(const Sequence& seq, int idx,
                                         const std::array<int, st::SymbolCount>& symbolToChordMapping,
                                         const ChordPack& pack);

// Gate policy used by helpers
enum GateMode { GATE_SUSTAIN = 0, GATE_PULSE = 1 };

// Clear outputs to a stable 6-channel frame (0V CV, gates low)
void stableClearOutputs(rack::engine::Output* outputs, int cvOutputId, int gateOutputId, int chCount = MAX_VOICES);

// Apply gate policy; pulses array must be at least 6
// totalChannels controls the number of output channels exposed (for stable polyphony)
void applyGates(const rack::engine::Module::ProcessArgs& args,
                rack::engine::Output* outputs,
                int gateOutputId,
                rack::dsp::PulseGenerator pulses[MAX_VOICES],
                int activeVoices,
                GateMode gateMode,
                float gatePulseMs,
                bool stepChanged,
                int totalChannels = MAX_VOICES);
}} // namespace stx::transmutation
