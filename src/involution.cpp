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
        SHIFT_DEPTH_PARAM,      // Max Hz range for frequency shifter
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

    // Per-voice frequency shifters — A shifts up, B shifts down (counter-rotating stereo)
    std::array<shapetaker::involution::FrequencyShifter,
               shapetaker::PolyphonicProcessor::MAX_VOICES> freqShiftersA{};
    std::array<shapetaker::involution::FrequencyShifter,
               shapetaker::PolyphonicProcessor::MAX_VOICES> freqShiftersB{};

    // Per-voice cutoff smoothers to eliminate CV zipper noise
    shapetaker::dsp::VoiceArray<shapetaker::FastSmoother> cutoffASmoothers;
    shapetaker::dsp::VoiceArray<shapetaker::FastSmoother> cutoffBSmoothers;

    // Global chaos LFO (single instance — modulation is coherent across voices)
    shapetaker::involution::ChaosGenerator chaosGen;

    // Per-voice resonant APF phasers — run post-filter, swept by the chaos LFO
    std::array<shapetaker::involution::AllpassPhaser,
               shapetaker::PolyphonicProcessor::MAX_VOICES> phasersA{};
    std::array<shapetaker::involution::AllpassPhaser,
               shapetaker::PolyphonicProcessor::MAX_VOICES> phasersB{};

    // Per-voice output DC blockers
    struct OutputDCBlock {
        float lastInputA = 0.f, lastOutputA = 0.f;
        float lastInputB = 0.f, lastOutputB = 0.f;
        void reset() { lastInputA = lastOutputA = lastInputB = lastOutputB = 0.f; }
    };
    std::array<OutputDCBlock, shapetaker::PolyphonicProcessor::MAX_VOICES> outputDC{};

    // Rate bounds (also used by ChaosVisualizer)
    static constexpr float DEFAULT_SAMPLE_RATE = 44100.f;
    static constexpr float MOD_RATE_MIN_HZ = 0.01f;
    static constexpr float MOD_RATE_MAX_HZ = 2.f;
    static constexpr float MOD_RATE_DEFAULT_HZ = 0.5f;
    // Backward-compat aliases for the visualizer
    static constexpr float CHAOS_RATE_MIN_HZ = MOD_RATE_MIN_HZ;
    static constexpr float CHAOS_RATE_MAX_HZ = MOD_RATE_MAX_HZ;

    // Global parameter smoothers
    shapetaker::FastSmoother cutoffASmooth, cutoffBSmooth;
    shapetaker::FastSmoother resonanceASmooth, resonanceBSmooth;
    shapetaker::FastSmoother crossSmooth, modSmooth, shimmerSmooth, shiftDepthSmooth;
    shapetaker::FastSmoother modRateSmooth;

    static constexpr float PARAM_SMOOTH_TC     = 0.015f;
    static constexpr float CUTOFF_CV_SMOOTH_TC = 0.002f;
    static constexpr float CV_NORMALIZED_SCALE = 0.1f;    // 10V -> 1.0
    static constexpr float LINK_SWITCH_THRESHOLD = 0.5f;
    static constexpr float LINK_PARAM_EPSILON = 1e-6f;
    static constexpr float CUTOFF_CURVE_EXP    = 1.5f;    // x^1.5 knob shaping — tighter near cutoff
    static constexpr float CUTOFF_HZ_BASE      = 20.f;    // lowest cutoff frequency in Hz
    static constexpr float CUTOFF_HZ_OCTAVES   = 10.f;    // number of octaves across knob travel
    static constexpr float CHAOS_CUTOFF_RANGE  = 0.15f;   // ±normalized range for chaos LFO on cutoff
    static constexpr float MOD_RATE_CV_SCALE   = 0.5f;    // V/oct-ish scale for mod rate CV
    static constexpr float EFFECT_THRESHOLD    = 0.001f;  // min amount before cross/phaser is applied
    static constexpr float PHASER_CENTER_HZ    = 400.f;   // base phaser sweep frequency
    static constexpr float PHASER_MIN_CENTER_HZ = 20.f;
    static constexpr float NYQUIST_HEADROOM_RATIO = 0.45f;
    static constexpr float PHASER_CHAOS_SPREAD = 3.5f;    // LFO modulation depth in octaves (±3.5 oct at full mod)
    static constexpr float PHASER_MAX_FEEDBACK = 0.65f;   // max APF feedback (sharpens notches)
    static constexpr float PHASER_LEVEL_COMP   = 1.414f;  // ~+3dB makeup for notch-induced level loss
    static constexpr float FREQ_SHIFT_MAX_HZ   = 100.f;  // absolute ceiling for shift depth knob

    // Bidirectional linking state
    float lastCutoffA = -1.f, lastCutoffB = -1.f;
    float lastResonanceA = -1.f, lastResonanceB = -1.f;
    bool lastLinkCutoff = false, lastLinkResonance = false;

    // Visualizer-readable data
    float smoothedChaosRate = MOD_RATE_DEFAULT_HZ;  // alias for visualizer compat
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
        configParam(CROSS_PARAM,        0.f,  1.f,  0.f,   "Freq Shift",         "%", 0.f, 100.f);
        configParam(MOD_PARAM,          0.f,  1.f,  0.f,   "Mod Depth",          "%", 0.f, 100.f);
        configParam(SHIMMER_PARAM,      0.f,  1.f,  0.f,   "Phaser",             "%", 0.f, 100.f);
        configParam(MOD_RATE_PARAM, MOD_RATE_MIN_HZ, MOD_RATE_MAX_HZ, MOD_RATE_DEFAULT_HZ, "Mod Rate", " Hz");

        configSwitch(LINK_CUTOFF_PARAM,    0.f, 1.f, 0.f, "Link Cutoff Frequencies", {"Independent", "Linked"});
        configSwitch(LINK_RESONANCE_PARAM, 0.f, 1.f, 0.f, "Link Resonance Amounts",  {"Independent", "Linked"});

        configParam(CUTOFF_A_ATTEN_PARAM,    -1.f, 1.f, 0.f, "Cutoff A CV Attenuverter",    "%", 0.f, 100.f);
        configParam(RESONANCE_A_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance A CV Attenuverter", "%", 0.f, 100.f);
        configParam(CUTOFF_B_ATTEN_PARAM,    -1.f, 1.f, 0.f, "Cutoff B CV Attenuverter",    "%", 0.f, 100.f);
        configParam(RESONANCE_B_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance B CV Attenuverter", "%", 0.f, 100.f);
        configParam(SHIFT_DEPTH_PARAM, 0.f, FREQ_SHIFT_MAX_HZ, 20.f, "Shift Depth", " Hz");

        configInput(AUDIO_A_INPUT,       "Audio A");
        configInput(AUDIO_B_INPUT,       "Audio B");
        configInput(CUTOFF_A_CV_INPUT,   "Filter A Cutoff CV");
        configInput(RESONANCE_A_CV_INPUT,"Filter A Resonance CV");
        configInput(CUTOFF_B_CV_INPUT,   "Filter B Cutoff CV");
        configInput(RESONANCE_B_CV_INPUT,"Filter B Resonance CV");
        configInput(CROSS_CV_INPUT,      "Freq Shift CV");
        configInput(MOD_CV_INPUT,        "Mod Depth CV");
        configInput(SHIMMER_CV_INPUT,    "Phaser CV");
        configInput(MOD_RATE_CV_INPUT,   "Mod Rate CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");

        configLight(CHAOS_LIGHT,      "Cross Amount");
        configLight(CHAOS_LIGHT_GREEN,"Mod Amount");
        configLight(CHAOS_LIGHT_BLUE, "Phaser Amount");
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
        float sr = DEFAULT_SAMPLE_RATE;
        if (APP && APP->engine) {
            sr = APP->engine->getSampleRate();
        }
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            filtersA[v].setSampleRate(sr);
            filtersB[v].setSampleRate(sr);
        }
    }

    static inline float cvToNormalized(float cv) {
        return cv * CV_NORMALIZED_SCALE;
    }

    static inline float applyUnitCv(float base, float cv) {
        return base + cvToNormalized(cv);
    }

    static inline float applyAttenuvertedCv(float base, float cv, float attenuverter) {
        return base + cvToNormalized(cv) * attenuverter;
    }

    static inline void syncLinkedParamPair(
        bool linked, bool& wasLinked,
        float& currentA, float& currentB,
        float& lastA, float& lastB,
        Param& paramA, Param& paramB) {

        if (!linked) {
            return;
        }
        if (!wasLinked) {
            paramB.setValue(currentA);
            currentB = currentA;
            return;
        }

        bool aChanged = std::abs(currentA - lastA) > LINK_PARAM_EPSILON;
        bool bChanged = std::abs(currentB - lastB) > LINK_PARAM_EPSILON;
        if (aChanged && !bChanged) {
            paramB.setValue(currentA);
            currentB = currentA;
        } else if (bChanged && !aChanged) {
            paramA.setValue(currentB);
            currentA = currentB;
        } else if (aChanged && bChanged) {
            paramB.setValue(currentA);
            currentB = currentA;
        }
    }

    void onReset() override {
        chaosGen.reset();
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            freqShiftersA[v].reset();
            freqShiftersB[v].reset();
            outputDC[v].reset();
            filtersA[v].reset();
            filtersB[v].reset();
            phasersA[v].reset();
            phasersB[v].reset();
        }
        onSampleRateChange();
    }

    void process(const ProcessArgs& args) override {
        // ====================================================================
        // PARAMETER READ + BIDIRECTIONAL LINKING
        // ====================================================================
        bool linkCutoff    = params[LINK_CUTOFF_PARAM].getValue()    > LINK_SWITCH_THRESHOLD;
        bool linkResonance = params[LINK_RESONANCE_PARAM].getValue() > LINK_SWITCH_THRESHOLD;

        float currentCutoffA    = params[CUTOFF_A_PARAM].getValue();
        float currentCutoffB    = params[CUTOFF_B_PARAM].getValue();
        float currentResonanceA = params[RESONANCE_A_PARAM].getValue();
        float currentResonanceB = params[RESONANCE_B_PARAM].getValue();

        syncLinkedParamPair(
            linkCutoff, lastLinkCutoff,
            currentCutoffA, currentCutoffB,
            lastCutoffA, lastCutoffB,
            params[CUTOFF_A_PARAM], params[CUTOFF_B_PARAM]);
        syncLinkedParamPair(
            linkResonance, lastLinkResonance,
            currentResonanceA, currentResonanceB,
            lastResonanceA, lastResonanceB,
            params[RESONANCE_A_PARAM], params[RESONANCE_B_PARAM]);
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

        bool hasCrossCv = inputs[CROSS_CV_INPUT].isConnected();
        bool hasModCv = inputs[MOD_CV_INPUT].isConnected();
        bool hasShimmerCv = inputs[SHIMMER_CV_INPUT].isConnected();
        bool hasModRateCv = inputs[MOD_RATE_CV_INPUT].isConnected();
        bool hasCutoffACv = inputs[CUTOFF_A_CV_INPUT].isConnected();
        bool hasCutoffBCv = inputs[CUTOFF_B_CV_INPUT].isConnected();
        bool hasResonanceACv = inputs[RESONANCE_A_CV_INPUT].isConnected();
        bool hasResonanceBCv = inputs[RESONANCE_B_CV_INPUT].isConnected();

        float cutoffAAtten = params[CUTOFF_A_ATTEN_PARAM].getValue();
        float cutoffBAtten = params[CUTOFF_B_ATTEN_PARAM].getValue();
        float resonanceAAtten = params[RESONANCE_A_ATTEN_PARAM].getValue();
        float resonanceBAtten = params[RESONANCE_B_ATTEN_PARAM].getValue();

        float crossTarget = params[CROSS_PARAM].getValue();
        if (hasCrossCv)
            crossTarget = clamp(applyUnitCv(crossTarget, inputs[CROSS_CV_INPUT].getVoltage()), 0.f, 1.f);
        crossAmount = crossSmooth.process(crossTarget, args.sampleTime, PARAM_SMOOTH_TC);

        float modTarget = params[MOD_PARAM].getValue();
        if (hasModCv)
            modTarget = clamp(applyUnitCv(modTarget, inputs[MOD_CV_INPUT].getVoltage()), 0.f, 1.f);
        modAmount = modSmooth.process(modTarget, args.sampleTime, PARAM_SMOOTH_TC);

        float shimTarget = params[SHIMMER_PARAM].getValue();
        if (hasShimmerCv)
            shimTarget = clamp(applyUnitCv(shimTarget, inputs[SHIMMER_CV_INPUT].getVoltage()), 0.f, 1.f);
        shimmerAmount = shimmerSmooth.process(shimTarget, args.sampleTime, PARAM_SMOOTH_TC);

        float shiftDepth = shiftDepthSmooth.process(params[SHIFT_DEPTH_PARAM].getValue(), args.sampleTime, PARAM_SMOOTH_TC);

        float modRateTarget = clamp(params[MOD_RATE_PARAM].getValue(), MOD_RATE_MIN_HZ, MOD_RATE_MAX_HZ);
        if (hasModRateCv)
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
        // VISUALIZER DATA (sampled from voice 0 / global smoothers)
        // ====================================================================
        {
            float dCutoffA = cutoffA, dCutoffB = cutoffB;
            float dResA = resonanceA, dResB = resonanceB;
            if (hasCutoffACv)
                dCutoffA = clamp(applyAttenuvertedCv(dCutoffA, inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(0), cutoffAAtten), 0.f, 1.f);
            if (hasCutoffBCv)
                dCutoffB = clamp(applyAttenuvertedCv(dCutoffB, inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(0), cutoffBAtten), 0.f, 1.f);
            if (hasResonanceACv)
                dResA = clamp(applyAttenuvertedCv(dResA, inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(0), resonanceAAtten),
                              LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
            if (hasResonanceBCv)
                dResB = clamp(applyAttenuvertedCv(dResB, inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(0), resonanceBAtten),
                              LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
            effectiveCutoffA    = dCutoffA;
            effectiveCutoffB    = dCutoffB;
            effectiveResonanceA = dResA;
            effectiveResonanceB = dResB;
        }

        // ====================================================================
        // POLYPHONIC AUDIO PROCESSING
        // ====================================================================
        bool hasAudioA = inputs[AUDIO_A_INPUT].isConnected();
        bool hasAudioB = inputs[AUDIO_B_INPUT].isConnected();
        int channelsA = inputs[AUDIO_A_INPUT].getChannels();
        int channelsB = inputs[AUDIO_B_INPUT].getChannels();
        int channels  = std::min(std::max(channelsA, channelsB),
                                 shapetaker::PolyphonicProcessor::MAX_VOICES);

        if (!hasAudioA && !hasAudioB) {
            outputs[AUDIO_A_OUTPUT].setChannels(0);
            outputs[AUDIO_B_OUTPUT].setChannels(0);
        } else {
            outputs[AUDIO_A_OUTPUT].setChannels(channels);
            outputs[AUDIO_B_OUTPUT].setChannels(channels);

            for (int c = 0; c < channels; c++) {
                // --- Input ---
                float audioA = 0.f, audioB = 0.f;
                if (hasAudioA && hasAudioB) {
                    audioA = inputs[AUDIO_A_INPUT].getVoltage(c);
                    audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                } else if (hasAudioA) {
                    audioA = audioB = inputs[AUDIO_A_INPUT].getVoltage(c);
                } else {
                    audioA = audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                }
                if (!std::isfinite(audioA)) audioA = 0.f;
                if (!std::isfinite(audioB)) audioB = 0.f;


                // --- Per-voice cutoff with CV and chaos modulation ---
                float voiceCutoffA = cutoffA;
                float voiceCutoffB = cutoffB;
                float voiceResA    = resonanceA;
                float voiceResB    = resonanceB;

                if (hasCutoffACv) {
                    voiceCutoffA = applyAttenuvertedCv(voiceCutoffA, inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(c), cutoffAAtten);
                }
                voiceCutoffA = clamp(voiceCutoffA + chaosOut * CHAOS_CUTOFF_RANGE, 0.f, 1.f);
                voiceCutoffA = cutoffASmoothers[c].process(voiceCutoffA, args.sampleTime, CUTOFF_CV_SMOOTH_TC);

                if (hasCutoffBCv) {
                    voiceCutoffB = applyAttenuvertedCv(voiceCutoffB, inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(c), cutoffBAtten);
                }
                voiceCutoffB = clamp(voiceCutoffB + chaosOut * CHAOS_CUTOFF_RANGE, 0.f, 1.f);
                voiceCutoffB = cutoffBSmoothers[c].process(voiceCutoffB, args.sampleTime, CUTOFF_CV_SMOOTH_TC);

                if (hasResonanceACv) {
                    voiceResA = clamp(applyAttenuvertedCv(voiceResA, inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(c), resonanceAAtten),
                                      LiquidFilter::RESONANCE_MIN, LiquidFilter::RESONANCE_MAX);
                }
                if (hasResonanceBCv) {
                    voiceResB = clamp(applyAttenuvertedCv(voiceResB, inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(c), resonanceBAtten),
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

                // --- Stereo frequency shifter (post-filter, pre-phaser) ---
                // A shifts up, B shifts down — spectral content counter-rotates across
                // the stereo field. Slow beating at low amounts; inharmonic shimmer at high.
                if (crossAmount > EFFECT_THRESHOLD) {
                    float shiftHz  = crossAmount * shiftDepth;
                    float shiftedA = freqShiftersA[c].process(processedA,  shiftHz, args.sampleRate);
                    float shiftedB = freqShiftersB[c].process(processedB, -shiftHz, args.sampleRate);
                    processedA = rack::math::crossfade(processedA, shiftedA, crossAmount);
                    processedB = rack::math::crossfade(processedB, shiftedB, crossAmount);
                }

                // --- Stereo resonant APF phaser (post-filter, swept by chaos LFO) ---
                // Channels A and B sweep in opposite directions so the phase notches
                // whirl across the stereo field — notches rising in A while falling in B.
                if (shimmerAmount > EFFECT_THRESHOLD) {
                    float spread   = chaosOut * PHASER_CHAOS_SPREAD;
                    float centerA  = clamp(PHASER_CENTER_HZ * std::pow(2.f,  spread),
                                          PHASER_MIN_CENTER_HZ, args.sampleRate * NYQUIST_HEADROOM_RATIO);
                    float centerB  = clamp(PHASER_CENTER_HZ * std::pow(2.f, -spread),
                                          PHASER_MIN_CENTER_HZ, args.sampleRate * NYQUIST_HEADROOM_RATIO);
                    float fb       = shimmerAmount * PHASER_MAX_FEEDBACK;
                    float apfCA    = shapetaker::involution::AllpassPhaser::apfCoeff(centerA, args.sampleRate);
                    float apfCB    = shapetaker::involution::AllpassPhaser::apfCoeff(centerB, args.sampleRate);
                    float phasedA  = phasersA[c].process(processedA, apfCA, fb) * PHASER_LEVEL_COMP;
                    float phasedB  = phasersB[c].process(processedB, apfCB, fb) * PHASER_LEVEL_COMP;
                    processedA = rack::math::crossfade(processedA, phasedA, shimmerAmount);
                    processedB = rack::math::crossfade(processedB, phasedB, shimmerAmount);
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
        lights[CHAOS_LIGHT].setBrightness(clamp(crossAmount, 0.f, 1.f));
        lights[CHAOS_LIGHT_GREEN].setBrightness(clamp(modAmount, 0.f, 1.f));
        lights[CHAOS_LIGHT_BLUE].setBrightness(clamp(shimmerAmount, 0.f, 1.f));
        lights[CHAOS_RATE_LIGHT].setBrightness(clamp(
            rack::math::rescale(modRate, MOD_RATE_MIN_HZ, MOD_RATE_MAX_HZ, 0.f, 1.f), 0.f, 1.f));
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
        params[CROSS_PARAM].setValue(charDist(rng));
        params[MOD_PARAM].setValue(charDist(rng));
        params[SHIMMER_PARAM].setValue(charDist(rng));
        params[MOD_RATE_PARAM].setValue(rateDist(rng));
        params[LINK_CUTOFF_PARAM].setValue((float)linkDist(rng));
        params[LINK_RESONANCE_PARAM].setValue((float)linkDist(rng));
        params[SHIFT_DEPTH_PARAM].setValue(charDist(rng) * FREQ_SHIFT_MAX_HZ);
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

        // ---- Shift Depth (max Hz for frequency shifter) ----
        addKnobWithShadow(createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("filter_morph", 45.166f, 108.088f), module, Involution::SHIFT_DEPTH_PARAM));

        // ---- Mod Rate (attenuverter-style small knob) ----
        addKnobWithShadow(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("chaos_rate_knob", 60.922f, 108.088f), module, Involution::MOD_RATE_PARAM));

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
