// Lightweight helpers for building and assigning poly voice targets
#pragma once
#include <vector>
#include <cmath>

namespace stx { namespace poly {

// Build target note CVs (V/oct) from semitone intervals for the requested voice count.
// If harmonyMode is true, push voices up by octaves and add fifths to odd voices to widen.
inline void buildTargetsFromIntervals(const std::vector<float>& intervalsSemitones,
                                      int voiceCount,
                                      bool harmonyMode,
                                      std::vector<float>& out) {
    out.clear();
    out.reserve(voiceCount);
    for (int voice = 0; voice < voiceCount; ++voice) {
        float semi = 0.f;
        if (!intervalsSemitones.empty()) {
            if (voice < (int)intervalsSemitones.size()) {
                semi = intervalsSemitones[voice];
            } else {
                int idx = voice % intervalsSemitones.size();
                int oct = voice / (int)intervalsSemitones.size();
                semi = intervalsSemitones[idx] + oct * 12.f;
            }
        }
        if (harmonyMode) {
            semi += 12.f;                 // +1 octave
            if (voice % 2 == 1) semi += 7.f; // add fifth on odd voices
        }
        out.push_back(semi / 12.f); // convert to V/oct, root at 0V
    }
}

// Assign targets to output channels to minimize per-voice jumps.
// last[6] holds last CV per channel (V/oct). Returns assigned vector sized to voiceCount.
inline void assignNearest(const std::vector<float>& targets,
                          const float last[6],
                          int voiceCount,
                          std::vector<float>& assigned) {
    assigned.assign(voiceCount, 0.f);
    std::vector<char> used(targets.size(), 0);
    for (int v = 0; v < voiceCount; ++v) {
        float pv = last[v];
        float bestDist = 1e9f;
        int bestIdx = -1;
        float bestCV = 0.f;
        for (size_t j = 0; j < targets.size(); ++j) {
            if (used[j]) continue;
            // consider octave-wrapped candidates to reduce jumps
            for (int k = -2; k <= 2; ++k) {
                float cand = targets[j] + k; // shift by octaves in V/oct
                float d = std::fabs(cand - pv);
                if (d < bestDist) {
                    bestDist = d;
                    bestIdx = (int)j;
                    bestCV = cand;
                }
            }
        }
        if (bestIdx >= 0) {
            used[bestIdx] = 1;
            assigned[v] = bestCV;
        } else if (!targets.empty()) {
            assigned[v] = targets[0];
        }
    }
}

}} // namespace stx::poly

