#include "plugin.hpp"
#include <cmath>
#include <random>

struct Involution : Module {
    enum ParamId {
        CUTOFF_A_PARAM,
        RESONANCE_A_PARAM,
        CUTOFF_B_PARAM,
        RESONANCE_B_PARAM,
        // New magical parameters
        CHAOS_AMOUNT_PARAM,
        CHAOS_RATE_PARAM,
        FILTER_MORPH_PARAM,
        // Phaser controls
        PHASER_FREQUENCY_PARAM,
        PHASER_FEEDBACK_PARAM,
        PHASER_MIX_PARAM,
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
        CHAOS_CV_INPUT,
        CHAOS_RATE_CV_INPUT,
        FILTER_MORPH_CV_INPUT,
        PHASER_FREQUENCY_CV_INPUT,
        PHASER_FEEDBACK_CV_INPUT,
        PHASER_MIX_CV_INPUT,
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
        LIGHTS_LEN
    };

    // Using extracted utility classes
    shapetaker::ChorusEffect chorus;

    // Using enhanced morphing filter from utilities
    shapetaker::MorphingFilter filterA, filterB, highpassFilter;

    // Using phaser effect from utilities
    shapetaker::PhaserEffect phaser;

    // Using delay and envelope follower from utilities
    shapetaker::VoiceArray<shapetaker::ShimmerDelay> shimmerA, shimmerB;
    shapetaker::VoiceArray<shapetaker::EnvelopeFollower> envelopeA, envelopeB;

    // Filter chains using utilities - 3 cascaded filters per voice for 6th order
    shapetaker::VoiceArray<std::array<shapetaker::MorphingFilter, 3>> lowpassA, lowpassB;
    shapetaker::VoiceArray<std::array<shapetaker::MorphingFilter, 2>> highpassA, highpassB;
    
    // Magical components using utilities
    shapetaker::VoiceArray<shapetaker::PhaserEffect> phaserA, phaserB;
    
    
    // Internal LFO and chaos
    float chaosPhaseA = 0.f, chaosPhaseB = 0.f;
    std::default_random_engine chaosGenerator;
    std::uniform_real_distribution<float> chaosDistribution{-1.f, 1.f};
    
    // LFO phases for rate controls
    float chaosLFOPhase = 0.f;
    
    // Schmitt trigger for phase invert button
    
    // Using fast smoother from utilities
    
    shapetaker::FastSmoother cutoffASmooth, cutoffBSmooth, resonanceASmooth, resonanceBSmooth;
    shapetaker::FastSmoother chaosSmooth, chaosRateSmooth;
    shapetaker::FastSmoother morphSmooth;
    shapetaker::FastSmoother phaserFreqSmooth, phaserFeedbackSmooth, phaserMixSmooth;
    
    // Parameter change tracking for bidirectional linking
    float lastCutoffA = -1.f, lastCutoffB = -1.f;
    float lastResonanceA = -1.f, lastResonanceB = -1.f;
    bool lastLinkCutoff = false, lastLinkResonance = false;

    // Smoothed values for visualizer access
    float smoothedChaosRate = 0.5f;
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
        
        // New magical parameters
        configParam(CHAOS_AMOUNT_PARAM, 0.f, 1.f, 0.15f, "Chaos Amount", "%", 0.f, 100.f);
        configParam(CHAOS_RATE_PARAM, 0.01f, 10.f, 0.5f, "Chaos LFO Rate", " Hz", 0.f, 0.f);

        // Custom parameter quantity to show real-time chaos rate including CV modulation
        struct ChaosRateQuantity : engine::ParamQuantity {
            float getDisplayValue() override {
                if (!module) return ParamQuantity::getDisplayValue();

                Involution* inv = static_cast<Involution*>(module);
                // Use the same calculation as the main process function
                float displayRate = getValue(); // Base knob value
                if (inv->inputs[Involution::CHAOS_RATE_CV_INPUT].isConnected()) {
                    float rateCv = inv->inputs[Involution::CHAOS_RATE_CV_INPUT].getVoltage();
                    displayRate += rateCv * 0.5f; // Same CV scaling as main module
                }
                displayRate = clamp(displayRate, 0.001f, 20.0f);

                return displayRate;
            }
        };

        // Replace the default param quantity with our custom one
        paramQuantities[CHAOS_RATE_PARAM] = new ChaosRateQuantity;
        paramQuantities[CHAOS_RATE_PARAM]->module = this;
        paramQuantities[CHAOS_RATE_PARAM]->paramId = CHAOS_RATE_PARAM;
        paramQuantities[CHAOS_RATE_PARAM]->minValue = 0.01f;
        paramQuantities[CHAOS_RATE_PARAM]->maxValue = 10.f;
        paramQuantities[CHAOS_RATE_PARAM]->defaultValue = 0.5f;
        paramQuantities[CHAOS_RATE_PARAM]->name = "Chaos LFO Rate";
        paramQuantities[CHAOS_RATE_PARAM]->unit = " Hz";
        configParam(FILTER_MORPH_PARAM, 0.f, 1.f, 0.f, "Filter Type Morph");
        
        // Phaser controls
        configParam(PHASER_FREQUENCY_PARAM, 0.f, 1.f, 0.5f, "Phaser Frequency", " Hz", 0.f, 50.f, 2000.f);
        configParam(PHASER_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Phaser Feedback", "%", 0.f, 100.f);
        configParam(PHASER_MIX_PARAM, 0.f, 1.f, 0.f, "Phaser Mix", "%", 0.f, 100.f);
        
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
        configInput(CHAOS_CV_INPUT, "Chaos CV");
        configInput(CHAOS_RATE_CV_INPUT, "Chaos Rate CV");
        configInput(FILTER_MORPH_CV_INPUT, "Filter Morph CV");
        configInput(PHASER_FREQUENCY_CV_INPUT, "Phaser Frequency CV");
        configInput(PHASER_FEEDBACK_CV_INPUT, "Phaser Feedback CV");
        configInput(PHASER_MIX_CV_INPUT, "Phaser Mix CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");
        
        configLight(CHAOS_LIGHT, "Chaos Activity");
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
        float chaosAmount = chaosSmooth.process(params[CHAOS_AMOUNT_PARAM].getValue(), args.sampleTime);
        float baseChaosRate = chaosRateSmooth.process(params[CHAOS_RATE_PARAM].getValue(), args.sampleTime);

        // Add CV modulation to chaos rate (additive, ±5Hz range)
        float chaosRate = baseChaosRate;
        if (inputs[CHAOS_RATE_CV_INPUT].isConnected()) {
            float rateCv = inputs[CHAOS_RATE_CV_INPUT].getVoltage(); // ±10V
            chaosRate += rateCv * 0.5f; // ±5Hz range when using ±10V CV
        }
        chaosRate = clamp(chaosRate, 0.001f, 20.0f); // Keep within reasonable bounds

        // Store smoothed chaos rate for visualizer access
        smoothedChaosRate = chaosRate;

        // Store effective resonance values for visualizer (always calculate, even without inputs)
        float displayResonanceA = resonanceASmooth.process(params[RESONANCE_A_PARAM].getValue(), args.sampleTime);
        float displayResonanceB = resonanceBSmooth.process(params[RESONANCE_B_PARAM].getValue(), args.sampleTime);

        if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
            float attenA = params[RESONANCE_A_ATTEN_PARAM].getValue();
            displayResonanceA += inputs[RESONANCE_A_CV_INPUT].getVoltage(0) * attenA / 10.f;
        }
        displayResonanceA = clamp(displayResonanceA, 0.707f, 1.5f);

        if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
            float attenB = params[RESONANCE_B_ATTEN_PARAM].getValue();
            displayResonanceB += inputs[RESONANCE_B_CV_INPUT].getVoltage(0) * attenB / 10.f;
        }
        displayResonanceB = clamp(displayResonanceB, 0.707f, 1.5f);

        effectiveResonanceA = displayResonanceA;
        effectiveResonanceB = displayResonanceB;

        // Store effective cutoff values for visualizer (always calculate, even without inputs)
        float displayCutoffA = cutoffASmooth.process(params[CUTOFF_A_PARAM].getValue(), args.sampleTime);
        float displayCutoffB = cutoffBSmooth.process(params[CUTOFF_B_PARAM].getValue(), args.sampleTime);

        if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
            float attenA = params[CUTOFF_A_ATTEN_PARAM].getValue();
            displayCutoffA += inputs[CUTOFF_A_CV_INPUT].getVoltage(0) * attenA / 10.f;
        }
        displayCutoffA = clamp(displayCutoffA, 0.f, 1.f);

        if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
            float attenB = params[CUTOFF_B_ATTEN_PARAM].getValue();
            displayCutoffB += inputs[CUTOFF_B_CV_INPUT].getVoltage(0) * attenB / 10.f;
        }
        displayCutoffB = clamp(displayCutoffB, 0.f, 1.f);

        effectiveCutoffA = displayCutoffA;
        effectiveCutoffB = displayCutoffB;

        float filterMorph = morphSmooth.process(params[FILTER_MORPH_PARAM].getValue(), args.sampleTime);
        
        // Add CV modulation to filter morph
        if (inputs[FILTER_MORPH_CV_INPUT].isConnected()) {
            float morphCv = inputs[FILTER_MORPH_CV_INPUT].getVoltage() / 10.f; // 10V -> 1.0
            filterMorph += morphCv;
            filterMorph = clamp(filterMorph, 0.f, 1.f);
        }
        
        // Get phaser parameters with CV modulation
        float phaserFreq = phaserFreqSmooth.process(params[PHASER_FREQUENCY_PARAM].getValue(), args.sampleTime);
        if (inputs[PHASER_FREQUENCY_CV_INPUT].isConnected()) {
            float freqCv = inputs[PHASER_FREQUENCY_CV_INPUT].getVoltage() / 10.f;
            phaserFreq += freqCv;
            phaserFreq = clamp(phaserFreq, 0.f, 1.f);
        }
        
        float phaserFeedback = phaserFeedbackSmooth.process(params[PHASER_FEEDBACK_PARAM].getValue(), args.sampleTime);
        if (inputs[PHASER_FEEDBACK_CV_INPUT].isConnected()) {
            float feedbackCv = inputs[PHASER_FEEDBACK_CV_INPUT].getVoltage() / 10.f;
            phaserFeedback += feedbackCv;
            phaserFeedback = clamp(phaserFeedback, 0.f, 1.f);
        }
        
        float phaserMix = phaserMixSmooth.process(params[PHASER_MIX_PARAM].getValue(), args.sampleTime);
        if (inputs[PHASER_MIX_CV_INPUT].isConnected()) {
            float mixCv = inputs[PHASER_MIX_CV_INPUT].getVoltage() / 10.f;
            phaserMix += mixCv;
            phaserMix = clamp(phaserMix, 0.f, 1.f);
        }
        
        // Convert phaser frequency parameter to Hz (50Hz to 2000Hz range)
        float phaserHz = 50.f + phaserFreq * 1950.f;
        
        // Static 12dB/octave highpass at 12Hz to remove DC and subsonic frequencies
        const float highpassCutoff = 12.0f;

        // Update LFO phases for rate controls
        chaosLFOPhase += chaosRate * args.sampleTime * 2.f * M_PI;
        if (chaosLFOPhase >= 2.f * M_PI) chaosLFOPhase -= 2.f * M_PI;
        
        
        // Generate LFO values (sine waves)
        float chaosLFO = std::sin(chaosLFOPhase);
        
        // Update internal chaos oscillators at base rate
        chaosPhaseA += 0.31f * args.sampleTime * 2.f * M_PI;
        chaosPhaseB += 0.37f * args.sampleTime * 2.f * M_PI;
        if (chaosPhaseA >= 2.f * M_PI) chaosPhaseA -= 2.f * M_PI;
        if (chaosPhaseB >= 2.f * M_PI) chaosPhaseB -= 2.f * M_PI;

        // Determine number of polyphonic channels (up to 6)
        int channelsA = inputs[AUDIO_A_INPUT].getChannels();
        int channelsB = inputs[AUDIO_B_INPUT].getChannels();
        int channels = std::max(channelsA, channelsB);
        channels = std::min(channels, 6); // Limit to 6 voices
        
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
                
                
                // Chaos modulation with LFO (per voice with slight phase offset)
                float chaosA = 0.f, chaosB = 0.f;
                if (chaosAmount > 0.f) {
                    // Apply LFO modulation to chaos amount
                    float modulatedChaosAmount = chaosAmount * (0.5f + 0.5f * chaosLFO);
                    
                    chaosA = (std::sin(chaosPhaseA + c * 0.1f) + chaosDistribution(chaosGenerator) * 0.3f) * modulatedChaosAmount * 0.2f;
                    chaosB = (std::sin(chaosPhaseB + c * 0.13f) + chaosDistribution(chaosGenerator) * 0.3f) * modulatedChaosAmount * 0.2f;
                    
                    if (inputs[CHAOS_CV_INPUT].isConnected()) {
                        float chaosCv = inputs[CHAOS_CV_INPUT].getPolyVoltage(c) / 10.f;
                        chaosA += chaosCv * modulatedChaosAmount * 0.3f;
                        chaosB += chaosCv * modulatedChaosAmount * 0.3f;
                    }
                }
                
                // Apply modulations to cutoff frequencies (per voice)
                float voiceCutoffA = cutoffA;
                float voiceCutoffB = cutoffB;
                float voiceResonanceA = resonanceA;
                float voiceResonanceB = resonanceB;
                
                if (inputs[CUTOFF_A_CV_INPUT].isConnected()) {
                    float attenA = params[CUTOFF_A_ATTEN_PARAM].getValue();
                    voiceCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(c) * attenA / 10.f;
                }
                voiceCutoffA += chaosA;
                voiceCutoffA = clamp(voiceCutoffA, 0.f, 1.f);

                if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
                    float attenB = params[CUTOFF_B_ATTEN_PARAM].getValue();
                    voiceCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(c) * attenB / 10.f;
                }
                voiceCutoffB += chaosB;
                voiceCutoffB = clamp(voiceCutoffB, 0.f, 1.f);
                
                // Apply modulations to resonance (per voice) with attenuverters
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

                // add a gentle low-frequency emphasis when the cutoff is closing
                float lowFocusA = std::pow(1.f - voiceCutoffA, 2.0f);
                float lowFocusB = std::pow(1.f - voiceCutoffB, 2.0f);
                voiceResonanceA = clamp(voiceResonanceA + lowFocusA * 0.18f, 0.707f, 1.6f);
                voiceResonanceB = clamp(voiceResonanceB + lowFocusB * 0.18f, 0.707f, 1.6f);
                
                // Apply cross-feedback from previous samples with stability limiting
                // Reduce feedback amount when resonance is high to prevent runaway
                float resonanceFactorA = (voiceResonanceA - 0.707f) / (1.6f - 0.707f); // 0.0 to ~1.0
                float resonanceFactorB = (voiceResonanceB - 0.707f) / (1.6f - 0.707f);
                float maxResonanceFactor = std::max(resonanceFactorA, resonanceFactorB);
                
                
                // Apply minimal safety reduction only in extreme cases
                float effectsLevel = chaosAmount;
                // Minimal safety only in extreme cases - effects nearly maxed AND max resonance
                if (effectsLevel > 0.95f && voiceResonanceA > 1.48f) {
                    float reductionFactor = 1.f - (effectsLevel - 0.95f) * 0.1f; // Max 0.5% reduction
                    reductionFactor = clamp(reductionFactor, 0.995f, 1.f);
                    voiceResonanceA *= reductionFactor;
                    voiceResonanceB *= reductionFactor;
                }

                // Calculate frequencies for this voice with adjusted curve
                float curveA = std::pow(voiceCutoffA, 1.6f);
                float curveB = std::pow(voiceCutoffB, 1.6f);
                float freqA = std::pow(2.f, curveA * 9.5f) * 20.f;
                float freqB = std::pow(2.f, curveB * 9.5f) * 20.f;
                
                freqA = std::min(freqA, args.sampleRate * 0.49f);
                freqB = std::min(freqB, args.sampleRate * 0.49f);

                // Update filter coefficients for this voice
                for (int i = 0; i < 3; i++) {
                    lowpassA[c][i].setMorphingFilter(freqA, voiceResonanceA, filterMorph, args.sampleRate);
                    lowpassB[c][i].setMorphingFilter(freqB, voiceResonanceB, filterMorph, args.sampleRate);
                }

                // Configure static 12dB/octave highpass filters
                for (int i = 0; i < 2; i++) {
                    // Use a stable highpass implementation with fixed low resonance
                    highpassA[c][i].setStableHighpass(highpassCutoff, args.sampleRate);
                    highpassB[c][i].setStableHighpass(highpassCutoff, args.sampleRate);
                }

                // Process Channel A for this voice
                float processedA = audioA;
                
                // Apply static 12dB highpass first (always active)
                for (int i = 0; i < 2; i++) {
                    processedA = highpassA[c][i].process(processedA);
                }
                
                // Apply morphing filters
                for (int i = 0; i < 3; i++) {
                    processedA = lowpassA[c][i].process(processedA);
                }

                float driveStrengthA = 1.2f + resonanceFactorA * 0.4f;
                float satA = std::tanh(processedA * driveStrengthA);
                float compensatedA = satA / std::tanh(driveStrengthA);
                float mixA = clamp(0.10f + resonanceFactorA * 0.12f, 0.f, 1.f);
                processedA += (compensatedA - processedA) * mixA;
                
                
                // Process Channel B for this voice
                float processedB = audioB;
                
                // Apply static 12dB highpass first (always active)
                for (int i = 0; i < 2; i++) {
                    processedB = highpassB[c][i].process(processedB);
                }
                
                // Apply morphing filters
                for (int i = 0; i < 3; i++) {
                    processedB = lowpassB[c][i].process(processedB);
                }

                float driveStrengthB = 1.2f + resonanceFactorB * 0.4f;
                float satB = std::tanh(processedB * driveStrengthB);
                float compensatedB = satB / std::tanh(driveStrengthB);
                float mixB = clamp(0.10f + resonanceFactorB * 0.12f, 0.f, 1.f);
                processedB += (compensatedB - processedB) * mixB;
                
                
                // Apply dedicated manual phaser effect
                if (phaserMix > 0.001f) { // Only process if mix is turned up
                    processedA = phaserA[c].process(processedA, phaserHz, phaserFeedback, phaserMix, args.sampleRate);
                    processedB = phaserB[c].process(processedB, phaserHz, phaserFeedback, phaserMix, args.sampleRate);
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
        
        // Chaos light with Chiaroscuro progression
        float chaosValue = params[CHAOS_AMOUNT_PARAM].getValue();
        float chaos_red, chaos_green, chaos_blue;
        if (chaosValue <= 0.5f) {
            // 0 to 0.5: Teal to bright blue-purple
            chaos_red = chaosValue * 2.0f * max_brightness;
            chaos_green = max_brightness;
            chaos_blue = max_brightness;
        } else {
            // 0.5 to 1.0: Bright blue-purple to dark purple
            chaos_red = max_brightness;
            chaos_green = 2.0f * (1.0f - chaosValue) * max_brightness;
            chaos_blue = max_brightness * (1.7f - chaosValue * 0.7f);
        }
        lights[CHAOS_LIGHT].setBrightness(chaos_red);
        lights[CHAOS_LIGHT + 1].setBrightness(chaos_green);
        lights[CHAOS_LIGHT + 2].setBrightness(chaos_blue);
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
        
        // Magical parameters - moderate amounts for musicality
        std::uniform_real_distribution<float> magicDist(0.0f, 0.6f);
        params[CHAOS_AMOUNT_PARAM].setValue(magicDist(rng));
        
        // Rate parameters - varied but not too extreme
        std::uniform_real_distribution<float> rateDist(0.2f, 0.8f);
        params[CHAOS_RATE_PARAM].setValue(rateDist(rng));
        
        // Filter morph - full range for variety
        std::uniform_real_distribution<float> morphDist(0.0f, 1.0f);
        params[FILTER_MORPH_PARAM].setValue(morphDist(rng));
        
        // Phaser parameters - moderate for musicality
        std::uniform_real_distribution<float> phaserFreqDist(0.3f, 0.8f);
        params[PHASER_FREQUENCY_PARAM].setValue(phaserFreqDist(rng));
        
        std::uniform_real_distribution<float> phaserFbDist(0.0f, 0.5f);
        params[PHASER_FEEDBACK_PARAM].setValue(phaserFbDist(rng));
        
        std::uniform_real_distribution<float> phaserMixDist(0.2f, 0.8f);
        params[PHASER_MIX_PARAM].setValue(phaserMixDist(rng));
        
        // Link switches - randomly enable/disable
        std::uniform_int_distribution<int> linkDist(0, 1);
        params[LINK_CUTOFF_PARAM].setValue((float)linkDist(rng));
        params[LINK_RESONANCE_PARAM].setValue((float)linkDist(rng));
    }
};

// Fractal Chaos Visualizer in vintage oscilloscope style
struct ChaosVisualizer : Widget {
    Involution* module;
    float time = 0.0f;
    float chaosPhase = 0.0f; // Smooth phase accumulator for chaos animation
    float filterMorphPhase = 0.0f; // Smooth phase for filter morph rotation
    float cutoffPhase = 0.0f; // Smooth phase for cutoff-based movement
    float resonancePhase = 0.0f; // Smooth phase for resonance effects
    shapetaker::FastSmoother visualChaosRateSmoother; // Dedicated smoother for visual smoothness
    shapetaker::FastSmoother visualCutoffASmoother, visualCutoffBSmoother;
    shapetaker::FastSmoother visualResonanceASmoother, visualResonanceBSmoother;
    shapetaker::FastSmoother visualFilterMorphSmoother, visualChaosAmountSmoother;
    
    ChaosVisualizer(Involution* module) : module(module) {
        box.size = Vec(173, 138); // 15% larger chaos visualizer screen
    }
    
    void step() override {
        Widget::step();
        float deltaTime = 1.0f / APP->window->getMonitorRefreshRate();
        time += deltaTime;

        // Accumulate all phases smoothly using current smoothed values
        if (module) {
            // Chaos phase accumulation
            float rawChaosRate = module->params[Involution::CHAOS_RATE_PARAM].getValue();
            if (module->inputs[Involution::CHAOS_RATE_CV_INPUT].isConnected()) {
                float rateCv = module->inputs[Involution::CHAOS_RATE_CV_INPUT].getVoltage();
                rawChaosRate += rateCv * 0.5f;
            }
            rawChaosRate = clamp(rawChaosRate, 0.001f, 20.0f);
            float smoothedChaosRate = visualChaosRateSmoother.process(rawChaosRate, deltaTime);
            chaosPhase += smoothedChaosRate * deltaTime;

            // Filter morph phase accumulation
            float smoothedFilterMorph = visualFilterMorphSmoother.process(
                module->params[Involution::FILTER_MORPH_PARAM].getValue(), deltaTime);
            filterMorphPhase += (smoothedFilterMorph + 0.1f) * 0.5f * deltaTime;

            // Cutoff phase accumulation
            float smoothedCutoffA = visualCutoffASmoother.process(module->effectiveCutoffA, deltaTime);
            float smoothedCutoffB = visualCutoffBSmoother.process(module->effectiveCutoffB, deltaTime);
            cutoffPhase += (smoothedCutoffA + smoothedCutoffB) * 0.2f * deltaTime;

            // Resonance phase accumulation
            float smoothedResonanceA = visualResonanceASmoother.process(module->effectiveResonanceA, deltaTime);
            float smoothedResonanceB = visualResonanceBSmoother.process(module->effectiveResonanceB, deltaTime);
            float avgResonance = (smoothedResonanceA + smoothedResonanceB) * 0.5f;
            float resonanceActivity = (avgResonance - 0.707f) * 2.0f;
            resonanceActivity = std::max(resonanceActivity, 0.0f);
            resonancePhase += resonanceActivity * 0.4f * deltaTime;
        }
    }
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        
        NVGcontext* vg = args.vg;
        float width = box.size.x;
        float height = box.size.y;
        float centerX = width / 2.0f;
        float centerY = height / 2.0f;
        float diamondSize = std::min(width, height) * 0.9f; // Bigger diamond
        
        // Draw diamond-shaped oscilloscope bezel
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - diamondSize/2);
        nvgLineTo(vg, centerX + diamondSize/2, centerY);
        nvgLineTo(vg, centerX, centerY + diamondSize/2);
        nvgLineTo(vg, centerX - diamondSize/2, centerY);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGB(40, 40, 45));
        nvgFill(vg);
        
        // Inner diamond shadow
        float innerSize = diamondSize * 0.9f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - innerSize/2);
        nvgLineTo(vg, centerX + innerSize/2, centerY);
        nvgLineTo(vg, centerX, centerY + innerSize/2);
        nvgLineTo(vg, centerX - innerSize/2, centerY);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGB(25, 25, 30));
        nvgFill(vg);
        
        // Diamond screen background with backlit effect
        float screenSize = innerSize * 0.85f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - screenSize/2);
        nvgLineTo(vg, centerX + screenSize/2, centerY);
        nvgLineTo(vg, centerX, centerY + screenSize/2);
        nvgLineTo(vg, centerX - screenSize/2, centerY);
        nvgClosePath(vg);
        
        // Enhanced backlit gradient - blue theme with more pronounced center glow
        NVGpaint backlitPaint = nvgRadialGradient(vg, centerX, centerY, 0, screenSize * 0.6f,
                                                 nvgRGB(18, 22, 28), nvgRGB(8, 10, 12));
        nvgFillPaint(vg, backlitPaint);
        nvgFill(vg);
        
        // Add additional center hotspot for stronger backlit effect
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - screenSize/4);
        nvgLineTo(vg, centerX + screenSize/4, centerY);
        nvgLineTo(vg, centerX, centerY + screenSize/4);
        nvgLineTo(vg, centerX - screenSize/4, centerY);
        nvgClosePath(vg);
        NVGpaint centerGlow = nvgRadialGradient(vg, centerX, centerY, 0, screenSize * 0.25f,
                                               nvgRGBA(25, 30, 40, 120), nvgRGBA(25, 30, 40, 0));
        nvgFillPaint(vg, centerGlow);
        nvgFill(vg);
        
        // Draw diamond grid lines properly within diamond bounds - blue theme
        nvgStrokeColor(vg, nvgRGBA(0, 100, 255, 20));
        nvgStrokeWidth(vg, 0.5f);
        
        // Draw grid lines that stay within the diamond shape
        float halfSize = screenSize / 2.0f;
        
        // Horizontal lines through diamond center
        for (int i = -2; i <= 2; i++) {
            if (i == 0) continue;
            float y = centerY + i * screenSize * 0.15f;
            float width = halfSize * (1.0f - abs(y - centerY) / halfSize); // Triangle width at this y
            
            nvgBeginPath(vg);
            nvgMoveTo(vg, centerX - width, y);
            nvgLineTo(vg, centerX + width, y);
            nvgStroke(vg);
        }
        
        // Vertical lines through diamond center  
        for (int i = -2; i <= 2; i++) {
            if (i == 0) continue;
            float x = centerX + i * screenSize * 0.15f;
            float height = halfSize * (1.0f - abs(x - centerX) / halfSize); // Triangle height at this x
            
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, centerY - height);
            nvgLineTo(vg, x, centerY + height);
            nvgStroke(vg);
        }
        
        if (module) {
            // Get ALL parameters with visual smoothing for glitch-free transitions
            float deltaTime = 1.0f / APP->window->getMonitorRefreshRate();

            float chaosAmount = visualChaosAmountSmoother.process(
                module->params[Involution::CHAOS_AMOUNT_PARAM].getValue(), deltaTime);
            float filterMorph = visualFilterMorphSmoother.process(
                module->params[Involution::FILTER_MORPH_PARAM].getValue(), deltaTime);
            float cutoffA = visualCutoffASmoother.process(
                module->effectiveCutoffA, deltaTime); // Use CV-modulated values
            float cutoffB = visualCutoffBSmoother.process(
                module->effectiveCutoffB, deltaTime); // Use CV-modulated values
            float resonanceA = visualResonanceASmoother.process(
                module->effectiveResonanceA, deltaTime); // Use CV-modulated values
            float resonanceB = visualResonanceBSmoother.process(
                module->effectiveResonanceB, deltaTime); // Use CV-modulated values

            // Always draw squares - all parameters now smoothed for visual stability
            drawSquareChaos(vg, centerX, centerY, screenSize * 0.4f, chaosAmount, chaosPhase,
                          filterMorph, cutoffA, cutoffB, resonanceA, resonanceB,
                          filterMorphPhase, cutoffPhase, resonancePhase);

        }
        
        // --- Vintage CRT Effects ---
        
        // Sharp CRT Phosphor Glow Effect - More defined edges
        
        // Outer glow layer - tighter, more controlled
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - screenSize/2 * 1.2f);
        nvgLineTo(vg, centerX + screenSize/2 * 1.2f, centerY);
        nvgLineTo(vg, centerX, centerY + screenSize/2 * 1.2f);
        nvgLineTo(vg, centerX - screenSize/2 * 1.2f, centerY);
        nvgClosePath(vg);
        NVGpaint outerGlow = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.35f, screenSize * 0.55f, 
                                              nvgRGBA(0, 110, 140, 60), nvgRGBA(0, 30, 40, 0));
        nvgFillPaint(vg, outerGlow);
        nvgFill(vg);
        
        // Inner glow layer - sharp, intense core
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - screenSize/2 * 1.05f);
        nvgLineTo(vg, centerX + screenSize/2 * 1.05f, centerY);
        nvgLineTo(vg, centerX, centerY + screenSize/2 * 1.05f);
        nvgLineTo(vg, centerX - screenSize/2 * 1.05f, centerY);
        nvgClosePath(vg);
        NVGpaint innerGlow = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.25f, screenSize * 0.38f, 
                                              nvgRGBA(0, 150, 200, 120), nvgRGBA(0, 45, 60, 0));
        nvgFillPaint(vg, innerGlow);
        nvgFill(vg);
        
        // CRT Spherical Bulging Effect
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - screenSize/2 * 0.9f);
        nvgLineTo(vg, centerX + screenSize/2 * 0.9f, centerY);
        nvgLineTo(vg, centerX, centerY + screenSize/2 * 0.9f);
        nvgLineTo(vg, centerX - screenSize/2 * 0.9f, centerY);
        nvgClosePath(vg);
        NVGpaint bulgeHighlight = nvgRadialGradient(vg,
            centerX - screenSize * 0.15f, centerY - screenSize * 0.15f, // Offset to top-left
            screenSize * 0.05f, screenSize * 0.4f,
            nvgRGBA(255, 255, 255, 25), // Subtle white highlight
            nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(vg, bulgeHighlight);
        nvgFill(vg);
        
        // Scanlines for authentic CRT feel
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 40));
        nvgStrokeWidth(vg, 0.5f);
        for (int i = 0; i < 20; i++) {
            float y = centerY - screenSize/2 + (i / 19.0f) * screenSize;
            float lineWidth = screenSize * (1.0f - 2.0f * abs(y - centerY) / screenSize);
            if (lineWidth > 0) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, centerX - lineWidth/2, y);
                nvgLineTo(vg, centerX + lineWidth/2, y);
                nvgStroke(vg);
            }
        }
        
        // Subtle vignette darkening at edges
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, centerY - screenSize/2);
        nvgLineTo(vg, centerX + screenSize/2, centerY);
        nvgLineTo(vg, centerX, centerY + screenSize/2);
        nvgLineTo(vg, centerX - screenSize/2, centerY);
        nvgClosePath(vg);
        NVGpaint vignette = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.2f, screenSize * 0.5f,
                                             nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 30));
        nvgFillPaint(vg, vignette);
        nvgFill(vg);
    }
    
private:
    void drawSquareChaos(NVGcontext* vg, float cx, float cy, float maxRadius,
                        float chaosAmount, float chaosPhase, float filterMorph,
                        float cutoffA, float cutoffB, float resonanceA, float resonanceB,
                        float filterMorphPhase, float cutoffPhase, float resonancePhase) {

        // Calculate total activity level (including filter params for base activity)
        float totalActivity = chaosAmount + (cutoffA + cutoffB) * 0.2f;

        // Resonance adds significant visual complexity
        float avgResonance = (resonanceA + resonanceB) * 0.5f;
        float resonanceActivity = (avgResonance - 0.707f) * 2.0f; // Normalize from Q range to activity
        resonanceActivity = std::max(resonanceActivity, 0.0f); // Only positive resonance adds activity
        totalActivity += resonanceActivity * 0.3f; // Resonance contributes to overall activity

        // Always show an interesting display - higher minimum activity level
        totalActivity = std::max(totalActivity, 0.35f);

        // Number of squares - more responsive to all parameters with higher base
        int baseSquares = 45 + (int)(filterMorph * 20); // Higher base and more filter morph influence
        int resonanceSquares = (int)(resonanceActivity * 80); // Resonance adds additional squares
        int activitySquares = (int)(totalActivity * 120);
        int numSquares = baseSquares + activitySquares + resonanceSquares;
        numSquares = clamp(numSquares, 45, 220); // Higher maximum to accommodate resonance
        
        for (int i = 0; i < numSquares; i++) {
            // Generate square position within diamond bounds
            float angle = (i / (float)numSquares) * 2.0f * M_PI * 3.7f; // Multiple spirals
            
            // Always have base rotation for visual interest, plus smooth phase-based modulation
            angle += time * 0.3f; // Always-present base rotation for visual interest
            angle += chaosPhase * 1.0f; // Primary chaos rotation (already smooth)
            angle += chaosPhase * 0.8f; // Secondary chaos rotation (already smooth)
            angle += filterMorphPhase; // Filter morph rotation (now smooth)
            angle += cutoffPhase; // Cutoff-based movement (now smooth)
            angle += resonancePhase; // Resonance spinning motion (now smooth)
            
            // Radius varies with parameters and time
            float baseRadius = (i / (float)numSquares) * maxRadius;
            float radiusVar = sinf(time * 3.0f + i * 0.2f) * maxRadius * 0.2f * chaosAmount;
            // Resonance adds pulsing radius variation - higher Q creates more intense pulsing
            float resonancePulse = sinf(time * 4.0f + i * 0.5f) * maxRadius * 0.15f * resonanceActivity;
            float radius = baseRadius + radiusVar + resonancePulse;
            radius *= (0.8f + cutoffA * 0.2f + cutoffB * 0.2f + resonanceActivity * 0.1f); // Cutoff and resonance influence
            
            float x = cx + cosf(angle) * radius;
            float y = cy + sinf(angle) * radius;
            
            // Proper diamond bounds checking - diamond has pointed ends
            // Diamond equation: |x-cx|/maxRadius + |y-cy|/maxRadius <= 1
            float distanceFromCenterX = abs(x - cx);
            float distanceFromCenterY = abs(y - cy);
            float diamondDistance = distanceFromCenterX / maxRadius + distanceFromCenterY / maxRadius;
            
            // If outside diamond, scale position back to fit
            if (diamondDistance > 0.9f) {
                float scale = 0.9f / diamondDistance;
                x = cx + (x - cx) * scale;
                y = cy + (y - cy) * scale;
            }
            
            // Square size varies with parameters
            float baseSize = 1.5f + chaosAmount * 3.0f;
            float sizeVar = sinf(time * 4.0f + i * 0.3f + filterMorph * 5.0f) * 1.0f;
            // Resonance makes squares larger and more dynamic
            float resonanceSize = resonanceActivity * 2.0f + sinf(time * 6.0f + i * 0.4f) * resonanceActivity * 1.5f;
            float squareSize = baseSize + sizeVar + resonanceSize;
            squareSize = clamp(squareSize, 0.5f, 6.0f); // Higher max to accommodate resonance

            // Calculate square color - simpler, better colors with resonance influence
            float hue = fmodf(time * 30.0f + i * 15.0f + filterMorph * 180.0f + resonanceActivity * 120.0f, 360.0f);
            
            // Brightness influenced by multiple parameters - updated for blue theme
            float baseBrightness = 0.3f; // Always show something faint
            float activityBrightness = chaosAmount * 0.7f;
            float filterBrightness = (cutoffA + cutoffB) * 0.1f;
            // Resonance creates bright, intense highlights
            float resonanceBrightness = resonanceActivity * 0.5f + sinf(time * 8.0f + i * 0.6f) * resonanceActivity * 0.3f;

            float brightness = baseBrightness + activityBrightness + filterBrightness + resonanceBrightness;
            brightness = clamp(brightness, 0.2f, 1.2f); // Higher max for resonance highlights
            brightness *= (1.0f - (radius / maxRadius) * 0.3f); // Fade toward edges
            
            NVGcolor color;
            if (hue < 120.0f) {
                // Blue to cyan range (shifted from green)
                float t = hue / 120.0f;
                color = nvgRGBA((int)(0 * brightness), (int)((100 + t * 155) * brightness), 
                               (int)(255 * brightness), (int)(brightness * 255));
            } else if (hue < 240.0f) {
                // Cyan to purple range  
                float t = (hue - 120.0f) / 120.0f;
                color = nvgRGBA((int)(t * 100 * brightness), (int)((255 - t * 100) * brightness), 
                               (int)(255 * brightness), (int)(brightness * 255));
            } else {
                // Purple to blue range
                float t = (hue - 240.0f) / 120.0f;
                color = nvgRGBA((int)((150 - t * 150) * brightness), (int)(0 * brightness), 
                               (int)(255 * brightness), (int)(brightness * 255));
            }
            
            // Draw the square
            nvgBeginPath(vg);
            nvgRect(vg, x - squareSize/2, y - squareSize/2, squareSize, squareSize);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
    }
    
};



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

        // Parse SVG panel for precise positioning
        shapetaker::ui::LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/Involution.svg"));

        // Helper function that uses SVG parser with fallbacks to direct millimeter coordinates
        // Usage: centerPx("svg_element_id", fallback_x_mm, fallback_y_mm)
        // When SVG elements are added to the panel with matching IDs, they will automatically
        // position controls precisely. Until then, fallback coordinates are used.
        auto centerPx = [&](const std::string& id, float defx, float defy) -> Vec {
            return parser.centerPx(id, defx, defy);
        };
        
        // Main Filter Section - using SVG parser for automatic positioning
        addParam(createParamCentered<ShapetakerKnobLarge>(
            centerPx("cutoff_a", 24.027f, 25.232f),
            module, Involution::CUTOFF_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(
            centerPx("resonance_a", 11.935f, 56.941f),
            module, Involution::RESONANCE_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobLarge>(
            centerPx("cutoff_b", 66.305f, 25.232f),
            module, Involution::CUTOFF_B_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(
            centerPx("resonance_b", 78.397f, 56.941f),
            module, Involution::RESONANCE_B_PARAM));
        
        // Link switches - using SVG parser with fallbacks
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("link_cutoff", 45.166f, 26.154f),
            module, Involution::LINK_CUTOFF_PARAM));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("link_resonance", 45.166f, 82.513f),
            module, Involution::LINK_RESONANCE_PARAM));

        // Attenuverters for CV inputs
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_a_atten", 9.027f, 40.232f),
            module, Involution::CUTOFF_A_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_a_atten", 13.026f, 74.513f),
            module, Involution::RESONANCE_A_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("cutoff_b_atten", 81.305f, 40.232f),
            module, Involution::CUTOFF_B_ATTEN_PARAM));
        addParam(createParamCentered<ShapetakerAttenuverterOscilloscope>(
            centerPx("resonance_b_atten", 79.253f, 74.513f),
            module, Involution::RESONANCE_B_ATTEN_PARAM));

        // Character Controls - using SVG parser with fallbacks
        // Highpass is now static at 12Hz - no control needed
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(
            centerPx("filter_morph", 45.166f, 98.585f),
            module, Involution::FILTER_MORPH_PARAM));
        
        // Special Effects - using SVG parser with updated coordinates
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("chaos_amount", 15.910f, 92.085f), module, Involution::CHAOS_AMOUNT_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(centerPx("chaos_rate", 71.897f, 92.085f), module, Involution::CHAOS_RATE_PARAM));
        
        // Chaos Visualizer - using SVG parser for automatic positioning
        ChaosVisualizer* chaosViz = new ChaosVisualizer(module);
        Vec screenCenter = centerPx("oscope_screen", 45.166f, 56.941f);
        chaosViz->box.pos = Vec(screenCenter.x - 86.5, screenCenter.y - 69); // Center the 173x138 screen
        addChild(chaosViz);
        
        // Chaos light - using SVG parser and custom JewelLED
        addChild(createLightCentered<ChaosJewelLED>(centerPx("chaos_light", 30.538f, 103.088f), module, Involution::CHAOS_LIGHT));

        // CV inputs - using SVG parser with updated coordinates
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("cutoff_a_cv", 26.538f, 43.513f), module, Involution::CUTOFF_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("resonance_a_cv", 26.538f, 70.513f), module, Involution::RESONANCE_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("cutoff_b_cv", 63.794f, 43.513f), module, Involution::CUTOFF_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("resonance_b_cv", 63.794f, 70.513f), module, Involution::RESONANCE_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("chaos_amount_cv", 59.794f, 103.088f), module, Involution::CHAOS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("chaos_lfo_cv", 30.794f, 103.088f), module, Involution::CHAOS_RATE_CV_INPUT));

        // Audio I/O - direct millimeter coordinates
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio_a_input", 17.579f, 117.102f), module, Involution::AUDIO_A_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("audio_b_input", 36.530f, 117.102f), module, Involution::AUDIO_B_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio_a_output", 55.480f, 117.102f), module, Involution::AUDIO_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("audio_b_output", 74.431f, 117.102f), module, Involution::AUDIO_B_OUTPUT));
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
