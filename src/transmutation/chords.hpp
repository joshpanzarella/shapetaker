// Transmutation: chord data structures
#pragma once
#include <string>
#include <vector>
#include "../utilities.hpp"
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
    std::string mode;
    std::string scale;
    std::string genre;
    std::string mood;
    std::string complexity;
    std::string voicingStyle;
    std::vector<std::string> tags;
    std::vector<ChordData> chords;
    std::string description;
};

// API
bool loadChordPackFromFile(const std::string& filepath, ChordPack& out);
void loadDefaultChordPack(ChordPack& out);
void randomizeSymbolAssignment(const ChordPack& pack,
                               std::array<int, st::SymbolCount>& symbolToChordMapping,
                               std::array<int, 12>& buttonToSymbolMapping);

}} // namespace stx::transmutation
