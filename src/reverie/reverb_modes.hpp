#pragma once

#include <rack.hpp>
#include <cmath>
#include "../dsp/filters.hpp"
#include "../dsp/delays.hpp"
#include "../dsp/audio.hpp"
#include "dattorro_plate.hpp"
#include "pitch_shifter.hpp"
#include "reverse_grain_buffer.hpp"

namespace shapetaker {
namespace reverie {

// Mode identifiers
enum ReverbMode {
    MODE_FIELD_BLUR = 0,
    MODE_AFTERIMAGE = 1,
    MODE_REVERSE = 2,
    MODE_LOFI = 3,
    MODE_MODULATED = 4
};

// Per-voice reverb mode processor
// Wraps the DattorroPlate and adds mode-specific pre/post processing
class ReverbModeProcessor {
private:
    float sampleRate;

    // ---- Field Blur components ----
    shapetaker::dsp::ChorusEffect fieldBlurChorusL;
    shapetaker::dsp::ChorusEffect fieldBlurChorusR;
    GranularPitchShifter fieldBlurShimmer;
    float shimmerFeedbackL, shimmerFeedbackR;

    // ---- Afterimage components ----
    shapetaker::dsp::BiquadFilter afterimageResonantL;
    shapetaker::dsp::BiquadFilter afterimageResonantR;
    GranularPitchShifter afterimageShifterL;
    GranularPitchShifter afterimageShifterR;

    // ---- Reverse components ----
    ReverseGrainBuffer reverseBufferL;
    ReverseGrainBuffer reverseBufferR;

    // ---- Lo-Fi components ----
    shapetaker::dsp::OnePoleLowpass lofiFilterL;
    shapetaker::dsp::OnePoleLowpass lofiFilterR;
    float lofiHoldL, lofiHoldR;
    int lofiCounter;
    float lofiLfoPhase;

    // ---- Modulated components ----
    shapetaker::dsp::ChorusEffect modulatedChorusL;
    shapetaker::dsp::ChorusEffect modulatedChorusR;

public:
    ReverbModeProcessor() {
        sampleRate = 44100.0f;
        shimmerFeedbackL = shimmerFeedbackR = 0.0f;
        lofiHoldL = lofiHoldR = 0.0f;
        lofiCounter = 0;
        lofiLfoPhase = 0.0f;
    }

    void setSampleRate(float sr) {
        sampleRate = sr;
        fieldBlurChorusL.setSampleRate(sr);
        fieldBlurChorusR.setSampleRate(sr);
        fieldBlurShimmer.setSampleRate(sr);
        fieldBlurShimmer.setPitchRatio(2.0f); // +1 octave

        afterimageResonantL.reset();
        afterimageResonantR.reset();
        afterimageShifterL.setSampleRate(sr);
        afterimageShifterR.setSampleRate(sr);
        afterimageShifterL.setPitchRatio(0.5f); // -1 octave (dark)
        afterimageShifterR.setPitchRatio(0.5f);

        reverseBufferL.setSampleRate(sr);
        reverseBufferR.setSampleRate(sr);

        lofiFilterL.reset();
        lofiFilterR.reset();

        modulatedChorusL.setSampleRate(sr);
        modulatedChorusR.setSampleRate(sr);
    }

    void reset() {
        fieldBlurChorusL.reset();
        fieldBlurChorusR.reset();
        fieldBlurShimmer.reset();
        shimmerFeedbackL = shimmerFeedbackR = 0.0f;
        afterimageResonantL.reset();
        afterimageResonantR.reset();
        afterimageShifterL.reset();
        afterimageShifterR.reset();
        reverseBufferL.reset();
        reverseBufferR.reset();
        lofiFilterL.reset();
        lofiFilterR.reset();
        lofiHoldL = lofiHoldR = 0.0f;
        lofiCounter = 0;
        lofiLfoPhase = 0.0f;
        modulatedChorusL.reset();
        modulatedChorusR.reset();
    }

    // Main processing function
    // Processes input through the plate reverb with mode-specific modifications
    void process(DattorroPlate& plate, float inL, float inR,
                 float decay, float damping,
                 float param1, float param2,
                 int mode,
                 float& outL, float& outR) {

        switch (mode) {
            case MODE_FIELD_BLUR:
                processFieldBlur(plate, inL, inR, decay, damping, param1, param2, outL, outR);
                break;
            case MODE_AFTERIMAGE:
                processAfterimage(plate, inL, inR, decay, damping, param1, param2, outL, outR);
                break;
            case MODE_REVERSE:
                processReverse(plate, inL, inR, decay, damping, param1, param2, outL, outR);
                break;
            case MODE_LOFI:
                processLoFi(plate, inL, inR, decay, damping, param1, param2, outL, outR);
                break;
            case MODE_MODULATED:
                processModulated(plate, inL, inR, decay, damping, param1, param2, outL, outR);
                break;
            default:
                // Fallback: clean plate
                plate.modDepthScale = 1.0f;
                plate.process(inL, inR, decay, damping, outL, outR);
                break;
        }
    }

private:
    // ========================================================================
    // FIELD BLUR - Shoegaze / Soft Focus
    // Regenerative shimmer: pitch-shifted tank output feeds BACK into plate
    // Each repeat cascades upward in pitch for ethereal shoegaze wash
    // P1 = Chorus Depth (ensemble thickness), P2 = Shimmer (regenerative)
    // ========================================================================
    void processFieldBlur(DattorroPlate& plate, float inL, float inR,
                          float decay, float damping,
                          float chorusDepthParam, float shimmerLevel,
                          float& outL, float& outR) {
        // Increased tank modulation for lush character
        plate.modDepthScale = 1.0f + chorusDepthParam * 1.0f + shimmerLevel * 0.5f;

        // --- Regenerative shimmer: pitch-shift the TANK OUTPUT and feed back in ---
        // At P2=0: no shimmer feedback, just clean plate
        // At P2=1: shimmer feedback creates ascending harmonics in tail
        float shimmerGain = shimmerLevel * 0.35f; // max 35% feedback (conservative)

        // Soft-limit the feedback to prevent runaway
        float fbL = std::tanh(shimmerFeedbackL);
        float fbR = std::tanh(shimmerFeedbackR);
        float shimmerMono = (fbL + fbR) * 0.5f;
        float shimmerSignal = fieldBlurShimmer.process(shimmerMono) * shimmerGain;

        // Clamp shimmer contribution to safe range
        shimmerSignal = rack::math::clamp(shimmerSignal, -1.0f, 1.0f);

        // Mix shimmer feedback into plate input
        float plateInL = inL + shimmerSignal;
        float plateInR = inR + shimmerSignal;

        float plateL, plateR;
        plate.process(plateInL, plateInR, decay, damping, plateL, plateR);

        // Store normalized tank output for next sample's shimmer feedback
        // Use lastTankOut (pre-scaling) to avoid compounding the 1.4x output gain
        shimmerFeedbackL = plate.lastTankOut[0];
        shimmerFeedbackR = plate.lastTankOut[1];

        // --- Stereo chorus post-process (P1 controls depth/thickness) ---
        // At P1=0: clean stereo plate output
        // At P1=1: deep stereo ensemble effect
        if (chorusDepthParam < 0.01f) {
            outL = plateL;
            outR = plateR;
            return;
        }

        // Asymmetric L/R rates for wide stereo image
        float rate = 0.4f + chorusDepthParam * 2.6f;       // 0.4 - 3.0 Hz
        float depth = 0.2f + chorusDepthParam * 0.7f;       // 0.2 - 0.9
        float chorusMix = chorusDepthParam * 0.65f;          // 0 to 65% wet
        float rateSpread = chorusDepthParam * 0.8f;          // L/R rate offset

        outL = fieldBlurChorusL.process(plateL, rate - rateSpread * 0.5f,
                                        depth, chorusMix, sampleRate);
        outR = fieldBlurChorusR.process(plateR, rate + rateSpread * 0.5f,
                                        depth, chorusMix, sampleRate);
    }

    // ========================================================================
    // AFTERIMAGE - Ghost/Spectral
    // Plate with resonant filter + pitch shift in feedback, deep modulation
    // P1 = Mod Rate, P2 = Diffusion (resonant filter Q)
    // ========================================================================
    void processAfterimage(DattorroPlate& plate, float inL, float inR,
                           float decay, float damping,
                           float modRate, float diffusion,
                           float& outL, float& outR) {
        // At P1=0, P2=0: clean plate with standard mod depth
        // P1 gradually increases modulation (ghostly movement)
        // P2 adds spectral processing (resonant filter + pitch shift)
        float lfoRate = 0.8f + modRate * 4.2f;
        plate.setLFORate(lfoRate);
        plate.modDepthScale = 1.0f + modRate * 4.0f;

        float plateL, plateR;
        plate.process(inL, inR, decay, damping, plateL, plateR);

        // At P2=0: output is 100% clean plate (no spectral processing)
        if (diffusion < 0.01f) {
            outL = plateL;
            outR = plateR;
            // Still feed the filters to keep state warm
            afterimageShifterL.process(0.0f);
            afterimageShifterR.process(0.0f);
            return;
        }

        // Resonant filter: sweeps from warm to vocal/nasal
        float centerFreq = 400.0f + diffusion * 2100.0f; // 400-2500 Hz
        float Q = 0.7f + diffusion * 6.0f; // Q 0.7-6.7
        afterimageResonantL.setParameters(shapetaker::dsp::BiquadFilter::BANDPASS,
                                          centerFreq, Q, sampleRate);
        afterimageResonantR.setParameters(shapetaker::dsp::BiquadFilter::BANDPASS,
                                          centerFreq, Q, sampleRate);

        float resonantL = afterimageResonantL.process(plateL);
        float resonantR = afterimageResonantR.process(plateR);

        // Moderate gain boost for resonant signal
        float resonantGain = 1.0f + diffusion * 3.0f;
        resonantL *= resonantGain;
        resonantR *= resonantGain;

        // Pitch shift the resonant signal (octave down for ghostly quality)
        float shiftedL = afterimageShifterL.process(resonantL);
        float shiftedR = afterimageShifterR.process(resonantR);

        // Crossfade from clean plate to spectral ghost
        // At P2=0: 100% plate
        // At P2=1: 35% plate + 40% resonant + 25% shifted
        float plateMix = 1.0f - diffusion * 0.65f;
        float resonantMix = diffusion * 0.40f;
        float shiftedMix = diffusion * 0.25f;
        outL = plateL * plateMix + resonantL * resonantMix + shiftedL * shiftedMix;
        outR = plateR * plateMix + resonantR * resonantMix + shiftedR * shiftedMix;

        // Gentle soft limit (less aggressive than before)
        outL = std::tanh(outL * 0.9f) * 1.11f;
        outR = std::tanh(outR * 0.9f) * 1.11f;
    }

    // ========================================================================
    // REVERSE
    // Input -> ReverseGrainBuffer -> Plate
    // P1 = Window Size, P2 = Feedback
    // ========================================================================
    void processReverse(DattorroPlate& plate, float inL, float inR,
                        float decay, float damping,
                        float windowSize, float feedback,
                        float& outL, float& outR) {
        plate.modDepthScale = 1.0f;

        // Set grain buffer window size
        reverseBufferL.setWindowSize(windowSize, sampleRate);
        reverseBufferR.setWindowSize(windowSize, sampleRate);

        // Feed input (+ feedback from plate output) into reverse buffer
        // Higher max feedback for self-oscillating reverse textures
        float fbL = plate.lastTankOut[0] * feedback * 0.85f;
        float fbR = plate.lastTankOut[1] * feedback * 0.85f;

        float reversedL = reverseBufferL.process(inL + fbL);
        float reversedR = reverseBufferR.process(inR + fbR);

        // Feed reversed signal into plate reverb
        plate.process(reversedL, reversedR, decay, damping, outL, outR);

        // Mix some direct reversed signal for immediacy
        outL = outL * 0.7f + reversedL * 0.3f;
        outR = outR * 0.7f + reversedR * 0.3f;
    }

    // ========================================================================
    // LO-FI
    // Plate -> sample rate reduction -> saturation -> LP filter -> wow/flutter
    // P1 = Degradation, P2 = Wow/Flutter
    // ========================================================================
    void processLoFi(DattorroPlate& plate, float inL, float inR,
                     float decay, float damping,
                     float degradation, float wowFlutter,
                     float& outL, float& outR) {
        // At P1=0, P2=0: clean plate
        float lofidamping = damping + (1.0f - damping) * degradation * 0.5f;
        plate.modDepthScale = 1.0f;

        float plateL, plateR;
        plate.process(inL, inR, decay, lofidamping, plateL, plateR);

        // At P1=0, P2=0: bypass all processing
        if (degradation < 0.01f && wowFlutter < 0.01f) {
            outL = plateL;
            outR = plateR;
            lofiHoldL = plateL;
            lofiHoldR = plateR;
            return;
        }

        // Sample rate reduction (sample-and-hold)
        int holdSamples = 1 + (int)(degradation * degradation * 31.0f);
        lofiCounter++;
        if (lofiCounter >= holdSamples) {
            lofiCounter = 0;
            lofiHoldL = plateL;
            lofiHoldR = plateR;
        }
        float crushedL = lofiHoldL;
        float crushedR = lofiHoldR;

        // Soft saturation
        float satDrive = 1.0f + degradation * 3.0f;
        crushedL = std::tanh(crushedL * satDrive) / std::tanh(satDrive);
        crushedR = std::tanh(crushedR * satDrive) / std::tanh(satDrive);

        // LP filter only when degradation > 0 (at P1=0, cutoff is 18kHz = transparent)
        float baseCutoff = 18000.0f - degradation * 16500.0f; // 18kHz down to 1.5kHz
        float lpCutoff = baseCutoff;

        // Advance wow/flutter LFO (always running for smooth onset)
        float wowRate = 0.5f + wowFlutter * 3.5f;
        lofiLfoPhase += wowRate * 2.0f * (float)M_PI / sampleRate;
        if (lofiLfoPhase >= 2.0f * (float)M_PI) {
            lofiLfoPhase -= 2.0f * (float)M_PI;
        }

        float totalMod = 0.0f;
        if (wowFlutter > 0.01f) {
            float wow = std::sin(lofiLfoPhase);
            float flutter = std::sin(lofiLfoPhase * 3.17f + 0.7f)
                          + std::sin(lofiLfoPhase * 5.43f + 2.1f) * 0.5f;
            float wowMod = wow * wowFlutter;
            float flutterMod = flutter * wowFlutter * 0.3f;
            totalMod = wowMod + flutterMod;

            float cutoffMod = baseCutoff * totalMod * 0.3f;
            lpCutoff = rack::math::clamp(baseCutoff + cutoffMod, 400.0f, 18000.0f);
        }

        outL = lofiFilterL.process(crushedL, lpCutoff, sampleRate);
        outR = lofiFilterR.process(crushedR, lpCutoff, sampleRate);

        if (wowFlutter > 0.01f) {
            float ampMod = 1.0f + totalMod * 0.15f;
            outL *= ampMod;
            outR *= ampMod;
            plate.modDepthScale = 1.0f + std::fabs(totalMod) * 2.0f;
        }
    }

    // ========================================================================
    // MODULATED
    // Plate with deep tank modulation + chorus post-process
    // P1 = Mod Depth, P2 = Detune
    // ========================================================================
    void processModulated(DattorroPlate& plate, float inL, float inR,
                          float decay, float damping,
                          float modDepth, float detune,
                          float& outL, float& outR) {
        // At P1=0, P2=0: clean plate with standard mod depth
        plate.modDepthScale = 1.0f + modDepth * 7.0f;
        plate.setLFORate(0.8f + modDepth * 0.4f + detune * 0.5f);

        float plateL, plateR;
        plate.process(inL, inR, decay, damping, plateL, plateR);

        // At P1=0, P2=0: bypass chorus entirely (clean plate)
        float chorusAmount = modDepth * 0.4f + detune * 0.5f; // 0 to ~0.9
        if (chorusAmount < 0.01f) {
            outL = plateL;
            outR = plateR;
            return;
        }

        float chorusRate = 0.5f + modDepth * 1.5f;
        float chorusDepth = 0.2f + modDepth * 0.5f + detune * 0.4f;

        // Asymmetric L/R chorus rates for real detuning
        float rateSpread = detune * 1.5f;
        float chorusMix = chorusAmount; // Starts at 0, scales with knob settings

        outL = modulatedChorusL.process(plateL, chorusRate - rateSpread * 0.5f,
                                        chorusDepth, chorusMix, sampleRate);
        outR = modulatedChorusR.process(plateR, chorusRate + rateSpread * 0.5f,
                                        chorusDepth, chorusMix, sampleRate);
    }
};

} // namespace reverie
} // namespace shapetaker
