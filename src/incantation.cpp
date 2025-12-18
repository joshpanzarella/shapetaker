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
        
        // Tap/Step input
        TAP_STEP_INPUT,
        
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

    // Polyphonic support (up to 8 voices like other Shapetaker modules)
    static const int MAX_POLY_VOICES = 8;
    
    // 8 resonant filters per voice
    ResonantFilter filters[MAX_POLY_VOICES][8];
    FilterEnvelope envelopes[MAX_POLY_VOICES][8];
    LFO lfo;
    
    
    // Pattern step length (8 steps per pattern, matching original MuRF)
    static constexpr int PATTERN_STEPS = 8;
    
    // Authentic MuRF patterns - 8 steps, each step defines which of the 8 filters are active
    // Each pattern entry is a bitmask where bit 0 = filter 1, bit 7 = filter 8
    static constexpr uint8_t MURF_AUTHENTIC_PATTERNS[12][8] = {
        // Pattern 1: Static - all filters on (no sequencing)
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        
        // Pattern 2: Upward Staircase (filters turn on sequentially low to high)
        {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80},
        
        // Pattern 3: Downward Staircase (filters turn on sequentially high to low)
        {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01},
        
        // Pattern 4: Up and Down (alternating pattern)
        {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA}, // 01010101, 10101010 alternating
        
        // Pattern 5: Repeater (pairs of filters)
        {0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC}, // 00110011, 11001100
        
        // Pattern 6: X-Factor (cross/diagonal pattern)
        {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81}, // Outside-in diamond
        
        // Pattern 7: Perpetual Motion (complex moving pattern)
        {0x11, 0x22, 0x44, 0x88, 0x44, 0x22, 0x11, 0x88},
        
        // Pattern 8: Upward Cascade (building complexity)
        {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF},
        
        // Pattern 9: Pyramid (center frequencies outward)
        {0x18, 0x3C, 0x7E, 0xFF, 0x7E, 0x3C, 0x18, 0x00},
        
        // Pattern 10: Asymmetry (uneven/off-beat pattern)
        {0x89, 0x46, 0x32, 0x1C, 0x64, 0xC8, 0x91, 0x23},
        
        // Pattern 11: Arpeggiated Perpetual Motion (sparse arpeggio)
        {0x11, 0x00, 0x44, 0x00, 0x22, 0x00, 0x88, 0x00},
        
        // Pattern 12: Complex/Random (rich polyrhythmic pattern)
        {0x95, 0x2A, 0x54, 0xA8, 0x51, 0x8A, 0x45, 0xA2}
    };
    
    // State variables
    bool bassVoicing = true; // true = BASS, false = MIDS
    bool lfoOn = false;
    float driveLevel = 0.f;
    int currentPattern = 1; // 0-11 (Pattern 1-12)
    float animationPhase = 0.f;
    float animationRate = 1.f;
    int currentStep = 0; // Current step in the pattern (0-15)
    float stepPhase = 0.f; // Phase within current step
    
    // For tap tempo
    float tapTimes[3] = {0.f, 0.f, 0.f};
    int tapIndex = 0;
    bool tapValid = false;
    

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
            {"1 - Static", "2 - Up Stairs", "3 - Down Stairs", "4 - Up/Down", 
             "5 - Repeater", "6 - X-Factor", "7 - Perpetual", "8 - Cascade",
             "9 - Pyramid", "10 - Asymmetry", "11 - Arp Motion", "12 - Complex"});
        configParam(ENVELOPE_PARAM, 0.f, 10.f, 2.f, "Envelope");
        configParam(RATE_PARAM, 0.08f, 4.f, 1.f, "Rate", "Hz");
        
        // Switches
        configSwitch(FREQ_SWITCH_PARAM, 0.f, 1.f, 0.f, "Frequency Voicing", {"BASS", "MIDS"});
        configSwitch(LFO_SWITCH_PARAM, 0.f, 1.f, 0.f, "LFO", {"OFF", "ON"});
        configSwitch(Q_FACTOR_SWITCH_PARAM, 0.f, 1.f, 0.f, "Filter Resonance", {"Normal", "High Q"});
        configSwitch(CV_BYPASS_SWITCH_PARAM, 0.f, 1.f, 0.f, "Filter CV", {"ACTIVE", "BYPASS"});
        
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
        
        configInput(TAP_STEP_INPUT, "Tap/Step");
        
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
                          inputs[AUDIO_LEFT_INPUT].getChannels() : 0;
        int rightChannels = hasStereoInput ? inputs[AUDIO_RIGHT_INPUT].getChannels() : 0;
        
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
        
        // Handle tap tempo input (only if connected and recent taps)
        if (inputs[TAP_STEP_INPUT].isConnected()) {
            static bool lastTapHigh = false;
            bool currentTapHigh = (inputs[TAP_STEP_INPUT].getVoltage() > 1.f);
            
            if (currentTapHigh && !lastTapHigh) {
                // Rising edge detected - tap tempo
                float currentTime = args.sampleTime * args.frame;
                
                // Shift tap times
                tapTimes[2] = tapTimes[1];
                tapTimes[1] = tapTimes[0];
                tapTimes[0] = currentTime;
                
                // Calculate average tempo if we have enough taps
                if (tapTimes[2] > 0.f) {
                    float avgInterval = ((tapTimes[0] - tapTimes[1]) + (tapTimes[1] - tapTimes[2])) / 2.f;
                    if (avgInterval > 0.1f && avgInterval < 4.f) { // Reasonable range
                        animationRate = 1.f / avgInterval; // Convert to Hz
                        tapValid = true;
                    }
                }
            }
            lastTapHigh = currentTapHigh;
            
            // Timeout tap tempo after 5 seconds of no input
            if (tapValid && (args.sampleTime * args.frame - tapTimes[0]) > 5.f) {
                tapValid = false;
            }
        } else {
            // No tap input connected, disable tap tempo
            tapValid = false;
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
        
        // Update step sequencer - rate is in Hz for the full pattern
        // Each pattern cycle should complete in (1/rate) seconds
        // Each step should advance every (1/rate)/PATTERN_STEPS seconds
        float stepRate = rate * PATTERN_STEPS; // Steps per second
        stepPhase += stepRate * args.sampleTime;
        
        if (stepPhase >= 1.f) {
            stepPhase -= 1.f;
            currentStep = (currentStep + 1) % PATTERN_STEPS;
            
            // Trigger envelopes for active filters in this step (all voices)
            if (currentPattern > 0) { // Pattern 1 (index 0) is static, no triggers
                // Get the bitmask for this step in the current pattern
                uint8_t activeFilters = MURF_AUTHENTIC_PATTERNS[currentPattern][currentStep];
                
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
        animationPhase = (currentStep + stepPhase) / PATTERN_STEPS;
        
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
        
        // Get LFO modulation if enabled
        float lfoModulation = 0.f;
        if (lfoOn) {
            // Update LFO frequency from CV input
            float lfoFreq = 0.5f; // Base frequency
            if (inputs[LFO_SWEEP_CV_INPUT].isConnected()) {
                lfoFreq += inputs[LFO_SWEEP_CV_INPUT].getVoltage() * 0.2f;
            }
            lfo.setFreq(clamp(lfoFreq, 0.05f, 5.f));
            
            lfoModulation = lfo.process(args.sampleTime);
            
            // Apply LFO frequency sweeping by updating filter frequencies for this voice
            float sampleRate = APP->engine->getSampleRate();
            bool highQ = (params[Q_FACTOR_SWITCH_PARAM].getValue() > 0.5f);
            float lowpassQ = highQ ? 1.5f : 0.9f;
            float bandpassQ = highQ ? 4.5f : 2.5f;
            
            for (int i = 0; i < 8; i++) {
                float baseFreq = bassVoicing ? BASS_FREQS[i] : MIDS_FREQS[i];
                float modulatedFreq = baseFreq * (1.f + lfoModulation * 0.3f); // ±30% frequency sweep
                
                if (bassVoicing && i == 0) {
                    filters[voice][i].setLowpass(modulatedFreq, lowpassQ, sampleRate);
                } else {
                    filters[voice][i].setBandpass(modulatedFreq, bandpassQ, sampleRate);
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
                if (currentPattern == 0) {
                    // Pattern 1: Static - no envelope modulation, just slider values + CV
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

struct IncantationWidget : ModuleWidget {
    IncantationWidget(Incantation* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Incantation.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        auto svgPath = asset::plugin(pluginInstance, "res/panels/Incantation.svg");
        LayoutHelper::PanelSVGParser parser(svgPath);
        auto centerPx = LayoutHelper::createCenterPxHelper(parser);

        // Main controls (top section - more spaced out)
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltLarge>(centerPx("inc-drive-knob", 24.f, 20.f), module, Incantation::DRIVE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltLarge>(centerPx("inc-mix-knob", 47.f, 20.f), module, Incantation::MIX_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltLarge>(centerPx("inc-output-knob", 70.f, 20.f), module, Incantation::OUTPUT_PARAM));

        // Second row of controls - more spaced
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(centerPx("inc-pattern-knob", 24.f, 35.f), module, Incantation::PATTERN_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(centerPx("inc-envelope-knob", 47.f, 35.f), module, Incantation::ENVELOPE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobAltMedium>(centerPx("inc-rate-knob", 70.f, 35.f), module, Incantation::RATE_PARAM));
        
        // Preset buttons - spread out more
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("inc-preset-zero", 24.f, 48.f), module, Incantation::PRESET_ZERO_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("inc-preset-half", 37.f, 48.f), module, Incantation::PRESET_HALF_PARAM));
        addParam(createParamCentered<ShapetakerVintageMomentary>(centerPx("inc-preset-full", 50.f, 48.f), module, Incantation::PRESET_FULL_PARAM));
        
        // Filter sliders - more spacing between them
        for (int i = 0; i < 8; i++) {
            std::string id = "inc-filter-slider-" + std::to_string(i);
            float fallbackX = 16.f + i * 10.f;
            addParam(createParamCentered<VintageSlider>(centerPx(id, fallbackX, 65.f), module, Incantation::FILTER_1_PARAM + i));
        }
        
        // Filter CV inputs - aligned below sliders with same spacing
        for (int i = 0; i < 8; i++) {
            std::string id = "inc-filter-cv-" + std::to_string(i);
            float fallbackX = 16.f + i * 10.f;
            addInput(createInputCentered<ShapetakerBNCPort>(centerPx(id, fallbackX, 85.f), module, Incantation::FILTER_1_CV_INPUT + i));
        }
        
        // CV Bypass switch - positioned to the left of filter sliders
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("inc-cv-bypass-switch", 10.5f, 75.f), module, Incantation::CV_BYPASS_SWITCH_PARAM));
        
        // Switches - spread out more
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("inc-freq-switch", 26.f, 100.f), module, Incantation::FREQ_SWITCH_PARAM));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("inc-lfo-switch", 47.f, 100.f), module, Incantation::LFO_SWITCH_PARAM));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(
            centerPx("inc-q-switch", 68.f, 100.f), module, Incantation::Q_FACTOR_SWITCH_PARAM));
        
        // Main inputs - better spacing  
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-audio-left-in", 14.f, 115.f), module, Incantation::AUDIO_LEFT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-audio-right-in", 26.f, 115.f), module, Incantation::AUDIO_RIGHT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-envelope-cv-in", 38.f, 115.f), module, Incantation::ENVELOPE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-rate-cv-in", 50.f, 115.f), module, Incantation::RATE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-lfo-sweep-cv-in", 62.f, 115.f), module, Incantation::LFO_SWEEP_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-mix-cv-in", 74.f, 115.f), module, Incantation::MIX_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("inc-tap-step-in", 86.f, 115.f), module, Incantation::TAP_STEP_INPUT));
        
        // Outputs - moved up to prevent cutoff
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("inc-left-output", 66.f, 125.f), module, Incantation::LEFT_MONO_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("inc-right-output", 80.f, 125.f), module, Incantation::RIGHT_OUTPUT));
        
        // Lights - repositioned
        addChild(createLightCentered<MediumLight<RedLight>>(centerPx("inc-rate-light", 82.f, 35.f), module, Incantation::RATE_LIGHT));
        addChild(createLightCentered<MediumLight<GreenRedLight>>(centerPx("inc-drive-light", 15.f, 20.f), module, Incantation::DRIVE_LIGHT));
    }
};

Model* modelIncantation = createModel<Incantation, IncantationWidget>("Incantation");
