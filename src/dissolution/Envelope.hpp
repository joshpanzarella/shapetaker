#pragma once
#include <cmath>
#include <algorithm>

namespace shapetaker {
namespace dissolution {

// Simple AR (Attack-Release) envelope generator
class Envelope {
public:
    enum Stage {
        IDLE,
        ATTACK,
        SUSTAIN,
        RELEASE
    };

    Envelope() : stage(IDLE), value(0.f), attackTime(0.01f), releaseTime(1.0f),
                 attackCoeff(0.f), releaseCoeff(0.f), sampleRate(44100.f) {
        updateCoefficients();
    }

    void setSampleRate(float sr) {
        sampleRate = sr;
        updateCoefficients();
    }

    void setAttackTime(float seconds) {
        attackTime = std::max(0.001f, seconds);
        updateCoefficients();
    }

    void setReleaseTime(float seconds) {
        releaseTime = std::max(0.001f, seconds);
        updateCoefficients();
    }

    void trigger() {
        stage = ATTACK;
    }

    void release() {
        if (stage != IDLE) {
            stage = RELEASE;
        }
    }

    void forceSustain() {
        stage = SUSTAIN;
        value = 1.f;
    }

    void reset() {
        stage = IDLE;
        value = 0.f;
    }

    bool isActive() const {
        return stage != IDLE;
    }

    bool isComplete() const {
        return stage == IDLE;
    }

    float process() {
        switch (stage) {
            case IDLE:
                value = 0.f;
                break;

            case ATTACK:
                value += (1.f - value) * (1.f - attackCoeff);
                if (value >= 0.999f) {
                    value = 1.f;
                    stage = SUSTAIN;
                }
                break;

            case SUSTAIN:
                value = 1.f;
                break;

            case RELEASE:
                value *= releaseCoeff;
                if (value <= 0.001f) {
                    value = 0.f;
                    stage = IDLE;
                }
                break;
        }

        return value;
    }

    float getValue() const {
        return value;
    }

    Stage getStage() const {
        return stage;
    }

private:
    void updateCoefficients() {
        // Exponential curve coefficients
        attackCoeff = std::exp(-1.f / (attackTime * sampleRate));
        releaseCoeff = std::exp(-1.f / (releaseTime * sampleRate));
    }

    Stage stage;
    float value;
    float attackTime;
    float releaseTime;
    float attackCoeff;
    float releaseCoeff;
    float sampleRate;
};

} // namespace dissolution
} // namespace shapetaker
