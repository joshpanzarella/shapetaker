// Transmutation sequencing helpers
#include <array>
#include <algorithm>
#include <cmath>
#include <rack.hpp>
#include "engine.hpp"
#include "chords.hpp"

namespace stx { namespace transmutation {

bool isStepChanged(const SequenceStep* prev, const SequenceStep* curr) {
    if (!prev && curr) return true;
    if (!curr && prev) return true;
    if (!prev && !curr) return false;
    return (prev->chordIndex != curr->chordIndex) || (prev->voiceCount != curr->voiceCount);
}

const SequenceStep* resolveEffectiveStep(const Sequence& seq, int idx,
                                         const std::array<int, st::SymbolCount>& symbolToChordMapping,
                                         const ChordPack& pack) {
    if (seq.length <= 0) return nullptr;
    int i = (idx % seq.length + seq.length) % seq.length;
    for (int n = 0; n < seq.length; ++n) {
        const SequenceStep& st = seq.steps[i];
        if (st.chordIndex == -2) { // TIE: walk backward
            i = (i - 1 + seq.length) % seq.length;
            continue;
        }
        if (st.chordIndex >= 0 && st.chordIndex < st::SymbolCount) {
            int mapped = symbolToChordMapping[st.chordIndex];
            if (mapped >= 0 && mapped < (int)pack.chords.size())
                return &seq.steps[i];
        }
        return nullptr; // rest or invalid mapping
    }
    return nullptr;
}

void stableClearOutputs(rack::engine::Output* outputs, int cvOutputId, int gateOutputId, int chCount) {
    outputs[cvOutputId].setChannels(chCount);
    outputs[gateOutputId].setChannels(chCount);
    for (int v = 0; v < chCount; ++v) {
        outputs[cvOutputId].setVoltage(0.f, v);
        outputs[gateOutputId].setVoltage(0.f, v);
    }
}

void applyGates(const rack::engine::Module::ProcessArgs& args,
                rack::engine::Output* outputs,
                int gateOutputId,
                rack::dsp::PulseGenerator pulses[MAX_VOICES],
                int activeVoices,
                GateMode gateMode,
                float gatePulseMs,
                bool stepChanged) {
    const int chCount = MAX_VOICES;
    outputs[gateOutputId].setChannels(chCount);
    if (gateMode == GATE_SUSTAIN) {
        for (int v = 0; v < chCount; ++v) {
            outputs[gateOutputId].setVoltage(v < activeVoices ? 10.f : 0.f, v);
        }
        return;
    }
    // Pulse mode
    if (stepChanged) {
        float pw = std::max(0.001f, gatePulseMs / 1000.f);
        for (int v = 0; v < activeVoices && v < MAX_VOICES; ++v) pulses[v].trigger(pw);
    }
    for (int v = 0; v < chCount; ++v) {
        bool high = pulses[v].process(args.sampleTime);
        outputs[gateOutputId].setVoltage(high ? 10.f : 0.f, v);
    }
}

}} // namespace stx::transmutation
