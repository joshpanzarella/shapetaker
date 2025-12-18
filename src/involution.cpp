#include "plugin.hpp"
#include <cmath>
#include <random>
#include <limits>
#include <array>
#include <algorithm>
#include <vector>
#include "involution/liquid_filter.hpp"

struct Involution : Module {
    enum ParamId {
        CUTOFF_A_PARAM,
        RESONANCE_A_PARAM,
        CUTOFF_B_PARAM,
        RESONANCE_B_PARAM,
        // New magical parameters
        CHAOS_AMOUNT_PARAM,
        CHAOS_RATE_PARAM,
        AURA_PARAM,
        ORBIT_PARAM,
        TIDE_PARAM,
        // Link switches
        LINK_CUTOFF_PARAM,
        LINK_RESONANCE_PARAM,
        // Attenuverters for CV inputs
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
        CHAOS_RATE_CV_INPUT,
        AURA_CV_INPUT,
        ORBIT_CV_INPUT,
        TIDE_CV_INPUT,
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
        AURA_LIGHT,
        ORBIT_LIGHT,
        TIDE_LIGHT,
        LIGHTS_LEN
    };
    // Liquid 6th-order filters - one per voice per channel
    shapetaker::dsp::VoiceArray<LiquidFilter> filtersA;
    shapetaker::dsp::VoiceArray<LiquidFilter> filtersB;

    // Per-voice cutoff smoothers to eliminate CV zipper noise
    shapetaker::dsp::VoiceArray<shapetaker::FastSmoother> cutoffASmoothers;
    shapetaker::dsp::VoiceArray<shapetaker::FastSmoother> cutoffBSmoothers;

    static constexpr int ETHEREAL_STAGES = 4;
    static constexpr float ETHEREAL_MAX_DELAY_SEC = 0.45f;

    struct EtherealDelay {
        std::vector<float> buffer;
        int size = 0;
        int writePos = 0;
        float lastOutput = 0.f;

        void configure(float sampleRate) {
            size = std::max(16, static_cast<int>(std::ceil(sampleRate * ETHEREAL_MAX_DELAY_SEC)) + 4);
            buffer.assign(size, 0.f);
            writePos = 0;
            lastOutput = 0.f;
        }

        float process(float input, float delaySamples, float feedback) {
            if (size < 2) {
                return input;
            }
            delaySamples = rack::math::clamp(delaySamples, 1.f, static_cast<float>(size - 2));
            float readPos = static_cast<float>(writePos) - delaySamples;
            while (readPos < 0.f) {
                readPos += static_cast<float>(size);
            }
            int i0 = static_cast<int>(readPos);
            int i1 = (i0 + 1) % size;
            float frac = readPos - static_cast<float>(i0);
            float s0 = buffer[i0];
            float s1 = buffer[i1];
            float output = s0 + (s1 - s0) * frac;
            buffer[writePos] = input + output * rack::math::clamp(feedback, -0.95f, 0.95f);
            writePos = (writePos + 1) % size;
            lastOutput = output;
            return output;
        }

        void reset() {
            std::fill(buffer.begin(), buffer.end(), 0.f);
            writePos = 0;
            lastOutput = 0.f;
        }

        float maxDelaySamples() const {
            return size > 2 ? static_cast<float>(size - 2) : 0.f;
        }
    };

    struct EtherealVoiceState {
        std::array<EtherealDelay, ETHEREAL_STAGES> delays;
        std::array<float, ETHEREAL_STAGES> modPhase{};
        std::array<float, ETHEREAL_STAGES> phaseOffset{};
        std::array<float, ETHEREAL_STAGES> drift{};
        std::array<float, ETHEREAL_STAGES> smoothedStageBlend{};
        std::array<float, ETHEREAL_STAGES> smoothedDelay{};
        std::array<float, ETHEREAL_STAGES> dcBlock{};
        float smoothedDiffusion = 0.f;
        float smoothedFeedback = 0.f;
        float smoothedHaloMix = 0.f;
        float smoothedShimmer = 0.f;
        float smoothedModDepth = 0.f;
        float smoothedStereoSkew = 0.f;

        void reset() {
            for (auto& d : delays) {
                d.reset();
            }
            modPhase.fill(0.f);
            smoothedStageBlend.fill(0.f);
            smoothedDelay.fill(12.f);
            dcBlock.fill(0.f);
            smoothedDiffusion = 0.f;
            smoothedFeedback = 0.f;
            smoothedHaloMix = 0.f;
            smoothedShimmer = 0.f;
            smoothedModDepth = 0.f;
            smoothedStereoSkew = 0.f;
        }
    };

    std::array<EtherealVoiceState, shapetaker::PolyphonicProcessor::MAX_VOICES> etherealVoicesA{};
    std::array<EtherealVoiceState, shapetaker::PolyphonicProcessor::MAX_VOICES> etherealVoicesB{};
    float etherealPhase = 0.f;

    // Chaos rate configuration
    static constexpr float CHAOS_RATE_MIN_HZ = 0.01f;
    static constexpr float CHAOS_RATE_MAX_HZ = 20.f;
    static constexpr float CHAOS_CLOCK_TIMEOUT = 2.f;

    rack::dsp::SchmittTrigger chaosClockTrigger;
    float chaosClockElapsed = 0.f;
    float chaosClockPeriod = 0.f;
    float chaosTargetRate = 0.5f;

    // Parameter smoothing
    
    shapetaker::FastSmoother cutoffASmooth, cutoffBSmooth, resonanceASmooth, resonanceBSmooth;
    shapetaker::FastSmoother auraSmooth, orbitSmooth, tideSmooth;
    shapetaker::FastSmoother chaosRateSmooth;
    shapetaker::FastSmoother driveSmooth;
    shapetaker::FastSmoother effectGateSmooth;

    static constexpr float CHARACTER_SMOOTH_TC = 0.015f;
    static constexpr float EFFECT_GATE_SMOOTH_TC = 0.04f;
    static constexpr float ETHEREAL_PARAM_SMOOTH_TC = 0.05f;
    static constexpr float CUTOFF_CV_SMOOTH_TC = 0.002f;  // Very fast smoothing to preserve modulation character while eliminating zipper noise
    static constexpr float DRIVE_SMOOTH_TC = 0.012f;

    // Character drama shaping - set mix to 0 to revert to legacy linear response
    static constexpr float AURA_DRAMA_MIX = 1.f;
    static constexpr float AURA_DRAMA_EXP = 0.55f;
    static constexpr float ORBIT_DRAMA_MIX = 1.f;
    static constexpr float ORBIT_DRAMA_EXP = 0.35f;
    static constexpr float TIDE_DRAMA_MIX = 1.f;
    static constexpr float TIDE_DRAMA_EXP = 0.7f;
    
    // Parameter change tracking for bidirectional linking
    float lastCutoffA = -1.f, lastCutoffB = -1.f;
    float lastResonanceA = -1.f, lastResonanceB = -1.f;
    bool lastLinkCutoff = false, lastLinkResonance = false;

    // Smoothed values for visualizer access
    float smoothedChaosRate = 0.5f;
    float smoothedDrive = 1.0f;
    float effectiveResonanceA = 0.707f;
    float effectiveResonanceB = 0.707f;
    float effectiveCutoffA = 1.0f; // Store final modulated cutoff values
    float effectiveCutoffB = 1.0f;

    Involution() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(CUTOFF_A_PARAM, 0.f, 1.f, 1.f, "Filter A Cutoff", " Hz", std::pow(2.f, 10.f), 20.f);
        configParam(RESONANCE_A_PARAM, 0.707f, 1.5f, 0.707f, "Filter A Resonance");
        configParam(CUTOFF_B_PARAM, 0.f, 1.f, 1.f, "Filter B Cutoff", " Hz", std::pow(2.f, 10.f), 20.f);
        configParam(RESONANCE_B_PARAM, 0.707f, 1.5f, 0.707f, "Filter B Resonance");
        
        // Drive / character controls
        configParam(CHAOS_AMOUNT_PARAM, 0.f, 1.f, 0.75f, "Drive", "%", 0.f, 100.f);
        configParam(AURA_PARAM, 0.f, 1.f, 0.35f, "Aura", "%", 0.f, 100.f);
        configParam(ORBIT_PARAM, 0.f, 1.f, 0.4f, "Orbit", "%", 0.f, 100.f);
        configParam(TIDE_PARAM, 0.f, 1.f, 0.5f, "Tide", "%", 0.f, 100.f);
        configParam(CHAOS_RATE_PARAM, 0.01f, 20.f, 0.5f, "Chaos LFO Rate", " Hz", 0.f, 0.f);

        chaosTargetRate = clamp(params[CHAOS_RATE_PARAM].getValue(), CHAOS_RATE_MIN_HZ, CHAOS_RATE_MAX_HZ);
        smoothedChaosRate = chaosTargetRate;
        chaosRateSmooth.reset(chaosTargetRate);
        smoothedDrive = params[CHAOS_AMOUNT_PARAM].getValue();
        driveSmooth.reset(smoothedDrive);
        smoothedDrive = params[CHAOS_AMOUNT_PARAM].getValue();
        driveSmooth.reset(smoothedDrive);

        std::mt19937 etherealRng(rack::random::u32());
        std::uniform_real_distribution<float> phaseDistrib(0.f, 2.f * static_cast<float>(M_PI));
        std::uniform_real_distribution<float> driftDistrib(0.65f, 1.45f);
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; ++v) {
            for (int s = 0; s < ETHEREAL_STAGES; ++s) {
                etherealVoicesA[v].phaseOffset[s] = phaseDistrib(etherealRng);
                etherealVoicesB[v].phaseOffset[s] = phaseDistrib(etherealRng);
                etherealVoicesA[v].drift[s] = driftDistrib(etherealRng);
                etherealVoicesB[v].drift[s] = driftDistrib(etherealRng);
            }
        }

        // Custom parameter quantity to show real-time chaos rate including CV modulation
        struct ChaosRateQuantity : engine::ParamQuantity {
            float getDisplayValue() override {
                if (!module) return ParamQuantity::getDisplayValue();

                Involution* inv = static_cast<Involution*>(module);
                float currentRate = inv->smoothedChaosRate;
                if (!std::isfinite(currentRate) || currentRate <= 0.f) {
                    currentRate = getValue();
                }
                return clamp(currentRate, Involution::CHAOS_RATE_MIN_HZ, Involution::CHAOS_RATE_MAX_HZ);
            }
        };

        // Replace the default param quantity with our custom one
        paramQuantities[CHAOS_RATE_PARAM] = new ChaosRateQuantity;
        paramQuantities[CHAOS_RATE_PARAM]->module = this;
        paramQuantities[CHAOS_RATE_PARAM]->paramId = CHAOS_RATE_PARAM;
        paramQuantities[CHAOS_RATE_PARAM]->minValue = 0.01f;
        paramQuantities[CHAOS_RATE_PARAM]->maxValue = 20.f;
        paramQuantities[CHAOS_RATE_PARAM]->defaultValue = 0.5f;
        paramQuantities[CHAOS_RATE_PARAM]->name = "Chaos LFO Rate";
        paramQuantities[CHAOS_RATE_PARAM]->unit = " Hz";

        // Link switches
        configSwitch(LINK_CUTOFF_PARAM, 0.f, 1.f, 0.f, "Link Cutoff Frequencies", {"Independent", "Linked"});
        configSwitch(LINK_RESONANCE_PARAM, 0.f, 1.f, 0.f, "Link Resonance Amounts", {"Independent", "Linked"});

        // Attenuverters for CV inputs
        configParam(CUTOFF_A_ATTEN_PARAM, -1.f, 1.f, 0.f, "Cutoff A CV Attenuverter", "%", 0.f, 100.f);
        configParam(RESONANCE_A_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance A CV Attenuverter", "%", 0.f, 100.f);
        configParam(CUTOFF_B_ATTEN_PARAM, -1.f, 1.f, 0.f, "Cutoff B CV Attenuverter", "%", 0.f, 100.f);
        configParam(RESONANCE_B_ATTEN_PARAM, -1.f, 1.f, 0.f, "Resonance B CV Attenuverter", "%", 0.f, 100.f);

        configInput(AUDIO_A_INPUT, "Audio A");
        configInput(AUDIO_B_INPUT, "Audio B");
        configInput(CUTOFF_A_CV_INPUT, "Filter A Cutoff CV");
        configInput(RESONANCE_A_CV_INPUT, "Filter A Resonance CV");
        configInput(CUTOFF_B_CV_INPUT, "Filter B Cutoff CV");
        configInput(RESONANCE_B_CV_INPUT, "Filter B Resonance CV");
        configInput(CHAOS_RATE_CV_INPUT, "Chaos Rate CV");
        configInput(AURA_CV_INPUT, "Aura CV");
        configInput(ORBIT_CV_INPUT, "Orbit CV");
        configInput(TIDE_CV_INPUT, "Tide CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");

        configLight(CHAOS_LIGHT, "Drive Activity");
        configLight(AURA_LIGHT, "Aura Activity");
        configLight(ORBIT_LIGHT, "Orbit Activity");
        configLight(TIDE_LIGHT, "Tide Activity");

        // Initialize filters with default sample rate
        onSampleRateChange();

        auraSmooth.reset(params[AURA_PARAM].getValue());
        orbitSmooth.reset(params[ORBIT_PARAM].getValue());
        tideSmooth.reset(params[TIDE_PARAM].getValue());
        effectGateSmooth.reset(0.f);

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void onSampleRateChange() override {
        float sr = APP->engine->getSampleRate();

        // Update all liquid filters with new sample rate
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; v++) {
            filtersA[v].setSampleRate(sr);
            filtersB[v].setSampleRate(sr);
            for (int s = 0; s < ETHEREAL_STAGES; ++s) {
                etherealVoicesA[v].delays[s].configure(sr);
                etherealVoicesB[v].delays[s].configure(sr);
            }
            etherealVoicesA[v].reset();
            etherealVoicesB[v].reset();
        }
    }

    void onReset() override {
        etherealPhase = 0.f;
        onSampleRateChange();
        chaosClockTrigger.reset();
        chaosClockElapsed = 0.f;
        chaosClockPeriod = 0.f;
        chaosTargetRate = clamp(params[CHAOS_RATE_PARAM].getValue(), CHAOS_RATE_MIN_HZ, CHAOS_RATE_MAX_HZ);
        smoothedChaosRate = chaosTargetRate;
        chaosRateSmooth.reset(chaosTargetRate);
        effectGateSmooth.reset(0.f);
        for (int v = 0; v < shapetaker::PolyphonicProcessor::MAX_VOICES; ++v) {
            etherealVoicesA[v].reset();
            etherealVoicesB[v].reset();
        }
    }

    void process(const ProcessArgs& args) override {
        // Read link switch states
        bool linkCutoff = params[LINK_CUTOFF_PARAM].getValue() > 0.5f;
        bool linkResonance = params[LINK_RESONANCE_PARAM].getValue() > 0.5f;
        
        // Get current raw parameter values
        float currentCutoffA = params[CUTOFF_A_PARAM].getValue();
        float currentCutoffB = params[CUTOFF_B_PARAM].getValue();
        float currentResonanceA = params[RESONANCE_A_PARAM].getValue();
        float currentResonanceB = params[RESONANCE_B_PARAM].getValue();
        
        // Handle bidirectional cutoff linking
        if (linkCutoff) {
            // Check if linking was just enabled
            if (!lastLinkCutoff) {
                // Sync B to A when linking is first enabled
                params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                currentCutoffB = currentCutoffA;
            } else {
                // Check which parameter changed and sync the other
                const float epsilon = 1e-6f;
                bool aChanged = std::abs(currentCutoffA - lastCutoffA) > epsilon;
                bool bChanged = std::abs(currentCutoffB - lastCutoffB) > epsilon;
                
                if (aChanged && !bChanged) {
                    // A changed, sync B to A
                    params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                    currentCutoffB = currentCutoffA;
                } else if (bChanged && !aChanged) {
                    // B changed, sync A to B
                    params[CUTOFF_A_PARAM].setValue(currentCutoffB);
                    currentCutoffA = currentCutoffB;
                } else if (aChanged && bChanged) {
                    // Both changed (shouldn't happen normally), prefer A
                    params[CUTOFF_B_PARAM].setValue(currentCutoffA);
                    currentCutoffB = currentCutoffA;
                }
            }
        }
        
        // Handle bidirectional resonance linking
        if (linkResonance) {
            // Check if linking was just enabled
            if (!lastLinkResonance) {
                // Sync B to A when linking is first enabled
                params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                currentResonanceB = currentResonanceA;
            } else {
                // Check which parameter changed and sync the other
                const float epsilon = 1e-6f;
                bool aChanged = std::abs(currentResonanceA - lastResonanceA) > epsilon;
                bool bChanged = std::abs(currentResonanceB - lastResonanceB) > epsilon;
                
                if (aChanged && !bChanged) {
                    // A changed, sync B to A
                    params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                    currentResonanceB = currentResonanceA;
                } else if (bChanged && !aChanged) {
                    // B changed, sync A to B
                    params[RESONANCE_A_PARAM].setValue(currentResonanceB);
                    currentResonanceA = currentResonanceB;
                } else if (aChanged && bChanged) {
                    // Both changed (shouldn't happen normally), prefer A
                    params[RESONANCE_B_PARAM].setValue(currentResonanceA);
                    currentResonanceB = currentResonanceA;
                }
            }
        }
        
        // Store current values for next frame comparison
        lastCutoffA = currentCutoffA;
        lastCutoffB = currentCutoffB;
        lastResonanceA = currentResonanceA;
        lastResonanceB = currentResonanceB;
        lastLinkCutoff = linkCutoff;
        lastLinkResonance = linkResonance;
        
        // Apply smoothing to the synchronized values
        float cutoffA = cutoffASmooth.process(currentCutoffA, args.sampleTime);
        float cutoffB = cutoffBSmooth.process(currentCutoffB, args.sampleTime);
        float resonanceA = resonanceASmooth.process(currentResonanceA, args.sampleTime);
        float resonanceB = resonanceBSmooth.process(currentResonanceB, args.sampleTime);
        
        // Magical parameters - smoothed for immediate response
        // Drive is locked at maximum; Aura/Orbit/Tide sculpt the ethereal diffusion.

        float auraAmount = auraSmooth.process(params[AURA_PARAM].getValue(), args.sampleTime, CHARACTER_SMOOTH_TC);
        float orbitAmount = orbitSmooth.process(params[ORBIT_PARAM].getValue(), args.sampleTime, CHARACTER_SMOOTH_TC);
        float tideAmount = tideSmooth.process(params[TIDE_PARAM].getValue(), args.sampleTime, CHARACTER_SMOOTH_TC);

        if (inputs[AURA_CV_INPUT].isConnected()) {
            auraAmount += inputs[AURA_CV_INPUT].getVoltage(0) / 10.f;
        }
        if (inputs[ORBIT_CV_INPUT].isConnected()) {
            orbitAmount += inputs[ORBIT_CV_INPUT].getVoltage(0) / 10.f;
        }
        if (inputs[TIDE_CV_INPUT].isConnected()) {
            tideAmount += inputs[TIDE_CV_INPUT].getVoltage(0) / 10.f;
        }
        auraAmount = clamp(auraAmount, 0.f, 1.f);
        orbitAmount = clamp(orbitAmount, 0.f, 1.f);
        tideAmount = clamp(tideAmount, 0.f, 1.f);

        float auraRaw = auraAmount;
        float orbitRaw = orbitAmount;
        float tideRaw = tideAmount;

        auto applyCharacterDrama = [](float value, float mix, float exponent) {
            float valueClamped = rack::math::clamp(value, 0.f, 1.f);
            float mixClamped = rack::math::clamp(mix, 0.f, 1.f);
            float exponentClamped = rack::math::clamp(exponent, 0.125f, 8.f);
            float shaped = std::pow(valueClamped, exponentClamped);
            return rack::math::crossfade(valueClamped, shaped, mixClamped);
        };

        auraAmount = applyCharacterDrama(auraRaw, AURA_DRAMA_MIX, AURA_DRAMA_EXP);
        orbitAmount = applyCharacterDrama(orbitRaw, ORBIT_DRAMA_MIX, ORBIT_DRAMA_EXP);
        tideAmount = applyCharacterDrama(tideRaw, TIDE_DRAMA_MIX, TIDE_DRAMA_EXP);

        // Drive amount now follows knob/CV (chaos amount)
        float driveTarget = clamp(params[CHAOS_AMOUNT_PARAM].getValue(), 0.f, 1.f);
        // Let the chaos CV add some drive excitement too
        if (inputs[CHAOS_RATE_CV_INPUT].isConnected()) {
            driveTarget += clamp(inputs[CHAOS_RATE_CV_INPUT].getVoltage(0) / 10.f, -1.f, 1.f) * 0.25f; // small CV influence
        }
        driveTarget = clamp(driveTarget, 0.f, 1.25f);
        smoothedDrive = driveSmooth.process(driveTarget, args.sampleTime, DRIVE_SMOOTH_TC);

        float rawEffectIntensity = std::max(auraAmount, std::max(orbitAmount, tideAmount));
        rawEffectIntensity = rack::math::clamp(rawEffectIntensity + smoothedDrive * 0.35f, 0.f, 1.35f);
        float effectGateTarget = rack::math::clamp(rawEffectIntensity, 0.f, 1.f);
        float effectBlend = effectGateSmooth.process(effectGateTarget, args.sampleTime, EFFECT_GATE_SMOOTH_TC);

        float driveLight = smoothedDrive;
        float auraLight = auraRaw;
        float orbitLight = orbitRaw;
        float tideLight = tideRaw;

        float knobChaosRate = clamp(params[CHAOS_RATE_PARAM].getValue(), CHAOS_RATE_MIN_HZ, CHAOS_RATE_MAX_HZ);
        float chaosTarget = knobChaosRate;

        // Treat CHAOS_RATE_CV_INPUT as bipolar CV (continuous) instead of clock-only
        if (inputs[CHAOS_RATE_CV_INPUT].isConnected()) {
            float cvRate = inputs[CHAOS_RATE_CV_INPUT].getVoltage(0); // allow poly but use first channel as global rate mod
            // 10V moves full range; small bias for negative voltages
            chaosTarget += cvRate * 0.5f; // 2 V/Oct-ish scaling
        }

        chaosTarget = clamp(chaosTarget, CHAOS_RATE_MIN_HZ, CHAOS_RATE_MAX_HZ);
        chaosTargetRate = chaosTarget;
        float chaosRate = chaosRateSmooth.process(chaosTargetRate, args.sampleTime);
        chaosRate = clamp(chaosRate, CHAOS_RATE_MIN_HZ, CHAOS_RATE_MAX_HZ);

        // Store smoothed chaos rate for visualizer access
        smoothedChaosRate = chaosRate;

        float tideRateHz = 0.4f + tideAmount * 6.5f + chaosRate * 0.25f;
        tideRateHz = clamp(tideRateHz, 0.01f, 25.f);
        etherealPhase += tideRateHz * args.sampleTime * 2.f * M_PI;
        if (etherealPhase > 2.f * M_PI) {
            etherealPhase = std::fmod(etherealPhase, 2.f * M_PI);
        }
        float swirlSeed = etherealPhase;

        // Store effective resonance values for visualizer (always calculate, even without inputs)
        float displayResonanceA = resonanceA;
        float displayResonanceB = resonanceB;

        if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
            float attenA = params[RESONANCE_A_ATTEN_PARAM].getValue();
            displayResonanceA += inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(0) * attenA / 10.f;
        }
        displayResonanceA = clamp(displayResonanceA, 0.707f, 1.5f);

        if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
            float attenB = params[RESONANCE_B_ATTEN_PARAM].getValue();
            displayResonanceB += inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(0) * attenB / 10.f;
        }
        displayResonanceB = clamp(displayResonanceB, 0.707f, 1.5f);

        effectiveResonanceA = displayResonanceA;
        effectiveResonanceB = displayResonanceB;

        // Store effective cutoff values for visualizer (always calculate, even without inputs)
        float displayCutoffA = cutoffA;
        float displayCutoffB = cutoffB;

        if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
            float attenA = params[CUTOFF_A_ATTEN_PARAM].getValue();
            displayCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(0) * attenA / 10.f;
        }
        displayCutoffA = clamp(displayCutoffA, 0.f, 1.f);

        if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
            float attenB = params[CUTOFF_B_ATTEN_PARAM].getValue();
            displayCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(0) * attenB / 10.f;
        }
        displayCutoffB = clamp(displayCutoffB, 0.f, 1.f);

        effectiveCutoffA = displayCutoffA;
        effectiveCutoffB = displayCutoffB;

        // Determine number of polyphonic channels (up to 8)
        int channelsA = inputs[AUDIO_A_INPUT].getChannels();
        int channelsB = inputs[AUDIO_B_INPUT].getChannels();
        int channels = std::max(channelsA, channelsB);
        channels = std::min(channels, shapetaker::PolyphonicProcessor::MAX_VOICES); // Limit to max voices
        
        // If no inputs connected, set no output channels
        if (!inputs[AUDIO_A_INPUT].isConnected() && !inputs[AUDIO_B_INPUT].isConnected()) {
            outputs[AUDIO_A_OUTPUT].setChannels(0);
            outputs[AUDIO_B_OUTPUT].setChannels(0);
        } else {
            // Set output channel count
            outputs[AUDIO_A_OUTPUT].setChannels(channels);
            outputs[AUDIO_B_OUTPUT].setChannels(channels);

            // Process each voice
            for (int c = 0; c < channels; c++) {
                // Get audio inputs for this voice
                float audioA = 0.f, audioB = 0.f;
                bool hasInputA = inputs[AUDIO_A_INPUT].isConnected();
                bool hasInputB = inputs[AUDIO_B_INPUT].isConnected();
                
                if (hasInputA && hasInputB) {
                    audioA = inputs[AUDIO_A_INPUT].getVoltage(c);
                    audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                } else if (hasInputA) {
                    audioA = audioB = inputs[AUDIO_A_INPUT].getVoltage(c);
                } else if (hasInputB) {
                    audioA = audioB = inputs[AUDIO_B_INPUT].getVoltage(c);
                }
                
                // ============================================================================
                // LIQUID 6TH-ORDER FILTER PROCESSING
                // ============================================================================
                float processedA = audioA;
                float processedB = audioB;

                // Only process if we have valid, finite audio input
                if (std::isfinite(audioA) && std::isfinite(audioB)) {
                    float voiceCutoffA = cutoffA;
                    float voiceCutoffB = cutoffB;
                    float voiceResonanceA = resonanceA;
                    float voiceResonanceB = resonanceB;

                    if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
                        float attenA = params[CUTOFF_A_ATTEN_PARAM].getValue();
                        voiceCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(c) * attenA / 10.f;
                    }
                    voiceCutoffA = clamp(voiceCutoffA, 0.f, 1.f);
                    // Apply per-voice smoothing to eliminate CV zipper noise
                    voiceCutoffA = cutoffASmoothers[c].process(voiceCutoffA, args.sampleTime, CUTOFF_CV_SMOOTH_TC);

                    if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
                        float attenB = params[CUTOFF_B_ATTEN_PARAM].getValue();
                        voiceCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(c) * attenB / 10.f;
                    }
                    voiceCutoffB = clamp(voiceCutoffB, 0.f, 1.f);
                    // Apply per-voice smoothing to eliminate CV zipper noise
                    voiceCutoffB = cutoffBSmoothers[c].process(voiceCutoffB, args.sampleTime, CUTOFF_CV_SMOOTH_TC);

                    if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
                        float attenA = params[RESONANCE_A_ATTEN_PARAM].getValue();
                        voiceResonanceA += inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(c) * attenA / 10.f;
                    }
                    voiceResonanceA = clamp(voiceResonanceA, 0.707f, 1.5f);

                    if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
                        float attenB = params[RESONANCE_B_ATTEN_PARAM].getValue();
                        voiceResonanceB += inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(c) * attenB / 10.f;
                    }
                    voiceResonanceB = clamp(voiceResonanceB, 0.707f, 1.5f);

                    // Convert cutoff from normalized (0-1) to Hz (20Hz - 20480Hz)
                    float cutoffAHz = 20.f * std::pow(2.f, voiceCutoffA * 10.f);
                    float cutoffBHz = 20.f * std::pow(2.f, voiceCutoffB * 10.f);

                    // Process through liquid filters with drive tied to Chaos Amount
                    float driveScaled = 1.f + smoothedDrive * 7.f;
                    processedA = filtersA[c].process(audioA, cutoffAHz, voiceResonanceA, driveScaled);
                    processedB = filtersB[c].process(audioB, cutoffBHz, voiceResonanceB, driveScaled);

                    float resonanceNormA = clamp((voiceResonanceA - 0.707f) / (1.5f - 0.707f), 0.f, 1.f);
                    float resonanceNormB = clamp((voiceResonanceB - 0.707f) / (1.5f - 0.707f), 0.f, 1.f);

                    constexpr float baseDelaySeconds[ETHEREAL_STAGES] = {0.018f, 0.031f, 0.047f, 0.085f};
                    constexpr float stageWeights[ETHEREAL_STAGES] = {0.55f, 0.38f, 0.26f, 0.19f};

                    auto processEthereal = [&](float input, float cutoffNorm, float resonanceNorm,
                                                EtherealVoiceState& state, float stereoSkew) {
                        float cutoffDrama = clamp(cutoffNorm, 0.f, 1.f);
                        float resonanceInfluence = clamp(resonanceNorm, 0.f, 1.f);
                        const float paramAlpha = args.sampleTime / (ETHEREAL_PARAM_SMOOTH_TC + args.sampleTime);
                        auto smoothScalar = [&](float& slot, float target, float minVal, float maxVal) -> float {
                            slot += paramAlpha * (target - slot);
                            slot = rack::math::clamp(slot, minVal, maxVal);
                            return slot;
                        };

                        float effectAmount = effectBlend;
                        float lowFreqBlend = rack::math::clamp((cutoffDrama - 0.18f) / 0.22f, 0.f, 1.f);
                        lowFreqBlend = lowFreqBlend * lowFreqBlend * (3.f - 2.f * lowFreqBlend);

                        float diffusionTarget = effectAmount * (0.32f
                            + auraAmount * 1.05f
                            + tideAmount * 0.28f
                            + resonanceInfluence * 0.25f
                            + smoothedDrive * 0.18f);
                        diffusionTarget = clamp(diffusionTarget, 0.f, 0.97f);
                        float diffusion = smoothScalar(state.smoothedDiffusion, diffusionTarget, 0.f, 0.97f);

                        float feedbackTarget = effectAmount * (0.18f
                            + orbitAmount * 0.9f
                            + auraAmount * 0.22f
                            + smoothedDrive * 0.25f);
                        // Higher ceiling but still clamped for stability
                        feedbackTarget = clamp(feedbackTarget, 0.f, 0.82f);
                        float feedback = smoothScalar(state.smoothedFeedback, feedbackTarget, 0.f, 0.82f);

                        float shimmerTarget = effectAmount * (tideAmount * 0.9f + auraAmount * 0.55f + smoothedDrive * 0.25f);
                        float shimmerLift = smoothScalar(state.smoothedShimmer, shimmerTarget, 0.f, 1.0f);

                        float haloBlendTarget = effectAmount * (0.4f
                            + auraAmount * 0.7f
                            + orbitAmount * 0.28f
                            + smoothedDrive * 0.22f);
                        haloBlendTarget = clamp(haloBlendTarget, 0.f, 0.95f);
                        float haloBlend = smoothScalar(state.smoothedHaloMix, haloBlendTarget, 0.f, 0.95f);

                        float modulationDepthTarget = effectAmount * (
                            0.0025f
                            + tideAmount * 0.006f
                            + auraAmount * 0.0035f
                            + smoothedDrive * 0.002f
                        ) * args.sampleRate;
                        float modDepth = smoothScalar(state.smoothedModDepth, modulationDepthTarget, 0.f, args.sampleRate * 0.065f);

                        float stereoWidthTarget = stereoSkew * (0.35f + orbitAmount * 0.55f);
                        stereoWidthTarget += (orbitAmount - 0.5f) * 0.2f;
                        stereoWidthTarget = rack::math::clamp(stereoWidthTarget, -1.f, 1.f);
                        float effectiveStereoSkew = smoothScalar(state.smoothedStereoSkew, stereoWidthTarget, -1.f, 1.f);

                        float driftRate = clamp(0.15f + chaosRate * (0.25f + tideAmount * 0.2f) + 0.05f * cutoffDrama, 0.05f, 5.f);
                        float phaseIncrement = driftRate * args.sampleTime * 2.f * static_cast<float>(M_PI);

                        float feed = input;
                        float cloud = 0.f;

                        for (int s = 0; s < ETHEREAL_STAGES; ++s) {
                            state.modPhase[s] += phaseIncrement * (state.drift[s] + 0.15f * s);
                            if (state.modPhase[s] > 2.f * static_cast<float>(M_PI)) {
                                state.modPhase[s] -= 2.f * static_cast<float>(M_PI);
                            }

                            float baseDelay = baseDelaySeconds[s] * args.sampleRate;
                            float sizeScale = 1.05f + auraAmount * 0.5f + tideAmount * 0.35f;
                            float stereoBias = 1.f + effectiveStereoSkew * 0.08f * (s + 1);
                            float tonalBias = 0.85f + 0.3f * cutoffDrama + 0.12f * resonanceNorm;
                            float modulation = std::sin(state.modPhase[s] + state.phaseOffset[s] + swirlSeed * (0.28f + 0.18f * s));
                            float delayTarget = baseDelay * sizeScale * tonalBias * stereoBias + modulation * modDepth * (1.2f + 0.25f * s);
                            float maxDelaySamples = std::max(state.delays[s].maxDelaySamples(), 16.f);
                            float smoothedDelay = smoothScalar(state.smoothedDelay[s], delayTarget, 12.f, maxDelaySamples);

                            float stageBlendBase =
                                effectAmount * (0.3f + 0.12f * s)
                                + auraAmount * 0.24f
                                + orbitAmount * 0.18f
                                + smoothedDrive * 0.18f;
                            float stageBlendTarget = clamp(stageBlendBase, 0.f, 0.82f);
                            stageBlendTarget = smoothScalar(state.smoothedStageBlend[s], stageBlendTarget, 0.f, 0.8f);
                            float stageBlend = stageBlendTarget;

                            float stageInputDry = rack::math::crossfade(feed, cloud, stageBlend);
                            float stageInput = stageInputDry + shimmerLift * cloud * 0.25f;
                            float stageOutputRaw = state.delays[s].process(stageInput, smoothedDelay, feedback);
                            // DC block and soft saturation to prevent long-tail runaway
                            constexpr float dcCoeff = 0.995f;
                            state.dcBlock[s] = dcCoeff * state.dcBlock[s] + (1.f - dcCoeff) * stageOutputRaw;
                            float stageOutput = stageOutputRaw - state.dcBlock[s];
                            stageOutput = std::tanh(stageOutput * 0.85f) * 1.05f;
                            cloud += stageOutput * stageWeights[s];
                            feed = rack::math::crossfade(feed, stageOutput, diffusion);
                        }

                        float halo = cloud * (1.f + shimmerLift * 0.6f);
                        halo = rack::math::clamp(halo, -14.f, 14.f);
                        float haloMix = clamp(haloBlend, 0.f, 0.95f);

                        // Treat halo as an additive ambience but allow wetter blends for "juicy" tone.
                        float wetGain = haloMix * (0.75f + 0.32f * effectAmount + 0.18f * smoothedDrive);
                        float blended = input * (1.f - haloMix * 0.25f) + wetGain * halo;

                        // Subtle makeup gain tied to Aura keeps perceived loudness closer to the dry tone
                        constexpr float MAKEUP_MAX_DB = 3.6f;
                        float makeupLinear = std::pow(10.f, (auraAmount * MAKEUP_MAX_DB) / 20.f);
                        blended *= makeupLinear;
                        return rack::math::clamp(blended, -14.f, 14.f);
                    };

                    processedA = processEthereal(processedA, voiceCutoffA, resonanceNormA, etherealVoicesA[c], -0.6f);
                    processedB = processEthereal(processedB, voiceCutoffB, resonanceNormB, etherealVoicesB[c], 0.6f);
                }
                
                
                // Set output voltages for this voice
                outputs[AUDIO_A_OUTPUT].setVoltage(processedA, c);
                outputs[AUDIO_B_OUTPUT].setVoltage(processedB, c);
            }

        }
        
        // Update lights to show parameter values with Chiaroscuro-style color progression
        // Both lights: Teal (0%) -> Bright blue-purple (50%) -> Dark purple (100%)
        const float base_brightness = 0.4f;
        const float max_brightness = base_brightness;
        
        // Drive light retains the original Chiaroscuro color morph
        float driveValue = driveLight;
        float drive_red, drive_green, drive_blue;
        if (driveValue <= 0.5f) {
            drive_red = driveValue * 2.0f * max_brightness;
            drive_green = max_brightness;
            drive_blue = max_brightness;
        } else {
            drive_red = max_brightness;
            drive_green = 2.0f * (1.0f - driveValue) * max_brightness;
            drive_blue = max_brightness * (1.7f - driveValue * 0.7f);
        }
        lights[CHAOS_LIGHT].setBrightness(drive_red);
        lights[CHAOS_LIGHT + 1].setBrightness(drive_green);
        lights[CHAOS_LIGHT + 2].setBrightness(drive_blue);

        lights[AURA_LIGHT].setBrightness(auraLight);
        lights[ORBIT_LIGHT].setBrightness(orbitLight);
        lights[TIDE_LIGHT].setBrightness(tideLight);
    }
    
    // Integrate with Rack's default "Randomize" menu item
    void onRandomize() override {
        // Randomize filter parameters with musical ranges
        std::mt19937 rng(rack::random::u32());
        
        // Cutoff frequencies - keep in musical range (100Hz to 8kHz)
        std::uniform_real_distribution<float> cutoffDist(0.2f, 0.9f);
        params[CUTOFF_A_PARAM].setValue(cutoffDist(rng));
        params[CUTOFF_B_PARAM].setValue(cutoffDist(rng));
        
        // Resonance - moderate range to avoid harsh sounds
        std::uniform_real_distribution<float> resDist(0.1f, 0.7f);
        params[RESONANCE_A_PARAM].setValue(resDist(rng));
        params[RESONANCE_B_PARAM].setValue(resDist(rng));
        
        // Highpass is now static at 12Hz - no randomization needed
        
        // Drive is fixed at maximum saturation; keep the stored parameter at 1.0
        params[CHAOS_AMOUNT_PARAM].setValue(1.f);
        std::uniform_real_distribution<float> auraDist(0.2f, 0.8f);
        std::uniform_real_distribution<float> orbitDist(0.1f, 0.7f);
        std::uniform_real_distribution<float> tideDist(0.2f, 0.9f);
        params[AURA_PARAM].setValue(auraDist(rng));
        params[ORBIT_PARAM].setValue(orbitDist(rng));
        params[TIDE_PARAM].setValue(tideDist(rng));
        
        // Rate parameters - varied but not too extreme
        std::uniform_real_distribution<float> rateDist(0.2f, 0.8f);
        params[CHAOS_RATE_PARAM].setValue(rateDist(rng));
        chaosTargetRate = clamp(params[CHAOS_RATE_PARAM].getValue(), CHAOS_RATE_MIN_HZ, CHAOS_RATE_MAX_HZ);
        smoothedChaosRate = chaosTargetRate;
        chaosRateSmooth.reset(chaosTargetRate);

        // Filter morph - full range for variety
        // Link switches - randomly enable/disable
        std::uniform_int_distribution<int> linkDist(0, 1);
        params[LINK_CUTOFF_PARAM].setValue((float)linkDist(rng));
        params[LINK_RESONANCE_PARAM].setValue((float)linkDist(rng));
    }
};

// Chaos visualizer widget - extracted to separate files
#include "involution/chaos_visualizer.hpp"
#include "involution/chaos_visualizer_impl.hpp"

// Custom SVG-based JewelLED for chaos light
struct ChaosJewelLED : ModuleLightWidget {
    ChaosJewelLED() {
        box.size = Vec(20, 20);  // Medium size

        // Try to load the medium jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));

        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }

        // Set up RGB colors for chaos activity
        addBaseColor(nvgRGB(255, 0, 0));   // Red
        addBaseColor(nvgRGB(0, 255, 0));   // Green
        addBaseColor(nvgRGB(0, 0, 255));   // Blue
    }

    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            // Fallback drawing if SVG doesn't load (medium size)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.5);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 6.5);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }

        ModuleLightWidget::draw(args);
    }
};

struct InvolutionWidget : ModuleWidget {
    InvolutionWidget(Involution* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Involution.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Create positioning helper from SVG panel
        using LayoutHelper = shapetaker::ui::LayoutHelper;
        auto centerPx = LayoutHelper::createCenterPxHelper(
            asset::plugin(pluginInstance, "res/panels/Involution.svg")
        );
        
        // Main Filter Section - using SVG parser for automatic positioning
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltHuge>(
            centerPx("cutoff_v", 24.026f, 24.174f),
            module, Involution::CUTOFF_A_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("resonance_v", 11.935f, 57.750f),
            module, Involution::RESONANCE_A_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltHuge>(
            centerPx("cutoff_z", 66.305f, 24.174f),
            module, Involution::CUTOFF_B_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(
            centerPx("resonance_z", 78.397f, 57.750f),
            module, Involution::RESONANCE_B_PARAM));
        
        // Link switches - using SVG parser with fallbacks
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("link_cutoff", 45.166f, 29.894f),
            module, Involution::LINK_CUTOFF_PARAM));
        addParam(createParamCentered<ShapetakerVintageRussianToggle>(
            centerPx("link_resonance", 45.166f, 84.630f),
            module, Involution::LINK_RESONANCE_PARAM));

        // Attenuverters for CV inputs
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_v_atten", 9.027f, 41.042f),
            module, Involution::CUTOFF_A_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_v_atten", 11.935f, 76.931f),
            module, Involution::RESONANCE_A_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_z_atten", 81.305f, 41.042f),
            module, Involution::CUTOFF_B_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_z_atten", 78.397f, 76.931f),
            module, Involution::RESONANCE_B_ATTEN_PARAM));

        // Character Controls - using SVG parser with fallbacks
        // Highpass is now static at 12Hz - no control needed
        // Drive knob is fixed; reuse area for Aura/Orbit/Tide controls
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(centerPx("aura_knob", 15.910f, 94.088f), module, Involution::AURA_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(centerPx("orbit_knob", 45.166f, 94.088f), module, Involution::ORBIT_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltSmall>(centerPx("tide_knob", 74.422f, 94.088f), module, Involution::TIDE_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(centerPx("chaos_rate_knob", 60.922f, 108.088f), module, Involution::CHAOS_RATE_PARAM));
        
        // Chaos Visualizer - using SVG parser for automatic positioning
        ChaosVisualizer* chaosViz = new ChaosVisualizer(module);
        Vec screenCenter = centerPx("oscope_screen",
                                    std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN());
        if (!std::isfinite(screenCenter.x) || !std::isfinite(screenCenter.y)) {
            screenCenter = centerPx("resonance_a_cv-1", 45.166f, 57.750f);
        }
        chaosViz->box.pos = Vec(screenCenter.x - 86.5, screenCenter.y - 69); // Center the 173x138 screen
        addChild(chaosViz);
        
        // Chaos light - using SVG parser and custom JewelLED
        addChild(createLightCentered<ChaosJewelLED>(centerPx("chaos_light", 29.559f, 103.546f), module, Involution::CHAOS_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(centerPx("aura_light", 15.910f, 103.546f), module, Involution::AURA_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(centerPx("orbit_light", 45.166f, 103.546f), module, Involution::ORBIT_LIGHT));
        addChild(createLightCentered<MediumLight<GreenLight>>(centerPx("tide_light", 74.422f, 103.546f), module, Involution::TIDE_LIGHT));

        // CV inputs - using SVG parser with updated coordinates
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("cutoff_v_cv", 24.027f, 44.322f), module, Involution::CUTOFF_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("resonance_v_cv", 24.027f, 68.931f), module, Involution::RESONANCE_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("cutoff_z_cv", 66.305f, 44.322f), module, Involution::CUTOFF_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("resonance_z_cv", 66.305f, 68.931f), module, Involution::RESONANCE_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("aura_cv", 15.910f, 84.630f), module, Involution::AURA_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("orbit_cv", 45.166f, 84.630f), module, Involution::ORBIT_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("tide_cv", 74.422f, 84.630f), module, Involution::TIDE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("chaos_cv", 60.922f, 84.630f), module, Involution::CHAOS_RATE_CV_INPUT));
        // Audio I/O - direct millimeter coordinates
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio_l_input", 10.276f, 118.977f), module, Involution::AUDIO_A_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio_r_input", 27.721f, 119.245f), module, Involution::AUDIO_B_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio_l_output", 63.436f, 119.347f), module, Involution::AUDIO_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio_r_output", 81.706f, 119.347f), module, Involution::AUDIO_B_OUTPUT));
    }
    
    // Draw panel background texture to match other modules
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/vcv-panel-background.png"));
        if (bg) {
            NVGpaint paint = nvgImagePattern(args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, bg->handle, 1.0f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        }
        ModuleWidget::draw(args);
    }
};

Model* modelInvolution = createModel<Involution, InvolutionWidget>("Involution");
