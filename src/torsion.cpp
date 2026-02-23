#include "plugin.hpp"
#include "dsp/audio.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace {
    constexpr float OUTPUT_SCALE = 5.f;
    constexpr float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    // 1-pole high-pass DC blocker: y[n] = x[n] - x[n-1] + coeff * y[n-1]
    // coeff = 1 - 2π * fc * T, cached per sample rate for a fixed ~20 Hz cutoff
    struct DcBlocker {
        float x1 = 0.f, y1 = 0.f;
        float process(float x, float coeff) {
            float y = x - x1 + coeff * y1;
            x1 = x; y1 = y;
            return y;
        }
        void reset() { x1 = y1 = 0.f; }
    };

    float softWarpAmount(float amount) {
        // Gentle knee near zero to avoid audible "kick" when torsion leaves 0%
        constexpr float knee = 0.04f;  // 4% knee
        if (amount <= knee) {
            return (amount * amount) / knee;  // quadratic fade-in, continuous at knee
        }
        return amount;
    }

    enum class CZWarpShape {
        Single,
        Resonant,
        Double,
        SawPulse,
        Pulse,
        Count
    };

    float shapeBias(float symmetry, float amount) {
        symmetry = rack::math::clamp(symmetry, 0.f, 1.f);
        amount = rack::math::clamp(amount, 0.f, 1.f);
        float centered = symmetry - 0.5f;
        return centered * (0.35f + 0.25f * amount);
    }

    constexpr int kFastPowTSize = 256;
    constexpr int kFastPowExpSize = 128;
    constexpr float kFastPowExpMin = 0.05f;
    constexpr float kFastPowExpMax = 5.f;
    static bool gFastPowLookupInitialized = false;
    static float gFastPowLookupTable[kFastPowExpSize][kFastPowTSize] = {};

    inline void initializeFastPowLookupTable() {
        if (gFastPowLookupInitialized) {
            return;
        }

        for (int ei = 0; ei < kFastPowExpSize; ++ei) {
            float expVal = kFastPowExpMin + (kFastPowExpMax - kFastPowExpMin) * ((float)ei / (kFastPowExpSize - 1));
            for (int ti = 0; ti < kFastPowTSize; ++ti) {
                float tVal = (float)ti / (kFastPowTSize - 1);
                gFastPowLookupTable[ei][ti] = std::pow(tVal, expVal);
            }
        }
        gFastPowLookupInitialized = true;
    }

    // Precomputed pow() table for curve/wave shaping to avoid repeated std::pow calls
    inline float fastPowLookup(float t, float exponent) {
        constexpr int kTSize = kFastPowTSize;
        constexpr int kExpSize = kFastPowExpSize;
        constexpr float kExpMin = kFastPowExpMin;
        constexpr float kExpMax = kFastPowExpMax;

        t = rack::math::clamp(t, 0.f, 1.f);
        exponent = rack::math::clamp(exponent, kExpMin, kExpMax);

        if (t <= 0.f) {
            return 0.f;
        }
        if (t >= 1.f) {
            return 1.f;
        }

        if (!gFastPowLookupInitialized) {
            initializeFastPowLookupTable();
        }

        float ePos = (exponent - kExpMin) * (kExpSize - 1) / (kExpMax - kExpMin);
        int eIdx = rack::math::clamp((int)ePos, 0, kExpSize - 2);
        float eFrac = ePos - (float)eIdx;

        float tPos = t * (kTSize - 1);
        int tIdx = rack::math::clamp((int)tPos, 0, kTSize - 2);
        float tFrac = tPos - (float)tIdx;

        float v00 = gFastPowLookupTable[eIdx][tIdx];
        float v01 = gFastPowLookupTable[eIdx][tIdx + 1];
        float v10 = gFastPowLookupTable[eIdx + 1][tIdx];
        float v11 = gFastPowLookupTable[eIdx + 1][tIdx + 1];

        float v0 = rack::math::crossfade(v00, v01, tFrac);
        float v1 = rack::math::crossfade(v10, v11, tFrac);
        return rack::math::crossfade(v0, v1, eFrac);
    }

    inline void fastSinCos2Pi(float phase, float& s, float& c) {
        float theta = 2.f * float(M_PI) * phase;
#if defined(__APPLE__) && !defined(__GLIBC__)
        __sincosf(theta, &s, &c);
#elif defined(__GNUC__) || defined(__clang__)
        ::sincosf(theta, &s, &c);
#else
        s = std::sinf(theta);
        c = std::cosf(theta);
#endif
    }

    float warpSegment(float phase, float breakPoint, float attackCurve, float releaseCurve) {
        const float minBreak = 0.02f;
        const float maxBreak = 0.98f;
        breakPoint = rack::math::clamp(breakPoint, minBreak, maxBreak);
        attackCurve = rack::math::clamp(attackCurve, 0.05f, 4.f);
        releaseCurve = rack::math::clamp(releaseCurve, 0.05f, 4.f);

        if (phase < breakPoint) {
            float t = breakPoint <= 0.f ? 0.f : phase / breakPoint;
            t = rack::math::clamp(t, 0.f, 1.f);
            return 0.5f * fastPowLookup(t, attackCurve);
        }

        float denom = 1.f - breakPoint;
        float t = denom <= 0.f ? 1.f : (phase - breakPoint) / denom;
        t = rack::math::clamp(t, 0.f, 1.f);
        return 0.5f + 0.5f * (1.f - fastPowLookup(1.f - t, releaseCurve));
    }

    float computeBreakPoint(float amount, float bias) {
        float baseBreak = rack::math::clamp(1.f - amount * 0.98f, 0.02f, 0.98f);
        return rack::math::clamp(baseBreak + bias, 0.02f, 0.98f);
    }

    float applyCZWarp(float phase, float amount, float bias, CZWarpShape shape) {
        amount = rack::math::clamp(amount, 0.f, 1.f);
        phase = rack::math::eucMod(phase, 1.f);
        if (amount <= 1e-5f) {
            return phase;
        }

        float breakPoint = computeBreakPoint(amount, bias);

        switch (shape) {
            case CZWarpShape::Single:
                return warpSegment(phase, breakPoint, 1.f, 1.f);
            case CZWarpShape::Resonant: {
                float attackCurve = lerp(1.f, 0.22f, amount);
                float releaseCurve = lerp(1.f, 2.8f, amount);
                return warpSegment(phase, breakPoint, attackCurve, releaseCurve);
            }
            case CZWarpShape::Double: {
                float localPhase = (phase < 0.5f) ? phase * 2.f : (phase - 0.5f) * 2.f;
                float localBreak = rack::math::clamp(breakPoint * lerp(0.9f, 0.55f, amount), 0.02f, 0.98f);
                float warped = warpSegment(localPhase, localBreak, 1.f, 1.f);
                return (phase < 0.5f) ? warped * 0.5f : 0.5f + warped * 0.5f;
            }
            case CZWarpShape::SawPulse: {
                float sawBreak = rack::math::clamp(breakPoint * lerp(0.85f, 0.5f, amount), 0.02f, 0.95f);
                float attackCurve = lerp(1.f, 0.4f, amount);
                float releaseCurve = lerp(1.f, 0.2f, amount);
                return warpSegment(phase, sawBreak, attackCurve, releaseCurve);
            }
            case CZWarpShape::Pulse: {
                float pulseBreak = rack::math::clamp(breakPoint * lerp(0.7f, 0.18f, amount), 0.02f, 0.9f);
                float attackCurve = lerp(1.f, 0.6f, amount);
                float releaseCurve = lerp(1.f, 2.2f, amount);
                return warpSegment(phase, pulseBreak, attackCurve, releaseCurve);
            }
            default:
                break;
        }
        return phase;
    }

}

struct Torsion : Module {
    enum InteractionMode {
        INTERACTION_NONE,
        INTERACTION_RESET_SYNC,
        INTERACTION_DCW_FOLLOW,
        INTERACTION_RING_MOD,
        INTERACTION_MODES_LEN
    };

    enum ParamId {
        COARSE_PARAM,
        DETUNE_PARAM,
        TORSION_PARAM,
        SYMMETRY_PARAM,
        TORSION_ATTEN_PARAM,
        WARP_SHAPE_PARAM,
        STAGE_RATE_PARAM,
        STAGE_TIME_PARAM,
        STAGE1_PARAM,
        STAGE2_PARAM,
        STAGE3_PARAM,
        STAGE4_PARAM,
        STAGE5_PARAM,
        STAGE6_PARAM,
        FEEDBACK_PARAM,
        FEEDBACK_ATTEN_PARAM,
        SAW_WAVE_PARAM,
        TRIANGLE_WAVE_PARAM,
        SQUARE_WAVE_PARAM,
        DIRTY_MODE_PARAM,
        SUB_LEVEL_PARAM,
        SUB_SYNC_PARAM,
        CHORUS_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        VOCT_INPUT,
        GATE_INPUT,
        TORSION_CV_INPUT,
        FEEDBACK_CV_INPUT,
        STAGE_TRIG_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        MAIN_L_OUTPUT,
        MAIN_R_OUTPUT,
        EDGE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        STAGE_LIGHT_1,
        STAGE_LIGHT_2,
        STAGE_LIGHT_3,
        STAGE_LIGHT_4,
        STAGE_LIGHT_5,
        STAGE_LIGHT_6,
        LIGHTS_LEN
    };

    shapetaker::PolyphonicProcessor polyProcessor;
    shapetaker::dsp::VoiceArray<float> primaryPhase;
    shapetaker::dsp::VoiceArray<float> secondaryPhase;
    shapetaker::dsp::VoiceArray<float> subPhase;
    shapetaker::dsp::VoiceArray<float> feedbackSignal;
    shapetaker::dsp::VoiceArray<float> torsionSlew;
    shapetaker::dsp::VoiceArray<float> trigTail;

    shapetaker::dsp::VoiceArray<float> stagePositions;
    shapetaker::dsp::VoiceArray<bool> stageActive;
    shapetaker::dsp::VoiceArray<float> stageEnvelope;
    shapetaker::dsp::VoiceArray<rack::dsp::SchmittTrigger> stageTriggers;
    shapetaker::dsp::VoiceArray<bool> gateHeld;

    shapetaker::dsp::VoiceArray<float> vintageDrift;
    shapetaker::dsp::VoiceArray<float> vintageDriftTimer;
    shapetaker::dsp::VoiceArray<float> velocityHold;

    static constexpr int kChorusMaxDelaySamples = 4096;
    static constexpr float kChorusBaseDelayMs = 14.f;
    static constexpr float kChorusDepthMs = 4.2f;
    static constexpr float kChorusRateHz = 0.42f;
    static constexpr float kChorusMix = 0.35f;
    static constexpr float kChorusCrossMix = 0.25f;

    struct ChorusVoiceState {
        shapetaker::dsp::AudioProcessor::DelayLine<kChorusMaxDelaySamples> delayL;
        shapetaker::dsp::AudioProcessor::DelayLine<kChorusMaxDelaySamples> delayR;
        float phase = 0.f;

        void reset() {
            delayL.clear();
            delayR.clear();
            phase = 0.f;
        }
    };
    shapetaker::dsp::VoiceArray<ChorusVoiceState> chorusVoices;

    // DC blocking filters for clean output (prevents clicks/pops)
    shapetaker::dsp::VoiceArray<DcBlocker> dcBlockers;

    // Click suppression fade-out ramp for smooth envelope endings
    shapetaker::dsp::VoiceArray<float> clickSuppressor;  // 1.0 = normal, 0.0 = fully faded

    InteractionMode interactionMode = INTERACTION_NONE;
    bool vintageMode = false;
    bool dcwKeyTrackEnabled = false;
    bool dcwVelocityEnabled = false;
    bool chorusEnabled = false;
    bool phaseResetEnabled = false;
    static constexpr int kNumStages = 6;
    static constexpr float kStageRateBase = 10.f;
    float vintageClockPhase = 0.f;

    static constexpr float kVintageHissLevel = 0.00075f;
    static constexpr float kVintageClockLevel = 0.0005f;
    static constexpr float kVintageClockFreq = 9000.f;  // Hz
    static constexpr float kVintageDriftRange = 0.0045f; // +/- range in octaves (~5.5 cents)
    static constexpr float kVintageDriftHoldMin = 0.18f;
    static constexpr float kVintageDriftHoldMax = 0.45f;
    static constexpr float kVintageIdleHissLevel = 0.00035f;

    // Cached exponential coefficients (computed once per sample rate change)
    float cachedDcBlockCoeff = 0.f;
    float cachedSlewCoeff = 0.f;
    float cachedSuppressorDecay = 0.f;
    float cachedSuppressorRise = 0.f;
    float cachedSuppressorEndDecay = 0.f;
    float cachedReleaseCoeffBase = 0.f;
    float cachedTorsionSlewCoeff = 0.f;
    float cachedTrigTailCoeff = 0.f;
    float cachedInactiveDecay = 0.f;

    // Cached polyphony compensation (updated when channel count changes)
    float cachedPolyComp = 1.f;
    int cachedChannelCount = 0;

    // Release coefficient lookup table (pre-computed for common rates)
    static constexpr int kReleaseTableSize = 64;
    float releaseCoeffTable[kReleaseTableSize] = {};

    // Chorus LFO decimation
    int chorusLfoCounter = 0;
    static constexpr int kChorusLfoDecimation = 16;  // Update every 16 samples
    float chorusModALast = 0.f;
    float chorusModBLast = 0.f;

    // Vintage mode noise optimization - pre-generated noise samples
    static constexpr int kVintageNoiseBufferSize = 8192;
    float vintageNoiseBuffer[kVintageNoiseBufferSize] = {};
    int vintageNoiseIndex = 0;

    Torsion() {
        initializeFastPowLookupTable();
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(COARSE_PARAM, -2.f, 2.f, 0.f, "octave", " oct");
        if (auto* quantity = paramQuantities[COARSE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        configParam(DETUNE_PARAM, 0.f, 20.f, 0.f, "detune", " cents");

        shapetaker::ParameterHelper::configGain(this, TORSION_PARAM, "torsion depth", 0.0f);
        shapetaker::ParameterHelper::configGain(this, SYMMETRY_PARAM, "symmetry warp", 0.0f);

        shapetaker::ParameterHelper::configAttenuverter(this, TORSION_ATTEN_PARAM, "torsion cv");

        configSwitch(WARP_SHAPE_PARAM, 0.f, (float)((int)CZWarpShape::Count - 1), 0.f, "warp shape",
            {"single sine", "resonant", "double sine", "saw pulse", "pulse"});
        if (auto* quantity = paramQuantities[WARP_SHAPE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        // Stage envelope controls
        configParam(STAGE_RATE_PARAM, -0.9f, 2.f, 0.f, "stage time scale", "%", 100.f);

        configParam(STAGE_TIME_PARAM, 0.f, 0.f, 0.f, "stage time scale (legacy)");

        // Stage levels for DCW envelope - ADSR-like shape by default
        shapetaker::ParameterHelper::configGain(this, STAGE1_PARAM, "stage 1 level", 1.0f);
        shapetaker::ParameterHelper::configGain(this, STAGE2_PARAM, "stage 2 level", 1.0f);
        shapetaker::ParameterHelper::configGain(this, STAGE3_PARAM, "stage 3 level", 0.5f);
        shapetaker::ParameterHelper::configGain(this, STAGE4_PARAM, "stage 4 level", 0.5f);
        shapetaker::ParameterHelper::configGain(this, STAGE5_PARAM, "stage 5 level", 0.0f);
        shapetaker::ParameterHelper::configGain(this, STAGE6_PARAM, "stage 6 level", 0.0f);

        shapetaker::ParameterHelper::configGain(this, FEEDBACK_PARAM, "feedback amount", 0.0f);
        shapetaker::ParameterHelper::configAttenuverter(this, FEEDBACK_ATTEN_PARAM, "feedback cv");

        configParam(SAW_WAVE_PARAM, 0.f, 1.f, 0.f, "sawtooth wave");
        configParam(TRIANGLE_WAVE_PARAM, 0.f, 1.f, 0.f, "triangle wave");
        configParam(SQUARE_WAVE_PARAM, 0.f, 1.f, 0.f, "square wave");
        configSwitch(DIRTY_MODE_PARAM, 0.f, 1.f, 0.f, "saturation mode", {"clean", "dirty"});
        if (auto* quantity = paramQuantities[DIRTY_MODE_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }
        configSwitch(CHORUS_PARAM, 0.f, 1.f, 0.f, "chorus", {"off", "on"});
        if (auto* quantity = paramQuantities[CHORUS_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        // Sub oscillator with extended range for powerful bass
        configParam(SUB_LEVEL_PARAM, 0.f, 2.0f, 0.0f, "sub osc level", "%", 0.f, 100.f);
        configSwitch(SUB_SYNC_PARAM, 0.f, 3.f, 0.f, "oscillator interaction",
            {"independent", "sync B to A resets", "B DCW follows A", "ring mod mix"});
        if (auto* quantity = paramQuantities[SUB_SYNC_PARAM]) {
            quantity->snapEnabled = true;
            quantity->smoothEnabled = false;
        }

        shapetaker::ParameterHelper::configCVInput(this, VOCT_INPUT, "v/oct");
        shapetaker::ParameterHelper::configGateInput(this, GATE_INPUT, "dcw gate");
        shapetaker::ParameterHelper::configCVInput(this, TORSION_CV_INPUT, "torsion cv");
        shapetaker::ParameterHelper::configCVInput(this, FEEDBACK_CV_INPUT, "feedback cv");
        shapetaker::ParameterHelper::configGateInput(this, STAGE_TRIG_INPUT, "dcw trigger");

        shapetaker::ParameterHelper::configAudioOutput(this, MAIN_L_OUTPUT, "L");
        shapetaker::ParameterHelper::configAudioOutput(this, MAIN_R_OUTPUT, "R");
        shapetaker::ParameterHelper::configAudioOutput(this, EDGE_OUTPUT, "edge difference");

        velocityHold.forEach([](float& v) { v = 1.f; });
        resetChorusState();

        // Pre-generate vintage noise buffer for performance
        for (int i = 0; i < kVintageNoiseBufferSize; ++i) {
            vintageNoiseBuffer[i] = rack::random::uniform() * 2.f - 1.f;
        }

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void resetChorusState() {
        chorusVoices.forEach([](ChorusVoiceState& voice) {
            voice.reset();
        });
    }

    void updateCachedCoefficients(float sampleTime) {
        // DC blocker: fixed 20 Hz cutoff regardless of sample rate
        cachedDcBlockCoeff = 1.f - (2.f * float(M_PI) * 20.f * sampleTime);
        // Cache exponential coefficients to avoid recomputing every sample
        cachedSlewCoeff = std::exp(-sampleTime * 500.f);   // ~2 ms attack smoothing
        cachedSuppressorDecay = std::exp(-sampleTime * 100.f);
        cachedSuppressorEndDecay = std::exp(-sampleTime * 800.f); // ~2 ms tail for end-of-seq
        cachedSuppressorRise = std::exp(-sampleTime * 700.f);   // ~1.4 ms fade-in
        cachedTrigTailCoeff = std::exp(-sampleTime * 400.f);    // ~2.5 ms post-stage tail for trig mode
        cachedInactiveDecay = std::exp(-sampleTime * 40.f);     // ~25 ms decay tail for inactive trig mode
        cachedReleaseCoeffBase = sampleTime;  // Will multiply by rate later
        cachedTorsionSlewCoeff = std::exp(-sampleTime * 400.f);  // ~2.5 ms torsion CV smoothing

        // Pre-compute release coefficient lookup table for rates 22-80
        // This covers the range used in the release calculation
        for (int i = 0; i < kReleaseTableSize; ++i) {
            float rate = 22.f + (58.f / (kReleaseTableSize - 1)) * i;  // Linear from 22 to 80
            releaseCoeffTable[i] = std::exp(-sampleTime * rate);
        }
    }

    void onReset() override {
        primaryPhase.reset();
        secondaryPhase.reset();
        subPhase.reset();
        feedbackSignal.reset();
        stagePositions.reset();
        stageActive.reset();
        stageEnvelope.reset();
        dcBlockers.forEach([](DcBlocker& db) { db.reset(); });
        clickSuppressor.reset();
        for (int i = 0; i < 16; i++) {
            clickSuppressor[i] = 1.0f;  // Start fully active
        }
        gateHeld.reset();
        stageTriggers.reset();
        vintageDrift.reset();
        vintageDriftTimer.reset();
        velocityHold.forEach([](float& v) { v = 1.f; });
        trigTail.reset();
        phaseResetEnabled = false;
        interactionMode = INTERACTION_NONE;
        vintageMode = false;
        dcwKeyTrackEnabled = false;
        dcwVelocityEnabled = false;
        chorusEnabled = false;
        params[CHORUS_PARAM].setValue(0.f);
        vintageClockPhase = 0.f;
        resetChorusState();
        torsionSlew.reset();
        for (int i = 0; i < LIGHTS_LEN; ++i) {
            lights[i].setBrightness(0.f);
        }
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        updateCachedCoefficients(e.sampleTime);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        int modeFromSwitch = rack::math::clamp(
            (int)std::round(params[SUB_SYNC_PARAM].getValue()), 0, INTERACTION_MODES_LEN - 1);
        interactionMode = (InteractionMode)modeFromSwitch;
        json_object_set_new(rootJ, "interactionMode", json_integer(modeFromSwitch));
        json_object_set_new(rootJ, "vintageMode", json_boolean(vintageMode));
        json_object_set_new(rootJ, "dcwKeyTrackEnabled", json_boolean(dcwKeyTrackEnabled));
        json_object_set_new(rootJ, "dcwVelocityEnabled", json_boolean(dcwVelocityEnabled));
        chorusEnabled = params[CHORUS_PARAM].getValue() > 0.5f;
        json_object_set_new(rootJ, "chorusEnabled", json_boolean(chorusEnabled));
        json_object_set_new(rootJ, "phaseResetEnabled", json_boolean(phaseResetEnabled));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        if (!rootJ) {
            return;
        }

        if (json_t* paramsJ = json_object_get(rootJ, "params")) {
            if (json_is_array(paramsJ)) {
                if (json_t* stageRateJ = json_array_get(paramsJ, STAGE_RATE_PARAM)) {
                    if (json_t* valueJ = json_object_get(stageRateJ, "value")) {
                        if (json_is_number(valueJ)) {
                            float storedValue = json_number_value(valueJ);
                            if (storedValue > 2.f || storedValue < -0.9f) {
                                float converted = (storedValue - kStageRateBase) / kStageRateBase;
                                params[STAGE_RATE_PARAM].setValue(
                                    rack::math::clamp(converted, -0.9f, 2.f));
                            }
                        }
                    }
                }
            }
        }

        json_t* modeJ = json_object_get(rootJ, "interactionMode");
        if (modeJ) {
            interactionMode = (InteractionMode)json_integer_value(modeJ);
            interactionMode = (InteractionMode)rack::math::clamp(
                (int)interactionMode, 0, INTERACTION_MODES_LEN - 1);
            params[SUB_SYNC_PARAM].setValue((float)interactionMode);
        }
        json_t* vintageJ = json_object_get(rootJ, "vintageMode");
        if (vintageJ) {
            vintageMode = json_is_true(vintageJ);
        }
        json_t* trackJ = json_object_get(rootJ, "dcwKeyTrackEnabled");
        if (trackJ) {
            dcwKeyTrackEnabled = json_is_true(trackJ);
        }
        json_t* velocityJ = json_object_get(rootJ, "dcwVelocityEnabled");
        if (velocityJ) {
            dcwVelocityEnabled = json_is_true(velocityJ);
        }
        json_t* chorusJ = json_object_get(rootJ, "chorusEnabled");
        if (chorusJ) {
            chorusEnabled = json_is_true(chorusJ);
            params[CHORUS_PARAM].setValue(chorusEnabled ? 1.f : 0.f);
        }
        json_t* phaseResetJ = json_object_get(rootJ, "phaseResetEnabled");
        if (phaseResetJ) {
            phaseResetEnabled = json_is_true(phaseResetJ);
        }
        chorusEnabled = params[CHORUS_PARAM].getValue() > 0.5f;
        resetChorusState();
    }

    void process(const ProcessArgs& args) override {
        int channels = polyProcessor.updateChannels(
            {inputs[VOCT_INPUT], inputs[GATE_INPUT], inputs[TORSION_CV_INPUT], inputs[FEEDBACK_CV_INPUT], inputs[STAGE_TRIG_INPUT]},
            {outputs[MAIN_L_OUTPUT], outputs[MAIN_R_OUTPUT], outputs[EDGE_OUTPUT]});

        bool chorusParamOn = params[CHORUS_PARAM].getValue() > 0.5f;
        if (chorusEnabled != chorusParamOn) {
            chorusEnabled = chorusParamOn;
            resetChorusState();
        }

        float coarse = params[COARSE_PARAM].getValue();
        float detuneCents = params[DETUNE_PARAM].getValue();
        float detuneOct = detuneCents / 1200.f;

        float torsionBase = params[TORSION_PARAM].getValue();
        float symmetryBase = params[SYMMETRY_PARAM].getValue();
        float torsionAtten = params[TORSION_ATTEN_PARAM].getValue();

        float stageTimeScale = params[STAGE_RATE_PARAM].getValue();
        float rate = kStageRateBase * rack::math::clamp(1.f + stageTimeScale, 0.01f, 4.f);
        float stageLevels[kNumStages] = {
            params[STAGE1_PARAM].getValue(),
            params[STAGE2_PARAM].getValue(),
            params[STAGE3_PARAM].getValue(),
            params[STAGE4_PARAM].getValue(),
            params[STAGE5_PARAM].getValue(),
            params[STAGE6_PARAM].getValue()
        };
        CZWarpShape warpShape = (CZWarpShape)rack::math::clamp(
            (int)params[WARP_SHAPE_PARAM].getValue(), 0, (int)CZWarpShape::Count - 1);
        bool useSaw = params[SAW_WAVE_PARAM].getValue() > 0.5f;
        bool useTriangle = params[TRIANGLE_WAVE_PARAM].getValue() > 0.5f;
        bool useSquare = params[SQUARE_WAVE_PARAM].getValue() > 0.5f;
        bool dirtyMode = params[DIRTY_MODE_PARAM].getValue() > 0.5f;

        // Move these param reads outside the voice loop (optimization #2)
        float feedbackBase = params[FEEDBACK_PARAM].getValue();
        float feedbackAtten = params[FEEDBACK_ATTEN_PARAM].getValue();
        bool feedbackCvConnected = inputs[FEEDBACK_CV_INPUT].isConnected();
        bool voctConnected = inputs[VOCT_INPUT].isConnected();
        bool torsionCvConnected = inputs[TORSION_CV_INPUT].isConnected();
        float subLevel = params[SUB_LEVEL_PARAM].getValue();
        InteractionMode activeInteraction = (InteractionMode)rack::math::clamp(
            (int)std::round(params[SUB_SYNC_PARAM].getValue()), 0, INTERACTION_MODES_LEN - 1);
        interactionMode = activeInteraction;

        // Check if any waveforms are enabled for conditional generation (optimization #1)
        bool anyWaveformEnabled = useSaw || useTriangle || useSquare;

        // Pre-compute harmonic coefficients as compile-time constants
        constexpr float kHarmonic3Inv = 1.f / 3.f;
        constexpr float kHarmonic5Inv = 1.f / 5.f;

        // Hoist waveform building lambda outside voice loop for better performance
        auto buildWarpedVoice = [&](float warpedPhase, float amount) -> float {
            // Early exit if no waveforms enabled - just return sine wave
            if (!anyWaveformEnabled) {
                float s, c;
                fastSinCos2Pi(warpedPhase, s, c);
                float loudness = 1.f + amount * 1.2f;
                return rack::math::clamp(s * loudness, -3.f, 3.f);
            }

            // Calculate sin/cos together to reduce trig calls
            float sin1 = 0.f;
            float cos1 = 0.f;
            fastSinCos2Pi(warpedPhase, sin1, cos1);

            float voice = 0.f;
            int layers = 0;

            float sin2 = 0.f;
            float cos2 = 0.f;
            float sin3 = 0.f;
            float sin5 = 0.f;

            if (useSaw || useSquare || useTriangle) {
                sin2 = 2.f * sin1 * cos1;
                cos2 = cos1 * cos1 - sin1 * sin1;
            }

            if (useSquare || useTriangle) {
                sin3 = sin2 * cos1 + cos2 * sin1;
            }

            if (useSquare) {
                float sin4 = 2.f * sin2 * cos2;
                float cos4 = cos2 * cos2 - sin2 * sin2;
                sin5 = sin4 * cos1 + cos4 * sin1;
            }

            if (useSaw) {
                voice += sin1 + 0.5f * sin2;
                layers++;
            }
            if (useTriangle) {
                // Bandlimited triangle: (8/π²)(sin1 - sin3/9)
                // Matches the aliasing character of the other waveforms
                constexpr float kTriScale = 8.f / (float(M_PI) * float(M_PI));
                voice += kTriScale * (sin1 - sin3 * kHarmonic3Inv * kHarmonic3Inv);
                layers++;
            }
            if (useSquare) {
                voice += sin1 + kHarmonic3Inv * sin3 + kHarmonic5Inv * sin5;
                layers++;
            }

            if (layers == 0) {
                voice = sin1;
            } else {
                voice /= (float)layers;
            }

            float loudness = 1.f + amount * 1.2f;
            return rack::math::clamp(voice * loudness, -3.f, 3.f);
        };

        float clockSignal = 0.f;
        if (vintageMode) {
            vintageClockPhase += args.sampleTime * kVintageClockFreq;
            if (vintageClockPhase >= 1.f) {
                vintageClockPhase -= std::floor(vintageClockPhase);
            }
            float clockSin, clockCos;
            fastSinCos2Pi(vintageClockPhase, clockSin, clockCos);
            clockSignal = clockSin * kVintageClockLevel;
        }

        // Cache polyphony compensation - only recalculate when channel count changes
        if (channels != cachedChannelCount) {
            cachedChannelCount = channels;
            cachedPolyComp = (channels > 1) ? (1.f / std::sqrt((float)channels)) : 1.f;
        }
        float polyComp = cachedPolyComp;
        float noiseComp = rack::math::clamp(polyComp, 0.8f, 1.f);
        bool gateConnectedGlobal = inputs[GATE_INPUT].isConnected();
        bool trigConnectedGlobal = inputs[STAGE_TRIG_INPUT].isConnected();
        bool silenceWithoutGate = !gateConnectedGlobal && !trigConnectedGlobal;

        float chorusPhaseInc = 0.f;
        int chorusBaseSamples = 0;
        int chorusDepthSamples = 0;
        float chorusDryMix = 0.f;
        float chorusWetMix = 0.f;
        float chorusCrossMix = 0.f;
        if (chorusEnabled) {
            chorusPhaseInc = 2.f * float(M_PI) * kChorusRateHz * args.sampleTime;
            chorusBaseSamples = (int)std::round(kChorusBaseDelayMs * 0.001f * args.sampleRate);
            chorusDepthSamples = (int)std::round(kChorusDepthMs * 0.001f * args.sampleRate);
            chorusDepthSamples = std::max(1, chorusDepthSamples);
            chorusDryMix = std::cos(kChorusMix * float(M_PI) * 0.5f);
            chorusWetMix = std::sin(kChorusMix * float(M_PI) * 0.5f);
            chorusCrossMix = kChorusCrossMix * chorusWetMix;

            // Optimization #3: Decimate chorus LFO calculation
            // Only update modulation values every N samples, interpolate between
            chorusLfoCounter++;
            if (chorusLfoCounter >= kChorusLfoDecimation) {
                chorusLfoCounter = 0;
                // Update phase for first voice (shared across all voices for simplicity)
                float phase = chorusVoices[0].phase + chorusPhaseInc * kChorusLfoDecimation;
                if (phase > 2.f * float(M_PI)) {
                    phase -= 2.f * float(M_PI);
                }
                chorusVoices[0].phase = phase;
                chorusModALast = std::sin(phase);
                chorusModBLast = std::sin(phase + 2.f * float(M_PI) / 3.f);
            }
        }

        for (int ch = 0; ch < channels; ++ch) {
            if (silenceWithoutGate) {
                stageActive[ch] = false;
                stageEnvelope[ch] = 0.f;
                stagePositions[ch] = 0.f;
                clickSuppressor[ch] = 1.f;
                feedbackSignal[ch] = 0.f;
                outputs[MAIN_L_OUTPUT].setVoltage(0.f, ch);
                outputs[MAIN_R_OUTPUT].setVoltage(0.f, ch);
                outputs[EDGE_OUTPUT].setVoltage(0.f, ch);
                continue;
            }

            bool gateConnected = gateConnectedGlobal;
            bool trigConnected = trigConnectedGlobal && !gateConnected;
            float gateVolt = 0.f;
            float trigVolt = 0.f;
            bool gateHigh = !gateConnected;
            bool retriggered = false;
            bool trigFired = false;
            if (gateConnected) {
                gateVolt = inputs[GATE_INPUT].getPolyVoltage(ch);
                gateHigh = gateVolt >= 1.f;
                retriggered = gateHigh && !gateHeld[ch];
            } else if (trigConnected) {
                trigVolt = inputs[STAGE_TRIG_INPUT].getPolyVoltage(ch);
                trigFired = stageTriggers[ch].process(trigVolt);
                retriggered = trigFired;
            }

            float drift = 0.f;
            if (vintageMode) {
                float timer = vintageDriftTimer[ch] - args.sampleTime;
                if (timer <= 0.f) {
                    vintageDrift[ch] = rack::random::uniform() * 2.f - 1.f;
                    vintageDrift[ch] *= kVintageDriftRange;
                    float hold = rack::random::uniform();
                    hold = lerp(kVintageDriftHoldMin, kVintageDriftHoldMax, hold);
                    timer = hold;
                }
                vintageDriftTimer[ch] = timer;
                drift = vintageDrift[ch];
            }

            float pitch = coarse;
            pitch += drift;
            if (voctConnected) {
                pitch += inputs[VOCT_INPUT].getPolyVoltage(ch);
            }

            float freqA = rack::dsp::FREQ_C4 *
                          rack::dsp::exp2_taylor5(rack::math::clamp(pitch, -8.f, 8.f));
            float freqB = rack::dsp::FREQ_C4 *
                          rack::dsp::exp2_taylor5(rack::math::clamp(pitch + detuneOct, -8.f, 8.f));

            // Phase reset on retrigger for consistent starts (user-toggleable)
            if (phaseResetEnabled && retriggered) {
                primaryPhase[ch] = 0.f;
                secondaryPhase[ch] = 0.f;
                subPhase[ch] = 0.f;
            }

            float phaseA = primaryPhase[ch] + freqA * args.sampleTime;
            bool wrappedA = phaseA >= 1.f;
            if (wrappedA) {
                // Simple subtraction is faster than floor when phase is expected to be < 2.0
                phaseA -= 1.f;
                // Handle edge case where phase might be >= 2.0 (very high frequencies)
                if (phaseA >= 1.f) {
                    phaseA -= std::floor(phaseA);
                }
            }

            float phaseB = secondaryPhase[ch] + freqB * args.sampleTime;
            if (activeInteraction == INTERACTION_RESET_SYNC && wrappedA) {
                phaseB = phaseA;
            }
            if (phaseB >= 1.f) {
                // Simple subtraction is faster than floor when phase is expected to be < 2.0
                phaseB -= 1.f;
                // Handle edge case where phase might be >= 2.0 (very high frequencies)
                if (phaseB >= 1.f) {
                    phaseB -= std::floor(phaseB);
                }
            }

            primaryPhase[ch] = phaseA;
            secondaryPhase[ch] = phaseB;

            // Sub-oscillator at -1 octave (free-running)
            float freqSub = freqA * 0.5f;
            float phaseSub = subPhase[ch] + freqSub * args.sampleTime;
            if (phaseSub >= 1.f) {
                // Simple subtraction is faster than floor when phase is expected to be < 2.0
                phaseSub -= 1.f;
                // Handle edge case where phase might be >= 2.0 (very high frequencies)
                if (phaseSub >= 1.f) {
                    phaseSub -= std::floor(phaseSub);
                }
            }
            subPhase[ch] = phaseSub;

            float stagePos = stagePositions[ch];

            bool endedThisFrame = false;

            if (gateConnected) {
                gateHigh = gateVolt >= 1.f;
                bool prevGate = gateHeld[ch];

                if (gateHigh) {
                    if (!prevGate) {
                        stagePos = 0.f;
                        velocityHold[ch] = rack::math::clamp(gateVolt / 10.f, 0.f, 1.f);
                    }
                    float effectiveRate = rate;
                    stagePos += effectiveRate * args.sampleTime;

                    // Simple forward-only: hold at end
                    if (stagePos >= (float)kNumStages) {
                        stagePos = (float)kNumStages - 0.01f;
                    }
                    stageActive[ch] = true;
                } else {
                    if (prevGate) {
                        clickSuppressor[ch] = 1.f;
                    }
                    stageActive[ch] = stageEnvelope[ch] > 1e-4f;
                }
                gateHeld[ch] = gateHigh;
            } else if (trigConnected) {
                if (trigFired) {
                    stagePos = 0.f;
                    stageActive[ch] = true;
                    velocityHold[ch] = rack::math::clamp(std::fabs(trigVolt) / 10.f, 0.f, 1.f);
                }

                if (stageActive[ch]) {
                    float effectiveRate = rate;
                    stagePos += effectiveRate * args.sampleTime;

                    // Simple forward-only: stop at end
                    if (stagePos >= (float)kNumStages) {
                        stagePos = 0.f;
                        stageActive[ch] = false;
                        endedThisFrame = true;
                    }
                }
            } else {
                // Free-running mode (no trigger connected)
                // Play once then hold at end
                if (stagePos >= (float)kNumStages - 0.01f) {
                    stagePos = (float)kNumStages - 0.01f;  // Hold at last stage
                } else {
                    float effectiveRate = rate;
                    stagePos += effectiveRate * args.sampleTime;

                    if (stagePos >= (float)kNumStages) {
                        stagePos = (float)kNumStages - 0.01f;
                    }
                }
                stageActive[ch] = true;
            }

            if (!gateConnected) {
                gateHeld[ch] = false;
                if (!trigConnected) {
                    velocityHold[ch] = 1.f;
                }
            }

            bool releasing = gateConnected && !gateHigh;

            if (!std::isfinite(stagePos)) {
                stagePos = 0.f;
            } else {
                stagePos = rack::math::clamp(stagePos, 0.f, (float)kNumStages);
            }

            stagePositions[ch] = stagePos;

            // Optimization #3: Reduce redundant clamping - clamp once at the end
            float torsionA = torsionBase;
            if (torsionCvConnected) {
                torsionA += inputs[TORSION_CV_INPUT].getPolyVoltage(ch) * torsionAtten * 0.1f;
            }

            // Apply modulation factors without intermediate clamping
            if (dcwKeyTrackEnabled) {
                float offset = rack::math::clamp(pitch, -3.f, 3.f);
                float keyFactor = 1.f + offset * 0.18f;
                keyFactor = rack::math::clamp(keyFactor, 0.25f, 1.75f);
                torsionA *= keyFactor;
            }
            if (dcwVelocityEnabled) {
                // velocityHold is already clamped when set, no need to clamp again
                float velocityFactor = lerp(0.35f, 1.f, velocityHold[ch]);
                torsionA *= velocityFactor;
            }
            // Single final clamp for torsion, then soft-knee and light slew to avoid zipper
            float torsionTarget = rack::math::clamp(torsionA, 0.f, 1.f);
            torsionTarget = softWarpAmount(torsionTarget);
            float torsionSmoothed = torsionSlew[ch] + (torsionTarget - torsionSlew[ch]) * (1.f - cachedTorsionSlewCoeff);
            torsionSlew[ch] = torsionSmoothed;
            torsionA = torsionSmoothed;

            // Symmetry with single clamp
            float symmetry = symmetryBase;
            symmetry = rack::math::clamp(symmetry, 0.f, 1.f);

            float targetStageValue = 0.f;
            if (!releasing && (stageActive[ch] || !trigConnected)) {
                int stageIndex = rack::math::clamp((int)stagePos, 0, kNumStages - 1);
                // Don't wrap around to stage 1 when at the last stage - hold at final stage value
                int nextStage = rack::math::clamp(stageIndex + 1, 0, kNumStages - 1);
                float stagePhase = stagePos - stageIndex;

                // Crossfade between current and next stage
                // When at last stage, nextStage == stageIndex, so it just holds the value
                targetStageValue = rack::math::crossfade(stageLevels[stageIndex], stageLevels[nextStage], stagePhase);
            }

            // Envelope smoothing: ~2ms time constant for smooth but responsive transitions
            // Use cached coefficient (optimization #4)
            float env = stageEnvelope[ch];
            if (releasing) {
                float releaseRate = rack::math::clamp(22.f + rate * 2.8f, 22.f, 80.f);
                // Use lookup table for release coefficient (avoid exp() call)
                float tableIndex = (releaseRate - 22.f) * (kReleaseTableSize - 1) / 58.f;
                int idx = (int)tableIndex;
                idx = rack::math::clamp(idx, 0, kReleaseTableSize - 1);
                float releaseCoeff = releaseCoeffTable[idx];
                env *= releaseCoeff;
            } else {
                env = env + (targetStageValue - env) * (1.f - cachedSlewCoeff);
            }

            // For triggered mode with no explicit release, force a short tail when the sequence ends
            if (!stageActive[ch] && trigConnected) {
                env *= cachedSuppressorEndDecay;  // tight fade to avoid end clicks even if last stage is high
            }
            stageEnvelope[ch] = env;

            if (releasing && env <= 1e-4f) {
                stageActive[ch] = false;
            }

            // Click suppression system: trigger fade-out when envelope is very low
            // This prevents pops from complex waveforms cutting off abruptly
            const float clickSuppressionThreshold = 0.05f;  // Trigger when envelope drops below 5%
            if (env < clickSuppressionThreshold) {
                // Fast exponential fade-out over ~10ms to ensure smooth zero-crossing
                // Use cached coefficient (optimization #4)
                clickSuppressor[ch] *= cachedSuppressorDecay;
            } else if (endedThisFrame) {
                // Force a tight tail at sequence end to avoid residual clicks
                clickSuppressor[ch] *= cachedSuppressorEndDecay;
            } else {
                // Fade-in smoothly when coming back from near-zero to avoid upward clicks
                float rise = 1.f - cachedSuppressorRise;
                clickSuppressor[ch] += (1.f - clickSuppressor[ch]) * rise;
            }

            // In triggered mode when inactive, let envelope decay naturally instead of hard reset
            if (trigConnected && !stageActive[ch]) {
                // Gently pull toward zero with a slower slew to avoid clicks
                env = stageEnvelope[ch] * cachedInactiveDecay;
                stageEnvelope[ch] = env;
            }

            // Only silence output when envelope AND click suppressor are truly negligible
            if (env <= 1e-6f && clickSuppressor[ch] <= 1e-6f) {
                outputs[MAIN_L_OUTPUT].setVoltage(0.f, ch);
                outputs[MAIN_R_OUTPUT].setVoltage(0.f, ch);
                outputs[EDGE_OUTPUT].setVoltage(0.f, ch);
                stageEnvelope[ch] = 0.f;
                clickSuppressor[ch] = 1.0f;  // Reset for next trigger
                continue;
            }

            float dcwEnv = rack::math::clamp(env * torsionA, 0.f, 1.f);
            float dcwA = softWarpAmount(dcwEnv);
            float dcwB = softWarpAmount(dcwEnv);

            if (activeInteraction == INTERACTION_DCW_FOLLOW) {
                float influence = std::fabs(std::sin(2.f * M_PI * phaseA));
                dcwB = rack::math::clamp(dcwEnv * influence, 0.f, 1.f);
            }

            float feedbackAmount = feedbackBase;
            if (feedbackCvConnected) {
                feedbackAmount += inputs[FEEDBACK_CV_INPUT].getPolyVoltage(ch) * feedbackAtten * 0.1f;
            }
            feedbackAmount = rack::math::clamp(feedbackAmount, 0.f, 1.f);

            // Apply feedback to phase (use cached param read - optimization #2)
            float feedbackMod = feedbackSignal[ch] * feedbackAmount * 0.3f;
            float phaseAFinal = phaseA + feedbackMod;
            phaseAFinal = phaseAFinal - std::floor(phaseAFinal);

            float biasA = shapeBias(symmetry, dcwA);
            float biasB = shapeBias(symmetry, dcwB);
            float warpedA = applyCZWarp(phaseAFinal, dcwA, biasA, warpShape);
            float warpedB = applyCZWarp(phaseB, dcwB, biasB, warpShape);

            // Unwarped bases (selected waveforms, no torsion) for smooth crossfade and edge calc
            float baseA = buildWarpedVoice(phaseAFinal, 0.f);
            float baseB = buildWarpedVoice(phaseB, 0.f);

            // Build warped voices using hoisted lambda
            float shapedA = buildWarpedVoice(warpedA, dcwA);
            float shapedB = buildWarpedVoice(warpedB, dcwB);
            // Crossfade toward unwarped base when torsion is low to avoid zippering/zeroing
            shapedA = rack::math::crossfade(baseA, shapedA, dcwA);
            shapedB = rack::math::crossfade(baseB, shapedB, dcwB);

            float interactionGain = 1.f;
            if (activeInteraction == INTERACTION_DCW_FOLLOW) {
                shapedB = rack::math::crossfade(shapedB, baseB, 0.25f);
                interactionGain = 1.15f;
            } else if (activeInteraction == INTERACTION_RING_MOD) {
                shapedB = shapedA * shapedB;
                interactionGain = 1.7f;
            }

            // Generate sub-oscillator (pure sine wave, -1 octave)
            float subSin, subCos;
            fastSinCos2Pi(phaseSub, subSin, subCos);
            float subSignal = subSin * subLevel;
            float primaryActivity = 0.5f * (std::fabs(shapedA) + std::fabs(shapedB));
            float subTrim = 1.f / (1.f + primaryActivity * 0.9f);
            subSignal *= subTrim;

            // Main output: mix both oscillators with balanced gain staging
            // Envelope modulates torsion, not amplitude directly
            float mainSignal = env * interactionGain * 0.5f * (shapedA + shapedB) + subSignal * env;

            // Edge output: blend between base tone (low torsion) and torsion difference (high torsion)
            float baseSum = baseA + baseB;
            float torsionDifference = (shapedA - baseA) + (shapedB - baseB);
            float edgeContribution = torsionDifference + baseSum * (1.f - dcwEnv);
            float edgeGain = 0.4f;
            float edgeSignal = env * interactionGain * edgeGain * edgeContribution;

            // In trigger mode, apply a dedicated fast tail once the stage sequence has ended
            if (trigConnected) {
                float tail = trigTail[ch];
                if (stageActive[ch]) {
                    tail = 1.f;
                } else {
                    tail *= cachedTrigTailCoeff;
                }
                trigTail[ch] = tail;
                mainSignal *= tail;
                edgeSignal *= tail;
            }

            mainSignal *= polyComp;
            edgeSignal *= polyComp;

            if (vintageMode) {
                // Use pre-generated noise buffer instead of calling random() every sample
                float hiss = vintageNoiseBuffer[vintageNoiseIndex] * kVintageHissLevel * noiseComp;
                vintageNoiseIndex = (vintageNoiseIndex + 1) & (kVintageNoiseBufferSize - 1);
                float bleed = clockSignal * noiseComp;
                mainSignal += hiss + bleed;
                edgeSignal += hiss * 0.4f;  // keep edge noise subtler
            }

            if (!std::isfinite(mainSignal) || !std::isfinite(edgeSignal)) {
                mainSignal = 0.f;
                edgeSignal = 0.f;
            }

            // Optional saturation: dirty mode keeps the original drive, clean mode adds gentle limiting
            float mainOut;
            float edgeOut;
            if (dirtyMode) {
                // Stronger, slightly asymmetric drive for audible grit
                float drive = 2.0f;
                float asym = 0.08f;
                float drivenMain = std::tanh(mainSignal * drive + asym * mainSignal * mainSignal);
                float drivenEdge = std::tanh(edgeSignal * drive + asym * edgeSignal * edgeSignal);
                constexpr float dirtyScale = 0.75f;
                mainOut = drivenMain * dirtyScale;
                edgeOut = drivenEdge * dirtyScale;
            } else {
                constexpr float cleanDrive = 0.75f;
                constexpr float cleanScale = 1.f / cleanDrive;  // Unity gain around 0 V
                mainOut = std::tanh(mainSignal * cleanDrive) * cleanScale;
                edgeOut = std::tanh(edgeSignal * cleanDrive) * cleanScale;
            }

            // Apply click suppressor to prevent pops at envelope end
            // This creates a smooth fade-out ramp when envelope is very low
            mainOut *= clickSuppressor[ch];
            edgeOut *= clickSuppressor[ch];

            // Hard-stop any residual after tail in trig mode to eliminate end clicks
            if (trigConnected && !stageActive[ch]) {
                if (trigTail[ch] < 5e-3f && env < 1e-4f) {
                    mainOut = 0.f;
                    edgeOut = 0.f;
                    feedbackSignal[ch] = 0.f;
                }
            }

            if (vintageMode) {
                // Use pre-generated noise buffer for idle hiss too
                float idleHiss = vintageNoiseBuffer[(vintageNoiseIndex + ch) & (kVintageNoiseBufferSize - 1)] * kVintageIdleHissLevel * noiseComp;
                mainOut += idleHiss;
                edgeOut += idleHiss * 0.6f;
            }

            // DC blocking filter — 20 Hz cutoff, sample-rate-adaptive coefficient
            float dcBlockedMain = dcBlockers[ch].process(mainOut, cachedDcBlockCoeff);

            // Store feedback signal for next sample (before DC blocking for stability)
            feedbackSignal[ch] = mainOut;

            float stereoLeft = dcBlockedMain;
            float stereoRight = dcBlockedMain;
            if (chorusEnabled) {
                ChorusVoiceState& chorusState = chorusVoices[ch];
                // Use cached/decimated LFO values — fractional delay for smooth sweep
                float modA = chorusModALast;
                float modB = chorusModBLast;
                float input = dcBlockedMain;
                float fracDelayA = (float)chorusBaseSamples + (float)chorusDepthSamples * ((modA + 1.f) * 0.5f);
                float fracDelayB = (float)chorusBaseSamples + (float)chorusDepthSamples * ((modB + 1.f) * 0.5f);
                float delayOutL = chorusState.delayL.processInterpolated(input, fracDelayA);
                float delayOutR = chorusState.delayR.processInterpolated(input, fracDelayB);
                stereoLeft = input * chorusDryMix + delayOutL * chorusWetMix + delayOutR * chorusCrossMix;
                stereoRight = input * chorusDryMix + delayOutR * chorusWetMix + delayOutL * chorusCrossMix;
            }

            outputs[MAIN_L_OUTPUT].setVoltage(
                shapetaker::AudioProcessor::softLimit(stereoLeft * OUTPUT_SCALE, 10.0f), ch);
            outputs[MAIN_R_OUTPUT].setVoltage(
                shapetaker::AudioProcessor::softLimit(stereoRight * OUTPUT_SCALE, 10.0f), ch);
            outputs[EDGE_OUTPUT].setVoltage(
                shapetaker::AudioProcessor::softLimit(edgeOut * OUTPUT_SCALE, 10.0f), ch);
        }

        // Update polyphonic stage LEDs with brightness stacking
        // Accumulate brightness from all active voices for a "heat map" effect
        float stageBrightness[kNumStages] = {};

        for (int ch = 0; ch < channels; ++ch) {
            bool active = stageActive[ch];
            if (active) {
                float stagePos = stagePositions[ch];
                // Accumulate brightness contribution from each voice
                for (int i = 0; i < kNumStages; ++i) {
                    float distance = std::fabs(stagePos - (float)i);
                    float brightness = rack::math::clamp(1.f - distance, 0.f, 1.f);
                    stageBrightness[i] += brightness;
                }
            }
        }

        // Normalize brightness by channel count to prevent over-saturation
        float lightSlew = args.sampleTime * 8.f;
        float normalizeFactor = (channels > 1) ? (1.f / std::sqrt((float)channels)) : 1.f;
        for (int i = 0; i < kNumStages; ++i) {
            // Boost brightness for clearer indication inside the bezel
            float normalizedBrightness = rack::math::clamp(stageBrightness[i] * normalizeFactor * 2.4f, 0.f, 1.f);
            lights[STAGE_LIGHT_1 + i].setSmoothBrightness(normalizedBrightness, lightSlew);
        }
    }
};

// Custom slider with LED indicator that follows the handle (like VCV Random module)
struct VintageSliderLED : app::SvgSlider {
    // LED parameters - warm tube glow color
    static constexpr float LED_RADIUS = 4.0f;
    static constexpr float LED_GLOW_RADIUS = 10.0f;
    // Track is 12px wide; handle (12px) aligns exactly — no offset needed.
    static constexpr float TRACK_CENTER_OFFSET_X = 0.0f;
    NVGcolor ledColor = nvgRGBf(1.0f, 0.6f, 0.2f);

    VintageSliderLED() {
        setBackgroundSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_track_small.svg"))
        );
        setHandleSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_handle_small.svg"))
        );

        // Track 12x60px. Travel: 60 - 18 = 42px.
        box.size = Vec(12.f, 60.f);
        maxHandlePos = Vec(0.f, 0.f);
        minHandlePos = Vec(0.f, 42.f);
    }

    void draw(const DrawArgs& args) override {
        app::SvgSlider::draw(args);

        float value = 0.5f;
        if (auto* pq = getParamQuantity()) {
            value = pq->getScaledValue();
        }

        Vec ledPos = Vec(box.size.x * 0.5f, box.size.y * 0.5f);
        if (handle) {
            ledPos = Vec(handle->box.pos.x + handle->box.size.x * 0.5f,
                         handle->box.pos.y + handle->box.size.y * 0.5f);
        }

        drawLED(args, ledPos, value);
        drawSlotRim(args);
    }

    void drawSlotRim(const DrawArgs& args) {
        const float slotX = 1.5f;
        const float slotW = 9.0f;
        const float slotR = 2.5f;
        const float slotY = (box.size.y > 90.f) ? 2.0f : 1.0f;
        const float slotH = box.size.y - slotY * 2.0f;

        // Outer definition stroke
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, slotX - 0.3f, slotY - 0.3f,
                       slotW + 0.6f, slotH + 0.6f, slotR + 0.3f);
        nvgStrokeWidth(args.vg, 0.6f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 100));
        nvgStroke(args.vg);

        // Top rim highlight
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX + slotR * 0.4f, slotY);
        nvgLineTo(args.vg, slotX + slotW - slotR * 0.4f, slotY);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(220, 220, 220, 90));
        nvgStroke(args.vg);

        // Bottom rim shadow
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX + slotR * 0.4f, slotY + slotH);
        nvgLineTo(args.vg, slotX + slotW - slotR * 0.4f, slotY + slotH);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 100));
        nvgStroke(args.vg);

        // Left wall highlight
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX, slotY + slotR * 0.6f);
        nvgLineTo(args.vg, slotX, slotY + slotH - slotR * 0.6f);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(190, 190, 190, 55));
        nvgStroke(args.vg);

        // Right wall shadow
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX + slotW, slotY + slotR * 0.6f);
        nvgLineTo(args.vg, slotX + slotW, slotY + slotH - slotR * 0.6f);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 70));
        nvgStroke(args.vg);

        // Inner depth shadow — top fades out as handle approaches top so LED glow is unobstructed
        float topFade = 1.0f;
        if (handle) {
            topFade = rack::math::clamp(handle->box.pos.y / 18.0f, 0.0f, 1.0f);
        }
        NVGpaint topDepth = nvgLinearGradient(args.vg,
            0.f, slotY + 0.5f, 0.f, slotY + 13.0f,
            nvgRGBA(0, 0, 0, (int)(165.f * topFade)), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, slotX + 0.3f, slotY + 0.5f,
                       slotW - 0.6f, 13.0f, slotR * 0.3f);
        nvgFillPaint(args.vg, topDepth);
        nvgFill(args.vg);

        // Inner depth shadow — bottom
        const float botY = slotY + slotH - 13.5f;
        NVGpaint botDepth = nvgLinearGradient(args.vg,
            0.f, botY, 0.f, botY + 13.0f,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 130));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, slotX + 0.3f, botY,
                       slotW - 0.6f, 13.0f, slotR * 0.3f);
        nvgFillPaint(args.vg, botDepth);
        nvgFill(args.vg);

        // Handle contact shadows
        if (handle) {
            const float hTop = handle->box.pos.y;
            const float hBot = handle->box.pos.y + handle->box.size.y;
            const float contactH = 6.0f;
            const float slotEnd = slotY + slotH;
            if (hTop > slotY + 1.0f) {
                float shadowTop = std::max(slotY + 0.5f, hTop - contactH);
                NVGpaint topContact = nvgLinearGradient(args.vg,
                    0.f, shadowTop, 0.f, hTop,
                    nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 120));
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, slotX + 0.5f, shadowTop,
                               slotW - 1.0f, hTop - shadowTop, slotR * 0.2f);
                nvgFillPaint(args.vg, topContact);
                nvgFill(args.vg);
            }
            if (hBot < slotEnd - 1.0f) {
                float shadowBot = std::min(slotEnd - 0.5f, hBot + contactH);
                NVGpaint botContact = nvgLinearGradient(args.vg,
                    0.f, hBot, 0.f, shadowBot,
                    nvgRGBA(0, 0, 0, 120), nvgRGBA(0, 0, 0, 0));
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, slotX + 0.5f, hBot,
                               slotW - 1.0f, shadowBot - hBot, slotR * 0.2f);
                nvgFillPaint(args.vg, botContact);
                nvgFill(args.vg);
            }
        }
    }

    void drawLED(const DrawArgs& args, Vec pos, float brightness) {
        brightness = rack::math::clamp(brightness, 0.f, 1.f);
        if (brightness <= 0.f) {
            return;  // Slider at minimum, LED fully off
        }

        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        // Outer glow (softer, larger)
        float glowAlpha = brightness * 0.55f;
        NVGcolor glowColor = nvgRGBAf(
            ledColor.r,
            ledColor.g,
            ledColor.b,
            glowAlpha
        );

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, pos.x, pos.y, LED_GLOW_RADIUS);
        NVGpaint glowPaint = nvgRadialGradient(
            args.vg,
            pos.x, pos.y,
            0.f,
            LED_GLOW_RADIUS,
            glowColor,
            nvgRGBAf(ledColor.r, ledColor.g, ledColor.b, 0.f)
        );
        nvgFillPaint(args.vg, glowPaint);
        nvgFill(args.vg);

        // Inner LED core (brighter center)
        float coreAlpha = brightness * 0.95f;
        NVGcolor coreColor = nvgRGBAf(
            rack::math::clamp(ledColor.r * 1.1f, 0.f, 1.f),
            rack::math::clamp(ledColor.g * 1.1f, 0.f, 1.f),
            rack::math::clamp(ledColor.b * 0.95f, 0.f, 1.f),
            coreAlpha
        );

        nvgBeginPath(args.vg);
        nvgCircle(args.vg, pos.x, pos.y, LED_RADIUS);
        NVGpaint corePaint = nvgRadialGradient(
            args.vg,
            pos.x, pos.y - LED_RADIUS * 0.3f,
            0.f,
            LED_RADIUS,
            nvgRGBAf(1.f, 0.92f, 0.6f, coreAlpha),  // Warm white center highlight
            coreColor
        );
        nvgFillPaint(args.vg, corePaint);
        nvgFill(args.vg);

        nvgRestore(args.vg);
    }
};

// Larger variant of VintageSliderLED for the full-height stage sliders
struct VintageSliderLEDLarge : app::SvgSlider {
    static constexpr float LED_RADIUS = 4.0f;
    static constexpr float LED_GLOW_RADIUS = 10.0f;
    // Track is 12px wide; handle (12px) aligns exactly — no offset needed.
    static constexpr float TRACK_CENTER_OFFSET_X = 0.0f;
    NVGcolor ledColor = nvgRGBf(1.0f, 0.6f, 0.2f);

    VintageSliderLEDLarge() {
        setBackgroundSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_track_large.svg"))
        );
        setHandleSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_handle_small.svg"))
        );
        // Track 12x120px. Travel: 120 - 18 = 102px.
        box.size = Vec(12.f, 120.f);
        maxHandlePos = Vec(0.f, 0.f);
        minHandlePos = Vec(0.f, 102.f);
    }

    void draw(const DrawArgs& args) override {
        app::SvgSlider::draw(args);
        float value = 0.5f;
        if (auto* pq = getParamQuantity()) {
            value = pq->getScaledValue();
        }
        Vec ledPos = Vec(box.size.x * 0.5f, box.size.y * 0.5f);
        if (handle) {
            ledPos = Vec(handle->box.pos.x + handle->box.size.x * 0.5f,
                         handle->box.pos.y + handle->box.size.y * 0.5f);
        }
        drawLED(args, ledPos, value);
        drawSlotRim(args);
    }

    void drawSlotRim(const DrawArgs& args) {
        const float slotX = 1.5f;
        const float slotW = 9.0f;
        const float slotR = 2.5f;
        const float slotY = (box.size.y > 90.f) ? 2.0f : 1.0f;
        const float slotH = box.size.y - slotY * 2.0f;

        // Outer definition stroke
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, slotX - 0.3f, slotY - 0.3f,
                       slotW + 0.6f, slotH + 0.6f, slotR + 0.3f);
        nvgStrokeWidth(args.vg, 0.6f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 100));
        nvgStroke(args.vg);

        // Top rim highlight
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX + slotR * 0.4f, slotY);
        nvgLineTo(args.vg, slotX + slotW - slotR * 0.4f, slotY);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(220, 220, 220, 90));
        nvgStroke(args.vg);

        // Bottom rim shadow
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX + slotR * 0.4f, slotY + slotH);
        nvgLineTo(args.vg, slotX + slotW - slotR * 0.4f, slotY + slotH);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 100));
        nvgStroke(args.vg);

        // Left wall highlight
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX, slotY + slotR * 0.6f);
        nvgLineTo(args.vg, slotX, slotY + slotH - slotR * 0.6f);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(190, 190, 190, 55));
        nvgStroke(args.vg);

        // Right wall shadow
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, slotX + slotW, slotY + slotR * 0.6f);
        nvgLineTo(args.vg, slotX + slotW, slotY + slotH - slotR * 0.6f);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 70));
        nvgStroke(args.vg);

        // Inner depth — top
        NVGpaint topDepth = nvgLinearGradient(args.vg,
            0.f, slotY + 0.5f, 0.f, slotY + 13.0f,
            nvgRGBA(0, 0, 0, 165), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, slotX + 0.3f, slotY + 0.5f,
                       slotW - 0.6f, 13.0f, slotR * 0.3f);
        nvgFillPaint(args.vg, topDepth);
        nvgFill(args.vg);

        // Inner depth — bottom
        const float botY = slotY + slotH - 13.5f;
        NVGpaint botDepth = nvgLinearGradient(args.vg,
            0.f, botY, 0.f, botY + 13.0f,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 130));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, slotX + 0.3f, botY,
                       slotW - 0.6f, 13.0f, slotR * 0.3f);
        nvgFillPaint(args.vg, botDepth);
        nvgFill(args.vg);

        // Handle contact shadows
        if (handle) {
            const float hTop = handle->box.pos.y;
            const float hBot = handle->box.pos.y + handle->box.size.y;
            const float contactH = 6.0f;
            const float slotEnd = slotY + slotH;
            if (hTop > slotY + 1.0f) {
                float shadowTop = std::max(slotY + 0.5f, hTop - contactH);
                NVGpaint topContact = nvgLinearGradient(args.vg,
                    0.f, shadowTop, 0.f, hTop,
                    nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 120));
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, slotX + 0.5f, shadowTop,
                               slotW - 1.0f, hTop - shadowTop, slotR * 0.2f);
                nvgFillPaint(args.vg, topContact);
                nvgFill(args.vg);
            }
            if (hBot < slotEnd - 1.0f) {
                float shadowBot = std::min(slotEnd - 0.5f, hBot + contactH);
                NVGpaint botContact = nvgLinearGradient(args.vg,
                    0.f, hBot, 0.f, shadowBot,
                    nvgRGBA(0, 0, 0, 120), nvgRGBA(0, 0, 0, 0));
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, slotX + 0.5f, hBot,
                               slotW - 1.0f, shadowBot - hBot, slotR * 0.2f);
                nvgFillPaint(args.vg, botContact);
                nvgFill(args.vg);
            }
        }
    }

    void drawLED(const DrawArgs& args, Vec pos, float brightness) {
        brightness = rack::math::clamp(brightness, 0.f, 1.f);
        if (brightness <= 0.f) return;
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        float glowAlpha = brightness * 0.55f;
        NVGcolor glowColor = nvgRGBAf(ledColor.r, ledColor.g, ledColor.b, glowAlpha);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, pos.x, pos.y, LED_GLOW_RADIUS);
        NVGpaint glowPaint = nvgRadialGradient(args.vg, pos.x, pos.y, 0.f, LED_GLOW_RADIUS,
            glowColor, nvgRGBAf(ledColor.r, ledColor.g, ledColor.b, 0.f));
        nvgFillPaint(args.vg, glowPaint);
        nvgFill(args.vg);
        float coreAlpha = brightness * 0.95f;
        NVGcolor coreColor = nvgRGBAf(
            rack::math::clamp(ledColor.r * 1.1f, 0.f, 1.f),
            rack::math::clamp(ledColor.g * 1.1f, 0.f, 1.f),
            rack::math::clamp(ledColor.b * 0.95f, 0.f, 1.f),
            coreAlpha);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, pos.x, pos.y, LED_RADIUS);
        NVGpaint corePaint = nvgRadialGradient(args.vg, pos.x, pos.y - LED_RADIUS * 0.3f, 0.f,
            LED_RADIUS, nvgRGBAf(1.f, 0.92f, 0.6f, coreAlpha), coreColor);
        nvgFillPaint(args.vg, corePaint);
        nvgFill(args.vg);
        nvgRestore(args.vg);
    }
};

// VintageFourWaySwitch removed - replaced with ShapetakerDavies1900hSmallDot for cleaner aesthetic

struct TorsionWidget : ModuleWidget {
    void draw(const DrawArgs& args) override {
        // Match the uniform Clairaudient/Tessellation/Transmutation leather treatment
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            // Keep leather grain density consistent across panel widths via fixed-height tiling.
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

        // Draw a black inner frame to fully mask any edge tinting
        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }

    TorsionWidget(Torsion* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Torsion.svg")));

        shapetaker::ui::LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        shapetaker::ui::LayoutHelper::PanelSVGParser parser(
            asset::plugin(pluginInstance, "res/panels/Torsion.svg"));
        auto centerPx = shapetaker::ui::LayoutHelper::createCenterPxHelper(parser);

        // === Control positioning synchronized with SVG ===
        // Note: centerPx() reads from SVG first, fallback values provided for safety

        // Top row knobs
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("coarse_knob", 11.44458f, 18.983347f), module, Torsion::COARSE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("torsion_knob", 9.9738617f, 58.915623f), module, Torsion::TORSION_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("sub_level_knob", 31.29785f, 18.983347f), module, Torsion::SUB_LEVEL_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("warp_shape_knob", 27.021532f, 39.152843f), module, Torsion::WARP_SHAPE_PARAM));

        // Second row knobs
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("detune_knob", 81.164387f, 18.983347f), module, Torsion::DETUNE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("symmetry_knob", 49.012959f, 39.152843f), module, Torsion::SYMMETRY_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("stage_rate_knob", 81.164387f, 39.421593f), module, Torsion::STAGE_RATE_PARAM));

        // Chorus on/off
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("chorus-switch", 9.925744f, 39.152843f), module, Torsion::CHORUS_PARAM));

        // Attenuverters / nearby CV helpers
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("torsion_atten", 9.9738617f, 76.715012f), module, Torsion::TORSION_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("feedback_atten", 81.68639f, 76.681564f), module, Torsion::FEEDBACK_ATTEN_PARAM));

        // Middle section knobs
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(
            centerPx("feedback_knob", 81.68639f, 58.848732f), module, Torsion::FEEDBACK_PARAM));

        // Toggle switches (bottom row)
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("saw_wave_switch", 9.9738617f, 101.65123f), module, Torsion::SAW_WAVE_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("tri_wave_switch", 25.361994f, 101.65123f), module, Torsion::TRIANGLE_WAVE_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("dirty_mode_switch", 56.13826f, 101.65123f), module, Torsion::DIRTY_MODE_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggle>(
            centerPx("square_wave_switch", 40.750126f, 101.65123f), module, Torsion::SQUARE_WAVE_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggleFourPos>(
            centerPx("sub_sync_switch", 81.68639f, 101.65123f), module, Torsion::SUB_SYNC_PARAM));

        // Helper lambda to position sliders by their top-left corner from SVG rect coords
        auto parseTranslate = [](const std::string& transformAttr) -> Vec {
            Vec offset(0.f, 0.f);
            if (transformAttr.empty()) {
                return offset;
            }
            size_t translatePos = transformAttr.find("translate");
            if (translatePos == std::string::npos) {
                return offset;
            }
            size_t open = transformAttr.find('(', translatePos);
            size_t close = transformAttr.find(')', translatePos);
            if (open == std::string::npos || close == std::string::npos || close <= open) {
                return offset;
            }
            std::string inside = transformAttr.substr(open + 1, close - open - 1);
            for (char& c : inside) {
                if (c == ',' || c == ';') {
                    c = ' ';
                }
            }
            std::stringstream ss(inside);
            ss >> offset.x;
            if (!(ss >> offset.y)) {
                offset.y = 0.f;
            }
            return offset;
        };

        constexpr const char* SLIDER_GROUP_ID = "slider_guides";
        Vec sliderGroupOffset = Vec(0.f, 0.f);
        {
            std::string sliderGroupTag = parser.findTagForId(SLIDER_GROUP_ID);
            std::string transformAttr = shapetaker::ui::LayoutHelper::PanelSVGParser::getAttrStr(
                sliderGroupTag, "transform", "");
            sliderGroupOffset = parseTranslate(transformAttr);
        }

        auto createSlider = [&](const char* id, float fallbackX, float fallbackY, int paramId,
                                const char* anchorCenterId = nullptr,
                                float fallbackCenterX = 0.f, float fallbackCenterY = 0.f) {
            auto* slider = createParam<VintageSliderLEDLarge>(Vec(0, 0), module, paramId);

            if (anchorCenterId) {
                Vec centerPxValue = centerPx(anchorCenterId, fallbackCenterX, fallbackCenterY);
                slider->box.pos = Vec(centerPxValue.x - slider->box.size.x * 0.5f,
                                      centerPxValue.y - slider->box.size.y * 0.5f);
                slider->box.pos.x += VintageSliderLEDLarge::TRACK_CENTER_OFFSET_X;
            } else {
                // Get rect position from SVG
                std::string tag = parser.findTagForId(id);
                float rectX = shapetaker::ui::LayoutHelper::PanelSVGParser::getAttr(tag, "x", fallbackX);
                float rectY = shapetaker::ui::LayoutHelper::PanelSVGParser::getAttr(tag, "y", fallbackY);

                // Apply group transform offset and convert to pixels
                float finalX = rectX + sliderGroupOffset.x;
                float finalY = rectY + sliderGroupOffset.y;
                Vec rectTopLeftPx = shapetaker::ui::LayoutHelper::mm2px(Vec(finalX, finalY));

                // Position by top-left corner
                slider->box.pos = rectTopLeftPx;
            }

            addParam(slider);
        };

        const char* stageSliderIds[Torsion::kNumStages] = {
            "stage_1_slider",
            "stage_2_slider",
            "stage_3_slider",
            "stage_4_slider",
            "stage_5_slider",
            "stage_6_slider"
        };
        const char* stageSliderDotIds[Torsion::kNumStages] = {
            "stage_1_dot",
            "stage_2_dot",
            "stage_3_dot",
            "stage_4_dot",
            "stage_5_dot",
            "stage_6_dot"
        };
        constexpr float stageSliderFallbackX[Torsion::kNumStages] = {
            21.081661f, 28.481663f, 35.881664f, 43.281666f, 50.681667f, 58.081669f
        };
        constexpr float stageSliderFallbackY = 64.25f;
        constexpr float sliderFallbackWidth = 2.1166666f;
        constexpr float sliderFallbackHeight = 42.f;
        for (int i = 0; i < Torsion::kNumStages; ++i) {
            float fallbackCenterX = stageSliderFallbackX[i] + sliderGroupOffset.x + sliderFallbackWidth * 0.5f;
            float fallbackCenterY = stageSliderFallbackY + sliderGroupOffset.y + sliderFallbackHeight * 0.5f;
            createSlider(stageSliderIds[i], stageSliderFallbackX[i], stageSliderFallbackY,
                        Torsion::STAGE1_PARAM + i,
                        stageSliderDotIds[i],
                        fallbackCenterX,
                        fallbackCenterY);
        }

        // Parse DCW stage LED positions directly from SVG
        // These lights are positioned exactly between the two slider rows
        // Stage position indicator LEDs (synchronized with SVG)
        const char* stageLightIds[Torsion::kNumStages] = {
            "stage_1_light",
            "stage_2_light",
            "stage_3_light",
            "stage_4_light",
            "stage_5_light",
            "stage_6_light"
        };
        constexpr float stageLightFallbackX[Torsion::kNumStages] = {
            22.755007f, 30.176956f, 37.598907f, 45.020859f, 52.44281f, 59.864761f
        };
        constexpr float stageLightFallbackY = 47.5f;

        for (int i = 0; i < Torsion::kNumStages; ++i) {
            Vec lightPos = centerPx(stageLightIds[i], stageLightFallbackX[i], stageLightFallbackY);
            addChild(createLightCentered<shapetaker::ui::BrassBezelSmallLight<WhiteLight>>(
                lightPos, module, Torsion::STAGE_LIGHT_1 + i));
        }

        // Loop direction LEDs removed - redundant with stage position indicators
        // addChild(createLightCentered<SmallLight<GreenLight>>(
        //     centerPx("loop_forward_light", centerCol + 13.f, 42.f),
        //     module,
        //     Torsion::LOOP_FORWARD_LIGHT));
        // addChild(createLightCentered<SmallLight<RedLight>>(
        //     centerPx("loop_reverse_light", centerCol - 13.f, 42.f),
        //     module,
        //     Torsion::LOOP_REVERSE_LIGHT));

        // === I/O Section (Bottom) ===
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("voct_cv", 9.9738617f, 114.70013f), module, Torsion::VOCT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("gate_input", 22.284367f, 114.70013f), module, Torsion::GATE_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("torsion_cv", 9.9738617f, 89.714966f), module, Torsion::TORSION_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("feedback_cv", 81.68639f, 89.714966f), module, Torsion::FEEDBACK_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("stage_trig_cv", 34.594872f, 114.70013f), module, Torsion::STAGE_TRIG_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_output", 46.90538f, 114.70013f), module, Torsion::MAIN_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_output_r", 59.194309f, 114.70013f), module, Torsion::MAIN_R_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("edge_output", 81.68639f, 114.70013f), module, Torsion::EDGE_OUTPUT));
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);

        Torsion* module = dynamic_cast<Torsion*>(this->module);
        if (!module) {
            return;
        }

        menu->addChild(new ui::MenuSeparator());
        auto* heading = new ui::MenuLabel;
        heading->text = "Oscillator interaction";
        menu->addChild(heading);
        auto* panelMapped = new ui::MenuLabel;
        panelMapped->text = "Controlled by SUBSYNC switch";
        menu->addChild(panelMapped);
        auto* m0 = new ui::MenuLabel; m0->text = "OFF: Independent"; menu->addChild(m0);
        auto* m1 = new ui::MenuLabel; m1->text = "POS1: Sync B to A resets"; menu->addChild(m1);
        auto* m2 = new ui::MenuLabel; m2->text = "ON: B DCW follows A"; menu->addChild(m2);
        auto* m3 = new ui::MenuLabel; m3->text = "POS4: Ring mod mix"; menu->addChild(m3);
        menu->addChild(new ui::MenuSeparator());

        auto* dcwHeading = new ui::MenuLabel;
        dcwHeading->text = "DCW enhancements";
        menu->addChild(dcwHeading);

        struct KeyTrackItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->dcwKeyTrackEnabled = !module->dcwKeyTrackEnabled;
            }
            void step() override {
                rightText = module->dcwKeyTrackEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        struct VelocityItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->dcwVelocityEnabled = !module->dcwVelocityEnabled;
            }
            void step() override {
                rightText = module->dcwVelocityEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* keyTrackItem = new KeyTrackItem;
        keyTrackItem->module = module;
        keyTrackItem->text = "Key tracking (DCW depth)";
        menu->addChild(keyTrackItem);

        auto* velocityItem = new VelocityItem;
        velocityItem->module = module;
        velocityItem->text = "Velocity sensitivity (DCW depth)";
        menu->addChild(velocityItem);

        menu->addChild(new ui::MenuSeparator());

        struct VintageModeItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->vintageMode = !module->vintageMode;
            }
            void step() override {
                rightText = module->vintageMode ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        menu->addChild(new ui::MenuSeparator());

        struct PhaseResetItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->phaseResetEnabled = !module->phaseResetEnabled;
            }
            void step() override {
                rightText = module->phaseResetEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* phaseResetItem = new PhaseResetItem;
        phaseResetItem->module = module;
        phaseResetItem->text = "Reset phases on gate/trig";
        menu->addChild(phaseResetItem);

        auto* vintageItem = new VintageModeItem;
        vintageItem->module = module;
        vintageItem->text = "Vintage mode (hiss/bleed/drift)";
        menu->addChild(vintageItem);
        }
    };

    Model* modelTorsion = createModel<Torsion, TorsionWidget>("Torsion");
