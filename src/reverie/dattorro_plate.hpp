#pragma once

#include <rack.hpp>
#include <cmath>
#include <cstring>

namespace shapetaker {
namespace reverie {

// Dattorro plate reverb algorithm (JAES 1997) - COMPLETE topology
// Reference: "Effect Design Part 1: Reverberator and Other Filters"
static const float DATTORRO_REF_RATE = 29761.0f;

// Reference delay lengths (in samples at 29761 Hz)
// Input diffusion chain (4 cascaded allpasses)
static const int REF_INPUT_AP1 = 142;
static const int REF_INPUT_AP2 = 107;
static const int REF_INPUT_AP3 = 379;
static const int REF_INPUT_AP4 = 277;

// Tank left half: modAP -> delay1 -> damp -> decay -> AP2 -> delay2
static const int REF_TANK_MOD_AP_L  = 672;   // Modulated allpass
static const int REF_TANK_DELAY1_L  = 4453;  // First delay
static const int REF_TANK_AP2_L     = 1800;  // Second allpass (not modulated)
static const int REF_TANK_DELAY2_L  = 3720;  // Second delay

// Tank right half: modAP -> delay1 -> damp -> decay -> AP2 -> delay2
static const int REF_TANK_MOD_AP_R  = 908;
static const int REF_TANK_DELAY1_R  = 4217;
static const int REF_TANK_AP2_R     = 2656;
static const int REF_TANK_DELAY2_R  = 3163;

// Output tap positions (at 29761 Hz) - organized by SOURCE ELEMENT
// Left output taps
static const int TAP_L_FROM_D1R_A = 266;    // +  from delay1 right
static const int TAP_L_FROM_D1R_B = 2974;   // +  from delay1 right
static const int TAP_L_FROM_AP2R  = 1913;   // -  from AP2 right
static const int TAP_L_FROM_D2R   = 1996;   // +  from delay2 right
static const int TAP_L_FROM_D1L   = 1990;   // -  from delay1 left
static const int TAP_L_FROM_AP2L  = 187;    // -  from AP2 left
static const int TAP_L_FROM_D2L   = 1066;   // -  from delay2 left

// Right output taps
static const int TAP_R_FROM_D1L_A = 353;    // +  from delay1 left
static const int TAP_R_FROM_D1L_B = 3627;   // +  from delay1 left
static const int TAP_R_FROM_AP2L  = 1228;   // -  from AP2 left
static const int TAP_R_FROM_D2L   = 2673;   // +  from delay2 left
static const int TAP_R_FROM_D1R   = 2111;   // -  from delay1 right
static const int TAP_R_FROM_AP2R  = 335;    // -  from AP2 right
static const int TAP_R_FROM_D2R   = 121;    // -  from delay2 right

// Max scale: support up to 192kHz
static const int MAX_SCALE = 7;

// Individual buffer max sizes
static const int MAX_INPUT_AP1     = REF_INPUT_AP1 * MAX_SCALE + 16;
static const int MAX_INPUT_AP2     = REF_INPUT_AP2 * MAX_SCALE + 16;
static const int MAX_INPUT_AP3     = REF_INPUT_AP3 * MAX_SCALE + 16;
static const int MAX_INPUT_AP4     = REF_INPUT_AP4 * MAX_SCALE + 16;
static const int MAX_TANK_MOD_AP_L = REF_TANK_MOD_AP_L * MAX_SCALE + 64;
static const int MAX_TANK_MOD_AP_R = REF_TANK_MOD_AP_R * MAX_SCALE + 64;
static const int MAX_TANK_DELAY1_L = REF_TANK_DELAY1_L * MAX_SCALE + 16;
static const int MAX_TANK_DELAY1_R = REF_TANK_DELAY1_R * MAX_SCALE + 16;
static const int MAX_TANK_AP2_L    = REF_TANK_AP2_L * MAX_SCALE + 16;
static const int MAX_TANK_AP2_R    = REF_TANK_AP2_R * MAX_SCALE + 16;
static const int MAX_TANK_DELAY2_L = REF_TANK_DELAY2_L * MAX_SCALE + 16;
static const int MAX_TANK_DELAY2_R = REF_TANK_DELAY2_R * MAX_SCALE + 16;

static const int TOTAL_BUFFER_SIZE =
    MAX_INPUT_AP1 + MAX_INPUT_AP2 + MAX_INPUT_AP3 + MAX_INPUT_AP4 +
    MAX_TANK_MOD_AP_L + MAX_TANK_MOD_AP_R +
    MAX_TANK_DELAY1_L + MAX_TANK_DELAY1_R +
    MAX_TANK_AP2_L + MAX_TANK_AP2_R +
    MAX_TANK_DELAY2_L + MAX_TANK_DELAY2_R;

struct AllPassSection {
    float* buffer;
    int size;
    int maxSize;
    int writePos;

    AllPassSection() : buffer(NULL), size(0), maxSize(0), writePos(0) {}

    void init(float* buf, int maxSz) {
        buffer = buf;
        maxSize = maxSz;
        size = 0;
        writePos = 0;
    }

    void setSize(int sz) {
        size = (sz < maxSize) ? sz : maxSize - 1;
        if (size < 1) size = 1;
    }

    float process(float input, float coefficient) {
        if (!buffer) return input;
        int readPos = writePos - size;
        if (readPos < 0) readPos += maxSize;

        float delayed = buffer[readPos];
        float output = -coefficient * input + delayed;
        buffer[writePos] = input + coefficient * output;

        writePos++;
        if (writePos >= maxSize) writePos = 0;

        return output;
    }

    float processModulated(float input, float coefficient, float modOffset) {
        if (!buffer) return input;
        float delayF = (float)size + modOffset;
        if (delayF < 1.0f) delayF = 1.0f;
        if (delayF > (float)(maxSize - 2)) delayF = (float)(maxSize - 2);

        int intDelay = (int)delayF;
        float frac = delayF - (float)intDelay;

        int readPos1 = writePos - intDelay;
        if (readPos1 < 0) readPos1 += maxSize;
        int readPos2 = readPos1 - 1;
        if (readPos2 < 0) readPos2 += maxSize;

        float delayed = buffer[readPos1] * (1.0f - frac) + buffer[readPos2] * frac;
        float output = -coefficient * input + delayed;
        buffer[writePos] = input + coefficient * output;

        writePos++;
        if (writePos >= maxSize) writePos = 0;

        return output;
    }

    // Read a tap from the allpass's internal delay buffer (for output taps)
    float readTap(int tapDelay) {
        if (!buffer) return 0.0f;
        if (tapDelay > size) tapDelay = size;
        if (tapDelay < 0) tapDelay = 0;
        int readPos = writePos - tapDelay;
        if (readPos < 0) readPos += maxSize;
        return buffer[readPos];
    }
};

struct DelaySection {
    float* buffer;
    int size;
    int maxSize;
    int writePos;

    DelaySection() : buffer(NULL), size(0), maxSize(0), writePos(0) {}

    void init(float* buf, int maxSz) {
        buffer = buf;
        maxSize = maxSz;
        size = 0;
        writePos = 0;
    }

    void setSize(int sz) {
        size = (sz < maxSize) ? sz : maxSize - 1;
        if (size < 1) size = 1;
    }

    void write(float input) {
        if (!buffer) return;
        buffer[writePos] = input;
        writePos++;
        if (writePos >= maxSize) writePos = 0;
    }

    float read() {
        if (!buffer) return 0.0f;
        int readPos = writePos - size;
        if (readPos < 0) readPos += maxSize;
        return buffer[readPos];
    }

    float readTap(int tapDelay) {
        if (!buffer) return 0.0f;
        if (tapDelay > size) tapDelay = size;
        if (tapDelay < 0) tapDelay = 0;
        int readPos = writePos - tapDelay;
        if (readPos < 0) readPos += maxSize;
        return buffer[readPos];
    }
};

class DattorroPlate {
private:
    float* memoryBlock;

    // Input diffusion: 4 cascaded allpasses
    AllPassSection inputAP[4];

    // Tank left half: modAP_L -> delay1_L -> damp -> decay -> ap2_L -> delay2_L
    AllPassSection modAP_L;
    DelaySection   delay1_L;
    AllPassSection ap2_L;
    DelaySection   delay2_L;

    // Tank right half: modAP_R -> delay1_R -> damp -> decay -> ap2_R -> delay2_R
    AllPassSection modAP_R;
    DelaySection   delay1_R;
    AllPassSection ap2_R;
    DelaySection   delay2_R;

    float dampState[2];
    float lfoPhase;
    float lfoRate;
    float lfoRateSmoothed;
    float modDepthScaleSmoothed;
    float smoothCoeff; // per-sample smoothing coefficient (~5Hz)
    float tankFeedback[2]; // [0] = output of left half, [1] = output of right half

    // Scaled delay sizes
    int sInputAP[4];
    int sModAP_L, sModAP_R;
    int sDelay1_L, sDelay1_R;
    int sAP2_L, sAP2_R;
    int sDelay2_L, sDelay2_R;

    // Scaled output tap positions
    // Left output taps
    int tapL_d1r_a, tapL_d1r_b, tapL_ap2r, tapL_d2r, tapL_d1l, tapL_ap2l, tapL_d2l;
    // Right output taps
    int tapR_d1l_a, tapR_d1l_b, tapR_ap2l, tapR_d2l, tapR_d1r, tapR_ap2r, tapR_d2r;

    float sampleRate;
    bool initialized;

    int scaleDelay(int refDelay) {
        return (int)(refDelay * sampleRate / DATTORRO_REF_RATE);
    }

    void allocateAndInit() {
        if (memoryBlock) return;
        memoryBlock = new float[TOTAL_BUFFER_SIZE]();

        int offset = 0;
        inputAP[0].init(&memoryBlock[offset], MAX_INPUT_AP1); offset += MAX_INPUT_AP1;
        inputAP[1].init(&memoryBlock[offset], MAX_INPUT_AP2); offset += MAX_INPUT_AP2;
        inputAP[2].init(&memoryBlock[offset], MAX_INPUT_AP3); offset += MAX_INPUT_AP3;
        inputAP[3].init(&memoryBlock[offset], MAX_INPUT_AP4); offset += MAX_INPUT_AP4;

        modAP_L.init(&memoryBlock[offset], MAX_TANK_MOD_AP_L); offset += MAX_TANK_MOD_AP_L;
        delay1_L.init(&memoryBlock[offset], MAX_TANK_DELAY1_L); offset += MAX_TANK_DELAY1_L;
        ap2_L.init(&memoryBlock[offset], MAX_TANK_AP2_L); offset += MAX_TANK_AP2_L;
        delay2_L.init(&memoryBlock[offset], MAX_TANK_DELAY2_L); offset += MAX_TANK_DELAY2_L;

        modAP_R.init(&memoryBlock[offset], MAX_TANK_MOD_AP_R); offset += MAX_TANK_MOD_AP_R;
        delay1_R.init(&memoryBlock[offset], MAX_TANK_DELAY1_R); offset += MAX_TANK_DELAY1_R;
        ap2_R.init(&memoryBlock[offset], MAX_TANK_AP2_R); offset += MAX_TANK_AP2_R;
        delay2_R.init(&memoryBlock[offset], MAX_TANK_DELAY2_R); offset += MAX_TANK_DELAY2_R;

        initialized = true;
    }

public:
    float lastTankOut[2];
    float modDepthScale;

    DattorroPlate() : memoryBlock(NULL), initialized(false) {
        std::memset(dampState, 0, sizeof(dampState));
        std::memset(tankFeedback, 0, sizeof(tankFeedback));
        std::memset(lastTankOut, 0, sizeof(lastTankOut));
        lfoPhase = 0.0f;
        lfoRate = 1.0f;
        lfoRateSmoothed = 1.0f;
        modDepthScale = 1.0f;
        modDepthScaleSmoothed = 1.0f;
        smoothCoeff = 0.0005f;
        sampleRate = 44100.0f;
    }

    ~DattorroPlate() {
        delete[] memoryBlock;
    }

    DattorroPlate(const DattorroPlate&) : memoryBlock(NULL), initialized(false) {
        std::memset(dampState, 0, sizeof(dampState));
        std::memset(tankFeedback, 0, sizeof(tankFeedback));
        std::memset(lastTankOut, 0, sizeof(lastTankOut));
        lfoPhase = 0.0f;
        lfoRate = 1.0f;
        lfoRateSmoothed = 1.0f;
        modDepthScale = 1.0f;
        modDepthScaleSmoothed = 1.0f;
        smoothCoeff = 0.0005f;
        sampleRate = 44100.0f;
    }

    DattorroPlate& operator=(const DattorroPlate&) {
        return *this;
    }

    void setSampleRate(float sr) {
        sampleRate = sr;
        // ~5Hz smoothing for modulation parameters (very gentle transitions)
        smoothCoeff = 1.0f - std::exp(-2.0f * (float)M_PI * 5.0f / sampleRate);
        allocateAndInit();

        // Scale input diffusion
        for (int i = 0; i < 4; i++) {
            static const int refInputAP[4] = {REF_INPUT_AP1, REF_INPUT_AP2, REF_INPUT_AP3, REF_INPUT_AP4};
            sInputAP[i] = scaleDelay(refInputAP[i]);
            inputAP[i].setSize(sInputAP[i]);
        }

        // Scale tank left half
        sModAP_L = scaleDelay(REF_TANK_MOD_AP_L);
        sDelay1_L = scaleDelay(REF_TANK_DELAY1_L);
        sAP2_L = scaleDelay(REF_TANK_AP2_L);
        sDelay2_L = scaleDelay(REF_TANK_DELAY2_L);
        modAP_L.setSize(sModAP_L);
        delay1_L.setSize(sDelay1_L);
        ap2_L.setSize(sAP2_L);
        delay2_L.setSize(sDelay2_L);

        // Scale tank right half
        sModAP_R = scaleDelay(REF_TANK_MOD_AP_R);
        sDelay1_R = scaleDelay(REF_TANK_DELAY1_R);
        sAP2_R = scaleDelay(REF_TANK_AP2_R);
        sDelay2_R = scaleDelay(REF_TANK_DELAY2_R);
        modAP_R.setSize(sModAP_R);
        delay1_R.setSize(sDelay1_R);
        ap2_R.setSize(sAP2_R);
        delay2_R.setSize(sDelay2_R);

        // Scale left output taps
        tapL_d1r_a = scaleDelay(TAP_L_FROM_D1R_A);
        tapL_d1r_b = scaleDelay(TAP_L_FROM_D1R_B);
        tapL_ap2r  = scaleDelay(TAP_L_FROM_AP2R);
        tapL_d2r   = scaleDelay(TAP_L_FROM_D2R);
        tapL_d1l   = scaleDelay(TAP_L_FROM_D1L);
        tapL_ap2l  = scaleDelay(TAP_L_FROM_AP2L);
        tapL_d2l   = scaleDelay(TAP_L_FROM_D2L);

        // Scale right output taps
        tapR_d1l_a = scaleDelay(TAP_R_FROM_D1L_A);
        tapR_d1l_b = scaleDelay(TAP_R_FROM_D1L_B);
        tapR_ap2l  = scaleDelay(TAP_R_FROM_AP2L);
        tapR_d2l   = scaleDelay(TAP_R_FROM_D2L);
        tapR_d1r   = scaleDelay(TAP_R_FROM_D1R);
        tapR_ap2r  = scaleDelay(TAP_R_FROM_AP2R);
        tapR_d2r   = scaleDelay(TAP_R_FROM_D2R);
    }

    void reset() {
        if (memoryBlock) {
            std::memset(memoryBlock, 0, TOTAL_BUFFER_SIZE * sizeof(float));
        }
        std::memset(dampState, 0, sizeof(dampState));
        std::memset(tankFeedback, 0, sizeof(tankFeedback));
        std::memset(lastTankOut, 0, sizeof(lastTankOut));
        lfoPhase = 0.0f;
        lfoRateSmoothed = lfoRate;
        modDepthScaleSmoothed = modDepthScale;

        for (int i = 0; i < 4; i++) inputAP[i].writePos = 0;
        modAP_L.writePos = 0;
        delay1_L.writePos = 0;
        ap2_L.writePos = 0;
        delay2_L.writePos = 0;
        modAP_R.writePos = 0;
        delay1_R.writePos = 0;
        ap2_R.writePos = 0;
        delay2_R.writePos = 0;
    }

    void process(float inputL, float inputR, float decay, float damping,
                 float& outL, float& outR) {
        if (!initialized) {
            outL = outR = 0.0f;
            return;
        }

        decay = rack::math::clamp(decay, 0.0f, 0.99f);
        damping = rack::math::clamp(damping, 0.0f, 0.99f);

        // Dattorro coefficients
        const float inputDiffCoeff1 = 0.75f;
        const float inputDiffCoeff2 = 0.625f;
        const float decayDiffCoeff1 = 0.7f;  // First tank APs (modulated)
        const float decayDiffCoeff2 = 0.5f;  // Second tank APs (not modulated)

        // Sum to mono input
        float input = (inputL + inputR) * 0.5f;

        // Input diffusion: 4 cascaded allpasses
        float diffused = input;
        diffused = inputAP[0].process(diffused, inputDiffCoeff1);
        diffused = inputAP[1].process(diffused, inputDiffCoeff1);
        diffused = inputAP[2].process(diffused, inputDiffCoeff2);
        diffused = inputAP[3].process(diffused, inputDiffCoeff2);

        // Smooth modulation parameters to prevent zippering
        lfoRateSmoothed += smoothCoeff * (lfoRate - lfoRateSmoothed);
        modDepthScaleSmoothed += smoothCoeff * (modDepthScale - modDepthScaleSmoothed);

        // LFO for tank modulation (using smoothed values)
        lfoPhase += lfoRateSmoothed * 2.0f * (float)M_PI / sampleRate;
        if (lfoPhase >= 2.0f * (float)M_PI) lfoPhase -= 2.0f * (float)M_PI;

        float modDepth = 8.0f * (sampleRate / DATTORRO_REF_RATE) * modDepthScaleSmoothed;
        float lfo1 = std::sin(lfoPhase) * modDepth;
        float lfo2 = std::sin(lfoPhase + 1.5707963f) * modDepth; // 90 degrees offset

        // ============================================================
        // LEFT HALF OF TANK
        // modAP_L -> delay1_L -> damp -> decay -> ap2_L -> delay2_L
        // ============================================================
        float leftIn = diffused + tankFeedback[1] * decay;

        // Step 1: Modulated allpass (decay diffusion 1)
        float leftAP1out = modAP_L.processModulated(leftIn, decayDiffCoeff1, lfo1);

        // Step 2: First delay line
        delay1_L.write(leftAP1out);
        float leftDelayed1 = delay1_L.read();

        // Step 3: One-pole lowpass damping
        dampState[0] = leftDelayed1 * (1.0f - damping) + dampState[0] * damping;

        // Step 4: Decay gain
        float leftDecayed = dampState[0] * decay;

        // Step 5: Second allpass (decay diffusion 2, NOT modulated)
        float leftAP2out = ap2_L.process(leftDecayed, decayDiffCoeff2);

        // Step 6: Second delay line
        delay2_L.write(leftAP2out);
        float leftOut = delay2_L.read();

        // ============================================================
        // RIGHT HALF OF TANK
        // modAP_R -> delay1_R -> damp -> decay -> ap2_R -> delay2_R
        // ============================================================
        float rightIn = diffused + tankFeedback[0] * decay;

        // Step 1: Modulated allpass (decay diffusion 1)
        float rightAP1out = modAP_R.processModulated(rightIn, decayDiffCoeff1, lfo2);

        // Step 2: First delay line
        delay1_R.write(rightAP1out);
        float rightDelayed1 = delay1_R.read();

        // Step 3: One-pole lowpass damping
        dampState[1] = rightDelayed1 * (1.0f - damping) + dampState[1] * damping;

        // Step 4: Decay gain
        float rightDecayed = dampState[1] * decay;

        // Step 5: Second allpass (decay diffusion 2, NOT modulated)
        float rightAP2out = ap2_R.process(rightDecayed, decayDiffCoeff2);

        // Step 6: Second delay line
        delay2_R.write(rightAP2out);
        float rightOut = delay2_R.read();

        // Cross-coupling feedback
        tankFeedback[0] = leftOut;   // Left output feeds to right input
        tankFeedback[1] = rightOut;  // Right output feeds to left input
        lastTankOut[0] = leftOut;
        lastTankOut[1] = rightOut;

        // ============================================================
        // OUTPUT TAPS - per Dattorro paper
        // Taps from 6 different elements for dense, smooth stereo output
        // ============================================================
        outL =   delay1_R.readTap(tapL_d1r_a)    // + from delay1 right
               + delay1_R.readTap(tapL_d1r_b)    // + from delay1 right
               - ap2_R.readTap(tapL_ap2r)        // - from AP2 right
               + delay2_R.readTap(tapL_d2r)      // + from delay2 right
               - delay1_L.readTap(tapL_d1l)      // - from delay1 left
               - ap2_L.readTap(tapL_ap2l)        // - from AP2 left
               - delay2_L.readTap(tapL_d2l);     // - from delay2 left

        outR =   delay1_L.readTap(tapR_d1l_a)    // + from delay1 left
               + delay1_L.readTap(tapR_d1l_b)    // + from delay1 left
               - ap2_L.readTap(tapR_ap2l)        // - from AP2 left
               + delay2_L.readTap(tapR_d2l)      // + from delay2 left
               - delay1_R.readTap(tapR_d1r)      // - from delay1 right
               - ap2_R.readTap(tapR_ap2r)        // - from AP2 right
               - delay2_R.readTap(tapR_d2r);     // - from delay2 right

        // Scale output (7 taps summed, target unity gain with input)
        outL *= 1.4f;
        outR *= 1.4f;

        // Denormal/NaN prevention (soft limiting handled by module output stage)
        if (!std::isfinite(outL) || std::fabs(outL) < 1e-20f) outL = 0.0f;
        if (!std::isfinite(outR) || std::fabs(outR) < 1e-20f) outR = 0.0f;
        if (!std::isfinite(dampState[0])) dampState[0] = 0.0f;
        if (!std::isfinite(dampState[1])) dampState[1] = 0.0f;
        if (!std::isfinite(tankFeedback[0])) tankFeedback[0] = 0.0f;
        if (!std::isfinite(tankFeedback[1])) tankFeedback[1] = 0.0f;
    }

    void setLFORate(float rate) {
        lfoRate = rack::math::clamp(rate, 0.1f, 10.0f);
    }
};

} // namespace reverie
} // namespace shapetaker
