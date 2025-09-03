// Transmutation: chord data structures
#pragma once
#include <string>
#include <vector>
// alchemySymbols.hpp not needed in chords

namespace stx { namespace transmutation {

struct ChordData {
    std::string name;
    std::vector<float> intervals; // semitones
    int preferredVoices;
    std::string category;
};

struct ChordPack {
    std::string name;
    std::string key;
    std::vector<ChordData> chords;
    std::string description;
};

// API
bool loadChordPackFromFile(const std::string& filepath, ChordPack& out);
void loadDefaultChordPack(ChordPack& out);
void randomizeSymbolAssignment(const ChordPack& pack,
                               std::array<int, 40>& symbolToChordMapping,
                               std::array<int, 12>& buttonToSymbolMapping);

}} // namespace stx::transmutation
