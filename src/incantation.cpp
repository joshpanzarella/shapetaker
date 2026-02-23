#include "plugin.hpp"
#include <cmath>
#include <random>
#include <string>

struct Incantation : Module {
    enum ParamId {
        // Main controls
        DRIVE_PARAM,
        OUTPUT_PARAM,
        MIX_PARAM,
        
        // Filter sliders (8 bands)
        FILTER_1_PARAM,
        FILTER_2_PARAM,
        FILTER_3_PARAM,
        FILTER_4_PARAM,
        FILTER_5_PARAM,
        FILTER_6_PARAM,
        FILTER_7_PARAM,
        FILTER_8_PARAM,
        
        // Animation controls
        PATTERN_PARAM,      // 12-position rotary switch (0-11)
        ENVELOPE_PARAM,     // Envelope shape morphing (0-10)
        RATE_PARAM,         // Animation rate
        
        // Switches
        FREQ_SWITCH_PARAM,  // BASS/MIDS voicing
        LFO_SWITCH_PARAM,   // LFO on/off
        Q_FACTOR_SWITCH_PARAM, // Low/High Q resonance
        CV_BYPASS_SWITCH_PARAM, // Bypass all filter CV inputs
        
        // Preset buttons
        PRESET_ZERO_PARAM,   // Set all faders to 0%
        PRESET_HALF_PARAM,   // Set all faders to 50%  
        PRESET_FULL_PARAM,   // Set all faders to 100%

        // MuRF-style bank switch
        BANK_SWITCH_PARAM,   // A / B-LFO pattern bank

        // Tap tempo button
        TAP_STEP_PARAM,

        PARAMS_LEN
    };

    enum InputId {
        AUDIO_LEFT_INPUT,   // Main/Left audio input
        AUDIO_RIGHT_INPUT,  // Right audio input for stereo

        // CV inputs
        ENVELOPE_CV_INPUT,
        RATE_CV_INPUT,
        LFO_SWEEP_CV_INPUT,
        MIX_CV_INPUT,

        // Individual filter CV inputs
        FILTER_1_CV_INPUT,
        FILTER_2_CV_INPUT,
        FILTER_3_CV_INPUT,
        FILTER_4_CV_INPUT,
        FILTER_5_CV_INPUT,
        FILTER_6_CV_INPUT,
        FILTER_7_CV_INPUT,
        FILTER_8_CV_INPUT,

        INPUTS_LEN
    };
    
    enum OutputId {
        LEFT_MONO_OUTPUT,
        RIGHT_OUTPUT,
        OUTPUTS_LEN
    };
    
    enum LightId {
        RATE_LIGHT,         // Animation rate indicator
        DRIVE_LIGHT,        // Green/Yellow/Red for input level
        LIGHTS_LEN
    };

    // Core filter frequencies for each voicing
    static constexpr float BASS_FREQS[8] = {
        110.0f,   // Lowpass cutoff
        160.0f, 240.0f, 350.0f, 525.0f, 775.0f, 1200.0f, 1800.0f
    };
    
    static constexpr float MIDS_FREQS[8] = {
        200.0f, 300.0f, 450.0f, 675.0f, 1000.0f, 1500.0f, 2200.0f, 3400.0f
    };

    // Resonant filter implementation
    struct ResonantFilter {
        float x1 = 0.f, x2 = 0.f;
        float y1 = 0.f, y2 = 0.f;
        float a0 = 1.f, a1 = 0.f, a2 = 0.f;
        float b1 = 0.f, b2 = 0.f;
        bool isLowpass = false;
        
        void setLowpass(float freq, float resonance, float sampleRate) {
            isLowpass = true;
            // Clamp to safe range — above 45% Nyquist the biquad becomes unstable
            freq = clamp(freq, 20.f, sampleRate * 0.45f);
            float omega = 2.f * M_PI * freq / sampleRate;
            float sin_omega = std::sin(omega);
            float cos_omega = std::cos(omega);
            float alpha = sin_omega / (2.f * resonance);

            float norm = 1.f / (1.f + alpha);
            a0 = ((1.f - cos_omega) / 2.f) * norm;
            a1 = (1.f - cos_omega) * norm;
            a2 = a0;
            b1 = (-2.f * cos_omega) * norm;
            b2 = (1.f - alpha) * norm;
        }

        void setBandpass(float freq, float resonance, float sampleRate) {
            isLowpass = false;
            // Clamp to safe range — above 45% Nyquist the biquad becomes unstable
            freq = clamp(freq, 20.f, sampleRate * 0.45f);
            float omega = 2.f * M_PI * freq / sampleRate;
            float sin_omega = std::sin(omega);
            float cos_omega = std::cos(omega);
            float alpha = sin_omega / (2.f * resonance);

            float norm = 1.f / (1.f + alpha);
            a0 = alpha * norm;
            a1 = 0.f;
            a2 = -alpha * norm;
            b1 = (-2.f * cos_omega) * norm;
            b2 = (1.f - alpha) * norm;
        }

        float process(float input) {
            float output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
            // Recover from any instability that slips through
            if (!std::isfinite(output)) {
                reset();
                return 0.f;
            }
            x2 = x1;
            x1 = input;
            y2 = y1;
            y1 = output;
            return output;
        }
        
        void reset() {
            x1 = x2 = y1 = y2 = 0.f;
        }
    };

    // Envelope generator for animation
    struct FilterEnvelope {
        float phase = 0.f;
        float attackTime = 0.1f;
        float decayTime = 0.5f;
        bool triggered = false;
        
        void trigger() {
            // Smooth retrigger to prevent clicks
            if (!triggered || phase > attackTime + decayTime * 0.8f) {
                // Only reset if envelope is finished or nearly finished
                triggered = true;
                phase = 0.f;
            }
            // If envelope is still active, let it continue to avoid clicks
        }
        
        float process(float sampleTime) {
            if (!triggered) return 0.f;
            
            phase += sampleTime;
            
            if (phase < attackTime) {
                // Very smooth attack curve using sine
                float attackPhase = phase / attackTime;
                attackPhase = clamp(attackPhase, 0.f, 1.f);
                return std::sin(attackPhase * M_PI * 0.5f); // Sine curve 0 to π/2
            } else if (phase < attackTime + decayTime) {
                // Smooth decay curve using cosine  
                float decayPhase = (phase - attackTime) / decayTime;
                decayPhase = clamp(decayPhase, 0.f, 1.f);
                return std::cos(decayPhase * M_PI * 0.5f); // Cosine curve π/2 to 0
            } else {
                triggered = false;
                return 0.f;
            }
        }
        
        void setEnvelopeShape(float shape, float animationRate = 1.f) {
            // Adaptive envelope times based on animation rate to prevent clicking
            // Faster rates need shorter envelopes to avoid overlap conflicts
            float rateScale = clamp(2.f / animationRate, 0.2f, 1.f); // Scale down for faster rates
            
            if (shape <= 5.f) {
                // Fast but not too fast - prevent clicks, scaled by rate
                attackTime = (0.005f + shape * 0.01f) * rateScale; // 5-55ms scaled by rate
                decayTime = (0.05f + shape * 0.05f) * rateScale; // 50-300ms scaled by rate
            } else {
                // Crossfading mode - longer overlaps, but still scaled
                float morph = (shape - 5.f) / 5.f;
                attackTime = (0.02f + morph * 0.08f) * rateScale; // 20-100ms scaled
                decayTime = (0.15f - morph * 0.05f) * rateScale; // 150-100ms scaled (still overlapping)
            }
            
            // Absolute minimums to prevent clicks
            attackTime = std::max(attackTime, 0.003f); // Never shorter than 3ms
            decayTime = std::max(decayTime, 0.02f);    // Never shorter than 20ms
        }
    };

    // LFO for frequency sweeping
    struct LFO {
        float phase = 0.f;
        float freq = 1.f;
        
        float process(float sampleTime) {
            phase += freq * sampleTime;
            if (phase >= 1.f) phase -= 1.f;
            return std::sin(phase * 2.f * M_PI);
        }
        
        void setFreq(float f) {
            freq = clamp(f, 0.08f, 20.f); // Match MuRF spec
        }
    };

    // Polyphonic support (up to 6 voices like other Shapetaker modules)
    static const int MAX_POLY_VOICES = 6;

    // 8 resonant filters per voice
    ResonantFilter filters[MAX_POLY_VOICES][8];
    FilterEnvelope envelopes[MAX_POLY_VOICES][8];
    LFO lfo;

    static constexpr int MAX_PATTERN_STEPS = 64;

    struct PatternDefinition {
        uint8_t stepCount;
        bool noAnimation;
        uint8_t steps[MAX_PATTERN_STEPS];
    };

    static const PatternDefinition MURF_BANK_A_PATTERNS[12];
    static const PatternDefinition MURF_BANK_B_PATTERNS[12];
    
    // State variables
    bool bassVoicing = true; // true = BASS, false = MIDS
    bool lfoOn = false;
    bool bankBLFO = false;
    bool currentPatternIsStatic = true;
    float driveLevel = 0.f;
    int currentPattern = 1; // 0-11 (Pattern 1-12)
    float animationPhase = 0.f;
    float animationRate = 1.f;
    int currentStep = 0;
    float stepPhase = 0.f; // Phase within current step
    
    // Smoothed sweep CV — one-pole filter prevents audio-rate CV from
    // rapidly modulating IIR filter coefficients and causing instability.
    // Updated once per sample (on voice 0) with a ~2ms time constant,
    // which passes LFO sweeps (<~80 Hz) while blocking audio-rate signals.
    float sweepCVSmooth = 0.f;

    // For tap tempo
    float tapTimes[3] = {0.f, 0.f, 0.f};
    int tapIndex = 0;
    bool tapValid = false;
    bool lastTapButtonHigh = false;
    

    Incantation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Main controls
        configParam(DRIVE_PARAM, 0.f, 2.f, 1.f, "Drive");
        configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output");
        configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix");
        
        // Filter sliders
        for (int i = 0; i < 8; i++) {
            configParam(FILTER_1_PARAM + i, 0.f, 1.f, 1.f, string::f("Filter %d", i + 1));
        }
        
        // Animation controls - Pattern selector (1-12)
        configSwitch(PATTERN_PARAM, 1.f, 12.f, 1.f, "Pattern", 
            {"1 - No Animation", "2 - Upward Staircase", "3 - Downward Cascade", "4 - Crisscross",
             "5 - Tremolo", "6 - Upward Bounce", "7 - Brownian Motion", "8 - Random-like",
             "9 - Double Up/Down", "10 - Downward Band Exp.", "11 - Polyrhythm", "12 - Rhythmicon"});
        configParam(ENVELOPE_PARAM, 0.f, 10.f, 2.f, "Envelope");
        configParam(RATE_PARAM, 0.08f, 4.f, 1.f, "Rate", "Hz");
        
        // Switches
        configSwitch(FREQ_SWITCH_PARAM, 0.f, 1.f, 0.f, "Frequency Voicing", {"BASS", "MIDS"});
        configSwitch(LFO_SWITCH_PARAM, 0.f, 1.f, 0.f, "LFO", {"OFF", "ON"});
        configSwitch(Q_FACTOR_SWITCH_PARAM, 0.f, 1.f, 0.f, "Filter Resonance", {"Normal", "High Q"});
        configSwitch(CV_BYPASS_SWITCH_PARAM, 0.f, 1.f, 0.f, "Filter CV", {"ACTIVE", "BYPASS"});
        configSwitch(BANK_SWITCH_PARAM, 0.f, 1.f, 0.f, "Pattern Bank", {"A", "B-LFO"});
        
        // Preset buttons (momentary)
        configButton(PRESET_ZERO_PARAM, "Set All Faders to 0%");
        configButton(PRESET_HALF_PARAM, "Set All Faders to 50%");  
        configButton(PRESET_FULL_PARAM, "Set All Faders to 100%");
        
        // Inputs
        configInput(AUDIO_LEFT_INPUT, "Audio Left/Mono");
        configInput(AUDIO_RIGHT_INPUT, "Audio Right");
        configInput(ENVELOPE_CV_INPUT, "Envelope CV");
        configInput(RATE_CV_INPUT, "Rate CV");
        configInput(LFO_SWEEP_CV_INPUT, "LFO/Sweep CV");
        configInput(MIX_CV_INPUT, "Mix CV");
        
        // Individual filter CV inputs (±5V range)
        for (int i = 0; i < 8; i++) {
            configInput(FILTER_1_CV_INPUT + i, string::f("Filter %d CV (±5V)", i + 1));
        }
        
        configParam(TAP_STEP_PARAM, 0.f, 1.f, 0.f, "Tap Tempo");
        
        // Outputs
        configOutput(LEFT_MONO_OUTPUT, "Left/Mono");
        configOutput(RIGHT_OUTPUT, "Right");
        
        // Initialize filters
        updateFilterVoicing();
        
       // Initialize LFO
       lfo.setFreq(1.f); // 1 Hz default

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void updateFilterVoicing() {
        float sampleRate = APP->engine->getSampleRate();
        bassVoicing = (params[FREQ_SWITCH_PARAM].getValue() < 0.5f);
        bool highQ = (params[Q_FACTOR_SWITCH_PARAM].getValue() > 0.5f);
        
        // Determine Q factors based on switch
        float lowpassQ = highQ ? 1.5f : 0.9f;     // High Q: more resonant lowpass
        float bandpassQ = highQ ? 4.5f : 2.5f;    // High Q: very resonant bandpass
        
        // Update all voices
        for (int v = 0; v < MAX_POLY_VOICES; v++) {
            for (int i = 0; i < 8; i++) {
                if (bassVoicing) {
                    if (i == 0) {
                        // First filter is lowpass in BASS mode
                        filters[v][i].setLowpass(BASS_FREQS[i], lowpassQ, sampleRate);
                    } else {
                        // Bandpass filters with variable resonance
                        filters[v][i].setBandpass(BASS_FREQS[i], bandpassQ, sampleRate);
                    }
                } else {
                    // All filters are bandpass in MIDS mode with variable Q
                    filters[v][i].setBandpass(MIDS_FREQS[i], bandpassQ, sampleRate);
                }
            }
        }
    }

    void process(const ProcessArgs& args) override {
        // Update voicing if changed
        bool newBassVoicing = (params[FREQ_SWITCH_PARAM].getValue() < 0.5f);
        static bool lastHighQ = false;
        bool currentHighQ = (params[Q_FACTOR_SWITCH_PARAM].getValue() > 0.5f);
        
        if (newBassVoicing != bassVoicing || currentHighQ != lastHighQ) {
            updateFilterVoicing();
            lastHighQ = currentHighQ;
        }
        
        // Update LFO state
        lfoOn = (params[LFO_SWITCH_PARAM].getValue() > 0.5f);
        bankBLFO = (params[BANK_SWITCH_PARAM].getValue() > 0.5f);
        
        // Handle preset buttons - simple preset functionality
        if (params[PRESET_ZERO_PARAM].getValue() > 0.5f) {
            // Set all faders to 0%
            for (int i = 0; i < 8; i++) {
                paramQuantities[FILTER_1_PARAM + i]->setValue(0.f);
            }
        }
        if (params[PRESET_HALF_PARAM].getValue() > 0.5f) {
            // Set all faders to 50%
            for (int i = 0; i < 8; i++) {
                paramQuantities[FILTER_1_PARAM + i]->setValue(0.5f);
            }
        }
        if (params[PRESET_FULL_PARAM].getValue() > 0.5f) {
            // Set all faders to 100%
            for (int i = 0; i < 8; i++) {
                paramQuantities[FILTER_1_PARAM + i]->setValue(1.f);
            }
        }
        
        // Update fader positions with CV in real-time (visual feedback)
        bool cvBypass = (params[CV_BYPASS_SWITCH_PARAM].getValue() > 0.5f);
        for (int i = 0; i < 8; i++) {
            if (inputs[FILTER_1_CV_INPUT + i].isConnected() && !cvBypass) {
                // Get CV modulation (use first voice for visual display)
                float cv = inputs[FILTER_1_CV_INPUT + i].getVoltage(0);
                // Scale CV to full fader range: -5V = 0%, 0V = 50%, +5V = 100%
                float combinedValue = clamp((cv / 10.f) + 0.5f, 0.f, 1.f);
                
                // Update the visual parameter to show the CV-modulated position
                paramQuantities[FILTER_1_PARAM + i]->setValue(combinedValue);
            }
            // If CV is bypassed or not connected, faders show manual slider positions
        }
        
        // Determine if we have stereo input
        bool hasStereoInput = inputs[AUDIO_RIGHT_INPUT].isConnected();
        bool hasStereoOutput = outputs[RIGHT_OUTPUT].isConnected();
        
        // Determine polyphony from left input
        int leftChannels = inputs[AUDIO_LEFT_INPUT].isConnected() ?
                          clamp(inputs[AUDIO_LEFT_INPUT].getChannels(), 0, MAX_POLY_VOICES) : 0;
        int rightChannels = hasStereoInput ? clamp(inputs[AUDIO_RIGHT_INPUT].getChannels(), 0, MAX_POLY_VOICES) : 0;
        
        // Set output channels
        if (hasStereoOutput) {
            outputs[LEFT_MONO_OUTPUT].setChannels(1);  // Always mono for L output in stereo mode
            outputs[RIGHT_OUTPUT].setChannels(1);      // Always mono for R output
        } else {
            outputs[LEFT_MONO_OUTPUT].setChannels(leftChannels);  // Polyphonic for mono mode
        }
        
        // Get drive parameter
        float drive = params[DRIVE_PARAM].getValue();
        
        // Calculate drive level for light (use left input, first channel)
        float firstChannelInput = inputs[AUDIO_LEFT_INPUT].isConnected() ? 
                                 inputs[AUDIO_LEFT_INPUT].getVoltage(0) : 0.f;
        driveLevel = std::abs(firstChannelInput * drive);
        
        // Update drive light
        if (driveLevel < 0.3f) {
            lights[DRIVE_LIGHT].setBrightness(0.f);
        } else if (driveLevel < 1.f) {
            lights[DRIVE_LIGHT].setBrightness(0.5f); // Green
        } else if (driveLevel < 2.f) {
            lights[DRIVE_LIGHT].setBrightness(0.8f); // Yellow
        } else {
            lights[DRIVE_LIGHT].setBrightness(1.f); // Red
        }
        
        // Handle tap tempo button — detect rising edge of panel button
        {
            bool currentTapHigh = (params[TAP_STEP_PARAM].getValue() > 0.5f);
            if (currentTapHigh && !lastTapButtonHigh) {
                // Rising edge — record tap time
                float currentTime = args.sampleTime * args.frame;
                tapTimes[2] = tapTimes[1];
                tapTimes[1] = tapTimes[0];
                tapTimes[0] = currentTime;
                // Average last two intervals once we have three taps
                if (tapTimes[2] > 0.f) {
                    float avgInterval = ((tapTimes[0] - tapTimes[1]) + (tapTimes[1] - tapTimes[2])) / 2.f;
                    if (avgInterval > 0.1f && avgInterval < 4.f) {
                        animationRate = 1.f / avgInterval;
                        tapValid = true;
                    }
                }
            }
            lastTapButtonHigh = currentTapHigh;
            // Timeout after 5 seconds of no taps
            if (tapValid && (args.sampleTime * args.frame - tapTimes[0]) > 5.f) {
                tapValid = false;
            }
        }
        
        // Process MuRF-style animation
        float rate;
        if (tapValid) {
            rate = animationRate; // Use tap tempo rate
        } else {
            rate = params[RATE_PARAM].getValue();
            if (inputs[RATE_CV_INPUT].isConnected()) {
                rate += inputs[RATE_CV_INPUT].getVoltage();
            }
        }
        rate = clamp(rate, 0.08f, 4.f);
        
        // Update current pattern selection (1-12 maps to array indices 0-11)
        currentPattern = (int)params[PATTERN_PARAM].getValue() - 1;
        currentPattern = clamp(currentPattern, 0, 11);

        const PatternDefinition& patternDef = bankBLFO ? MURF_BANK_B_PATTERNS[currentPattern]
                                                       : MURF_BANK_A_PATTERNS[currentPattern];
        const int patternSteps = std::max(1, (int)patternDef.stepCount);
        currentPatternIsStatic = patternDef.noAnimation;
        if (currentStep >= patternSteps) {
            currentStep = 0;
        }
        
        // Update step sequencer - rate is in Hz for the full pattern
        // Each pattern cycle should complete in (1/rate) seconds
        // Each step should advance every (1/rate)/patternSteps seconds
        float stepRate = rate * patternSteps; // Steps per second
        stepPhase += stepRate * args.sampleTime;
        
        if (stepPhase >= 1.f) {
            stepPhase -= 1.f;
            currentStep = (currentStep + 1) % patternSteps;

            // Trigger envelopes for active filters in this step (all voices)
            if (!currentPatternIsStatic) {
                // Get the bitmask for this step in the current pattern
                uint8_t activeFilters = patternDef.steps[currentStep];
                
                // Trigger envelope for each active filter on all voices
                for (int v = 0; v < MAX_POLY_VOICES; v++) {
                    for (int i = 0; i < 8; i++) {
                        if (activeFilters & (1 << i)) {
                            envelopes[v][i].trigger();
                        }
                    }
                }
            }
        }
        
        // Update overall animation phase for visual feedback
        animationPhase = (currentStep + stepPhase) / patternSteps;
        
        // Update envelope shapes
        float envelopeShape = params[ENVELOPE_PARAM].getValue();
        if (inputs[ENVELOPE_CV_INPUT].isConnected()) {
            envelopeShape += inputs[ENVELOPE_CV_INPUT].getVoltage();
        }
        envelopeShape = clamp(envelopeShape, 0.f, 10.f);
        
        for (int v = 0; v < MAX_POLY_VOICES; v++) {
            for (int i = 0; i < 8; i++) {
                envelopes[v][i].setEnvelopeShape(envelopeShape, rate);
            }
        }
        
        if (hasStereoOutput) {
            // Stereo mode: process L and R separately
            
            // Process Left channel
            if (inputs[AUDIO_LEFT_INPUT].isConnected()) {
                for (int ch = 0; ch < leftChannels; ch++) {
                    float leftInput = inputs[AUDIO_LEFT_INPUT].getVoltage(ch);
                    leftInput *= drive;
                    
                    float leftOutput = processFilterBank(leftInput, ch, args);
                    
                    // Apply mix
                    float mix = params[MIX_PARAM].getValue();
                    if (inputs[MIX_CV_INPUT].isConnected()) {
                        mix += inputs[MIX_CV_INPUT].getPolyVoltage(ch) / 10.f;
                    }
                    mix = clamp(mix, 0.f, 1.f);
                    
                    leftOutput = leftInput * (1.f - mix) + leftOutput * mix;
                    leftOutput *= params[OUTPUT_PARAM].getValue();
                    
                    // Sum all polyphonic channels to mono left output
                    if (ch == 0) {
                        outputs[LEFT_MONO_OUTPUT].setVoltage(leftOutput, 0);
                    } else {
                        outputs[LEFT_MONO_OUTPUT].setVoltage(outputs[LEFT_MONO_OUTPUT].getVoltage(0) + leftOutput, 0);
                    }
                }
            } else {
                outputs[LEFT_MONO_OUTPUT].setVoltage(0.f, 0);
            }
            
            // Process Right channel
            if (hasStereoInput) {
                // True stereo input
                for (int ch = 0; ch < rightChannels; ch++) {
                    float rightInput = inputs[AUDIO_RIGHT_INPUT].getVoltage(ch);
                    rightInput *= drive;
                    
                    // Use voice offset to avoid conflicts with left processing
                    int rightVoice = std::min(ch + 3, MAX_POLY_VOICES - 1); 
                    float rightOutput = processFilterBank(rightInput, rightVoice, args);
                    
                    // Apply mix
                    float mix = params[MIX_PARAM].getValue();
                    if (inputs[MIX_CV_INPUT].isConnected()) {
                        mix += inputs[MIX_CV_INPUT].getPolyVoltage(ch) / 10.f;
                    }
                    mix = clamp(mix, 0.f, 1.f);
                    
                    rightOutput = rightInput * (1.f - mix) + rightOutput * mix;
                    rightOutput *= params[OUTPUT_PARAM].getValue();
                    
                    // Sum all polyphonic channels to mono right output
                    if (ch == 0) {
                        outputs[RIGHT_OUTPUT].setVoltage(rightOutput, 0);
                    } else {
                        outputs[RIGHT_OUTPUT].setVoltage(outputs[RIGHT_OUTPUT].getVoltage(0) + rightOutput, 0);
                    }
                }
            } else {
                // Mono->Stereo: duplicate left to right
                outputs[RIGHT_OUTPUT].setVoltage(outputs[LEFT_MONO_OUTPUT].getVoltage(0), 0);
            }
        } else {
            // Mono mode: process all polyphonic channels to left/mono output
            if (inputs[AUDIO_LEFT_INPUT].isConnected()) {
                for (int ch = 0; ch < leftChannels; ch++) {
                    float input = inputs[AUDIO_LEFT_INPUT].getVoltage(ch);
                    input *= drive;
                    
                    float output = processFilterBank(input, ch, args);
                    
                    // Apply mix
                    float mix = params[MIX_PARAM].getValue();
                    if (inputs[MIX_CV_INPUT].isConnected()) {
                        mix += inputs[MIX_CV_INPUT].getPolyVoltage(ch) / 10.f;
                    }
                    mix = clamp(mix, 0.f, 1.f);
                    
                    output = input * (1.f - mix) + output * mix;
                    output *= params[OUTPUT_PARAM].getValue();
                    
                    outputs[LEFT_MONO_OUTPUT].setVoltage(output, ch);
                }
            }
        }
        
        // Update lights
        lights[RATE_LIGHT].setBrightness(0.5f + 0.5f * std::sin(animationPhase * 2.f * M_PI));
    }
    
    float processFilterBank(float input, int voice, const ProcessArgs& args) {
        float output = 0.f;
        
        // LFO/Sweep CV — behaviour matches the Moog MF-105 MuRF:
        //   LFO OFF: CV directly shifts the whole filter bank up/down in frequency
        //            (expression-pedal sweep mode; ±5 V gives ±2 octaves)
        //   LFO ON:  CV modulates the LFO rate (exponential, 0.08 Hz–20 Hz range)
        float lfoModulation = 0.f;
        float sweepMultiplier = 1.f; // multiplicative frequency shift when LFO is off

        bool sweepCVConnected = inputs[LFO_SWEEP_CV_INPUT].isConnected();
        float sweepCV = sweepCVConnected ? inputs[LFO_SWEEP_CV_INPUT].getVoltage() : 0.f;

        if (lfoOn) {
            // CV shifts LFO rate exponentially: 0 V → base rate, ±5 V → ×/÷ ~5.7
            float lfoFreq = 0.5f;
            if (sweepCVConnected) {
                lfoFreq = lfoFreq * std::pow(2.0f, sweepCV * 0.5f);
            }
            lfo.setFreq(clamp(lfoFreq, 0.08f, 20.f));
            lfoModulation = lfo.process(args.sampleTime);

            // Sweep the filter bank with the LFO (±30% frequency range)
            float sampleRate = APP->engine->getSampleRate();
            bool highQ = (params[Q_FACTOR_SWITCH_PARAM].getValue() > 0.5f);
            float lowpassQ = highQ ? 1.5f : 0.9f;
            float bandpassQ = highQ ? 4.5f : 2.5f;
            for (int i = 0; i < 8; i++) {
                float baseFreq = bassVoicing ? BASS_FREQS[i] : MIDS_FREQS[i];
                float modulatedFreq = baseFreq * (1.f + lfoModulation * 0.3f);
                if (bassVoicing && i == 0) {
                    filters[voice][i].setLowpass(modulatedFreq, lowpassQ, sampleRate);
                } else {
                    filters[voice][i].setBandpass(modulatedFreq, bandpassQ, sampleRate);
                }
            }
        } else if (sweepCVConnected) {
            // LFO off — CV directly shifts all filter frequencies as a group.
            // ±5 V = ±2 octaves (V/oct-style, half-scale so expression pedals feel natural).
            // Smooth the CV once per sample (voice == 0) with a ~2ms time constant so that
            // audio-rate signals can't modulate IIR coefficients fast enough to cause instability.
            if (voice == 0) {
                float smoothCoeff = 1.f - std::exp(-args.sampleTime / 0.002f);
                sweepCVSmooth += (sweepCV - sweepCVSmooth) * smoothCoeff;
            }
            sweepMultiplier = std::pow(2.0f, sweepCVSmooth * 0.4f);

            float sampleRate = APP->engine->getSampleRate();
            bool highQ = (params[Q_FACTOR_SWITCH_PARAM].getValue() > 0.5f);
            float lowpassQ = highQ ? 1.5f : 0.9f;
            float bandpassQ = highQ ? 4.5f : 2.5f;
            for (int i = 0; i < 8; i++) {
                float baseFreq = (bassVoicing ? BASS_FREQS[i] : MIDS_FREQS[i]) * sweepMultiplier;
                if (bassVoicing && i == 0) {
                    filters[voice][i].setLowpass(baseFreq, lowpassQ, sampleRate);
                } else {
                    filters[voice][i].setBandpass(baseFreq, bandpassQ, sampleRate);
                }
            }
        }
        
        // Process each filter with MuRF-style envelope control for this voice
        for (int i = 0; i < 8; i++) {
            // Base filter gain from slider
            float filterGain = params[FILTER_1_PARAM + i].getValue();
            
            // Add CV modulation for this filter (if not bypassed)
            bool cvBypass = (params[CV_BYPASS_SWITCH_PARAM].getValue() > 0.5f);
            if (inputs[FILTER_1_CV_INPUT + i].isConnected() && !cvBypass) {
                float cv = inputs[FILTER_1_CV_INPUT + i].getPolyVoltage(voice);
                float cvModulation = cv / 5.f; // ±5V = ±1.0 modulation
                filterGain = clamp(filterGain + cvModulation, 0.f, 1.f);
            }
            
            if (filterGain > 0.001f) {
                // Apply filter for this voice
                float filtered = filters[voice][i].process(input);
                
                // Apply envelope modulation based on pattern
                if (currentPatternIsStatic) {
                    // No-animation patterns: no envelope modulation, just slider values + CV
                    output += filtered * filterGain;
                } else {
                    // Other patterns: Use envelope to gate the filter
                    float envelope = envelopes[voice][i].process(args.sampleTime);
                    
                    // In MuRF style, the envelope acts as a gate/VCA for each filter
                    // The slider + CV sets the maximum level, the envelope controls when it's active
                    output += filtered * filterGain * envelope;
                }
            }
        }
        
        return output * 1.2f; // Higher output for musical resonant filters
    }
};

const Incantation::PatternDefinition Incantation::MURF_BANK_A_PATTERNS[12] = {
    // 1) No Animation
    {1, true, {0xFF}},
    // 2) Upward Staircase
    {8, false, {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}},
    // 3) Downward Cascade
    {24, false, {0xA0, 0x40, 0x20, 0x50, 0x20, 0x10, 0x28, 0x10, 0x08, 0x14, 0x08, 0x04, 0x0A, 0x04, 0x02, 0x05, 0x02, 0x01, 0x82, 0x01, 0x80, 0x41, 0x80, 0x40}},
    // 4) Crisscross
    {6, false, {0x81, 0x42, 0x24, 0x18, 0x24, 0x42}},
    // 5) Tremolo
    {4, false, {0xFF, 0xFF, 0xFF, 0xFF}},
    // 6) Upward Bounce
    {16, false, {0x01, 0x10, 0x02, 0x20, 0x04, 0x40, 0x08, 0x80, 0x10, 0x01, 0x20, 0x02, 0x40, 0x04, 0x80, 0x08}},
    // 7) Brownian Motion
    {64, false, {0x04, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x40, 0x20, 0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x20, 0x40, 0x80, 0x40, 0x20, 0x10, 0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08}},
    // 8) Random-like
    {37, false, {0x81, 0x02, 0x00, 0x10, 0x08, 0x21, 0x40, 0x21, 0x00, 0x80, 0x20, 0x82, 0x4A, 0x04, 0x19, 0x10, 0x20, 0x80, 0x40, 0x02, 0x00, 0x04, 0x0A, 0x00, 0x01, 0x20, 0x04, 0x00, 0x20, 0x08, 0x00, 0x01, 0x01, 0x08, 0x00, 0x00, 0x00}},
    // 9) Double Up and Down
    {16, false, {0xC3, 0x61, 0x30, 0x98, 0xCC, 0x66, 0x33, 0x61, 0xC3, 0x86, 0x0C, 0x19, 0x33, 0x66, 0xCC, 0x86}},
    // 10) Downward Band Expansion
    {32, false, {0x81, 0x83, 0x87, 0x8F, 0xC0, 0xC1, 0xC3, 0xC7, 0x60, 0xE0, 0xE1, 0xE3, 0x30, 0x70, 0xF0, 0xF1, 0x18, 0x38, 0x78, 0xF8, 0x0C, 0x1C, 0x3C, 0x7C, 0x06, 0x0E, 0x1E, 0x3E, 0x03, 0x07, 0x0F, 0x1F}},
    // 11) Polyrhythm
    {8, false, {0x67, 0x99, 0x55, 0xFA, 0x2D, 0x92, 0x4A, 0xE7}},
    // 12) Rhythmicon
    {16, false, {0xFF, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
};

const Incantation::PatternDefinition Incantation::MURF_BANK_B_PATTERNS[12] = {
    // 1) No Animation
    {1, true, {0xFF}},
    // 2) Downward Staircase
    {8, false, {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}},
    // 3) Upward Cascade
    {24, false, {0x05, 0x02, 0x04, 0x0A, 0x04, 0x08, 0x14, 0x08, 0x10, 0x28, 0x10, 0x20, 0x50, 0x20, 0x40, 0xA0, 0x40, 0x80, 0x41, 0x80, 0x01, 0x82, 0x01, 0x02}},
    // 4) Down and Up
    {16, false, {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}},
    // 5) Come Together
    {8, false, {0x01, 0x80, 0x02, 0x40, 0x04, 0x20, 0x08, 0x10}},
    // 6) Seesaw Panner
    {8, false, {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA}},
    // 7) Rising Falling
    {32, false, {0x08, 0x04, 0x02, 0x01, 0x10, 0x08, 0x04, 0x02, 0x20, 0x10, 0x08, 0x04, 0x40, 0x20, 0x10, 0x08, 0x80, 0x40, 0x20, 0x10, 0x01, 0x80, 0x40, 0x20, 0x02, 0x01, 0x80, 0x40, 0x04, 0x02, 0x01, 0x80}},
    // 8) Pulsar
    {16, false, {0xF0, 0x0F, 0x10, 0x08, 0x30, 0x0C, 0x10, 0x08, 0x70, 0x0E, 0x10, 0x08, 0x30, 0x0C, 0x10, 0x08}},
    // 9) Upward Notch
    {24, false, {0xF9, 0xF9, 0xF9, 0xF3, 0xF3, 0xF3, 0xE7, 0xE7, 0xE7, 0xCF, 0xCF, 0xCF, 0x9F, 0x9F, 0x9F, 0x3F, 0x3F, 0x3F, 0x7E, 0x7E, 0x7E, 0xFC, 0xFC, 0xFC}},
    // 10) Growing and Shrinking Band
    {32, false, {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF}},
    // 11) Double Cascade
    {32, false, {0x09, 0x14, 0x0A, 0x05, 0x12, 0x28, 0x14, 0x0A, 0x24, 0x50, 0x28, 0x14, 0x48, 0xA0, 0x50, 0x28, 0x90, 0x41, 0xA0, 0x50, 0x21, 0x82, 0x41, 0xA0, 0x42, 0x05, 0x82, 0x41, 0x84, 0x0A, 0x05, 0x82}},
    // 12) Inverted Rhythmicon
    {16, false, {0xFF, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
};

struct IncantationWidget : ModuleWidget {
    // Use fixed-density leather mapping to avoid horizontal stretch on
    // wider panels; blend an offset pass to soften repeat seams.
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;
            nvgSave(args.vg);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

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

    IncantationWidget(Incantation* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Incantation.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Incantation.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        // Controls currently present on the panel SVG.
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("drive_knob", 13.208855f, 19.975176f), module, Incantation::DRIVE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("mix_knob", 46.762012f, 19.582415f), module, Incantation::MIX_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageMedium>(centerPx("output_knob", 80.315178f, 19.2085f), module, Incantation::OUTPUT_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggleOffPos4>(
            centerPx("bank_switch", 30.777031f, 24.163727f), module, Incantation::BANK_SWITCH_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggleOffPos4>(
            centerPx("freq_switch", 30.777031f, 35.797565f), module, Incantation::FREQ_SWITCH_PARAM));

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("pattern_knob", 28.202541f, 36.118465f), module, Incantation::PATTERN_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("env_knob", 64.797302f, 36.118465f), module, Incantation::ENVELOPE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(centerPx("rate_knob", 48.417404f, 54.276268f), module, Incantation::RATE_PARAM));

        for (int i = 0; i < 8; i++) {
            std::string id = "fader_" + std::to_string(i + 1);
            float fallbackX = 7.9228535f + 11.161139f * i;
            auto* slider = createParamCentered<VintageSliderLarge>(
                centerPx(id, fallbackX, 81.385406f), module, Incantation::FILTER_1_PARAM + i);
            slider->box.pos.x += VintageSliderLarge::TRACK_CENTER_OFFSET_X;
            addParam(slider);
        }

        for (int i = 0; i < 8; i++) {
            std::string id = "filter_" + std::to_string(i + 1) + "_cv";
            float fallbackX = 13.208855f + 9.662611f * i;
            addInput(createInputCentered<ShapetakerBNCPort>(
                centerPx(id, fallbackX, 100.08533f), module, Incantation::FILTER_1_CV_INPUT + i));
        }

        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("audio_input_l", 20.993757f, 114.72874f), module, Incantation::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("audio_input_r", 34.338997f, 114.72874f), module, Incantation::AUDIO_RIGHT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("envelope_cv", 32.534077f, 114.78314f), module, Incantation::ENVELOPE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("rate_cv", 42.19669f, 114.78314f), module, Incantation::RATE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("lfo_sweep_cv", 51.859303f, 114.78314f), module, Incantation::LFO_SWEEP_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(
            centerPx("mix_cv", 61.521915f, 114.78314f), module, Incantation::MIX_CV_INPUT));
        addParam(createParamCentered<VCVButton>(
            centerPx("tap_step_input", 62.746994f, 29.392536f), module, Incantation::TAP_STEP_PARAM));
        addParam(createParamCentered<ShapetakerDarkToggleOffPos4>(
            centerPx("lfo_sweep_switch", 62.868774f, 39.325058f), module, Incantation::LFO_SWITCH_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("audio_output_l", 61.02948f, 114.72874f), module, Incantation::LEFT_MONO_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(
            centerPx("audio_output_r", 74.374725f, 114.72874f), module, Incantation::RIGHT_OUTPUT));
    }
};

Model* modelIncantation = createModel<Incantation, IncantationWidget>("Incantation");
