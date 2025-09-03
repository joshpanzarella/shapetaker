#pragma once
#include <rack.hpp>
#include <cmath>

using namespace rack;

namespace shapetaker {
namespace dsp {

// ============================================================================
// ENVELOPE UTILITIES
// ============================================================================

// Full ADSR envelope generator
class EnvelopeGenerator {
public:
    enum Stage {
        IDLE,
        ATTACK,
        DECAY,
        SUSTAIN,
        RELEASE
    };

private:
    Stage currentStage = IDLE;
    float currentLevel = 0.f;
    float attackRate = 0.001f;
    float decayRate = 0.001f;
    float sustainLevel = 0.7f;
    float releaseRate = 0.001f;
    bool gateHigh = false;

public:
    void setAttack(float seconds, float sampleRate) {
        attackRate = (seconds > 0.f) ? 1.f / (seconds * sampleRate) : 1.f;
    }
    
    void setDecay(float seconds, float sampleRate) {
        decayRate = (seconds > 0.f) ? 1.f / (seconds * sampleRate) : 1.f;
    }
    
    void setSustain(float level) {
        sustainLevel = rack::math::clamp(level, 0.f, 1.f);
    }
    
    void setRelease(float seconds, float sampleRate) {
        releaseRate = (seconds > 0.f) ? 1.f / (seconds * sampleRate) : 1.f;
    }
    
    void gate(bool high) {
        if (high && !gateHigh) {
            // Gate on: start attack
            currentStage = ATTACK;
        } else if (!high && gateHigh) {
            // Gate off: start release
            currentStage = RELEASE;
        }
        gateHigh = high;
    }
    
    float process() {
        switch (currentStage) {
            case IDLE:
                currentLevel = 0.f;
                break;
                
            case ATTACK:
                currentLevel += attackRate;
                if (currentLevel >= 1.f) {
                    currentLevel = 1.f;
                    currentStage = DECAY;
                }
                break;
                
            case DECAY:
                currentLevel -= decayRate * (currentLevel - sustainLevel);
                if (currentLevel <= sustainLevel + 0.001f) {
                    currentLevel = sustainLevel;
                    currentStage = SUSTAIN;
                }
                break;
                
            case SUSTAIN:
                currentLevel = sustainLevel;
                break;
                
            case RELEASE:
                currentLevel -= releaseRate * currentLevel;
                if (currentLevel <= 0.001f) {
                    currentLevel = 0.f;
                    currentStage = IDLE;
                }
                break;
        }
        
        return currentLevel;
    }
    
    Stage getCurrentStage() const { return currentStage; }
    float getCurrentLevel() const { return currentLevel; }
    bool isActive() const { return currentStage != IDLE; }
    
    void reset() {
        currentStage = IDLE;
        currentLevel = 0.f;
        gateHigh = false;
    }
};

// Simple trigger/gate utilities
class TriggerHelper {
public:
    // Process button trigger with automatic release
    static bool processButton(Param& param, float sampleTime) {
        float value = param.getValue();
        if (value > 0.5f) {
            param.setValue(0.f); // Auto-release
            return true;
        }
        return false;
    }
    
    // Process momentary trigger from parameter
    static bool processTrigger(Param& param, float& lastValue) {
        float value = param.getValue();
        bool triggered = (value > 0.5f && lastValue <= 0.5f);
        lastValue = value;
        return triggered;
    }
    
    // Legacy: Process trigger with SchmittTrigger, param value, input, and threshold  
    static bool processTrigger(rack::dsp::SchmittTrigger& trigger, float paramValue, Input& input, float threshold = 1.f) {
        float combinedValue = paramValue;
        if (input.isConnected()) {
            combinedValue += input.getVoltage();
        }
        return trigger.process(combinedValue, threshold);
    }
    
    // Process toggle button
    static bool processToggle(Param& param, bool& lastPressed) {
        float value = param.getValue();
        bool pressed = (value > 0.5f);
        bool triggered = pressed && !lastPressed;
        lastPressed = pressed;
        return triggered;
    }
    
    // Process toggle with value and state (backward compatibility)
    static bool processToggle(float value, bool& lastPressed, bool& state) {
        bool pressed = (value > 0.5f);
        bool triggered = pressed && !lastPressed;
        if (triggered) {
            state = !state;
        }
        lastPressed = pressed;
        return triggered;
    }
    
    // Legacy: Process toggle with SchmittTrigger, param value, and state
    static bool processToggle(rack::dsp::SchmittTrigger& trigger, float paramValue, bool& state) {
        bool triggered = trigger.process(paramValue);
        if (triggered) {
            state = !state;
        }
        return triggered;
    }
    
    // CV trigger detection with hysteresis
    static bool processCVTrigger(Input& input, bool& lastState, float threshold = 1.f) {
        float voltage = input.getVoltage();
        bool currentState = voltage > threshold;
        bool triggered = currentState && !lastState;
        lastState = currentState;
        return triggered;
    }
};

}} // namespace shapetaker::dsp