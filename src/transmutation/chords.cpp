// Transmutation chord pack helpers
#include "rack.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include "chords.hpp"
#include "../utilities.hpp"

using namespace rack;

namespace stx { namespace transmutation {

bool loadChordPackFromFile(const std::string& filepath, ChordPack& out) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        std::stringstream ss; ss << file.rdbuf();
        std::string content = ss.str();
        if (content.empty()) return false;

        json_error_t error;
        json_t* rootJ = json_loads(content.c_str(), 0, &error);
        if (!rootJ) return false;

        json_t* nameJ = json_object_get(rootJ, "name");
        json_t* keyJ  = json_object_get(rootJ, "key");
        json_t* descJ = json_object_get(rootJ, "description");
        json_t* chordsJ = json_object_get(rootJ, "chords");
        if (!nameJ || !keyJ || !chordsJ) {
            json_decref(rootJ);
            return false;
        }

        out.name = json_string_value(nameJ);
        out.key  = json_string_value(keyJ);
        out.description = descJ ? json_string_value(descJ) : "";
        out.chords.clear();

        size_t chordIndex;
        json_t* chordJ;
        json_array_foreach(chordsJ, chordIndex, chordJ) {
            json_t* chordNameJ = json_object_get(chordJ, "name");
            json_t* intervalsJ = json_object_get(chordJ, "intervals");
            json_t* voicesJ    = json_object_get(chordJ, "preferredVoices");
            json_t* categoryJ  = json_object_get(chordJ, "category");
            if (!chordNameJ || !intervalsJ) continue;

            ChordData c{};
            c.name = json_string_value(chordNameJ);
            c.preferredVoices = voicesJ ? (int)json_integer_value(voicesJ) : 3;
            c.category = categoryJ ? json_string_value(categoryJ) : "unknown";

            size_t intervalIndex; json_t* intervalJ;
            json_array_foreach(intervalsJ, intervalIndex, intervalJ) {
                c.intervals.push_back((float)json_real_value(intervalJ));
            }
            out.chords.push_back(std::move(c));
        }
        json_decref(rootJ);
        return true;
    } catch (...) {
        return false;
    }
}

void loadDefaultChordPack(ChordPack& out) {
    out.name = "Basic Major";
    out.key  = "C";
    out.description = "Basic major chord progressions";

    ChordData cmaj  = {"Cmaj",  {0, 4, 7},            3, "major"};
    ChordData dmin  = {"Dmin",  {2, 5, 9},            3, "minor"};
    ChordData emin  = {"Emin",  {4, 7, 11},           3, "minor"};
    ChordData fmaj  = {"Fmaj",  {5, 9, 0},            3, "major"};
    ChordData gmaj  = {"Gmaj",  {7, 11, 2},           3, "major"};
    ChordData amin  = {"Amin",  {9, 0, 4},            3, "minor"};
    ChordData gmaj7 = {"Gmaj7", {7, 11, 2, 5},        4, "major7"};
    ChordData fmaj7 = {"Fmaj7", {5, 9, 0, 4},         4, "major7"};
    ChordData dmin7 = {"Dmin7", {2, 5, 9, 0},         4, "minor7"};
    ChordData cmaj7 = {"Cmaj7", {0, 4, 7, 11},        4, "major7"};
    ChordData amin7 = {"Amin7", {9, 0, 4, 7},         4, "minor7"};
    ChordData emin7 = {"Emin7", {4, 7, 11, 2},        4, "minor7"};

    out.chords = {cmaj,dmin,emin,fmaj,gmaj,amin,gmaj7,fmaj7,dmin7,cmaj7,amin7,emin7};
}

void randomizeSymbolAssignment(const ChordPack& pack,
                               std::array<int, st::SymbolCount>& symbolToChordMapping,
                               std::array<int, 12>& buttonToSymbolMapping) {
    if (pack.chords.empty()) return;
    std::random_device rd;
    std::mt19937 gen(rd());

    // randomize which symbols appear on buttons (use full symbol set)
    std::vector<int> symbolIds(st::SymbolCount);
    for (int i = 0; i < st::SymbolCount; ++i) symbolIds[i] = i;
    std::shuffle(symbolIds.begin(), symbolIds.end(), gen);
    for (int i = 0; i < 12; ++i) buttonToSymbolMapping[i] = symbolIds[i % symbolIds.size()];

    // clear map and assign chords to the 12 button symbols (prefer unique)
    symbolToChordMapping.fill(-1);
    std::vector<int> chordIdx(pack.chords.size());
    for (int i = 0; i < (int)pack.chords.size(); ++i) chordIdx[i] = i;
    std::shuffle(chordIdx.begin(), chordIdx.end(), gen);

    std::uniform_int_distribution<> dis(0, (int)pack.chords.size() - 1);
    for (int b = 0; b < 12; ++b) {
        int sym = buttonToSymbolMapping[b];
        int idx = (b < (int)chordIdx.size()) ? chordIdx[b] : dis(gen);
        symbolToChordMapping[sym] = idx;
    }

    // final pass: ensure every shown symbol has a chord
    for (int b = 0; b < 12; ++b) {
        int sym = buttonToSymbolMapping[b];
        if (symbolToChordMapping[sym] < 0)
            symbolToChordMapping[sym] = dis(gen);
    }
}

}} // namespace stx::transmutation
