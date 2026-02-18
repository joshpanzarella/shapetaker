#include "plugin.hpp"
#include <cmath>
#include <random>
#include <limits>
#include <array>
#include <algorithm>
#include "involution/liquid_filter.hpp"
#include "involution/dsp.hpp"

struct Involution : Module {
    enum ParamId {
        CUTOFF_A_PARAM,
        RESONANCE_A_PARAM,
        CUTOFF_B_PARAM,
        RESONANCE_B_PARAM,
        MORPH_PARAM,           // LP→BP filter morph (replaces hidden Drive)
        CROSS_PARAM,           // Cross-feedback A↔B amount
        MOD_PARAM,             // Chaos LFO depth on cutoff
        SHIMMER_PARAM,         // Shimmer reverb wet level
        MOD_RATE_PARAM,        // ChaosGenerator rate
        LINK_CUTOFF_PARAM,
        LINK_RESONANCE_PARAM,
        CUTOFF_A_ATTEN_PARAM,
        RESONANCE_A_ATTEN_PARAM,
        CUTOFF_B_ATTEN_PARAM,
        RESONANCE_B_ATTEN_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_A_INPUT,
        AUDIO_B_INPUT,
        CUTOFF_A_CV_INPUT,
        RESONANCE_A_CV_INPUT,
        CUTOFF_B_CV_INPUT,
        RESONANCE_B_CV_INPUT,
        CROSS_CV_INPUT,
        MOD_CV_INPUT,
        SHIMMER_CV_INPUT,
        MOD_RATE_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_A_OUTPUT,
        AUDIO_B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        CHAOS_LIGHT,
        CHAOS_LIGHT_GREEN,
        CHAOS_LIGHT_BLUE,
        CHAOS_RATE_LIGHT,
        LIGHTS_LEN
    };

    // Per-voice 6th-order filters
    shapetaker::dsp::VoiceArray<LiquidFilter> filtersA;
    shapetaker::dsp::VoiceArray<LiquidFilter> filtersB;

    // Per-voice cross-feedback (A←B, B←A with tanh soft limiting)
    std::array<shapetaker::involution::CrossFeedback,
               shapetaker::PolyphonicProcessor::MAX_VOICES> crossFeedbackVoices;

    // Per-voice cutoff smoothers to eliminate CV zipper noise
    shapetaker::dsp::VoiceArray<shapetaker::FastSmoother> cutoffASmoothers;
    shapetaker::dsp::VoiceArray<shapetaker::FastSmoother> cutoffBSmoothers;

    // Global chaos LFO (single instance — modulation is coherent across voices)
    shapetaker::involution::ChaosGenerator chaosGen;

    // Global shimmer — shared delay buffers so poly voices blend naturally into the tail
    shapetaker::dsp::ShimmerDelay shimmerA;
    shapetaker::dsp::ShimmerDelay shimmerB;

    // Per-voice output DC blockers
    struct OutputDCBlock {
        float lastInputA = 0.f, lastOutputA = 0.f;
        float lastInputB = 0.f, lastOutputB = 0.f;
        void reset() { lastInputA = lastOutputA = lastInputB = lastOutputB = 0.f; }
    };
    std::array<OutputDCBlock, shapetaker::PolyphonicProcessor::MAX_VOICES> outputDC{};

    // Rate bounds (also used by ChaosVisualizer)
    static constexpr float MOD_RATE_MIN_HZ = 0.01f;
    static constexpr float MOD_RATE_MAX_HZ = 20.f;
    // Backward-compat aliases for the visualizer
    static constexpr float CHAOS_RATE_MIN_HZ = MOD_RATE_MIN_HZ;
    static constexpr float CHAOS_RATE_MAX_HZ = MOD_RATE_MAX_HZ;

    // Global parameter smoothers
    shapetaker::FastSmoother cutoffASmooth, cutoffBSmooth;
    shapetaker::FastSmoother resonanceASmooth, resonanceBSmooth;
    shapetaker::FastSmoother morphSmooth;
    shapetaker::FastSmoother crossSmooth, modSmooth, shimmerSmooth;
    shapetaker::FastSmoother modRateSmooth;

    static constexpr float PARAM_SMOOTH_TC     = 0.015f;
    static constexpr float CUTOFF_CV_SMOOTH_TC = 0.002f;
    static constexpr float CUTOFF_CURVE_EXP    = 1.5f;    // x^1.5 knob shaping — tighter near cutoff
    static constexpr float CUTOFF_HZ_BASE      = 20.f;    // lowest cutoff frequency in Hz
    static constexpr float CUTOFF_HZ_OCTAVES   = 10.f;    // number of octaves across knob travel
    static constexpr float CHAOS_CUTOFF_RANGE  = 0.15f;   // ±normalized range for chaos LFO on cutoff
    static constexpr float MOD_RATE_CV_SCALE   = 0.5f;    // V/oct-ish scale for mod rate CV
    static constexpr float EFFECT_THRESHOLD    = 0.001f;  // min amount before cross/shimmer is applied
    static constexpr float SHIMMER_DELAY_A_S   = 0.075f;  // shimmer delay time channel A (seconds)
    static constexpr float SHIMMER_DELAY_B_S   = 0.083f;  // shimmer delay time channel B (seconds)
    static constexpr float SHIMMER_FEEDBACK    = 0.30f;   // shimmer internal feedback
    static constexpr float SHIMMER_SEND        = 0.5f;    // shimmer wet send level
    static constexpr float SHIMMER_BLEND       = 0.55f;   // shimmer dry/wet crossfade amount

    // Bidirectional linking state
    float lastCutoffA = -1.f, lastCutoffB = -1.f;
    float lastResonanceA = -1.f, lastResonanceB = -1.f;
    bool lastLinkCutoff = false, lastLinkResonance = false;

    // Visualizer-readable data
    float smoothedChaosRate = 0.5f;  // alias for visualizer compat
    float effectiveResonanceA = 0.707f;
    float effectiveResonanceB = 0.707f;
    float effectiveCutoffA = 1.0f;
    float effectiveCutoffB = 1.0f;
    float crossAmount  = 0.f;
    float modAmount    = 0.f;
    float shimmerAmount = 0.f;

    // Screen color theme (0=Phosphor, 1=Ice, 2=Solar, 3=Amber)
    int chaosTheme = 0;

    Involution() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(CUTOFF_A_PARAM,     0.f,  1.f,  1.f,   "Filter A Cutoff",    " Hz", std::pow(2.f, CUTOFF_HZ_OCTAVES), CUTOFF_HZ_BASE);
        configParam(RESONANCE_A_PARAM,  LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX, LiquidFilter::RESONANCE_MIN, "Filter A Resonance");
        configParam(CUTOFF_B_PARAM,     0.f,  1.f,  1.f,   "Filter B Cutoff",    " Hz", std::pow(2.f, CUTOFF_HZ_OCTAVES), CUTOFF_HZ_BASE);
        configParam(RESONANCE_B_PARAM,  LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX, LiquidFilter::RESONANCE_MIN, "Filter B Resonance");
        configParam(MORPH_PARAM,        0.f,  1.f,  0.f,   "Filter Morph",       "", 0.f, 0.f);
        configParam(CROSS_PARAM,        0.f,  1.f,  0.f,   "Cross-feedback",     "%", 0.f, 100.f);
        configParam(MOD_PARAM,          0.f,  1.f,  0.f,   "Mod Depth",          "%", 0.f, 100.f);
        configParam(SHIMMER_PARAM,      0.f,  1.f,  0.f,   "Shimmer",            "%", 0.f, 100.f);
        configParam(MOD_RATE_PARAM, MOD_RATE_MIN_HZ, MOD_RATE_MAX_HZ, 0.5f, "Mod Rate", " Hz");

        configSwitch(LINK_CUTOFF_PARAM,    0.f, 1.f, 0.f, "Link Cutoff Frequencies", {"Independent", "Linked"});
        configSwitch(LINK_RESONANCE_PARAM, 0.f, 1.f, 0.f, "Link Resonance Amounts",  {"Independent", "Linked"});

        configParam(CUTOFF_A_ATTEN_PARAM,    -1.f, 1.f, 0.f, "Cutoff A CV Attenuverter",    "%", 0.f, 100.f);
        configParam(RESONANCE_A_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance A CV Attenuverter", "%", 0.f, 100.f);
        configParam(CUTOFF_B_ATTEN_PARAM,    -1.f, 1.f, 0.f, "Cutoff B CV Attenuverter",    "%", 0.f, 100.f);
        configParam(RESONANCE_B_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance B CV Attenuverter", "%", 0.f, 100.f);

        configInput(AUDIO_A_INPUT,       "Audio A");
        configInput(AUDIO_B_INPUT,       "Audio B");
        configInput(CUTOFF_A_CV_INPUT,   "Filter A Cutoff CV");
        configInput(RESONANCE_A_CV_INPUT,"Filter A Resonance CV");
        configInput(CUTOFF_B_CV_INPUT,   "Filter B Cutoff CV");
        configInput(RESONANCE_B_CV_INPUT,"Filter B Resonance CV");
        configInput(CROSS_CV_INPUT,      "Cross-feedback CV");
        configInput(MOD_CV_INPUT,        "Mod Depth CV");
        configInput(SHIMMER_CV_INPUT,    "Shimmer CV");
        configInput(MOD_RATE_CV_INPUT,   "Mod Rate CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");

        configLight(CHAOS_LIGHT,      "Cross/Mod/Shimmer Activity");
        configLight(CHAOS_RATE_LIGHT, "Mod Rate");

        onSampleRateChange();
        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "chaosTheme", json_integer(chaosTheme));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* j = json_object_get(rootJ, "chaosTheme");
        if (j) chaosTheme = clamp((int)json_integer_value(j), 0, 3);
    }

    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            filtersA[v].setSampleRate(sr);
            filtersB[v].setSampleRate(sr);
            filtersA[v].setFilterMode(LiquidFilter::MORPH);
            filtersB[v].setFilterMode(LiquidFilter::MORPH);
        }
        shimmerA.reset();
        shimmerB.reset();
    }

    void onReset() override {
        chaosGen.reset();
        shimmerA.reset();
        shimmerB.reset();
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            crossFeedbackVoices[v].reset();
            outputDC[v].reset();
            filtersA[v].reset();
            filtersB[v].reset();
        }
        onSampleRateChange();
    }

    void process(const ProcessArgs& args) override {
        // ====================================================================
        // PARAMETER READ + BIDIRECTIONAL LINKING
        // ====================================================================
        bool linkCutoff    = params[LINK_CUTOFF_PARAM].getValue()    > 0.5f;
        bool linkResonance = params[LINK_RESONANCE_PARAM].getValue() > 0.5f;

        float currentCutoffA    = params[CUTOFF_A_PARAM].getValue();
        float currentCutoffB    = params[CUTOFF_B_PARAM].getValue();
        float currentResonanceA = params[RESONANCE_A_PARAM].getValue();
        float currentResonanceB = params[RESONANCE_B_PARAM].getValue();

        if (linkCutoff) {
            if (!lastLinkCutoff) {
                params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                currentCutoffB = currentCutoffA;
            } else {
                const float eps = 1e-6f;
                bool aChg = std::abs(currentCutoffA - lastCutoffA) > eps;
                bool bChg = std::abs(currentCutoffB - lastCutoffB) > eps;
                if (aChg && !bChg) {
                    params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                    currentCutoffB = currentCutoffA;
                } else if (bChg && !aChg) {
                    params[CUTOFF_A_PARAM].setValue(currentCutoffB);
                    currentCutoffA = currentCutoffB;
                } else if (aChg && bChg) {
                    params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                    currentCutoffB = currentCutoffA;
                }
            }
        }
        if (linkResonance) {
            if (!lastLinkResonance) {
                params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                currentResonanceB = currentResonanceA;
            } else {
                const float eps = 1e-6f;
                bool aChg = std::abs(currentResonanceA - lastResonanceA) > eps;
                bool bChg = std::abs(currentResonanceB - lastResonanceB) > eps;
                if (aChg && !bChg) {
                    params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                    currentResonanceB = currentResonanceA;
                } else if (bChg && !aChg) {
                    params[RESONANCE_A_PARAM].setValue(currentResonanceB);
                    currentResonanceA = currentResonanceB;
                } else if (aChg && bChg) {
                    params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                    currentResonanceB = currentResonanceA;
                }
            }
        }
        lastCutoffA    = currentCutoffA;
        lastCutoffB    = currentCutoffB;
        lastResonanceA = currentResonanceA;
        lastResonanceB = currentResonanceB;
        lastLinkCutoff    = linkCutoff;
        lastLinkResonance = linkResonance;

        // ====================================================================
        // SMOOTH GLOBAL PARAMETERS
        // ====================================================================
        float cutoffA    = cutoffASmooth.process(currentCutoffA,    args.sampleTime);
        float cutoffB    = cutoffBSmooth.process(currentCutoffB,    args.sampleTime);
        float resonanceA = resonanceASmooth.process(currentResonanceA, args.sampleTime);
        float resonanceB = resonanceBSmooth.process(currentResonanceB, args.sampleTime);

        float morphAmt = morphSmooth.process(params[MORPH_PARAM].getValue(), args.sampleTime, PARAM_SMOOTH_TC);

        float crossTarget = params[CROSS_PARAM].getValue();
        if (inputs[CROSS_CV_INPUT].isConnected())
            crossTarget = clamp(crossTarget + inputs[CROSS_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
        crossAmount = crossSmooth.process(crossTarget, args.sampleTime, PARAM_SMOOTH_TC);

        float modTarget = params[MOD_PARAM].getValue();
        if (inputs[MOD_CV_INPUT].isConnected())
            modTarget = clamp(modTarget + inputs[MOD_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
        modAmount = modSmooth.process(modTarget, args.sampleTime, PARAM_SMOOTH_TC);

        float shimTarget = params[SHIMMER_PARAM].getValue();
        if (inputs[SHIMMER_CV_INPUT].isConnected())
            shimTarget = clamp(shimTarget + inputs[SHIMMER_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
        shimmerAmount = shimmerSmooth.process(shimTarget, args.sampleTime, PARAM_SMOOTH_TC);

        float modRateTarget = clamp(params[MOD_RATE_PARAM].getValue(), MOD_RATE_MIN_HZ, MOD_RATE_MAX_HZ);
        if (inputs[MOD_RATE_CV_INPUT].isConnected())
            modRateTarget = clamp(modRateTarget + inputs[MOD_RATE_CV_INPUT].getVoltage() * MOD_RATE_CV_SCALE,
                                  MOD_RATE_MIN_HZ, MOD_RATE_MAX_HZ);
        float modRate = modRateSmooth.process(modRateTarget, args.sampleTime);
        smoothedChaosRate = modRate; // visualizer compat alias

        // ====================================================================
        // CHAOS LFO — single global source, output scaled by mod depth
        // ====================================================================
        float chaosOut = chaosGen.process(modRate, modAmount, args.sampleTime);
        // chaosOut is approximately ±modAmount

        // ====================================================================
        // SET FILTER MORPH FOR ALL VOICES
        // ====================================================================
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            filtersA[v].setFilterMorph(morphAmt);
            filtersB[v].setFilterMorph(morphAmt);
        }

        // ====================================================================
        // VISUALIZER DATA (sampled from voice 0 / global smoothers)
        // ====================================================================
        {
            float dCutoffA = cutoffA, dCutoffB = cutoffB;
            float dResA = resonanceA, dResB = resonanceB;
            if (inputs[CUTOFF_A_CV_INPUT].isConnected())
                dCutoffA = clamp(dCutoffA + inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(0)
                                 * params[CUTOFF_A_ATTEN_PARAM].getValue() / 10.f, 0.f, 1.f);
            if (inputs[CUTOFF_B_CV_INPUT].isConnected())
                dCutoffB = clamp(dCutoffB + inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(0)
                                 * params[CUTOFF_B_ATTEN_PARAM].getValue() / 10.f, 0.f, 1.f);
            if (inputs[RESONANCE_A_CV_INPUT].isConnected())
                dResA = clamp(dResA + inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(0)
                              * params[RESONANCE_A_ATTEN_PARAM].getValue() / 10.f, LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
            if (inputs[RESONANCE_B_CV_INPUT].isConnected())
                dResB = clamp(dResB + inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(0)
                              * params[RESONANCE_B_ATTEN_PARAM].getValue() / 10.f, LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
            effectiveCutoffA    = dCutoffA;
            effectiveCutoffB    = dCutoffB;
            effectiveResonanceA = dResA;
            effectiveResonanceB = dResB;
        }

        // ====================================================================
        // POLYPHONIC AUDIO PROCESSING
        // ====================================================================
        int channelsA = inputs[AUDIO_A_INPUT].getChannels();
        int channelsB = inputs[AUDIO_B_INPUT].getChannels();
        int channels  = std::min(std::max(channelsA, channelsB),
                                 shapetaker::PolyphonicProcessor::MAX_VOICES);

        if (!inputs[AUDIO_A_INPUT].isConnected() && !inputs[AUDIO_B_INPUT].isConnected()) {
            outputs[AUDIO_A_OUTPUT].setChannels(0);
            outputs[AUDIO_B_OUTPUT].setChannels(0);
        } else {
            outputs[AUDIO_A_OUTPUT].setChannels(channels);
            outputs[AUDIO_B_OUTPUT].setChannels(channels);

            for (int c = 0; c < channels; c++) {
                // --- Input ---
                float audioA = 0.f, audioB = 0.f;
                bool hasA = inputs[AUDIO_A_INPUT].isConnected();
                bool hasB = inputs[AUDIO_B_INPUT].isConnected();
                if (hasA && hasB) {
                    audioA = inputs[AUDIO_A_INPUT].getVoltage(c);
                    audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                } else if (hasA) {
                    audioA = audioB = inputs[AUDIO_A_INPUT].getVoltage(c);
                } else {
                    audioA = audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                }
                if (!std::isfinite(audioA)) audioA = 0.f;
                if (!std::isfinite(audioB)) audioB = 0.f;

                // --- Cross-feedback (pre-filter, soft-limited) ---
                if (crossAmount > EFFECT_THRESHOLD) {
                    auto cf = crossFeedbackVoices[c].process(audioA, audioB, crossAmount);
                    audioA = cf.outputA;
                    audioB = cf.outputB;
                }

                // --- Per-voice cutoff with CV and chaos modulation ---
                float voiceCutoffA = cutoffA;
                float voiceCutoffB = cutoffB;
                float voiceResA    = resonanceA;
                float voiceResB    = resonanceB;

                if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
                    float att = params[CUTOFF_A_ATTEN_PARAM].getValue();
                    voiceCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(c) * att / 10.f;
                }
                voiceCutoffA = clamp(voiceCutoffA + chaosOut * CHAOS_CUTOFF_RANGE, 0.f, 1.f);
                voiceCutoffA = cutoffASmoothers[c].process(voiceCutoffA, args.sampleTime, CUTOFF_CV_SMOOTH_TC);

                if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
                    float att = params[CUTOFF_B_ATTEN_PARAM].getValue();
                    voiceCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(c) * att / 10.f;
                }
                voiceCutoffB = clamp(voiceCutoffB + chaosOut * CHAOS_CUTOFF_RANGE, 0.f, 1.f);
                voiceCutoffB = cutoffBSmoothers[c].process(voiceCutoffB, args.sampleTime, CUTOFF_CV_SMOOTH_TC);

                if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
                    float att = params[RESONANCE_A_ATTEN_PARAM].getValue();
                    voiceResA = clamp(voiceResA + inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(c) * att / 10.f,
                                      LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
                }
                if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
                    float att = params[RESONANCE_B_ATTEN_PARAM].getValue();
                    voiceResB = clamp(voiceResB + inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(c) * att / 10.f,
                                      LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
                }

                // --- Liquid filter (drive = 1.0 — no pre-gain) ---
                // x^1.5 slows the sweep through the musical sweet spot (200Hz–3kHz),
                // which occupies roughly 55–80% of knob travel. Gives a deliberate,
                // elastic feel — the filter "breathes" through harmonics rather than
                // shooting past them.
                float shapedA   = std::pow(voiceCutoffA, CUTOFF_CURVE_EXP);
                float shapedB   = std::pow(voiceCutoffB, CUTOFF_CURVE_EXP);
                float cutoffAHz = CUTOFF_HZ_BASE * std::pow(2.f, shapedA * CUTOFF_HZ_OCTAVES);
                float cutoffBHz = CUTOFF_HZ_BASE * std::pow(2.f, shapedB * CUTOFF_HZ_OCTAVES);

                float processedA = filtersA[c].process(audioA, cutoffAHz, voiceResA, 1.0f);
                float processedB = filtersB[c].process(audioB, cutoffBHz, voiceResB, 1.0f);

                // Store filter outputs so CrossFeedback can inject them into the
                // other channel's input on the next cycle.
                if (crossAmount > EFFECT_THRESHOLD)
                    crossFeedbackVoices[c].storeOutputs(processedA, processedB);

                // --- Global shimmer (shared buffer — poly voices blend into one reverb tail) ---
                if (shimmerAmount > EFFECT_THRESHOLD) {
                    // Slightly different delay times for A and B give natural stereo width
                    float shimOutA = shimmerA.process(processedA, SHIMMER_DELAY_A_S, SHIMMER_FEEDBACK, shimmerAmount * SHIMMER_SEND);
                    float shimOutB = shimmerB.process(processedB, SHIMMER_DELAY_B_S, SHIMMER_FEEDBACK, shimmerAmount * SHIMMER_SEND);
                    processedA = rack::math::crossfade(processedA, shimOutA, shimmerAmount * SHIMMER_BLEND);
                    processedB = rack::math::crossfade(processedB, shimOutB, shimmerAmount * SHIMMER_BLEND);
                }

                // --- Output DC blocking ---
                processedA = shapetaker::dsp::AudioProcessor::processDCBlock(
                    processedA, outputDC[c].lastInputA, outputDC[c].lastOutputA);
                processedB = shapetaker::dsp::AudioProcessor::processDCBlock(
                    processedB, outputDC[c].lastInputB, outputDC[c].lastOutputB);

                outputs[AUDIO_A_OUTPUT].setVoltage(processedA, c);
                outputs[AUDIO_B_OUTPUT].setVoltage(processedB, c);
            }
        }

        // ====================================================================
        // LIGHTS — Cross=Red, Mod=Green, Shimmer=Blue
        // ====================================================================
        lights[CHAOS_LIGHT    ].setBrightness(clamp(crossAmount,   0.f, 1.f));
        lights[CHAOS_LIGHT + 1].setBrightness(clamp(modAmount,     0.f, 1.f));
        lights[CHAOS_LIGHT + 2].setBrightness(clamp(shimmerAmount, 0.f, 1.f));
        lights[CHAOS_RATE_LIGHT].setBrightness(0.f);
    }

    void onRandomize() override {
        std::mt19937 rng(rack::random::u32());
        std::uniform_real_distribution<float> cutoffDist(0.2f, 0.9f);
        std::uniform_real_distribution<float> resDist(0.1f, 0.6f);
        std::uniform_real_distribution<float> charDist(0.f, 0.7f);
        std::uniform_real_distribution<float> rateDist(0.2f, 0.8f);
        std::uniform_int_distribution<int>    linkDist(0, 1);

        params[CUTOFF_A_PARAM].setValue(cutoffDist(rng));
        params[CUTOFF_B_PARAM].setValue(cutoffDist(rng));
        params[RESONANCE_A_PARAM].setValue(resDist(rng));
        params[RESONANCE_B_PARAM].setValue(resDist(rng));
        params[MORPH_PARAM].setValue(charDist(rng));
        params[CROSS_PARAM].setValue(charDist(rng));
        params[MOD_PARAM].setValue(charDist(rng));
        params[SHIMMER_PARAM].setValue(charDist(rng));
        params[MOD_RATE_PARAM].setValue(rateDist(rng));
        params[LINK_CUTOFF_PARAM].setValue((float)linkDist(rng));
        params[LINK_RESONANCE_PARAM].setValue((float)linkDist(rng));
    }
};

// Chaos visualizer widget — extracted to separate files
#include "involution/chaos_visualizer.hpp"
#include "involution/chaos_visualizer_impl.hpp"

// Custom SVG-based JewelLED for the activity light
struct ChaosJewelLED : ModuleLightWidget {
    ChaosJewelLED() {
        box.size = Vec(20, 20);

        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }

        addBaseColor(nvgRGB(186, 92, 220));
        addBaseColor(nvgRGB(148, 124, 255));
        addBaseColor(nvgRGB(255, 118, 214));
    }

    void step() override {
        if (module) {
            Involution* inv = dynamic_cast<Involution*>(module);
            if (inv) {
                int themeIdx = clamp(inv->chaosTheme, 0, NUM_CHAOS_THEMES - 1);
                const ChaosThemePalette& t = CHAOS_THEMES[themeIdx];
                for (int i = 0; i < 3; i++) baseColors[i] = t.ledBaseColors[i];
            }
        }
        ModuleLightWidget::step();

        if (module) {
            float r = module->lights[firstLightId + 0].getBrightness();
            float g = module->lights[firstLightId + 1].getBrightness();
            float b = module->lights[firstLightId + 2].getBrightness();
            color = nvgRGBAf(r, g, b, fmaxf(fmaxf(r, g), b));
        } else {
            color = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
        }
    }

    void drawLight(const DrawArgs& args) override {
        float brightness = color.a;
        brightness = std::max(brightness, color.r);
        brightness = std::max(brightness, color.g);
        brightness = std::max(brightness, color.b);
        if (brightness <= 1e-3f) return;

        const float minSize  = std::min(box.size.x, box.size.y);
        const float cx       = box.size.x * 0.5f;
        const float cy       = box.size.y * 0.5f;
        const float lensRadius = 0.42f * minSize;
        float glow = clamp(brightness * 2.0f, 0.f, 1.f) * shapetaker::ui::kJewelMaxBrightness;

        nvgSave(args.vg);
        nvgScissor(args.vg, cx - lensRadius, cy - lensRadius, lensRadius * 2.f, lensRadius * 2.f);

        NVGcolor washInner = color; washInner.a = 0.85f * glow;
        NVGcolor washOuter = color; washOuter.a = 0.45f * glow;
        NVGpaint wash = nvgRadialGradient(args.vg, cx, cy, 0.f, lensRadius, washInner, washOuter);
        nvgBeginPath(args.vg); nvgCircle(args.vg, cx, cy, lensRadius);
        nvgFillPaint(args.vg, wash); nvgFill(args.vg);

        NVGcolor coreInner = nvgRGBAf(
            std::min(color.r * 1.5f + 0.3f, 1.f),
            std::min(color.g * 1.5f + 0.3f, 1.f),
            std::min(color.b * 1.5f + 0.3f, 1.f), 0.9f * glow);
        NVGcolor coreOuter = color; coreOuter.a = 0.2f * glow;
        NVGpaint core = nvgRadialGradient(args.vg, cx, cy, 0.f, lensRadius * 0.5f, coreInner, coreOuter);
        nvgBeginPath(args.vg); nvgCircle(args.vg, cx, cy, lensRadius * 0.5f);
        nvgFillPaint(args.vg, core); nvgFill(args.vg);

        NVGcolor spec = nvgRGBAf(1.f, 1.f, 1.f, 0.35f * glow);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx - lensRadius * 0.15f, cy - lensRadius * 0.15f, lensRadius * 0.18f);
        nvgFillColor(args.vg, spec); nvgFill(args.vg);

        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }

    void drawHalo(const DrawArgs& args) override {
        float brightness = color.a;
        brightness = std::max(brightness, color.r);
        brightness = std::max(brightness, color.g);
        brightness = std::max(brightness, color.b);
        if (brightness <= 1e-3f) return;

        float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        float radius  = std::min(box.size.x, box.size.y) * 0.5f;
        float oradius = radius * 1.5f;

        NVGcolor icol = color; icol.a = 0.25f * brightness;
        NVGcolor ocol = color; ocol.a = 0.0f;
        NVGpaint paint = nvgRadialGradient(args.vg, cx, cy, radius * 0.15f, oradius, icol, ocol);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, cx - oradius, cy - oradius, oradius * 2.f, oradius * 2.f);
        nvgFillPaint(args.vg, paint); nvgFill(args.vg);
    }

    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg); nvgCircle(args.vg, 10, 10, 9.5);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0)); nvgFill(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 10, 10, 6.5);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33)); nvgFill(args.vg);
        }
        widget::Widget::draw(args);
        nvgSave(args.vg);
        nvgGlobalAlpha(args.vg, shapetaker::ui::kJewelMaxBrightness);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        drawLight(args);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        drawHalo(args);
        nvgRestore(args.vg);
    }
};

// Glow halo drawn behind the jewel lens
struct ChaosRateGlow : ModuleLightWidget {
    ChaosRateGlow() { box.size = Vec(20.f, 20.f); }

    void draw(const DrawArgs& args) override {
        float brightness = module ? module->lights[firstLightId].getBrightness() : 0.f;
        brightness = clamp(brightness, 0.f, 1.f);
        if (brightness <= 0.f) return;

        NVGcolor glowBase = nvgRGB(255, 0, 255);
        if (module) {
            Involution* inv = dynamic_cast<Involution*>(module);
            if (inv) {
                int themeIdx = clamp(inv->chaosTheme, 0, NUM_CHAOS_THEMES - 1);
                glowBase = CHAOS_THEMES[themeIdx].ledGlowColor;
            }
        }

        NVGcontext* vg = args.vg;
        float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        float radius = std::min(box.size.x, box.size.y) * 0.5f;

        nvgSave(vg);
        NVGcolor haloInner = glowBase; haloInner.a = brightness * (90.f / 255.f);
        NVGcolor haloOuter = glowBase; haloOuter.a = 0.f;
        NVGpaint glow = nvgRadialGradient(vg, cx, cy, radius * 0.2f, radius * 1.2f, haloInner, haloOuter);
        nvgBeginPath(vg); nvgCircle(vg, cx, cy, radius * 1.15f);
        nvgFillPaint(vg, glow); nvgFill(vg);

        NVGcolor coreColor = glowBase; coreColor.a = brightness * (140.f / 255.f);
        nvgBeginPath(vg); nvgCircle(vg, cx, cy, radius * 0.45f);
        nvgFillColor(vg, coreColor); nvgFill(vg);
        nvgRestore(vg);
    }
};

struct InvolutionWidget : ModuleWidget {
    InvolutionWidget(Involution* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Involution.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        auto centerPx = LayoutHelper::createCenterPxHelper(
            asset::plugin(pluginInstance, "res/panels/Involution.svg"));

        auto addKnobWithShadow = [this](app::ParamWidget* knob) {
            ::addKnobWithShadow(this, knob);
        };

        // ---- Filter A/B cutoff (extra large vintage) ----
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageXLarge>(
            centerPx("cutoff_v", 24.026f, 24.174f), module, Involution::CUTOFF_A_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageXLarge>(
            centerPx("cutoff_z", 66.305f, 24.174f), module, Involution::CUTOFF_B_PARAM));

        // ---- Filter A/B resonance (medium vintage) ----
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("resonance_v", 11.935f, 57.750f), module, Involution::RESONANCE_A_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageMedium>(
            centerPx("resonance_z", 78.397f, 57.750f), module, Involution::RESONANCE_B_PARAM));

        // ---- Link switches ----
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("link_cutoff", 45.166f, 29.894f), module, Involution::LINK_CUTOFF_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("link_resonance", 45.166f, 84.630f), module, Involution::LINK_RESONANCE_PARAM));

        // ---- Attenuverters ----
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_v_atten", 9.027f, 41.042f), module, Involution::CUTOFF_A_ATTEN_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_v_atten", 11.935f, 76.931f), module, Involution::RESONANCE_A_ATTEN_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_z_atten", 81.305f, 41.042f), module, Involution::CUTOFF_B_ATTEN_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_z_atten", 78.397f, 76.931f), module, Involution::RESONANCE_B_ATTEN_PARAM));

        // ---- Character controls: Cross / Mod / Shimmer ----
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("aura_knob", 15.910f, 94.088f), module, Involution::CROSS_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("orbit_knob", 45.166f, 94.088f), module, Involution::MOD_PARAM));
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("tide_knob", 74.422f, 94.088f), module, Involution::SHIMMER_PARAM));

        // ---- Mod Rate (attenuverter-style small knob) ----
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("chaos_rate_knob", 60.922f, 108.088f), module, Involution::MOD_RATE_PARAM));

        // ---- Filter Morph (center, between rate and audio I/O) ----
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("filter_morph", 45.166f, 108.088f), module, Involution::MORPH_PARAM));

        // ---- Chaos visualizer ----
        ChaosVisualizer* chaosViz = new ChaosVisualizer(module);
        Vec screenCenter = centerPx("oscope_screen",
                                    std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN());
        if (!std::isfinite(screenCenter.x) || !std::isfinite(screenCenter.y))
            screenCenter = centerPx("resonance_a_cv-1", 45.166f, 57.750f);
        chaosViz->box.pos = Vec(screenCenter.x - 86.5f, screenCenter.y - 69.f);
        addChild(chaosViz);

        // ---- Activity jewel LED ----
        Vec chaosCenter = centerPx("chaos_rate_light", 29.559f, 103.546f);
        addChild(createLightCentered<ChaosRateGlow>(chaosCenter, module, Involution::CHAOS_RATE_LIGHT));
        addChild(createLightCentered<ChaosJewelLED>(chaosCenter, module, Involution::CHAOS_LIGHT));

        // ---- CV inputs ----
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("cutoff_v_cv",    24.027f, 44.322f), module, Involution::CUTOFF_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("resonance_v_cv", 24.027f, 68.931f), module, Involution::RESONANCE_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("cutoff_z_cv",    66.305f, 44.322f), module, Involution::CUTOFF_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("resonance_z_cv", 66.305f, 68.931f), module, Involution::RESONANCE_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("aura_cv",  15.910f, 84.630f), module, Involution::CROSS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("orbit_cv", 45.166f, 84.630f), module, Involution::MOD_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("tide_cv",  74.422f, 84.630f), module, Involution::SHIMMER_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("chaos_cv", 60.922f, 84.630f), module, Involution::MOD_RATE_CV_INPUT));

        // ---- Audio I/O ----
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("audio_l_input",  10.276f, 118.977f), module, Involution::AUDIO_A_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("audio_r_input",  27.721f, 119.245f), module, Involution::AUDIO_B_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("audio_l_output", 63.436f, 119.347f), module, Involution::AUDIO_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("audio_r_output", 81.706f, 119.347f), module, Involution::AUDIO_B_OUTPUT));
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(
            asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;

            nvgSave(args.vg);

            // Base tile pass
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            // Offset low-opacity pass to soften seam visibility
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            // Slight darkening to match existing module tone
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
            nvgFill(args.vg);

            nvgRestore(args.vg);
        }
        ModuleWidget::draw(args);

        // Inner frame to mask any edge tinting
        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* inv = dynamic_cast<Involution*>(module);
        if (!inv) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem(
            "Screen Theme",
            CHAOS_THEME_NAMES[clamp(inv->chaosTheme, 0, NUM_CHAOS_THEMES - 1)],
            [=](Menu* childMenu) {
                for (int i = 0; i < NUM_CHAOS_THEMES; i++) {
                    childMenu->addChild(createCheckMenuItem(CHAOS_THEME_NAMES[i], "",
                        [=] { return inv->chaosTheme == i; },
                        [=] { inv->chaosTheme = i; }));
                }
            }));
    }
};

Model* modelInvolution = createModel<Involution, InvolutionWidget>("Involution");
