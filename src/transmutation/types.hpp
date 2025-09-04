#pragma once

#include <string>
#include <vector>
#include <array>

namespace shapetaker {
namespace transmutation {

// Chord data structure representing a single chord
struct ChordData {
    std::string name;
    std::vector<float> intervals;
    int preferredVoices;
    std::string category;
};

// Chord pack containing a collection of chords in a specific key/style
struct ChordPack {
    std::string name;
    std::string key;
    std::vector<ChordData> chords;
    std::string description;
};

// Individual step in a sequence
struct SequenceStep {
    int chordIndex;
    int voiceCount;
    int alchemySymbolId;
    
    SequenceStep() : chordIndex(-999), voiceCount(1), alchemySymbolId(-999) {} // -999 = uninitialized, -1 = REST
};

// Complete sequence with up to 64 steps
struct Sequence {
    std::array<SequenceStep, 64> steps;
    int length;
    int currentStep;
    bool running;
    float clockPhase;
    
    Sequence() : length(16), currentStep(0), running(false), clockPhase(0.0f) {}
};

} // namespace transmutation
} // namespace shapetaker