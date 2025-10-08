#include "plugin.hpp"
#include "random.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//
// Fatebinder (reimagined)
// A causal-modulation LFO featuring:
//  - Interleaved LFO Threads (A/B/C) ratioed to a master timebase
//  - Echo Matrix (per-thread circular delay with feedback)
//  - Quantized Drift (stepped bias with slewed approach)
//  - Event-Triggered actions (Bind/Recall/Invert)
//  - Binding Bus (internal cross-mod from Echo -> phase)
//

struct Fatebinder : Module {
    enum ParamId {
        // Master & Interleave
        MASTER_RATE_PARAM,        // coarse master rate (octave-style)
        MASTER_FINE_PARAM,        // fine trim
        RATIO_A_PARAM,            // ratio for A
        RATIO_B_PARAM,            // ratio for B
        RATIO_C_PARAM,            // ratio for C
        PHASE_SPREAD_PARAM,       // spread subordinate phases around master

        // Echo
        ECHO_TIME_PARAM,          // 50 ms .. 5 s
        ECHO_FEEDBACK_PARAM,      // 0..0.95
        ECHO_SEND_PARAM,          // how much each thread writes to echo bus

        // Quantized Drift
        DRIFT_STEP_PARAM,         // step size (V)
        DRIFT_RATE_PARAM,         // steps per second (0..2 Hz)
        DRIFT_SLEW_PARAM,         // slew time (s) for approach between steps

        // Output shape
        SHAPE_MORPH_PARAM,        // 0: sine -> triangle -> square (waveshaper)
        DEPTH_PARAM,              // global modulation depth / amplitude

        // Events (behavior toggles)
        INVERT_LATCH_PARAM,       // front-panel toggle (also via Event 3)

        PARAMS_LEN
    };

    enum InputId {
        MASTER_RATE_INPUT,
        MASTER_RESET_INPUT,       // hard sync master phase to 0
        EVENT1_INPUT,             // Bind: sync A/B/C to master phase + apply spread
        EVENT2_INPUT,             // Recall: momentarily replace live output with echo
        EVENT3_INPUT,             // Invert: toggle output polarity

        // Optional CVs
        RATIO_A_CV_INPUT,
        RATIO_B_CV_INPUT,
        RATIO_C_CV_INPUT,
        ECHO_TIME_CV_INPUT,
        ECHO_FEEDBACK_CV_INPUT,
        DRIFT_STEP_CV_INPUT,
        DRIFT_RATE_CV_INPUT,
        SHAPE_MORPH_CV_INPUT,
        DEPTH_CV_INPUT,

        INPUTS_LEN
    };

    enum OutputId {
        THREAD_A_OUTPUT,
        THREAD_B_OUTPUT,
        THREAD_C_OUTPUT,
        ECHO_OUTPUT,              // raw echo bus output
        DRIFT_OUTPUT,             // the quantized drift bias alone
        OUTPUTS_LEN
    };

    enum LightId {
        MASTER_LIGHT,
        A_LIGHT,
        B_LIGHT,
        C_LIGHT,
        ECHO_HOLD_LIGHT,
        INVERT_LIGHT,
        LIGHTS_LEN
    };

    // ----------------------------
    // Internal state
    // ----------------------------
    struct Thread {
        float phase = 0.f;     // 0..1
        float last = 0.f;      // last output (for lights)
    };

    Thread A, B, C;

    // Master phase/timebase
    float masterPhase = 0.f;

    // Echo matrix: a single shared circular buffer (mono) which threads can write to
    // and read from with settable delay time & feedback.
    std::vector<float> echoBuffer;
    size_t echoHead = 0;          // write head
    size_t echoDelaySamples = 2400; // default ~50 ms @ 48k

    // Quantized drift: stepped target with slew towards it
    float driftCurrent = 0.f;
    float driftTarget = 0.f;
    float driftSlewCoeff = 0.999f; // computed per sample from DRIFT_SLEW_PARAM
    float driftStepTimer = 0.f;     // countdown to next target step

    // Event triggers
    dsp::SchmittTrigger resetTrig;
    dsp::SchmittTrigger bindTrig;     // EVENT1
    dsp::SchmittTrigger recallTrig;   // EVENT2
    dsp::SchmittTrigger invertTrig;   // EVENT3

    // Latches
    bool echoHold = false; // when true, outputs prefer echo tap over live wave

    Fatebinder() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        using shapetaker::ParameterHelper;
        // Master rate: coarse as exponent, fine as +/- 0.1 oct
        ParameterHelper::configFrequency(this, MASTER_RATE_PARAM, "Master Rate", -6.f, 8.f, 0.f);
        configParam(MASTER_FINE_PARAM, -0.1f, 0.1f, 0.f, "Master Fine (oct)");

        // Ratios (displayed as multipliers)
        configParam(RATIO_A_PARAM, -4.f, 4.f, 0.f, "A Ratio (oct)");
        configParam(RATIO_B_PARAM, -4.f, 4.f, 1.f, "B Ratio (oct)");
        configParam(RATIO_C_PARAM, -4.f, 4.f, -1.f, "C Ratio (oct)");
        configParam(PHASE_SPREAD_PARAM, 0.f, 1.f, 0.33f, "Phase Spread");

        // Echo
        configParam(ECHO_TIME_PARAM, 0.05f, 5.f, 0.2f, "Echo Time (s)");
        configParam(ECHO_FEEDBACK_PARAM, 0.f, 0.95f, 0.35f, "Echo Feedback");
        configParam(ECHO_SEND_PARAM, 0.f, 1.f, 0.7f, "Echo Send");

        // Drift
        configParam(DRIFT_STEP_PARAM, 0.01f, 1.0f, 0.1f, "Drift Step (V)");
        configParam(DRIFT_RATE_PARAM, 0.f, 2.0f, 0.2f, "Drift Rate (Hz)");
        configParam(DRIFT_SLEW_PARAM, 0.001f, 2.0f, 0.1f, "Drift Slew (s)");

        // Waveshape & depth
        configParam(SHAPE_MORPH_PARAM, 0.f, 2.f, 0.f, "Waveshape Morph");
        configParam(DEPTH_PARAM, 0.f, 10.f, 5.f, "Depth (Vpp/2)");

        // Toggles
        configParam(INVERT_LATCH_PARAM, 0.f, 1.f, 0.f, "Invert Latch");

        // CV inputs
        ParameterHelper::configCVInput(this, MASTER_RATE_INPUT, "Master Rate CV");
        ParameterHelper::configCVInput(this, MASTER_RESET_INPUT, "Master Reset");
        ParameterHelper::configCVInput(this, EVENT1_INPUT, "Event 1: Bind");
        ParameterHelper::configCVInput(this, EVENT2_INPUT, "Event 2: Recall");
        ParameterHelper::configCVInput(this, EVENT3_INPUT, "Event 3: Invert");
        ParameterHelper::configCVInput(this, RATIO_A_CV_INPUT, "A Ratio CV");
        ParameterHelper::configCVInput(this, RATIO_B_CV_INPUT, "B Ratio CV");
        ParameterHelper::configCVInput(this, RATIO_C_CV_INPUT, "C Ratio CV");
        ParameterHelper::configCVInput(this, ECHO_TIME_CV_INPUT, "Echo Time CV");
        ParameterHelper::configCVInput(this, ECHO_FEEDBACK_CV_INPUT, "Echo Feedback CV");
        ParameterHelper::configCVInput(this, DRIFT_STEP_CV_INPUT, "Drift Step CV");
        ParameterHelper::configCVInput(this, DRIFT_RATE_CV_INPUT, "Drift Rate CV");
        ParameterHelper::configCVInput(this, SHAPE_MORPH_CV_INPUT, "Waveshape Morph CV");
        ParameterHelper::configCVInput(this, DEPTH_CV_INPUT, "Depth CV");

        // Outputs
        shapetaker::ParameterHelper::configAudioOutput(this, THREAD_A_OUTPUT, "Thread A");
        shapetaker::ParameterHelper::configAudioOutput(this, THREAD_B_OUTPUT, "Thread B");
        shapetaker::ParameterHelper::configAudioOutput(this, THREAD_C_OUTPUT, "Thread C");
        shapetaker::ParameterHelper::configAudioOutput(this, ECHO_OUTPUT,   "Echo");
        shapetaker::ParameterHelper::configAudioOutput(this, DRIFT_OUTPUT,  "Drift");

        // Lights
        configLight(MASTER_LIGHT, "Master");
        configLight(A_LIGHT, "A Activity");
        configLight(B_LIGHT, "B Activity");
        configLight(C_LIGHT, "C Activity");
        configLight(ECHO_HOLD_LIGHT, "Echo Hold");
        configLight(INVERT_LIGHT, "Invert");

        // Allocate echo buffer for up to 5 s @ max sample rate assumption (~96k safety)
        const size_t maxSamples = 48000 * 6; // 6 s margin at 48k
        echoBuffer.resize(maxSamples, 0.f);
    }

    // Utility: simple waveshape morph 0..2 : sine -> triangle -> square
    inline float morphWave(float phase01, float morph) {
        // phase01 in [0,1)
        float x = phase01;
        // base sine
        float s = std::sin(2.f * M_PI * x);
        // triangle
        float tri = 4.f * fabsf(x - floorf(x + 0.5f)) - 1.f; // -1..1
        // square
        float sq = s >= 0.f ? 1.f : -1.f;

        if (morph < 1.f) {
            return rack::simd::clamp(s * (1.f - morph) + tri * morph, -1.f, 1.f);
        } else {
            float t = morph - 1.f; // 0..1
            float mix = tri * (1.f - t) + sq * t;
            return rack::simd::clamp(mix, -1.f, 1.f);
        }
    }

    inline float applyDepthAndDrift(float v, float depth, float drift, bool invert) {
        float out = v * depth + drift;
        return invert ? -out : out;
    }

    void hardSyncThreads(float spread) {
        // Set A/B/C phases to master with spread around the circle
        A.phase = masterPhase;
        B.phase = masterPhase + spread;
        C.phase = masterPhase + 2.f * spread;
        A.phase -= floorf(A.phase);
        B.phase -= floorf(B.phase);
        C.phase -= floorf(C.phase);
    }

    void updateEchoConfig(float sampleRate, float timeSeconds) {
        timeSeconds = clamp(timeSeconds, 0.05f, 5.f);
        size_t want = (size_t)std::round(sampleRate * timeSeconds);
        want = std::max((size_t)1, std::min(want, echoBuffer.size() - 1));
        echoDelaySamples = want;
    }

    void stepDrift(float dt, float stepSize, float rateHz, float slewSeconds) {
        // Update target at rateHz
        if (rateHz > 0.f) {
            driftStepTimer -= dt;
            if (driftStepTimer <= 0.f) {
                // pick -1, 0, or +1 step (biased to 0 slightly)
                float r = rack::random::uniform();
                int dir = (r < 0.2f) ? -1 : (r > 0.8f ? +1 : 0);
                driftTarget += dir * stepSize;
                // keep within a musically sane window
                driftTarget = clamp(driftTarget, -5.f, 5.f);
                driftStepTimer += 1.f / rateHz;
            }
        }
        // compute per-sample slew coeff from time constant
        float tau = std::max(0.001f, slewSeconds);
        float a = std::exp(-dt / tau);
        driftCurrent = a * driftCurrent + (1.f - a) * driftTarget;
    }

    float readEchoTap(size_t delay) const {
        size_t idx = (echoHead + echoBuffer.size() - delay) % echoBuffer.size();
        return echoBuffer[idx];
    }

    void writeEcho(float send, float feedback, float sample) {
        // read current delayed sample
        size_t idxTap = (echoHead + echoBuffer.size() - echoDelaySamples) % echoBuffer.size();
        float delayed = echoBuffer[idxTap];
        // write new sample into head with feedback
        float writeVal = delayed * feedback + sample * send;
        echoBuffer[echoHead] = writeVal;
        // advance head
        echoHead = (echoHead + 1) % echoBuffer.size();
    }

    void process(const ProcessArgs& args) override {
        float dt = args.sampleTime;
        float sr = args.sampleRate;

        // --- Parameters & CV ---
        auto getExp = [&](int p, int cvIn){
            float v = params[p].getValue();
            if (inputs[cvIn].isConnected()) v += inputs[cvIn].getVoltage() * 0.1f; // 1V/oct like influence
            return std::pow(2.f, v);
        };

        float masterRateOct = params[MASTER_RATE_PARAM].getValue() + params[MASTER_FINE_PARAM].getValue();
        if (inputs[MASTER_RATE_INPUT].isConnected()) masterRateOct += inputs[MASTER_RATE_INPUT].getVoltage() * 0.1f;
        float masterHz = std::pow(2.f, masterRateOct); // exponential mapping
        masterHz = clamp(masterHz, 0.0001f, 50.f);

        float ratioA = std::pow(2.f, params[RATIO_A_PARAM].getValue() + (inputs[RATIO_A_CV_INPUT].isConnected() ? inputs[RATIO_A_CV_INPUT].getVoltage() * 0.1f : 0.f));
        float ratioB = std::pow(2.f, params[RATIO_B_PARAM].getValue() + (inputs[RATIO_B_CV_INPUT].isConnected() ? inputs[RATIO_B_CV_INPUT].getVoltage() * 0.1f : 0.f));
        float ratioC = std::pow(2.f, params[RATIO_C_PARAM].getValue() + (inputs[RATIO_C_CV_INPUT].isConnected() ? inputs[RATIO_C_CV_INPUT].getVoltage() * 0.1f : 0.f));
        ratioA = clamp(ratioA, 0.0001f, 64.f);
        ratioB = clamp(ratioB, 0.0001f, 64.f);
        ratioC = clamp(ratioC, 0.0001f, 64.f);

        float phaseSpread = params[PHASE_SPREAD_PARAM].getValue();

        float echoTime = params[ECHO_TIME_PARAM].getValue() + (inputs[ECHO_TIME_CV_INPUT].isConnected() ? inputs[ECHO_TIME_CV_INPUT].getVoltage() * 0.2f : 0.f);
        float echoFeedback = clamp(params[ECHO_FEEDBACK_PARAM].getValue() + (inputs[ECHO_FEEDBACK_CV_INPUT].isConnected() ? inputs[ECHO_FEEDBACK_CV_INPUT].getVoltage() * 0.05f : 0.f), 0.f, 0.95f);
        float echoSend = clamp(params[ECHO_SEND_PARAM].getValue(), 0.f, 1.f);
        updateEchoConfig(sr, echoTime);

        float driftStep = params[DRIFT_STEP_PARAM].getValue() + (inputs[DRIFT_STEP_CV_INPUT].isConnected() ? inputs[DRIFT_STEP_CV_INPUT].getVoltage() * 0.1f : 0.f);
        driftStep = clamp(driftStep, 0.005f, 2.f);
        float driftRate = params[DRIFT_RATE_PARAM].getValue() + (inputs[DRIFT_RATE_CV_INPUT].isConnected() ? inputs[DRIFT_RATE_CV_INPUT].getVoltage() * 0.1f : 0.f);
        driftRate = clamp(driftRate, 0.f, 4.f);
        float driftSlew = params[DRIFT_SLEW_PARAM].getValue();

        float morph = params[SHAPE_MORPH_PARAM].getValue() + (inputs[SHAPE_MORPH_CV_INPUT].isConnected() ? inputs[SHAPE_MORPH_CV_INPUT].getVoltage() * 0.2f : 0.f);
        morph = clamp(morph, 0.f, 2.f);
        float depth = params[DEPTH_PARAM].getValue() + (inputs[DEPTH_CV_INPUT].isConnected() ? inputs[DEPTH_CV_INPUT].getVoltage() : 0.f);
        depth = clamp(depth, 0.f, 10.f);

        bool invertLatch = params[INVERT_LATCH_PARAM].getValue() >= 0.5f;

        // --- Triggers ---
        if (resetTrig.process(inputs[MASTER_RESET_INPUT].getVoltage())) {
            masterPhase = 0.f;
        }
        if (bindTrig.process(inputs[EVENT1_INPUT].getVoltage())) {
            hardSyncThreads(phaseSpread);
        }
        if (recallTrig.process(inputs[EVENT2_INPUT].getVoltage())) {
            echoHold = !echoHold; // toggle hold
        }
        if (invertTrig.process(inputs[EVENT3_INPUT].getVoltage())) {
            invertLatch = !invertLatch;
            params[INVERT_LATCH_PARAM].setValue(invertLatch ? 1.f : 0.f);
        }

        // --- Advance master & threads ---
        masterPhase += dt * masterHz;
        masterPhase -= floorf(masterPhase);

        A.phase += dt * masterHz * ratioA;
        B.phase += dt * masterHz * ratioB;
        C.phase += dt * masterHz * ratioC;
        A.phase -= floorf(A.phase);
        B.phase -= floorf(B.phase);
        C.phase -= floorf(C.phase);

        // Quantized drift engine
        stepDrift(dt, driftStep, driftRate, driftSlew);

        // Binding bus: use echo tap to FM phases slightly (causal attractor feel)
        float echoTap = readEchoTap(echoDelaySamples);
        float fmAmt = 0.0015f; // gentle
        A.phase += fmAmt * echoTap; A.phase -= floorf(A.phase);
        B.phase += fmAmt * echoTap * 0.7f; B.phase -= floorf(B.phase);
        C.phase += fmAmt * echoTap * 1.2f; C.phase -= floorf(C.phase);

        // Waveshapes (morphing) per thread
        float aRaw = morphWave(A.phase, morph);
        float bRaw = morphWave(B.phase, morph * 0.85f);
        float cRaw = morphWave(C.phase, morph * 1.15f > 2.f ? 2.f : morph * 1.15f);

        // Mix to echo send bus (pre- or post-drift; choose pre for clearer echoes)
        float echoSendSample = (aRaw + bRaw + cRaw) / 3.f;
        writeEcho(echoSend, echoFeedback, echoSendSample);

        // If echoHold, prefer echo output as the thread signal body
        float bodyA = echoHold ? echoTap : aRaw;
        float bodyB = echoHold ? echoTap : bRaw;
        float bodyC = echoHold ? echoTap : cRaw;

        // Apply depth & drift & optional inversion
        bool invert = invertLatch;
        float aOut = applyDepthAndDrift(bodyA, depth, driftCurrent, invert);
        float bOut = applyDepthAndDrift(bodyB, depth, driftCurrent, invert);
        float cOut = applyDepthAndDrift(bodyC, depth, driftCurrent, invert);

        // Outputs with protection
        outputs[THREAD_A_OUTPUT].setVoltage(clamp(aOut, -10.f, 10.f));
        outputs[THREAD_B_OUTPUT].setVoltage(clamp(bOut, -10.f, 10.f));
        outputs[THREAD_C_OUTPUT].setVoltage(clamp(cOut, -10.f, 10.f));
        outputs[ECHO_OUTPUT].setVoltage(clamp(echoTap * depth, -10.f, 10.f));
        outputs[DRIFT_OUTPUT].setVoltage(clamp(driftCurrent, -10.f, 10.f));

        // Lights
        lights[MASTER_LIGHT].setBrightness(0.1f + 0.9f * (0.5f + 0.5f * std::sin(2.f * M_PI * masterPhase)));
        lights[A_LIGHT].setBrightness(fabsf(aRaw));
        lights[B_LIGHT].setBrightness(fabsf(bRaw));
        lights[C_LIGHT].setBrightness(fabsf(cRaw));
        lights[ECHO_HOLD_LIGHT].setBrightness(echoHold ? 1.f : 0.f);
        lights[INVERT_LIGHT].setBrightness(invert ? 1.f : 0.f);
    }
};

struct FatebinderWidget : ModuleWidget {
    FatebinderWidget(Fatebinder* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Fatebinder.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        using shapetaker::ui::LayoutHelper;
        auto mm = [](float x, float y) { return LayoutHelper::mm2px(Vec(x, y)); };

        // Top: Master & Events
        addParam(createParamCentered<ShapetakerKnobOscilloscopeLarge>(mm(18.0f, 18.0f), module, Fatebinder::MASTER_RATE_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm(32.0f, 18.0f), module, Fatebinder::MASTER_FINE_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(46.0f, 18.0f), module, Fatebinder::MASTER_RATE_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(60.0f, 18.0f), module, Fatebinder::MASTER_RESET_INPUT));

        addInput(createInputCentered<ShapetakerBNCPort>(mm(76.0f, 18.0f), module, Fatebinder::EVENT1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(90.0f, 18.0f), module, Fatebinder::EVENT2_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(104.0f, 18.0f), module, Fatebinder::EVENT3_INPUT));
        addParam(createParamCentered<ShapetakerVintageToggleSwitch>(mm(118.0f, 18.0f), module, Fatebinder::INVERT_LATCH_PARAM));

        // Ratios & phase spread row
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(22.0f, 44.0f), module, Fatebinder::RATIO_A_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(48.0f, 44.0f), module, Fatebinder::RATIO_B_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(74.0f, 44.0f), module, Fatebinder::RATIO_C_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm(100.0f, 44.0f), module, Fatebinder::PHASE_SPREAD_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(22.0f, 32.0f), module, Fatebinder::RATIO_A_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(48.0f, 32.0f), module, Fatebinder::RATIO_B_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(74.0f, 32.0f), module, Fatebinder::RATIO_C_CV_INPUT));

        // Echo section
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(22.0f, 70.0f), module, Fatebinder::ECHO_TIME_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm(38.0f, 70.0f), module, Fatebinder::ECHO_FEEDBACK_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm(54.0f, 70.0f), module, Fatebinder::ECHO_SEND_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(22.0f, 58.0f), module, Fatebinder::ECHO_TIME_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(38.0f, 58.0f), module, Fatebinder::ECHO_FEEDBACK_CV_INPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(70.0f, 70.0f), module, Fatebinder::ECHO_OUTPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(mm(84.0f, 70.0f), module, Fatebinder::ECHO_HOLD_LIGHT));

        // Drift & Shape
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(22.0f, 96.0f), module, Fatebinder::DRIFT_STEP_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm(38.0f, 96.0f), module, Fatebinder::DRIFT_RATE_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(mm(54.0f, 96.0f), module, Fatebinder::DRIFT_SLEW_PARAM));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(70.0f, 96.0f), module, Fatebinder::DRIFT_OUTPUT));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(96.0f, 96.0f), module, Fatebinder::SHAPE_MORPH_PARAM));
        addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(mm(122.0f, 96.0f), module, Fatebinder::DEPTH_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(96.0f, 84.0f), module, Fatebinder::SHAPE_MORPH_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(mm(122.0f, 84.0f), module, Fatebinder::DEPTH_CV_INPUT));
        addChild(createLightCentered<TinyLight<YellowLight>>(mm(110.0f, 84.0f), module, Fatebinder::INVERT_LIGHT));

        // Outputs
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(30.0f, 120.0f), module, Fatebinder::THREAD_A_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(62.0f, 120.0f), module, Fatebinder::THREAD_B_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(mm(94.0f, 120.0f), module, Fatebinder::THREAD_C_OUTPUT));
    }
};

Model* modelFatebinder = createModel<Fatebinder, FatebinderWidget>("Fatebinder");