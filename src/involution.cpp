#include "plugin.hpp"
#include <cmath>
#include <random>

struct Involution : Module {
    enum ParamId {
        CUTOFF_A_PARAM,
        RESONANCE_A_PARAM,
        CUTOFF_B_PARAM,
        RESONANCE_B_PARAM,
        HIGHPASS_CUTOFF_PARAM,
        // New magical parameters
        CROSS_FEEDBACK_PARAM,
        CHAOS_AMOUNT_PARAM,
        CHAOS_RATE_PARAM,
        SHIMMER_AMOUNT_PARAM,
        SHIMMER_RATE_PARAM,
        FILTER_MORPH_PARAM,
        // Phaser controls
        PHASER_FREQUENCY_PARAM,
        PHASER_FEEDBACK_PARAM,
        PHASER_MIX_PARAM,
        // Link switches
        LINK_CUTOFF_PARAM,
        LINK_RESONANCE_PARAM,
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
        SHIMMER_CV_INPUT,
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
        SHIMMER_LIGHT,
        SHIMMER_LIGHT_GREEN,
        SHIMMER_LIGHT_BLUE,
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
    
    // Cross-feedback delay lines (per voice)
    float crossFeedbackA[6] = {}, crossFeedbackB[6] = {};
    
    // Internal LFO and chaos
    float chaosPhaseA = 0.f, chaosPhaseB = 0.f;
    std::default_random_engine chaosGenerator;
    std::uniform_real_distribution<float> chaosDistribution{-1.f, 1.f};
    
    // LFO phases for rate controls
    float chaosLFOPhase = 0.f;
    float shimmerLFOPhase = 0.f;
    
    // Schmitt trigger for phase invert button
    
    // Using fast smoother from utilities
    
    shapetaker::FastSmoother cutoffASmooth, cutoffBSmooth, resonanceASmooth, resonanceBSmooth;
    shapetaker::FastSmoother chaosSmooth, chaosRateSmooth, shimmerSmooth, shimmerRateSmooth;
    shapetaker::FastSmoother morphSmooth, crossFeedbackSmooth;
    shapetaker::FastSmoother phaserFreqSmooth, phaserFeedbackSmooth, phaserMixSmooth;
    
    // Parameter change tracking for bidirectional linking
    float lastCutoffA = -1.f, lastCutoffB = -1.f;
    float lastResonanceA = -1.f, lastResonanceB = -1.f;
    bool lastLinkCutoff = false, lastLinkResonance = false;

    Involution() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(CUTOFF_A_PARAM, 0.f, 1.f, 1.f, "Filter A Cutoff", " Hz", std::pow(2.f, 10.f), 20.f);
        configParam(RESONANCE_A_PARAM, 0.707f, 1.5f, 0.707f, "Filter A Resonance");
        configParam(CUTOFF_B_PARAM, 0.f, 1.f, 1.f, "Filter B Cutoff", " Hz", std::pow(2.f, 10.f), 20.f);
        configParam(RESONANCE_B_PARAM, 0.707f, 1.5f, 0.707f, "Filter B Resonance");
        configParam(HIGHPASS_CUTOFF_PARAM, 0.f, 1.f, 0.f, "Highpass Cutoff", " Hz", 0.f, 50.f);
        
        // New magical parameters
        configParam(CROSS_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Cross Feedback", "%", 0.f, 100.f);
        configParam(CHAOS_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Chaos Amount", "%", 0.f, 100.f);
        configParam(CHAOS_RATE_PARAM, 0.01f, 10.f, 0.5f, "Chaos LFO Rate", " Hz", 0.f, 0.f);
        configParam(SHIMMER_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Shimmer", "%", 0.f, 100.f);
        configParam(SHIMMER_RATE_PARAM, 0.01f, 8.f, 0.3f, "Shimmer LFO Rate", " Hz", 0.f, 0.f);
        configParam(FILTER_MORPH_PARAM, 0.f, 1.f, 0.f, "Filter Type Morph");
        
        // Phaser controls
        configParam(PHASER_FREQUENCY_PARAM, 0.f, 1.f, 0.5f, "Phaser Frequency", " Hz", 0.f, 50.f, 2000.f);
        configParam(PHASER_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Phaser Feedback", "%", 0.f, 100.f);
        configParam(PHASER_MIX_PARAM, 0.f, 1.f, 0.f, "Phaser Mix", "%", 0.f, 100.f);
        
        // Link switches
        configSwitch(LINK_CUTOFF_PARAM, 0.f, 1.f, 0.f, "Link Cutoff Frequencies", {"Independent", "Linked"});
        configSwitch(LINK_RESONANCE_PARAM, 0.f, 1.f, 0.f, "Link Resonance Amounts", {"Independent", "Linked"});

        configInput(AUDIO_A_INPUT, "Audio A");
        configInput(AUDIO_B_INPUT, "Audio B");
        configInput(CUTOFF_A_CV_INPUT, "Filter A Cutoff CV");
        configInput(RESONANCE_A_CV_INPUT, "Filter A Resonance CV");
        configInput(CUTOFF_B_CV_INPUT, "Filter B Cutoff CV");
        configInput(RESONANCE_B_CV_INPUT, "Filter B Resonance CV");
        configInput(CHAOS_CV_INPUT, "Chaos CV");
        configInput(SHIMMER_CV_INPUT, "Shimmer CV");
        configInput(FILTER_MORPH_CV_INPUT, "Filter Morph CV");
        configInput(PHASER_FREQUENCY_CV_INPUT, "Phaser Frequency CV");
        configInput(PHASER_FEEDBACK_CV_INPUT, "Phaser Feedback CV");
        configInput(PHASER_MIX_CV_INPUT, "Phaser Mix CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");
        
        configLight(CHAOS_LIGHT, "Chaos Activity");
        configLight(SHIMMER_LIGHT, "Shimmer Activity");
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
        float crossFeedback = crossFeedbackSmooth.process(params[CROSS_FEEDBACK_PARAM].getValue(), args.sampleTime);
        float chaosAmount = chaosSmooth.process(params[CHAOS_AMOUNT_PARAM].getValue(), args.sampleTime);
        float chaosRate = chaosRateSmooth.process(params[CHAOS_RATE_PARAM].getValue(), args.sampleTime);
        float shimmerAmount = shimmerSmooth.process(params[SHIMMER_AMOUNT_PARAM].getValue(), args.sampleTime);
        float shimmerRate = shimmerRateSmooth.process(params[SHIMMER_RATE_PARAM].getValue(), args.sampleTime);
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
        
        float highpassCutoff = params[HIGHPASS_CUTOFF_PARAM].getValue() * 50.f;

        // Update LFO phases for rate controls
        chaosLFOPhase += chaosRate * args.sampleTime * 2.f * M_PI;
        if (chaosLFOPhase >= 2.f * M_PI) chaosLFOPhase -= 2.f * M_PI;
        
        shimmerLFOPhase += shimmerRate * args.sampleTime * 2.f * M_PI;
        if (shimmerLFOPhase >= 2.f * M_PI) shimmerLFOPhase -= 2.f * M_PI;
        
        // Generate LFO values (sine waves)
        float chaosLFO = std::sin(chaosLFOPhase);
        float shimmerLFO = std::sin(shimmerLFOPhase);
        
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
                    voiceCutoffA += inputs[CUTOFF_A_CV_INPUT].getPolyVoltage(c) / 10.f;
                }
                voiceCutoffA += chaosA;
                voiceCutoffA = clamp(voiceCutoffA, 0.f, 1.f);
                
                if (inputs[CUTOFF_B_CV_INPUT].isConnected()) {
                    voiceCutoffB += inputs[CUTOFF_B_CV_INPUT].getPolyVoltage(c) / 10.f;
                }
                voiceCutoffB += chaosB;
                voiceCutoffB = clamp(voiceCutoffB, 0.f, 1.f);
                
                // Apply modulations to resonance (per voice)
                if (inputs[RESONANCE_A_CV_INPUT].isConnected()) {
                    voiceResonanceA += inputs[RESONANCE_A_CV_INPUT].getPolyVoltage(c);
                }
                voiceResonanceA = clamp(voiceResonanceA, 0.707f, 1.5f);
                
                if (inputs[RESONANCE_B_CV_INPUT].isConnected()) {
                    voiceResonanceB += inputs[RESONANCE_B_CV_INPUT].getPolyVoltage(c);
                }
                voiceResonanceB = clamp(voiceResonanceB, 0.707f, 1.5f);
                
                // Apply cross-feedback from previous samples with stability limiting
                // Reduce feedback amount when resonance is high to prevent runaway
                float resonanceFactorA = (voiceResonanceA - 0.707f) / (1.5f - 0.707f); // 0.0 to 1.0
                float resonanceFactorB = (voiceResonanceB - 0.707f) / (1.5f - 0.707f); // 0.0 to 1.0
                float maxResonanceFactor = std::max(resonanceFactorA, resonanceFactorB);
                
                // Reduce cross feedback when resonance is high
                float safeCrossFeedback = crossFeedback;
                if (maxResonanceFactor > 0.6f) {
                    // Start reducing feedback at 60% resonance, more aggressively at higher resonance
                    float reduction = (maxResonanceFactor - 0.6f) / 0.4f; // 0.0 to 1.0
                    safeCrossFeedback *= (1.0f - reduction * 0.7f); // Reduce by up to 70%
                }
                
                // Apply cross-feedback with safety limiting
                audioA += clamp(crossFeedbackB[c], -2.0f, 2.0f) * safeCrossFeedback * 0.2f;
                audioB += clamp(crossFeedbackA[c], -2.0f, 2.0f) * safeCrossFeedback * 0.2f;
                
                // Apply minimal safety reduction only in extreme cases
                float effectsLevel = chaosAmount + shimmerAmount;
                // Minimal safety only in extreme cases - effects nearly maxed AND max resonance
                if (effectsLevel > 0.95f && voiceResonanceA > 1.45f) {
                    float reductionFactor = 1.f - (effectsLevel - 0.95f) * 0.1f; // Max 0.5% reduction
                    reductionFactor = clamp(reductionFactor, 0.995f, 1.f);
                    voiceResonanceA *= reductionFactor;
                    voiceResonanceB *= reductionFactor;
                }

                // Calculate frequencies for this voice with adjusted curve
                // Use a gentler square curve that still drops faster in middle but not too aggressive
                float curveA = voiceCutoffA * voiceCutoffA; // Square curve - less aggressive than cubic
                float curveB = voiceCutoffB * voiceCutoffB;
                float freqA = std::pow(2.f, curveA * 10.f) * 20.f;
                float freqB = std::pow(2.f, curveB * 10.f) * 20.f;
                
                freqA = std::min(freqA, args.sampleRate * 0.49f);
                freqB = std::min(freqB, args.sampleRate * 0.49f);

                // Update filter coefficients for this voice
                for (int i = 0; i < 3; i++) {
                    lowpassA[c][i].setMorphingFilter(freqA, voiceResonanceA, filterMorph, args.sampleRate);
                    lowpassB[c][i].setMorphingFilter(freqB, voiceResonanceB, filterMorph, args.sampleRate);
                }

                if (highpassCutoff > 0.f) {
                    for (int i = 0; i < 2; i++) {
                        // Use a stable highpass implementation with fixed low resonance
                        highpassA[c][i].setStableHighpass(highpassCutoff, args.sampleRate);
                        highpassB[c][i].setStableHighpass(highpassCutoff, args.sampleRate);
                    }
                }

                // Process Channel A for this voice
                float processedA = audioA;
                
                // Apply highpass first
                if (highpassCutoff > 0.f) {
                    for (int i = 0; i < 2; i++) {
                        processedA = highpassA[c][i].process(processedA);
                    }
                }
                
                // Apply morphing filters
                for (int i = 0; i < 3; i++) {
                    processedA = lowpassA[c][i].process(processedA);
                }
                
                // Apply shimmer with LFO modulation
                if (shimmerAmount > 0.f) {
                    // LFO modulates shimmer amount
                    float modulatedShimmerAmount = shimmerAmount * (0.5f + 0.5f * shimmerLFO);
                    
                    // Add CV modulation
                    if (inputs[SHIMMER_CV_INPUT].isConnected()) {
                        float shimmerCv = inputs[SHIMMER_CV_INPUT].getPolyVoltage(c) / 10.f;
                        modulatedShimmerAmount += shimmerCv * 0.5f;
                        modulatedShimmerAmount = clamp(modulatedShimmerAmount, 0.f, 1.f);
                    }
                    
                    float baseDelayTime = 0.03f + modulatedShimmerAmount * 0.07f;
                    float shimmerOut = shimmerA[c].process(processedA, baseDelayTime, 0.3f, modulatedShimmerAmount);
                    processedA += shimmerOut * modulatedShimmerAmount * 0.4f;
                }
                
                // Process Channel B for this voice
                float processedB = audioB;
                
                // Apply highpass first
                if (highpassCutoff > 0.f) {
                    for (int i = 0; i < 2; i++) {
                        processedB = highpassB[c][i].process(processedB);
                    }
                }
                
                // Apply morphing filters
                for (int i = 0; i < 3; i++) {
                    processedB = lowpassB[c][i].process(processedB);
                }
                
                // Apply shimmer with LFO modulation
                if (shimmerAmount > 0.f) {
                    // LFO modulates shimmer amount
                    float modulatedShimmerAmount = shimmerAmount * (0.5f + 0.5f * shimmerLFO);
                    
                    // Add CV modulation
                    if (inputs[SHIMMER_CV_INPUT].isConnected()) {
                        float shimmerCv = inputs[SHIMMER_CV_INPUT].getPolyVoltage(c) / 10.f;
                        modulatedShimmerAmount += shimmerCv * 0.5f;
                        modulatedShimmerAmount = clamp(modulatedShimmerAmount, 0.f, 1.f);
                    }
                    
                    float baseDelayTime = 0.03f + modulatedShimmerAmount * 0.07f;
                    float shimmerOut = shimmerB[c].process(processedB, baseDelayTime, 0.3f, modulatedShimmerAmount);
                    processedB += shimmerOut * modulatedShimmerAmount * 0.4f;
                }
                
                // Apply dedicated manual phaser effect
                if (phaserMix > 0.001f) { // Only process if mix is turned up
                    processedA = phaserA[c].process(processedA, phaserHz, phaserFeedback, phaserMix, args.sampleRate);
                    processedB = phaserB[c].process(processedB, phaserHz, phaserFeedback, phaserMix, args.sampleRate);
                }
                
                // Store cross-feedback for next sample
                crossFeedbackA[c] = processedA;
                crossFeedbackB[c] = processedB;
                
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
        
        // Shimmer light with same Chiaroscuro progression
        float shimmerValue = params[SHIMMER_AMOUNT_PARAM].getValue();
        float shimmer_red, shimmer_green, shimmer_blue;
        if (shimmerValue <= 0.5f) {
            // 0 to 0.5: Teal to bright blue-purple
            shimmer_red = shimmerValue * 2.0f * max_brightness;
            shimmer_green = max_brightness;
            shimmer_blue = max_brightness;
        } else {
            // 0.5 to 1.0: Bright blue-purple to dark purple
            shimmer_red = max_brightness;
            shimmer_green = 2.0f * (1.0f - shimmerValue) * max_brightness;
            shimmer_blue = max_brightness * (1.7f - shimmerValue * 0.7f);
        }
        lights[SHIMMER_LIGHT].setBrightness(shimmer_red);
        lights[SHIMMER_LIGHT + 1].setBrightness(shimmer_green);
        lights[SHIMMER_LIGHT + 2].setBrightness(shimmer_blue);
    }
};

// Fractal Chaos Visualizer in vintage oscilloscope style
struct ChaosVisualizer : Widget {
    Involution* module;
    float time = 0.0f;
    
    ChaosVisualizer(Involution* module) : module(module) {
        box.size = Vec(120, 100); // Bigger diamond display
    }
    
    void step() override {
        Widget::step();
        time += 1.0f / APP->window->getMonitorRefreshRate();
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
            // Get ALL parameters for comprehensive fractal control
            float chaosAmount = module->params[Involution::CHAOS_AMOUNT_PARAM].getValue();
            float chaosRate = module->params[Involution::CHAOS_RATE_PARAM].getValue();
            float shimmerAmount = module->params[Involution::SHIMMER_AMOUNT_PARAM].getValue();
            float shimmerRate = module->params[Involution::SHIMMER_RATE_PARAM].getValue();
            float crossFeedback = module->params[Involution::CROSS_FEEDBACK_PARAM].getValue();
            float filterMorph = module->params[Involution::FILTER_MORPH_PARAM].getValue();
            float cutoffA = module->params[Involution::CUTOFF_A_PARAM].getValue();
            float cutoffB = module->params[Involution::CUTOFF_B_PARAM].getValue();
            
            // Always draw squares
            drawSquareChaos(vg, centerX, centerY, screenSize * 0.4f, chaosAmount, chaosRate,
                          shimmerAmount, shimmerRate, crossFeedback, filterMorph, cutoffA, cutoffB);
            
            // Add pink ghost lines when magical effects are low
            float magicalActivity = chaosAmount + shimmerAmount + crossFeedback;
            if (magicalActivity < 0.4f) {
                drawPinkGhosts(vg, centerX, centerY, screenSize * 0.4f, magicalActivity,
                             filterMorph, cutoffA, cutoffB);
            }
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
                        float chaosAmount, float chaosRate, float shimmerAmount, 
                        float shimmerRate, float crossFeedback, float filterMorph,
                        float cutoffA, float cutoffB) {
        
        // Calculate total activity level (including filter params for base activity)
        float totalActivity = chaosAmount + shimmerAmount + crossFeedback + (cutoffA + cutoffB) * 0.2f;
        
        // Always show at least a faint display - minimum activity level
        totalActivity = std::max(totalActivity, 0.15f);
        
        // Number of squares - more responsive to all parameters
        int baseSquares = 25 + (int)(filterMorph * 15); // Filter morph adds base squares
        int activitySquares = (int)(totalActivity * 120);
        int numSquares = baseSquares + activitySquares;
        numSquares = clamp(numSquares, 25, 180);
        
        for (int i = 0; i < numSquares; i++) {
            // Generate square position within diamond bounds
            float angle = (i / (float)numSquares) * 2.0f * M_PI * 3.7f; // Multiple spirals
            
            // Rotation influenced by multiple parameters (not just chaos) - reduced max speed
            angle += time * chaosRate * 1.0f; // Chaos rate rotation (reduced from 2.0f)
            angle += time * shimmerRate * 0.8f; // Shimmer rate rotation (reduced from 1.5f)
            angle += time * (filterMorph + 0.1f) * 0.5f; // Filter morph adds base rotation (reduced from 0.8f)
            angle += time * (cutoffA + cutoffB) * 0.2f; // Cutoff frequencies add movement (reduced from 0.3f)
            
            // Radius varies with parameters and time
            float baseRadius = (i / (float)numSquares) * maxRadius;
            float radiusVar = sinf(time * 3.0f + i * 0.2f) * maxRadius * 0.2f * chaosAmount;
            float radius = baseRadius + radiusVar;
            radius *= (0.8f + cutoffA * 0.2f + cutoffB * 0.2f); // Cutoff influence
            
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
            float baseSize = 1.5f + shimmerAmount * 3.0f + crossFeedback * 2.0f;
            float sizeVar = sinf(time * 4.0f + i * 0.3f + filterMorph * 5.0f) * 1.0f;
            float squareSize = baseSize + sizeVar;
            squareSize = clamp(squareSize, 0.5f, 4.0f);
            
            // Calculate square color - simpler, better colors
            float hue = fmodf(time * 30.0f + i * 15.0f + filterMorph * 180.0f, 360.0f);
            
            // Brightness influenced by multiple parameters - updated for blue theme
            float baseBrightness = 0.3f; // Always show something faint
            float activityBrightness = chaosAmount * 0.4f + shimmerAmount * 0.3f + crossFeedback * 0.2f;
            float filterBrightness = (cutoffA + cutoffB) * 0.1f;
            
            float brightness = baseBrightness + activityBrightness + filterBrightness;
            brightness = clamp(brightness, 0.2f, 1.0f);
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
    
    void drawPinkGhosts(NVGcontext* vg, float cx, float cy, float maxRadius,
                       float magicalActivity, float filterMorph, float cutoffA, float cutoffB) {
        
        // Opacity inversely related to magical activity - more visible when effects are off
        float ghostOpacity = (0.4f - magicalActivity) / 0.4f; // 1.0 when magical = 0, 0.0 when magical = 0.4
        ghostOpacity = clamp(ghostOpacity, 0.0f, 1.0f);
        
        // Number of ghost lines based on filter activity
        int numGhosts = 3 + (int)((cutoffA + cutoffB + filterMorph) * 4); // 3-7 ghosts
        
        for (int i = 0; i < numGhosts; i++) {
            // Each ghost has its own phase and speed
            float ghostPhase = time * (0.5f + i * 0.1f + filterMorph * 0.3f);
            float ghostY = fmodf(ghostPhase, 4.0f) - 2.0f; // -2 to +2 cycle
            
            // Ghost starts from bottom and rises up, fading as it goes
            float normalizedY = (ghostY + 2.0f) / 4.0f; // 0 to 1
            float ghostCenterY = cy + (ghostY * maxRadius * 0.8f);
            
            // Ghost fades in at bottom, peaks in middle, fades out at top
            float fadeAlpha = 1.0f - abs(ghostY) / 2.0f;
            fadeAlpha = clamp(fadeAlpha, 0.0f, 1.0f);
            
            // X position drifts based on filter parameters
            float ghostDrift = sinf(time * 0.3f + i + cutoffA * 2.0f) * maxRadius * 0.3f;
            float ghostCenterX = cx + ghostDrift;
            
            // Ghost width varies with height and parameters
            float ghostWidth = maxRadius * (0.1f + cutoffB * 0.2f) * (1.0f + sinf(time + i) * 0.3f);
            
            // Calculate diamond bounds for this Y position
            float maxWidthAtY = maxRadius * (1.0f - abs(ghostCenterY - cy) / maxRadius);
            ghostWidth = std::min(ghostWidth, maxWidthAtY * 0.8f);
            
            if (ghostWidth > 0 && fadeAlpha > 0) {
                // Draw vertical pink ghost line
                nvgBeginPath(vg);
                nvgMoveTo(vg, ghostCenterX - ghostWidth/2, ghostCenterY - 2);
                nvgLineTo(vg, ghostCenterX + ghostWidth/2, ghostCenterY - 2);
                nvgLineTo(vg, ghostCenterX + ghostWidth/2, ghostCenterY + 2);
                nvgLineTo(vg, ghostCenterX - ghostWidth/2, ghostCenterY + 2);
                nvgClosePath(vg);
                
                // Pink/magenta ghost color with varying intensity - much more visible
                float intensity = fadeAlpha * ghostOpacity * (0.8f + sinf(time * 2.0f + i) * 0.2f);
                // Increase base opacity and intensity for better visibility
                intensity = clamp(intensity * 2.5f, 0.0f, 1.0f);
                nvgFillColor(vg, nvgRGBA(255, (int)(120 * intensity), (int)(200 * intensity), (int)(intensity * 200)));
                nvgFill(vg);
                
                // Add subtle glow around each ghost
                nvgBeginPath(vg);
                nvgRect(vg, ghostCenterX - ghostWidth, ghostCenterY - 4, ghostWidth * 2, 8);
                NVGpaint ghostGlow = nvgRadialGradient(vg, ghostCenterX, ghostCenterY, 0, ghostWidth * 1.5f,
                                                     nvgRGBA(255, 150, 200, (int)(intensity * 80)),
                                                     nvgRGBA(255, 150, 200, 0));
                nvgFillPaint(vg, ghostGlow);
                nvgFill(vg);
            }
        }
    }
};

// Use shared small jewel LED
using SmallJewelLED = shapetaker::SmallJewelLED;

struct InvolutionWidget : ModuleWidget {
    InvolutionWidget(Involution* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Involution.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Main Filter Section (using custom Shapetaker controls and updated coordinates)
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(Vec(55.066338, 76.52359), module, Involution::CUTOFF_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(Vec(143.7392, 76.52359), module, Involution::RESONANCE_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeXLarge>(Vec(55.066338, 148.54836), module, Involution::CUTOFF_B_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(Vec(143.7392, 148.54836), module, Involution::RESONANCE_B_PARAM));
        
        // Link switches
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(Vec(206.13919, 76.14698), module, Involution::LINK_CUTOFF_PARAM));
        addParam(createParamCentered<ShapetakerOscilloscopeSwitch>(Vec(206.13919, 148.54836), module, Involution::LINK_RESONANCE_PARAM));
        
        // Character Controls (middle section)
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(55.066338, 206.57312), module, Involution::HIGHPASS_CUTOFF_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(228.13919, 111.54836), module, Involution::FILTER_MORPH_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(222.13919, 206.57312), module, Involution::CROSS_FEEDBACK_PARAM));
        
        // Phaser Controls (right side)
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(Vec(139, 206.57312), module, Involution::PHASER_FREQUENCY_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(Vec(160, 206.57312), module, Involution::PHASER_FEEDBACK_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(Vec(181, 206.57312), module, Involution::PHASER_MIX_PARAM));
        
        // Special Effects with Lights
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(97.052765, 261.71121), module, Involution::CHAOS_AMOUNT_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(Vec(97.052765, 225.55661), module, Involution::CHAOS_RATE_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(Vec(191.69171, 261.71121), module, Involution::SHIMMER_AMOUNT_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(Vec(188.69171, 225.55661), module, Involution::SHIMMER_RATE_PARAM));
        
        // Chaos Visualizer - positioned in the center area
        ChaosVisualizer* chaosViz = new ChaosVisualizer(module);
        chaosViz->box.pos = Vec(135 - 60, 165 - 50); // Center the bigger diamond screen
        addChild(chaosViz);
        
        // Effect lights (positioned below the knobs)
        addChild(createLightCentered<SmallJewelLED>(Vec(97.052765, 289.63123), module, Involution::CHAOS_LIGHT));
        addChild(createLightCentered<SmallJewelLED>(Vec(191.69171, 286.63123), module, Involution::SHIMMER_LIGHT));

        // CV inputs
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(101.40276, 76.52359), module, Involution::CUTOFF_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(186.07562, 76.52359), module, Involution::RESONANCE_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(101.40276, 148.54836), module, Involution::CUTOFF_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(186.07562, 148.54836), module, Involution::RESONANCE_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(55.066338, 261.71121), module, Involution::CHAOS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(222.13919, 261.71121), module, Involution::SHIMMER_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(228.13919, 140), module, Involution::FILTER_MORPH_CV_INPUT));
        
        // Phaser CV inputs (below the knobs)
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(139, 230), module, Involution::PHASER_FREQUENCY_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(160, 230), module, Involution::PHASER_FEEDBACK_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(181, 230), module, Involution::PHASER_MIX_CV_INPUT));

        // Audio I/O
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(89.259193, 328.47842), module, Involution::AUDIO_A_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(47.189194, 328.47842), module, Involution::AUDIO_B_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(Vec(222.04919, 328.47842), module, Involution::AUDIO_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(Vec(179.97919, 328.47842), module, Involution::AUDIO_B_OUTPUT));
    }
};

Model* modelInvolution = createModel<Involution, InvolutionWidget>("Involution");
