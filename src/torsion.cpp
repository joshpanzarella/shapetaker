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

    // Precomputed pow() table for curve/wave shaping to avoid repeated std::pow calls
    inline float fastPowLookup(float t, float exponent) {
        constexpr int kTSize = 256;
        constexpr int kExpSize = 128;
        constexpr float kExpMin = 0.05f;
        constexpr float kExpMax = 5.f;

        static bool initialized = false;
        static float table[kExpSize][kTSize] = {};

        t = rack::math::clamp(t, 0.f, 1.f);
        exponent = rack::math::clamp(exponent, kExpMin, kExpMax);

        if (t <= 0.f) {
            return 0.f;
        }
        if (t >= 1.f) {
            return 1.f;
        }

        if (!initialized) {
            for (int ei = 0; ei < kExpSize; ++ei) {
                float expVal = kExpMin + (kExpMax - kExpMin) * ((float)ei / (kExpSize - 1));
                for (int ti = 0; ti < kTSize; ++ti) {
                    float tVal = (float)ti / (kTSize - 1);
                    table[ei][ti] = std::pow(tVal, expVal);
                }
            }
            initialized = true;
        }

        float ePos = (exponent - kExpMin) * (kExpSize - 1) / (kExpMax - kExpMin);
        int eIdx = rack::math::clamp((int)ePos, 0, kExpSize - 2);
        float eFrac = ePos - (float)eIdx;

        float tPos = t * (kTSize - 1);
        int tIdx = rack::math::clamp((int)tPos, 0, kTSize - 2);
        float tFrac = tPos - (float)tIdx;

        float v00 = table[eIdx][tIdx];
        float v01 = table[eIdx][tIdx + 1];
        float v10 = table[eIdx + 1][tIdx];
        float v11 = table[eIdx + 1][tIdx + 1];

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

    float generateSine(float phase) {
        // Sine wave is naturally bandlimited
        return std::sin(2.f * M_PI * phase);
    }

    // Curve shaping for stage transitions
    // curve: -1 = exponential (fast start), 0 = linear, +1 = logarithmic (slow start)
    float applyCurve(float t, float curve) {
        t = rack::math::clamp(t, 0.f, 1.f);
        curve = rack::math::clamp(curve, -1.f, 1.f);

        if (curve < -0.01f) {
            float amount = -curve;
            float exponent = lerp(1.f, 5.f, amount);
            float shaped = 1.f - fastPowLookup(1.f - t, exponent);
            float mix = lerp(0.6f, 0.9f, amount);
            return rack::math::crossfade(t, shaped, mix);
        } else if (curve > 0.01f) {
            float amount = curve;
            float exponent = lerp(1.f, 5.f, amount);
            float shaped = fastPowLookup(t, exponent);
            float mix = lerp(0.6f, 0.9f, amount);
            return rack::math::crossfade(t, shaped, mix);
        }
        return t; // Linear
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
        CURVE1_PARAM,
        CURVE2_PARAM,
        CURVE3_PARAM,
        CURVE4_PARAM,
        CURVE5_PARAM,
        CURVE6_PARAM,
        FEEDBACK_PARAM,
        FEEDBACK_ATTEN_PARAM,
        SAW_WAVE_PARAM,
        TRIANGLE_WAVE_PARAM,
        SQUARE_WAVE_PARAM,
        DIRTY_MODE_PARAM,
        SUB_LEVEL_PARAM,
        SUB_WARP_PARAM,
        SUB_SYNC_PARAM,
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
    shapetaker::dsp::VoiceArray<float> dcBlockerX1;  // Previous input
    shapetaker::dsp::VoiceArray<float> dcBlockerY1;  // Previous output

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

    static constexpr float kVintageHissLevel = 0.0012f;
    static constexpr float kVintageClockLevel = 0.0008f;
    static constexpr float kVintageClockFreq = 9000.f;  // Hz
    static constexpr float kVintageDriftRange = 0.0045f; // +/- range in octaves (~5.5 cents)
    static constexpr float kVintageDriftHoldMin = 0.18f;
    static constexpr float kVintageDriftHoldMax = 0.45f;
    static constexpr float kVintageIdleHissLevel = 0.00055f;

    // Cached exponential coefficients (computed once per sample rate change)
    float cachedSlewCoeff = 0.f;
    float cachedSuppressorDecay = 0.f;
    float cachedSuppressorRise = 0.f;
    float cachedSuppressorEndDecay = 0.f;
    float cachedReleaseCoeffBase = 0.f;
    float cachedTorsionSlewCoeff = 0.f;
    float cachedTrigTailCoeff = 0.f;

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
    static constexpr int kVintageNoiseBufferSize = 512;
    float vintageNoiseBuffer[kVintageNoiseBufferSize] = {};
    int vintageNoiseIndex = 0;

    Torsion() {
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

        // Curve shapers (-1 = exp, 0 = linear, +1 = log)
        configParam(CURVE1_PARAM, -1.f, 1.f, 0.f, "stage 1 curve");
        configParam(CURVE2_PARAM, -1.f, 1.f, 0.f, "stage 2 curve");
        configParam(CURVE3_PARAM, -1.f, 1.f, 0.f, "stage 3 curve");
        configParam(CURVE4_PARAM, -1.f, 1.f, 0.f, "stage 4 curve");
        configParam(CURVE5_PARAM, -1.f, 1.f, 0.f, "stage 5 curve");
        configParam(CURVE6_PARAM, -1.f, 1.f, 0.f, "stage 6 curve");

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

        // Sub oscillator with extended range for powerful bass
        configParam(SUB_LEVEL_PARAM, 0.f, 2.0f, 0.0f, "sub osc level", "%", 0.f, 100.f);
        configParam(SUB_WARP_PARAM, 0.f, 1.f, 0.f, "sub dcw depth");
        configSwitch(SUB_SYNC_PARAM, 0.f, 1.f, 0.f, "sub sync mode", {"free-run", "hard sync"});
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
        // Cache exponential coefficients to avoid recomputing every sample
        cachedSlewCoeff = std::exp(-sampleTime * 500.f);   // ~2 ms attack smoothing
        cachedSuppressorDecay = std::exp(-sampleTime * 100.f);
        cachedSuppressorEndDecay = std::exp(-sampleTime * 800.f); // ~2 ms tail for end-of-seq
        cachedSuppressorRise = std::exp(-sampleTime * 700.f);   // ~1.4 ms fade-in
        cachedTrigTailCoeff = std::exp(-sampleTime * 400.f);    // ~2.5 ms post-stage tail for trig mode
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
        dcBlockerX1.reset();
        dcBlockerY1.reset();
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
        vintageClockPhase = 0.f;
        resetChorusState();
        torsionSlew.reset();
        for (int i = 0; i < LIGHTS_LEN; ++i) {
            lights[i].setBrightness(0.f);
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "interactionMode", json_integer((int)interactionMode));
        json_object_set_new(rootJ, "vintageMode", json_boolean(vintageMode));
        json_object_set_new(rootJ, "dcwKeyTrackEnabled", json_boolean(dcwKeyTrackEnabled));
        json_object_set_new(rootJ, "dcwVelocityEnabled", json_boolean(dcwVelocityEnabled));
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
        }
        json_t* phaseResetJ = json_object_get(rootJ, "phaseResetEnabled");
        if (phaseResetJ) {
            phaseResetEnabled = json_is_true(phaseResetJ);
        }
        resetChorusState();
    }

    void process(const ProcessArgs& args) override {
        int channels = polyProcessor.updateChannels(
            {inputs[VOCT_INPUT], inputs[GATE_INPUT], inputs[TORSION_CV_INPUT], inputs[FEEDBACK_CV_INPUT], inputs[STAGE_TRIG_INPUT]},
            {outputs[MAIN_L_OUTPUT], outputs[MAIN_R_OUTPUT], outputs[EDGE_OUTPUT]});

        // Update cached exponential coefficients if sample rate changed
        static float lastSampleTime = 0.f;
        if (args.sampleTime != lastSampleTime) {
            updateCachedCoefficients(args.sampleTime);
            lastSampleTime = args.sampleTime;
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
        float stageCurves[kNumStages] = {
            params[CURVE1_PARAM].getValue(),
            params[CURVE2_PARAM].getValue(),
            params[CURVE3_PARAM].getValue(),
            params[CURVE4_PARAM].getValue(),
            params[CURVE5_PARAM].getValue(),
            params[CURVE6_PARAM].getValue()
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
        float subLevel = params[SUB_LEVEL_PARAM].getValue();
        float subWarpParam = params[SUB_WARP_PARAM].getValue();
        bool subHardSync = params[SUB_SYNC_PARAM].getValue() > 0.5f;

        // Check if any waveforms are enabled for conditional generation (optimization #1)
        bool anyWaveformEnabled = useSaw || useTriangle || useSquare;

        // Pre-compute harmonic coefficients as compile-time constants
        constexpr float kHarmonic3Inv = 1.f / 3.f;
        constexpr float kHarmonic5Inv = 1.f / 5.f;

        // Hoist waveform building lambda outside voice loop for better performance
        auto buildWarpedVoice = [&](float warpedPhase, float amount) -> float {
            // Early exit if no waveforms enabled - just return sine wave
            if (!anyWaveformEnabled) {
                float theta = 2.f * float(M_PI) * warpedPhase;
                float sin1 = std::sinf(theta);
                float loudness = 1.f + amount * 1.2f;
                return rack::math::clamp(sin1 * loudness, -3.f, 3.f);
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

            if (useSaw || useSquare) {
                sin2 = 2.f * sin1 * cos1;
                cos2 = cos1 * cos1 - sin1 * sin1;
            }

            if (useSquare) {
                float sin4 = 2.f * sin2 * cos2;
                float cos4 = cos2 * cos2 - sin2 * sin2;
                sin3 = sin2 * cos1 + cos2 * sin1;
                sin5 = sin4 * cos1 + cos4 * sin1;
            }

            if (useSaw) {
                float sawHybrid = sin1 + 0.5f * sin2;
                voice += sawHybrid;
                layers++;
            }
            if (useTriangle) {
                float triangleHybrid = 1.f - 4.f * std::fabs(warpedPhase - 0.5f);
                voice += triangleHybrid;
                layers++;
            }
            if (useSquare) {
                float squareHybrid = sin1 + kHarmonic3Inv * sin3 + kHarmonic5Inv * sin5;
                voice += squareHybrid;
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
            clockSignal = std::sin(2.f * float(M_PI) * vintageClockPhase) * kVintageClockLevel;
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
        if (chorusEnabled) {
            float sampleRate = args.sampleRate;
            chorusPhaseInc = 2.f * float(M_PI) * kChorusRateHz * args.sampleTime;
            chorusBaseSamples = (int)std::round(kChorusBaseDelayMs * 0.001f * sampleRate);
            chorusDepthSamples = (int)std::round(kChorusDepthMs * 0.001f * sampleRate);
            chorusDepthSamples = std::max(1, chorusDepthSamples);

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
            bool gateHigh = !gateConnected;
            bool retriggered = false;
            bool trigFired = false;
            if (gateConnected) {
                gateVolt = inputs[GATE_INPUT].getPolyVoltage(ch);
                gateHigh = gateVolt >= 1.f;
                retriggered = gateHigh && !gateHeld[ch];
            } else if (trigConnected) {
                float trigVolt = inputs[STAGE_TRIG_INPUT].getPolyVoltage(ch);
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
            if (inputs[VOCT_INPUT].isConnected()) {
                pitch += inputs[VOCT_INPUT].getPolyVoltage(ch);
            }

            float freqA = rack::dsp::FREQ_C4 *
                          std::pow(2.f, rack::math::clamp(pitch, -8.f, 8.f));
            float freqB = rack::dsp::FREQ_C4 *
                          std::pow(2.f, rack::math::clamp(pitch + detuneOct, -8.f, 8.f));

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
            if (interactionMode == INTERACTION_RESET_SYNC && wrappedA) {
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

            // Sub-oscillator at -1 octave with optional sync
            float freqSub = freqA * 0.5f;
            float phaseSub = subPhase[ch] + freqSub * args.sampleTime;
            // Use cached param read (optimization #2)
            if (wrappedA && subHardSync) {
                phaseSub = 0.f;  // Hard sync to primary oscillator
            }
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
                    // trigVolt was read for trigFired; reuse magnitude via last input read
                    velocityHold[ch] = rack::math::clamp(std::fabs(inputs[STAGE_TRIG_INPUT].getPolyVoltage(ch)) / 10.f, 0.f, 1.f);
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
            if (inputs[TORSION_CV_INPUT].isConnected()) {
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

                // Apply curve shaping to the transition
                float curvedPhase = applyCurve(stagePhase, stageCurves[stageIndex]);

                // Crossfade between current and next stage
                // When at last stage, nextStage == stageIndex, so it just holds the value
                targetStageValue = rack::math::crossfade(stageLevels[stageIndex], stageLevels[nextStage], curvedPhase);
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
                float decayCoeff = std::exp(-args.sampleTime * 40.f); // ~25ms decay tail
                env = stageEnvelope[ch] * decayCoeff;
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

            if (interactionMode == INTERACTION_DCW_FOLLOW) {
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
            if (interactionMode == INTERACTION_DCW_FOLLOW) {
                shapedB = rack::math::crossfade(shapedB, baseB, 0.25f);
                interactionGain = 1.15f;
            } else if (interactionMode == INTERACTION_RING_MOD) {
                shapedB = shapedA * shapedB;
                interactionGain = 1.7f;
            }

            // Generate sub-oscillator (pure sine wave, -1 octave) with optional DCW warp
            // Use cached param reads (optimization #2)
            float subWarpDepth = softWarpAmount(rack::math::clamp(env * subWarpParam, 0.f, 1.f));
            float subBias = shapeBias(symmetry, subWarpDepth);
            float subPhaseWarped = applyCZWarp(phaseSub, subWarpDepth, subBias, warpShape);
            float subLoudness = 1.f + subWarpDepth * 0.8f;
            float subSignal = generateSine(subPhaseWarped) * subLevel * subLoudness;
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

            // DC blocking filter to remove DC offset and reduce clicks/pops
            // Uses a 1-pole high-pass filter with very low cutoff (~20Hz at 44.1kHz)
            const float dcBlockCoeff = 0.999f;  // Very low cutoff for sub-bass preservation
            float dcBlockedMain = mainOut - dcBlockerX1[ch] + dcBlockCoeff * dcBlockerY1[ch];
            dcBlockerX1[ch] = mainOut;
            dcBlockerY1[ch] = dcBlockedMain;

            // Store feedback signal for next sample (before DC blocking for stability)
            feedbackSignal[ch] = mainOut;

            float stereoLeft = dcBlockedMain;
            float stereoRight = dcBlockedMain;
            if (chorusEnabled) {
                ChorusVoiceState& chorusState = chorusVoices[ch];
                // Optimization #3: Use cached/decimated LFO values
                float modA = chorusModALast;
                float modB = chorusModBLast;
                int delayA = chorusBaseSamples + (int)std::round(chorusDepthSamples * ((modA + 1.f) * 0.5f));
                int delayB = chorusBaseSamples + (int)std::round(chorusDepthSamples * ((modB + 1.f) * 0.5f));
                delayA = rack::math::clamp(delayA, 0, kChorusMaxDelaySamples - 1);
                delayB = rack::math::clamp(delayB, 0, kChorusMaxDelaySamples - 1);
                float input = dcBlockedMain;
                float delayOutL = chorusState.delayL.process(input, delayA);
                float delayOutR = chorusState.delayR.process(input, delayB);
                float dryMix = std::cos(kChorusMix * float(M_PI) * 0.5f);
                float wetMix = std::sin(kChorusMix * float(M_PI) * 0.5f);
                float crossMix = kChorusCrossMix * wetMix;
                stereoLeft = input * dryMix + delayOutL * wetMix + delayOutR * crossMix;
                stereoRight = input * dryMix + delayOutR * wetMix + delayOutL * crossMix;
            }

            outputs[MAIN_L_OUTPUT].setVoltage(stereoLeft * OUTPUT_SCALE, ch);
            outputs[MAIN_R_OUTPUT].setVoltage(stereoRight * OUTPUT_SCALE, ch);
            outputs[EDGE_OUTPUT].setVoltage(edgeOut * OUTPUT_SCALE, ch);
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
    static constexpr float TRACK_CENTER_OFFSET_X = 2.0f;  // Track is 8px inside 12px widget
    // Warm orange/amber tube glow color
    NVGcolor ledColor = nvgRGBf(1.0f, 0.6f, 0.2f);

    VintageSliderLED() {
        // Set the background (track) SVG - 8x60px (small compact version)
        setBackgroundSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_track_small.svg"))
        );

        // Set the handle SVG - 12x18px (small compact version)
        setHandleSvg(
            Svg::load(asset::plugin(pluginInstance,
            "res/sliders/vintage_slider_handle_small.svg"))
        );

        // SVG dimensions: track is 8x60px, handle is 12x18px
        // Widget box size matches track width and height
        box.size = Vec(12.f, 60.f);

        // Configure the slider travel range
        maxHandlePos = Vec(-2.f, 0.f);      // Top position (param minimum = 0), offset left 2px to center
        minHandlePos = Vec(-2.f, 42.f);     // Bottom position (param maximum = 1)
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

// VintageFourWaySwitch removed - replaced with ShapetakerKnobAltSmall for cleaner aesthetic

struct TorsionWidget : ModuleWidget {
    TorsionWidget(Torsion* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Torsion.svg")));

        shapetaker::ui::LayoutHelper::ScrewPositions::addStandardScrews<ScrewBlack>(this, box.size.x);

        shapetaker::ui::LayoutHelper::PanelSVGParser parser(
            asset::plugin(pluginInstance, "res/panels/Torsion.svg"));
        auto centerPx = shapetaker::ui::LayoutHelper::createCenterPxHelper(parser);

        // === Control positioning synchronized with SVG ===
        // Note: centerPx() reads from SVG first, fallback values provided for safety

        // Top row knobs
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("coarse_knob", 11.44458f, 17.659729f), module, Torsion::COARSE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("torsion_knob", 31.128044f, 17.659729f), module, Torsion::TORSION_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("sub_level_knob", 50.811508f, 17.659729f), module, Torsion::SUB_LEVEL_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("warp_shape_knob", 70.494972f, 17.659729f), module, Torsion::WARP_SHAPE_PARAM));

        // Second row knobs
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("detune_knob", 11.44458f, 37.985481f), module, Torsion::DETUNE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("symmetry_knob", 31.128044f, 37.985481f), module, Torsion::SYMMETRY_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("stage_rate_knob", 50.811508f, 37.985481f), module, Torsion::STAGE_RATE_PARAM));

        // Attenuverters / nearby CV helpers
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("torsion_atten", 11.407479f, 76.992867f), module, Torsion::TORSION_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("feedback_atten", 70.494972f, 74.118011f), module, Torsion::FEEDBACK_ATTEN_PARAM));

        // Middle section knobs
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("sub_dcw_depth_knob", 11.407479f, 58.311234f), module, Torsion::SUB_WARP_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("feedback_knob", 11.963207f, 80.037621f), module, Torsion::FEEDBACK_PARAM));

        // Toggle switches (bottom row)
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("saw_wave_switch", 11.953995f, 102.31097f), module, Torsion::SAW_WAVE_PARAM));
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("tri_wave_switch", 26.716593f, 102.10995f), module, Torsion::TRIANGLE_WAVE_PARAM));
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("dirty_mode_switch", 41.479195f, 102.10995f), module, Torsion::DIRTY_MODE_PARAM));
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("square_wave_switch", 56.241791f, 102.10995f), module, Torsion::SQUARE_WAVE_PARAM));
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("sub_sync_switch", 71.004387f, 102.10995f), module, Torsion::SUB_SYNC_PARAM));

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
            auto* slider = createParam<VintageSliderLED>(Vec(0, 0), module, paramId);

            if (anchorCenterId) {
                Vec centerPxValue = centerPx(anchorCenterId, fallbackCenterX, fallbackCenterY);
                slider->box.pos = Vec(centerPxValue.x - slider->box.size.x * 0.5f,
                                      centerPxValue.y - slider->box.size.y * 0.5f);
                slider->box.pos.x += VintageSliderLED::TRACK_CENTER_OFFSET_X;
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
            22.14f, 29.54f, 36.94f, 44.34f, 51.74f, 59.14f
        };
        constexpr float stageSliderFallbackY = 74.768f;
        constexpr float sliderFallbackWidth = 2.1166666f;
        constexpr float sliderFallbackHeight = 15.875f;
        for (int i = 0; i < Torsion::kNumStages; ++i) {
            float fallbackCenterX = stageSliderFallbackX[i] + sliderGroupOffset.x + sliderFallbackWidth * 0.5f;
            float fallbackCenterY = stageSliderFallbackY + sliderGroupOffset.y + sliderFallbackHeight * 0.5f;
            createSlider(stageSliderIds[i], stageSliderFallbackX[i], stageSliderFallbackY,
                        Torsion::STAGE1_PARAM + i,
                        stageSliderDotIds[i],
                        fallbackCenterX,
                        fallbackCenterY);
        }

        const char* curveSliderIds[Torsion::kNumStages] = {
            "curve_1_slider",
            "curve_2_slider",
            "curve_3_slider",
            "curve_4_slider",
            "curve_5_slider",
            "curve_6_slider"
        };
        constexpr float curveSliderFallbackY = 97.526f;
        const char* curveSliderDotIds[Torsion::kNumStages] = {
            "curve_1_dot",
            "curve_2_dot",
            "curve_3_dot",
            "curve_4_dot",
            "curve_5_dot",
            "curve_6_dot"
        };
        for (int i = 0; i < Torsion::kNumStages; ++i) {
            float fallbackCenterX = stageSliderFallbackX[i] + sliderGroupOffset.x + sliderFallbackWidth * 0.5f;
            float fallbackCenterY = curveSliderFallbackY + sliderGroupOffset.y + sliderFallbackHeight * 0.5f;
            createSlider(curveSliderIds[i], stageSliderFallbackX[i], curveSliderFallbackY,
                        Torsion::CURVE1_PARAM + i,
                        curveSliderDotIds[i],
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
            25.248032f, 32.648033f, 40.048035f, 47.448029f, 54.848045f, 62.248043f
        };
        constexpr float stageLightFallbackY = 71.004715f;

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
            centerPx("voct_cv", 11.953995f, 114.70013f), module, Torsion::VOCT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("gate_input", 23.764072f, 114.70013f), module, Torsion::GATE_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("torsion_cv", 11.407479f, 85.976151f), module, Torsion::TORSION_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("feedback_cv", 70.494972f, 85.976151f), module, Torsion::FEEDBACK_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("stage_trig_cv", 35.57415f, 114.70013f), module, Torsion::STAGE_TRIG_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_output", 47.384232f, 114.70013f), module, Torsion::MAIN_L_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("main_output_r", 59.194309f, 114.70013f), module, Torsion::MAIN_R_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("edge_output", 71.004387f, 114.70013f), module, Torsion::EDGE_OUTPUT));
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> panelTexture = APP->window->loadImage(
            asset::plugin(pluginInstance, "res/panels/vcv-panel-background.png"));

        if (panelTexture) {
            NVGpaint paint = nvgImagePattern(
                args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, panelTexture->handle, 1.f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        } else {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGB(0xf0, 0xeb, 0xe4));
            nvgFill(args.vg);
        }
        ModuleWidget::draw(args);
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);

        Torsion* module = dynamic_cast<Torsion*>(this->module);
        if (!module) {
            return;
        }

        menu->addChild(new ui::MenuSeparator());

        struct InteractionItem : ui::MenuItem {
            Torsion* module;
            Torsion::InteractionMode mode;
            void onAction(const event::Action& e) override {
                module->interactionMode = mode;
            }
            void step() override {
                rightText = (module->interactionMode == mode) ? "✔" : "";
                ui::MenuItem::step();
            }
        };

        auto* heading = new ui::MenuLabel;
        heading->text = "Oscillator interaction";
        menu->addChild(heading);

        const char* labels[] = {
            "Independent",
            "Sync B to A resets",
            "B DCW follows A",
            "Ring mod mix"
        };

        for (int i = 0; i < Torsion::INTERACTION_MODES_LEN; ++i) {
            auto* item = new InteractionItem;
            item->module = module;
            item->mode = (Torsion::InteractionMode)i;
            item->text = labels[i];
            menu->addChild(item);
        }

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

        struct ChorusItem : ui::MenuItem {
            Torsion* module;
            void onAction(const event::Action& e) override {
                module->chorusEnabled = !module->chorusEnabled;
                module->resetChorusState();
            }
            void step() override {
                rightText = module->chorusEnabled ? "✔" : "";
                ui::MenuItem::step();
            }
        };

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

        auto* chorusItem = new ChorusItem;
        chorusItem->module = module;
        chorusItem->text = "Chorus (stereo)";
        menu->addChild(chorusItem);

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
