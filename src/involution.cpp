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
        ALLPASS_FREQ_PARAM,
        ALLPASS_MIX_PARAM,
        STEREO_WIDTH_PARAM,
        PHASE_MOD_DEPTH_PARAM,
        INTERNAL_LFO_RATE_PARAM,
        PHASE_INVERT_PARAM,
        // New magical parameters
        CROSS_FEEDBACK_PARAM,
        ENVELOPE_FOLLOW_PARAM,
        CHAOS_AMOUNT_PARAM,
        SHIMMER_AMOUNT_PARAM,
        HARMONIC_EXCITER_PARAM,
        RESONANCE_DRIVE_PARAM,
        FILTER_MORPH_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_A_INPUT,
        AUDIO_B_INPUT,
        CUTOFF_A_CV_INPUT,
        RESONANCE_A_CV_INPUT,
        CUTOFF_B_CV_INPUT,
        RESONANCE_B_CV_INPUT,
        PHASE_MOD_CV_INPUT,
        CHAOS_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_A_OUTPUT,
        AUDIO_B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        PHASE_INVERT_LIGHT,
        CHAOS_LIGHT,
        SHIMMER_LIGHT,
        LIGHTS_LEN
    };

    // Enhanced filter with morphing capabilities
    struct MorphingBiquadFilter {
        float x1 = 0.f, x2 = 0.f;
        float y1 = 0.f, y2 = 0.f;
        float a0 = 1.f, a1 = 0.f, a2 = 0.f;
        float b1 = 0.f, b2 = 0.f;
        
        // For soft saturation/drive
        float saturate(float x, float drive) {
            if (drive <= 1.f) return x;
            x *= drive;
            return x / (1.f + std::abs(x) * 0.3f);
        }
        
        float process(float input, float drive = 1.f) {
            float output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
            
            if (drive > 1.f) {
                output = saturate(output, drive);
            }
            
            x2 = x1;
            x1 = input;
            y2 = y1;
            y1 = output;
            
            return output;
        }
        
        void setMorphingFilter(float freq, float resonance, float morph, float sampleRate) {
            float omega = 2.f * M_PI * freq / sampleRate;
            float sin_omega = std::sin(omega);
            float cos_omega = std::cos(omega);
            float alpha = sin_omega / (2.f * resonance);
            
            float norm = 1.f / (1.f + alpha);
            
            // Lowpass coefficients
            float lp_a0 = ((1.f - cos_omega) / 2.f) * norm;
            float lp_a1 = (1.f - cos_omega) * norm;
            float lp_a2 = lp_a0;
            
            // Bandpass coefficients
            float bp_a0 = alpha * norm;
            float bp_a1 = 0.f;
            float bp_a2 = -bp_a0;
            
            // Highpass coefficients
            float hp_a0 = ((1.f + cos_omega) / 2.f) * norm;
            float hp_a1 = -(1.f + cos_omega) * norm;
            float hp_a2 = hp_a0;
            
            // Morph between filter types
            if (morph < 0.5f) {
                // Morph between lowpass and bandpass
                float blend = morph * 2.f;
                a0 = lp_a0 * (1.f - blend) + bp_a0 * blend;
                a1 = lp_a1 * (1.f - blend) + bp_a1 * blend;
                a2 = lp_a2 * (1.f - blend) + bp_a2 * blend;
            } else {
                // Morph between bandpass and highpass
                float blend = (morph - 0.5f) * 2.f;
                a0 = bp_a0 * (1.f - blend) + hp_a0 * blend;
                a1 = bp_a1 * (1.f - blend) + hp_a1 * blend;
                a2 = bp_a2 * (1.f - blend) + hp_a2 * blend;
            }
            
            b1 = (-2.f * cos_omega) * norm;
            b2 = (1.f - alpha) * norm;
        }
        
        void setAllpass(float freq, float sampleRate) {
            float omega = 2.f * M_PI * freq / sampleRate;
            float tan_half_omega = std::tan(omega / 2.f);
            float norm = 1.f / (1.f + tan_half_omega);
            
            a0 = (1.f - tan_half_omega) * norm;
            a1 = -2.f * norm;
            a2 = (1.f + tan_half_omega) * norm;
            b1 = a1;
            b2 = a0;
        }
    };

    // Shimmer delay line
    struct ShimmerDelay {
        static const int MAX_DELAY = 4800; // 100ms at 48kHz
        float buffer[MAX_DELAY] = {};
        int writePos = 0;
        
        float process(float input, float delayTime, float feedback, float shimmer) {
            int delaySamples = (int)(delayTime * 48000.f);
            delaySamples = clamp(delaySamples, 1, MAX_DELAY - 1);
            
            int readPos = (writePos - delaySamples + MAX_DELAY) % MAX_DELAY;
            float delayed = buffer[readPos];
            
            // Add harmonic content for shimmer
            if (shimmer > 0.f) {
                delayed += std::sin(delayed * 3.14159f * 2.f) * shimmer * 0.3f;
            }
            
            buffer[writePos] = input + delayed * feedback;
            writePos = (writePos + 1) % MAX_DELAY;
            
            return delayed;
        }
    };

    // Envelope follower
    struct EnvelopeFollower {
        float envelope = 0.f;
        
        float process(float input, float attack = 0.001f, float release = 0.1f) {
            float rectified = std::abs(input);
            if (rectified > envelope) {
                envelope += (rectified - envelope) * attack;
            } else {
                envelope += (rectified - envelope) * release;
            }
            return envelope;
        }
    };

    // Filter chains
    MorphingBiquadFilter lowpassA[3]; // 3 biquads for 6th order
    MorphingBiquadFilter lowpassB[3];
    MorphingBiquadFilter highpassA[2]; // 2 biquads for 4th order
    MorphingBiquadFilter highpassB[2];
    MorphingBiquadFilter allpassA;
    MorphingBiquadFilter allpassB;
    
    // Magical components
    ShimmerDelay shimmerA, shimmerB;
    EnvelopeFollower envFollowerA, envFollowerB;
    
    // Cross-feedback delay lines
    float crossFeedbackA = 0.f, crossFeedbackB = 0.f;
    
    // Internal LFO and chaos
    float lfoPhase = 0.f;
    float chaosPhaseA = 0.f, chaosPhaseB = 0.f;
    std::default_random_engine chaosGenerator;
    std::uniform_real_distribution<float> chaosDistribution{-1.f, 1.f};
    
    // Schmitt trigger for phase invert button
    dsp::SchmittTrigger phaseInvertTrigger;
    bool phaseInverted = false;
    
    // Light pulsing
    float chaosLightPhase = 0.f;
    float shimmerLightPhase = 0.f;

    Involution() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(CUTOFF_A_PARAM, 0.f, 1.f, 0.5f, "Filter A Cutoff", " Hz", 2.f, 20.f);
        configParam(RESONANCE_A_PARAM, 0.1f, 1.2f, 0.707f, "Filter A Resonance");
        configParam(CUTOFF_B_PARAM, 0.f, 1.f, 0.5f, "Filter B Cutoff", " Hz", 2.f, 20.f);
        configParam(RESONANCE_B_PARAM, 0.1f, 1.2f, 0.707f, "Filter B Resonance");
        configParam(HIGHPASS_CUTOFF_PARAM, 0.f, 1.f, 0.f, "Highpass Cutoff", " Hz", 0.f, 100.f);
        configParam(ALLPASS_FREQ_PARAM, 0.f, 1.f, 0.5f, "Allpass Center Frequency", " Hz", 2.f, 2000.f);
        configParam(ALLPASS_MIX_PARAM, 0.f, 1.f, 0.f, "Allpass Mix", "%", 0.f, 100.f);
        configParam(STEREO_WIDTH_PARAM, 0.f, 1.f, 0.f, "Stereo Width", "%", 0.f, 100.f);
        configParam(PHASE_MOD_DEPTH_PARAM, 0.f, 1.f, 0.f, "Phase Modulation Depth", "%", 0.f, 100.f);
        configParam(INTERNAL_LFO_RATE_PARAM, 0.f, 1.f, 0.25f, "Internal LFO Rate", " Hz", 2.f, 0.1f);
        configSwitch(PHASE_INVERT_PARAM, 0.f, 1.f, 0.f, "Phase Invert B", {"Normal", "Inverted"});
        
        // New magical parameters
        configParam(CROSS_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Cross Feedback", "%", 0.f, 100.f);
        configParam(ENVELOPE_FOLLOW_PARAM, 0.f, 1.f, 0.f, "Envelope Following", "%", 0.f, 100.f);
        configParam(CHAOS_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Chaos Amount", "%", 0.f, 100.f);
        configParam(SHIMMER_AMOUNT_PARAM, 0.f, 1.f, 0.f, "Shimmer", "%", 0.f, 100.f);
        configParam(HARMONIC_EXCITER_PARAM, 0.f, 1.f, 0.f, "Harmonic Exciter", "%", 0.f, 100.f);
        configParam(RESONANCE_DRIVE_PARAM, 1.f, 5.f, 1.f, "Resonance Drive");
        configParam(FILTER_MORPH_PARAM, 0.f, 1.f, 0.f, "Filter Type Morph");

        configInput(AUDIO_A_INPUT, "Audio A");
        configInput(AUDIO_B_INPUT, "Audio B");
        configInput(CUTOFF_A_CV_INPUT, "Filter A Cutoff CV");
        configInput(RESONANCE_A_CV_INPUT, "Filter A Resonance CV");
        configInput(CUTOFF_B_CV_INPUT, "Filter B Cutoff CV");
        configInput(RESONANCE_B_CV_INPUT, "Filter B Resonance CV");
        configInput(PHASE_MOD_CV_INPUT, "Phase Modulation CV");
        configInput(CHAOS_CV_INPUT, "Chaos CV");

        configOutput(AUDIO_A_OUTPUT, "Audio A");
        configOutput(AUDIO_B_OUTPUT, "Audio B");
        
        configLight(PHASE_INVERT_LIGHT, "Phase Invert B");
        configLight(CHAOS_LIGHT, "Chaos Activity");
        configLight(SHIMMER_LIGHT, "Shimmer Activity");
    }

    void process(const ProcessArgs& args) override {
        // Handle phase invert button
        if (phaseInvertTrigger.process(params[PHASE_INVERT_PARAM].getValue())) {
            phaseInverted = !phaseInverted;
        }
        lights[PHASE_INVERT_LIGHT].setBrightness(phaseInverted ? 1.f : 0.f);

        // Get base parameter values
        float cutoffA = params[CUTOFF_A_PARAM].getValue();
        float cutoffB = params[CUTOFF_B_PARAM].getValue();
        float resonanceA = params[RESONANCE_A_PARAM].getValue();
        float resonanceB = params[RESONANCE_B_PARAM].getValue();
        
        // Magical parameters
        float crossFeedback = params[CROSS_FEEDBACK_PARAM].getValue();
        float envelopeFollow = params[ENVELOPE_FOLLOW_PARAM].getValue();
        float chaosAmount = params[CHAOS_AMOUNT_PARAM].getValue();
        float shimmerAmount = params[SHIMMER_AMOUNT_PARAM].getValue();
        float harmonicExciter = params[HARMONIC_EXCITER_PARAM].getValue();
        float resonanceDrive = params[RESONANCE_DRIVE_PARAM].getValue();
        float filterMorph = params[FILTER_MORPH_PARAM].getValue();
        
        float highpassCutoff = params[HIGHPASS_CUTOFF_PARAM].getValue() * 100.f;
        float allpassCenterFreq = std::pow(2.f, params[ALLPASS_FREQ_PARAM].getValue() * 11.f) * 20.f;
        float allpassMix = params[ALLPASS_MIX_PARAM].getValue();
        float stereoWidth = params[STEREO_WIDTH_PARAM].getValue();
        float phaseModDepth = params[PHASE_MOD_DEPTH_PARAM].getValue();
        float lfoRate = std::pow(2.f, params[INTERNAL_LFO_RATE_PARAM].getValue() * 6.f) * 0.1f;

        // Update internal oscillators
        lfoPhase += lfoRate * args.sampleTime * 2.f * M_PI;
        if (lfoPhase >= 2.f * M_PI) lfoPhase -= 2.f * M_PI;
        
        chaosPhaseA += (0.31f + chaosAmount * 0.1f) * args.sampleTime * 2.f * M_PI;
        chaosPhaseB += (0.37f + chaosAmount * 0.13f) * args.sampleTime * 2.f * M_PI;
        if (chaosPhaseA >= 2.f * M_PI) chaosPhaseA -= 2.f * M_PI;
        if (chaosPhaseB >= 2.f * M_PI) chaosPhaseB -= 2.f * M_PI;

        // Get audio inputs
        float audioA = 0.f, audioB = 0.f;
        bool hasInputA = inputs[AUDIO_A_INPUT].isConnected();
        bool hasInputB = inputs[AUDIO_B_INPUT].isConnected();
        
        if (hasInputA || hasInputB) {
            if (hasInputA && hasInputB) {
                audioA = inputs[AUDIO_A_INPUT].getVoltage();
                audioB = inputs[AUDIO_B_INPUT].getVoltage();
            } else if (hasInputA) {
                audioA = audioB = inputs[AUDIO_A_INPUT].getVoltage();
            } else {
                audioA = audioB = inputs[AUDIO_B_INPUT].getVoltage();
            }
            
            // Apply cross-feedback from previous samples
            audioA += crossFeedbackB * crossFeedback * 0.3f;
            audioB += crossFeedbackA * crossFeedback * 0.3f;
            
            // Apply phase inversion to channel B if enabled
            if (phaseInverted) {
                audioB = -audioB;
            }
            
            // Envelope following
            float envA = envFollowerA.process(audioA, 0.001f, 0.01f);
            float envB = envFollowerB.process(audioB, 0.001f, 0.01f);
            
            // Chaos modulation
            float chaosA = 0.f, chaosB = 0.f;
            if (chaosAmount > 0.f) {
                chaosA = (std::sin(chaosPhaseA) + chaosDistribution(chaosGenerator) * 0.3f) * chaosAmount * 0.2f;
                chaosB = (std::sin(chaosPhaseB) + chaosDistribution(chaosGenerator) * 0.3f) * chaosAmount * 0.2f;
                
                if (inputs[CHAOS_CV_INPUT].isConnected()) {
                    float chaosCv = inputs[CHAOS_CV_INPUT].getVoltage() / 10.f;
                    chaosA += chaosCv * chaosAmount * 0.3f;
                    chaosB += chaosCv * chaosAmount * 0.3f;
                }
            }
            
            // Apply modulations to cutoff frequencies
            cutoffA += inputs[CUTOFF_A_CV_INPUT].getVoltage() / 10.f;
            cutoffA += envA * envelopeFollow * 0.5f;
            cutoffA += chaosA;
            cutoffA = clamp(cutoffA, 0.f, 1.f);
            
            cutoffB += inputs[CUTOFF_B_CV_INPUT].getVoltage() / 10.f;
            cutoffB += envB * envelopeFollow * 0.5f;
            cutoffB += chaosB;
            cutoffB = clamp(cutoffB, 0.f, 1.f);
            
            // Apply modulations to resonance
            resonanceA += inputs[RESONANCE_A_CV_INPUT].getVoltage();
            resonanceA = clamp(resonanceA, 0.1f, 1.2f);
            
            resonanceB += inputs[RESONANCE_B_CV_INPUT].getVoltage();
            resonanceB = clamp(resonanceB, 0.1f, 1.2f);

            // Calculate phase modulation
            float phaseModulation = std::sin(lfoPhase) * phaseModDepth;
            if (inputs[PHASE_MOD_CV_INPUT].isConnected()) {
                phaseModulation += inputs[PHASE_MOD_CV_INPUT].getVoltage() / 10.f * phaseModDepth;
            }
            phaseModulation = clamp(phaseModulation, -1.f, 1.f);

            // Calculate frequencies
            float freqA = std::pow(2.f, cutoffA * 10.f) * 20.f;
            float freqB = std::pow(2.f, cutoffB * 10.f) * 20.f;
            
            freqA = std::min(freqA, args.sampleRate * 0.49f);
            freqB = std::min(freqB, args.sampleRate * 0.49f);

            // Update filter coefficients
            for (int i = 0; i < 3; i++) {
                lowpassA[i].setMorphingFilter(freqA, resonanceA, filterMorph, args.sampleRate);
                lowpassB[i].setMorphingFilter(freqB, resonanceB, filterMorph, args.sampleRate);
            }

            if (highpassCutoff > 0.f) {
                for (int i = 0; i < 2; i++) {
                    highpassA[i].setMorphingFilter(highpassCutoff, 0.707f, 1.f, args.sampleRate); // Full highpass
                    highpassB[i].setMorphingFilter(highpassCutoff, 0.707f, 1.f, args.sampleRate);
                }
            }

            // Calculate stereo allpass frequencies
            float stereoOffset = stereoWidth * 0.3f;
            float allpassFreqA = allpassCenterFreq * (1.f - stereoOffset + phaseModulation * 0.2f);
            float allpassFreqB = allpassCenterFreq * (1.f + stereoOffset - phaseModulation * 0.2f);
            allpassFreqA = std::min(allpassFreqA, args.sampleRate * 0.49f);
            allpassFreqB = std::min(allpassFreqB, args.sampleRate * 0.49f);
            
            allpassA.setAllpass(allpassFreqA, args.sampleRate);
            allpassB.setAllpass(allpassFreqB, args.sampleRate);

            // Process Channel A
            float processedA = audioA;
            
            // Apply highpass first
            if (highpassCutoff > 0.f) {
                for (int i = 0; i < 2; i++) {
                    processedA = highpassA[i].process(processedA);
                }
            }
            
            // Apply morphing filters with drive
            for (int i = 0; i < 3; i++) {
                processedA = lowpassA[i].process(processedA, resonanceDrive);
            }
            
            // Apply harmonic exciter
            if (harmonicExciter > 0.f) {
                float harmonics = std::sin(processedA * 6.28318f) * harmonicExciter * 0.1f;
                processedA += harmonics;
            }
            
            // Apply allpass for phase effects
            if (allpassMix > 0.f) {
                float allpassOut = allpassA.process(processedA);
                processedA = processedA * (1.f - allpassMix) + allpassOut * allpassMix;
            }
            
            // Apply shimmer
            if (shimmerAmount > 0.f) {
                float shimmerOut = shimmerA.process(processedA, 0.03f + shimmerAmount * 0.07f, 0.3f, shimmerAmount);
                processedA += shimmerOut * shimmerAmount * 0.4f;
            }
            
            // Process Channel B
            float processedB = audioB;
            
            // Apply highpass first
            if (highpassCutoff > 0.f) {
                for (int i = 0; i < 2; i++) {
                    processedB = highpassB[i].process(processedB);
                }
            }
            
            // Apply morphing filters with drive
            for (int i = 0; i < 3; i++) {
                processedB = lowpassB[i].process(processedB, resonanceDrive);
            }
            
            // Apply harmonic exciter
            if (harmonicExciter > 0.f) {
                float harmonics = std::sin(processedB * 6.28318f) * harmonicExciter * 0.1f;
                processedB += harmonics;
            }
            
            // Apply allpass for phase effects
            if (allpassMix > 0.f) {
                float allpassOut = allpassB.process(processedB);
                processedB = processedB * (1.f - allpassMix) + allpassOut * allpassMix;
            }
            
            // Apply shimmer
            if (shimmerAmount > 0.f) {
                float shimmerOut = shimmerB.process(processedB, 0.03f + shimmerAmount * 0.07f, 0.3f, shimmerAmount);
                processedB += shimmerOut * shimmerAmount * 0.4f;
            }
            
            // Store cross-feedback for next sample
            crossFeedbackA = processedA;
            crossFeedbackB = processedB;
            
            outputs[AUDIO_A_OUTPUT].setVoltage(processedA);
            outputs[AUDIO_B_OUTPUT].setVoltage(processedB);
        }
        
        // Update lights
        chaosLightPhase += args.sampleTime * 8.f;
        shimmerLightPhase += args.sampleTime * 3.f;
        
        lights[CHAOS_LIGHT].setBrightness(chaosAmount * (0.5f + 0.5f * std::sin(chaosLightPhase)));
        lights[SHIMMER_LIGHT].setBrightness(shimmerAmount * (0.3f + 0.7f * std::sin(shimmerLightPhase)));
    }
};

struct InvolutionWidget : ModuleWidget {
    InvolutionWidget(Involution* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Involution.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Main Filter Section (top, prominently placed)
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20, 25)), module, Involution::CUTOFF_A_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40, 25)), module, Involution::RESONANCE_A_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20, 45)), module, Involution::CUTOFF_B_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40, 45)), module, Involution::RESONANCE_B_PARAM));
        
        // Character Controls (middle section)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 65)), module, Involution::HIGHPASS_CUTOFF_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 65)), module, Involution::FILTER_MORPH_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(55, 65)), module, Involution::RESONANCE_DRIVE_PARAM));
        
        // Magical Effects (middle section)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 85)), module, Involution::CROSS_FEEDBACK_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 85)), module, Involution::ENVELOPE_FOLLOW_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(55, 85)), module, Involution::HARMONIC_EXCITER_PARAM));
        
        // Stereo & Phase Section (lower section)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 105)), module, Involution::ALLPASS_FREQ_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 100)), module, Involution::ALLPASS_MIX_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 110)), module, Involution::STEREO_WIDTH_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(45, 105)), module, Involution::PHASE_MOD_DEPTH_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(60, 105)), module, Involution::INTERNAL_LFO_RATE_PARAM));
        
        // Special Effects with Lights (bottom section)
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(20, 125)), module, Involution::CHAOS_AMOUNT_PARAM, Involution::CHAOS_LIGHT));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(mm2px(Vec(40, 125)), module, Involution::SHIMMER_AMOUNT_PARAM, Involution::SHIMMER_LIGHT));
        addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenLight>>>(mm2px(Vec(60, 125)), module, Involution::PHASE_INVERT_PARAM, Involution::PHASE_INVERT_LIGHT));

        // CV inputs (right side, within panel width)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(65, 20)), module, Involution::CUTOFF_A_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(65, 30)), module, Involution::RESONANCE_A_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(65, 40)), module, Involution::CUTOFF_B_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(65, 50)), module, Involution::RESONANCE_B_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(65, 70)), module, Involution::PHASE_MOD_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(65, 90)), module, Involution::CHAOS_CV_INPUT));

        // Audio I/O (repositioned to avoid overlaps)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5, 79)), module, Involution::AUDIO_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(13, 79)), module, Involution::AUDIO_B_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(21, 79)), module, Involution::AUDIO_A_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29, 79)), module, Involution::AUDIO_B_OUTPUT));
    }
};

Model* modelInvolution = createModel<Involution, InvolutionWidget>("Involution");