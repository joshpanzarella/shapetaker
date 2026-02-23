#include "plugin.hpp"
#include "transmutation/ui.hpp"
#include "ui/menu_helpers.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

struct NocturneTV : Module {
    static constexpr float TAU = 6.28318530718f;
    static constexpr float REFRESH_MIN_HZ = 3.f;
    static constexpr float REFRESH_MAX_HZ = 120.f;
    static constexpr float INPUT_GAIN_MIN = 0.1f;
    static constexpr float INPUT_GAIN_MAX = 1.5f;
    static constexpr int SCENE_STEP_COUNT = 14;

    enum ParamId {
        WARP_PARAM,
        NOISE_PARAM,
        TEAR_PARAM,
        DRIFT_PARAM,
        TINT_PARAM,
        INPUT_GAIN_PARAM,
        REFRESH_PARAM,
        CHANNEL_PARAM,
        MODE_PARAM,
        CHAOS_LATCH_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        SIGNAL_1_INPUT,
        SIGNAL_2_INPUT,
        SIGNAL_3_INPUT,
        SIGNAL_4_INPUT,
        WARP_CV_INPUT,
        NOISE_CV_INPUT,
        TEAR_CV_INPUT,
        DRIFT_CV_INPUT,
        TINT_CV_INPUT,
        EXPLODE_CV_INPUT,
        DARKNESS_CV_INPUT,
        FILL_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    std::atomic<float> uiWarp = {0.f};
    std::atomic<float> uiNoise = {0.f};
    std::atomic<float> uiTear = {0.f};
    std::atomic<float> uiDrift = {0.f};
    std::atomic<float> uiTint = {0.5f};
    std::atomic<float> uiSignalLevel = {0.f};
    std::atomic<float> uiClock = {0.f};
    std::atomic<int> uiMode = {1};
    std::atomic<float> uiChaosGate = {0.f};
    std::atomic<float> uiSpinCv = {0.f};
    std::atomic<float> uiExplode = {0.f};
    std::atomic<float> uiDarkness = {0.f};
    std::atomic<float> uiFill = {0.f};
    std::atomic<int> uiConnectedMask = {0};
    std::atomic<float> uiRefreshHz = {18.f};
    std::atomic<int> uiSceneIndex = {7};
    std::array<std::atomic<float>, 4> uiSignalRaw;
    std::array<std::atomic<float>, 4> uiSignalEnv;

    float demoPhase = 0.f;
    float signalMeter = 0.f;
    float uiClockSeconds = 0.f;
    std::array<float, 4> signalRawFollow = {0.f, 0.f, 0.f, 0.f};
    std::array<float, 4> signalEnvFollow = {0.f, 0.f, 0.f, 0.f};

    NocturneTV() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(WARP_PARAM, 0.f, 1.f, 0.20f, "Horizontal deflection");
        configParam(NOISE_PARAM, 0.f, 1.f, 0.16f, "RF noise");
        configParam(TEAR_PARAM, 0.f, 1.f, 0.12f, "Vertical hold");
        configParam(DRIFT_PARAM, 0.f, 1.f, 0.16f, "Feedback persistence");
        configParam(TINT_PARAM, 0.f, 1.f, 0.5f, "Chroma phase");
        configParam(INPUT_GAIN_PARAM, INPUT_GAIN_MIN, INPUT_GAIN_MAX, 1.f, "Input volume", "x");
        configParam(REFRESH_PARAM, REFRESH_MIN_HZ, REFRESH_MAX_HZ, 18.f, "Screen refresh", " Hz");
        configParam(CHANNEL_PARAM, 0.f, SCENE_STEP_COUNT - 1.f, 7.f, "Program");
        configParam(MODE_PARAM, 0.f, 3.f, 1.f, "Video engine");
        configParam(CHAOS_LATCH_PARAM, 0.f, 4.f, 0.f, "Chaos latch");
        getParamQuantity(CHANNEL_PARAM)->snapEnabled = true;
        getParamQuantity(MODE_PARAM)->snapEnabled = true;
        getParamQuantity(CHAOS_LATCH_PARAM)->snapEnabled = true;

        configInput(SIGNAL_1_INPUT, "Deflect bus");
        configInput(SIGNAL_2_INPUT, "Hold bus");
        configInput(SIGNAL_3_INPUT, "Luma key bus");
        configInput(SIGNAL_4_INPUT, "Chroma/feedback bus");
        configInput(WARP_CV_INPUT, "Warp CV");
        configInput(NOISE_CV_INPUT, "Noise CV");
        configInput(TEAR_CV_INPUT, "Tear CV");
        configInput(DRIFT_CV_INPUT, "Drift CV");
        configInput(TINT_CV_INPUT, "Tint CV");
        configInput(EXPLODE_CV_INPUT, "Explode CV");
        configInput(DARKNESS_CV_INPUT, "Darkness CV");
        configInput(FILL_CV_INPUT, "Fill CV");

        for (int i = 0; i < 4; ++i) {
            uiSignalRaw[i].store(0.f, std::memory_order_relaxed);
            uiSignalEnv[i].store(0.f, std::memory_order_relaxed);
        }
    }

    float readInputAverage(Input& in, float fallback) {
        if (!in.isConnected()) {
            return fallback;
        }
        int channels = std::max(1, in.getChannels());
        float sum = 0.f;
        for (int c = 0; c < channels; ++c) {
            sum += in.getVoltage(c);
        }
        return sum / static_cast<float>(channels);
    }

    void process(const ProcessArgs& args) override {
        float warp = clamp(params[WARP_PARAM].getValue() + inputs[WARP_CV_INPUT].getVoltage() * 0.2f, 0.f, 1.f);
        float noise = clamp(params[NOISE_PARAM].getValue() + inputs[NOISE_CV_INPUT].getVoltage() * 0.2f, 0.f, 1.f);
        float tear = clamp(params[TEAR_PARAM].getValue() + inputs[TEAR_CV_INPUT].getVoltage() * 0.2f, 0.f, 1.f);
        float drift = clamp(params[DRIFT_PARAM].getValue() + inputs[DRIFT_CV_INPUT].getVoltage() * 0.2f, 0.f, 1.f);
        float tint = clamp(params[TINT_PARAM].getValue() + inputs[TINT_CV_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
        float explode = 0.f;
        if (inputs[EXPLODE_CV_INPUT].isConnected()) {
            // Positive voltage expands shapes; negative half-cycles collapse back to baseline.
            explode = clamp(inputs[EXPLODE_CV_INPUT].getVoltage() / 5.f, 0.f, 1.f);
        }
        float darkness = 0.f;
        if (inputs[DARKNESS_CV_INPUT].isConnected()) {
            darkness = clamp(inputs[DARKNESS_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
        }
        float fill = 0.f;
        if (inputs[FILL_CV_INPUT].isConnected()) {
            fill = clamp(inputs[FILL_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
        }
        float inputGain = clamp(params[INPUT_GAIN_PARAM].getValue(), INPUT_GAIN_MIN, INPUT_GAIN_MAX);
        float refreshHz = clamp(params[REFRESH_PARAM].getValue(), REFRESH_MIN_HZ, REFRESH_MAX_HZ);
        int sceneIndex = clamp(static_cast<int>(std::round(params[CHANNEL_PARAM].getValue())), 0, SCENE_STEP_COUNT - 1);
        int mode = clamp(static_cast<int>(std::round(params[MODE_PARAM].getValue())), 0, 3);

        float chaosPos = clamp(std::round(params[CHAOS_LATCH_PARAM].getValue()), 0.f, 4.f);
        float chaosGate = chaosPos * 0.25f;
        const int connectedMask =
            (inputs[SIGNAL_1_INPUT].isConnected() ? 0x1 : 0) |
            (inputs[SIGNAL_2_INPUT].isConnected() ? 0x2 : 0) |
            (inputs[SIGNAL_3_INPUT].isConnected() ? 0x4 : 0) |
            (inputs[SIGNAL_4_INPUT].isConnected() ? 0x8 : 0);

        uiClockSeconds += args.sampleTime;
        if (uiClockSeconds > 100000.f) {
            uiClockSeconds = 0.f;
        }

        float sceneNorm = static_cast<float>(sceneIndex) / static_cast<float>(SCENE_STEP_COUNT - 1);
        demoPhase += args.sampleTime * (0.075f + drift * 0.20f + sceneNorm * 0.14f);
        if (demoPhase >= 1.f) {
            demoPhase -= std::floor(demoPhase);
        }
        float phase = demoPhase * TAU;

        // Internal motion keeps visuals alive when an input is unpatched.
        std::array<float, 4> fallback = {
            std::sin(phase * 1.3f + std::sin(phase * 0.21f) * 0.7f) * 4.0f,
            std::cos(phase * 1.8f + 0.9f) * 3.5f,
            std::sin(phase * 0.9f + std::cos(phase * 0.17f) * 1.4f) * 3.7f,
            std::cos(phase * 2.2f + std::sin(phase * 0.41f) * 1.0f) * 3.9f
        };

        std::array<float, 4> rawSignals = {};
        std::array<float, 4> rawNorm = {};
        float sumEnv = 0.f;
        float peak = 0.f;

        float rawSlew = clamp(args.sampleTime * 42.f, 0.f, 1.f);
        float envAttack = clamp(args.sampleTime * 32.f, 0.f, 1.f);
        float envRelease = clamp(args.sampleTime * 9.f, 0.f, 1.f);

        for (int i = 0; i < 4; ++i) {
            rawSignals[i] = readInputAverage(inputs[SIGNAL_1_INPUT + i], fallback[i]) * inputGain;
            rawNorm[i] = clamp(rawSignals[i] / 8.f, -1.f, 1.f);
            signalRawFollow[i] += (rawNorm[i] - signalRawFollow[i]) * rawSlew;

            float envTarget = std::fabs(rawNorm[i]);
            float coeff = envTarget > signalEnvFollow[i] ? envAttack : envRelease;
            signalEnvFollow[i] += (envTarget - signalEnvFollow[i]) * coeff;

            sumEnv += signalEnvFollow[i];
            peak = std::max(peak, std::fabs(rawSignals[i]));
        }

        float avgEnv = sumEnv * 0.25f;
        float level = clamp(peak / 8.f, 0.f, 1.f);
        signalMeter += (level - signalMeter) * 0.020f;

        // Route all signal buses into a synthetic CRT/video processor model:
        // S1 = horizontal deflection, S2 = vertical hold, S3 = key/contrast, S4 = chroma/feedback injection.
        float chaosBlend = 0.24f + 0.76f * chaosGate;
        float warpEff = clamp(std::pow(warp, 1.75f) * chaosBlend
                              + std::fabs(signalRawFollow[0]) * 0.42f
                              + signalEnvFollow[0] * 0.34f, 0.f, 1.f);
        float noiseEff = clamp(std::pow(noise, 2.0f) * chaosBlend
                               + signalEnvFollow[2] * 0.24f
                               + signalEnvFollow[1] * 0.26f, 0.f, 1.f);
        float tearEff = clamp(std::pow(tear, 1.8f) * chaosBlend
                              + std::fabs(signalRawFollow[1]) * 0.46f
                              + signalEnvFollow[1] * 0.34f, 0.f, 1.f);
        float driftEff = clamp(std::pow(drift, 1.45f) * (0.30f + 0.70f * chaosGate)
                               + signalEnvFollow[3] * 0.56f
                               + avgEnv * 0.14f, 0.f, 1.f);
        float tintEff = clamp(tint + signalRawFollow[3] * 0.24f + signalRawFollow[2] * 0.06f, 0.f, 1.f);

        uiWarp.store(warpEff, std::memory_order_relaxed);
        uiNoise.store(noiseEff, std::memory_order_relaxed);
        uiTear.store(tearEff, std::memory_order_relaxed);
        uiDrift.store(driftEff, std::memory_order_relaxed);
        uiTint.store(tintEff, std::memory_order_relaxed);
        uiSignalLevel.store(signalMeter, std::memory_order_relaxed);
        uiClock.store(uiClockSeconds, std::memory_order_relaxed);
        uiMode.store(mode, std::memory_order_relaxed);
        uiChaosGate.store(chaosGate, std::memory_order_relaxed);
        uiExplode.store(explode, std::memory_order_relaxed);
        uiDarkness.store(darkness, std::memory_order_relaxed);
        uiFill.store(fill, std::memory_order_relaxed);
        float spinCv = 0.f;
        if (inputs[SIGNAL_1_INPUT].isConnected()) {
            spinCv += std::fabs(readInputAverage(inputs[SIGNAL_1_INPUT], 0.f)) * 0.10f;
        }
        if (inputs[WARP_CV_INPUT].isConnected()) {
            spinCv += std::fabs(inputs[WARP_CV_INPUT].getVoltage()) * 0.08f;
        }
        uiSpinCv.store(clamp(spinCv, 0.f, 2.5f), std::memory_order_relaxed);
        uiConnectedMask.store(connectedMask, std::memory_order_relaxed);
        uiRefreshHz.store(refreshHz, std::memory_order_relaxed);
        uiSceneIndex.store(sceneIndex, std::memory_order_relaxed);

        for (int i = 0; i < 4; ++i) {
            uiSignalRaw[i].store(signalRawFollow[i], std::memory_order_relaxed);
            uiSignalEnv[i].store(signalEnvFollow[i], std::memory_order_relaxed);
        }
    }
};

struct NocturneTVScreen : Widget {
    NocturneTV* module = nullptr;
    std::shared_ptr<Font> font;

    bool snapshotReady = false;
    float snapshotTimer = 0.f;
    float snapshotWarp = 0.2f;
    float snapshotNoise = 0.2f;
    float snapshotTear = 0.2f;
    float snapshotDrift = 0.2f;
    float snapshotTint = 0.5f;
    float snapshotSignalLevel = 0.f;
    float snapshotTime = 0.f;
    float snapshotChaosGate = 0.f;
    float snapshotSpinCv = 0.f;
    float snapshotExplode = 0.f;
    float snapshotDarkness = 0.f;
    float snapshotFill = 0.f;
    int snapshotMode = 1;
    int snapshotConnectedMask = 0;
    int snapshotSceneIndex = 7;
    std::array<float, 4> snapshotSignalRaw = {0.f, 0.f, 0.f, 0.f};
    std::array<float, 4> snapshotSignalEnv = {0.f, 0.f, 0.f, 0.f};

    int displayedScene = 7;
    float sceneChangeTimer = 0.f;

    explicit NocturneTVScreen(NocturneTV* module) : module(module) {
    }

    static float nextRand(uint32_t& state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (state & 0xFFFFu) / 65535.f;
    }

    static float hashSigned(uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return static_cast<float>(x & 0x00ffffffu) / 8388607.5f - 1.f;
    }

    static float hash01(uint32_t x) {
        return clamp(hashSigned(x) * 0.5f + 0.5f, 0.f, 1.f);
    }

    static void disintegrate3D(float explode, float t, uint32_t key, float& x, float& y, float& z) {
        if (explode <= 1e-4f) {
            return;
        }

        float dx = hashSigned(key * 0x9e3779b9u + 0x68bc21ebu);
        float dy = hashSigned(key * 0x85ebca6bu + 0x02e5be93u);
        float dz = hashSigned(key * 0xc2b2ae35u + 0x27d4eb2fu);
        float norm = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (norm < 1e-4f) {
            dx = 0.577f;
            dy = -0.577f;
            dz = 0.577f;
        } else {
            dx /= norm;
            dy /= norm;
            dz /= norm;
        }

        float speed = 0.50f + hash01(key ^ 0x21f0aaadu) * 1.90f;
        float phase = t * speed + hash01(key ^ 0x9c30d539u) * NocturneTV::TAU;
        float burst = explode * (0.25f + hash01(key ^ 0x243f6a88u) * 1.45f);
        float flutter = explode * explode * (0.08f + hash01(key ^ 0xb7e15162u) * 0.52f);

        x += dx * burst + std::sin(phase + dy * 2.3f) * flutter;
        y += dy * burst + std::cos(phase * 1.11f + dz * 2.1f) * flutter;
        z += dz * burst + std::sin(phase * 0.93f + dx * 1.9f) * flutter;
    }

    static float smoothstep01(float x) {
        x = clamp(x, 0.f, 1.f);
        return x * x * (3.f - 2.f * x);
    }

    static float valueNoise1D(float x, uint32_t seed) {
        int xi = static_cast<int>(std::floor(x));
        float xf = x - static_cast<float>(xi);
        uint32_t ix = static_cast<uint32_t>(xi);
        float a = hashSigned(ix ^ seed);
        float b = hashSigned((ix + 1u) ^ seed);
        return a + (b - a) * smoothstep01(xf);
    }

    static float fractalNoise1D(float x, uint32_t seed) {
        float sum = 0.f;
        float amp = 0.58f;
        float freq = 0.68f;
        float norm = 0.f;
        for (int i = 0; i < 4; ++i) {
            sum += valueNoise1D(x * freq + static_cast<float>(i) * 11.3f,
                                seed + static_cast<uint32_t>(i) * 0x9e3779b9u) * amp;
            norm += amp;
            amp *= 0.56f;
            freq *= 1.93f;
        }
        return (norm > 0.f) ? (sum / norm) : 0.f;
    }

    static NVGcolor blendColor(const NVGcolor& a, const NVGcolor& b, float t) {
        t = clamp(t, 0.f, 1.f);
        return nvgRGBAf(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t);
    }

    NVGcolor tintTrace(float tint, bool secondary = false) const {
        const NVGcolor teal = nvgRGBAf(0.00f, 0.60f, 0.48f, 1.f);
        const NVGcolor indigo = nvgRGBAf(0.35f, 0.50f, 0.85f, 1.f);
        const NVGcolor purple = nvgRGBAf(0.44f, 0.12f, 0.72f, 1.f);
        const NVGcolor yellow = nvgRGBAf(0.94f, 0.90f, 0.55f, 1.f);
        const NVGcolor ink = nvgRGBAf(0.91f, 0.88f, 0.78f, 1.f);

        float t = clamp(tint, 0.f, 1.f);
        NVGcolor base;
        if (!secondary) {
            base = (t < 0.5f)
                ? blendColor(teal, indigo, t * 2.f)
                : blendColor(indigo, purple, (t - 0.5f) * 2.f);
            return blendColor(base, ink, 0.16f);
        }
        base = (t < 0.5f)
            ? blendColor(purple, teal, t * 2.f)
            : blendColor(teal, yellow, (t - 0.5f) * 2.f);
        return blendColor(base, ink, 0.22f);
    }

    void drawSyncEngine(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                        float warp, float noise, float hold, float drift,
                        const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                        const NVGcolor& a, const NVGcolor& b) {
        int rows = 56 + static_cast<int>(sceneNorm * 78.f);
        float rowH = h / static_cast<float>(rows);
        float roll = std::fmod(t * (6.f + hold * 40.f + std::fabs(sigRaw[1]) * 26.f), h);
        float deflect = 5.f + warp * 38.f + sigEnv[0] * 44.f;

        for (int r = 0; r < rows; ++r) {
            float fy = (static_cast<float>(r) + 0.5f) / static_cast<float>(rows);
            float y = std::fmod(fy * h + roll, h);
            float shift = std::sin(y * 0.064f + t * (0.55f + drift * 2.8f) + sigRaw[0] * 6.2f) * deflect;
            shift += std::sin(y * 0.017f - t * (7.0f + hold * 11.f) + sigRaw[1] * 4.4f) * hold * 22.f;
            shift += std::sin(y * 0.11f + sigRaw[3] * 2.8f) * (1.6f + sigEnv[3] * 8.f);

            float hueWobble = 0.5f + 0.5f * std::sin(fy * 8.2f + t * (0.45f + drift * 0.9f) + sigRaw[3] * 3.3f);
            NVGcolor c = blendColor(a, b, hueWobble);
            float alpha = 0.04f + noise * 0.11f + sigEnv[2] * 0.11f;
            nvgBeginPath(args.vg);
            nvgRect(args.vg, shift - w * 0.07f, y - rowH * 0.50f, w * 1.14f, rowH * (0.62f + noise * 1.7f));
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alpha));
            nvgFill(args.vg);
        }

        int syncPips = 5 + static_cast<int>(sceneNorm * 5.f);
        for (int i = 0; i < syncPips; ++i) {
            float fy = (static_cast<float>(i) + 0.5f) / static_cast<float>(syncPips);
            float y = fy * h;
            float pipW = 4.f + hold * 10.f;
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, y - 1.f, pipW, 2.f);
            nvgFillColor(args.vg, nvgRGBAf(b.r, b.g, b.b, 0.16f + hold * 0.28f));
            nvgFill(args.vg);
        }
    }

    void drawKeyerEngine(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                         float warp, float noise, float hold, float drift,
                         const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                         const NVGcolor& a, const NVGcolor& b) {
        float keyThreshold = clamp(0.48f + sigRaw[2] * 0.44f, 0.06f, 0.94f);
        float contrast = 1.25f + warp * 2.9f + sigEnv[2] * 2.6f;
        int cols = 14 + static_cast<int>(sceneNorm * 12.f);
        int rows = 9 + static_cast<int>(sceneNorm * 8.f);
        float cw = w / static_cast<float>(cols);
        float ch = h / static_cast<float>(rows);

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(cols);
                float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(rows);
                float lumaCarrier = 0.5f + 0.5f * std::sin(
                    fx * (8.f + warp * 25.f) +
                    fy * (5.f + hold * 16.f) +
                    t * (0.8f + drift * 2.4f) +
                    sigRaw[0] * 3.7f +
                    sigRaw[1] * 2.6f);
                float luma = clamp(std::pow(lumaCarrier, contrast), 0.f, 1.f);
                if (luma < keyThreshold) {
                    continue;
                }

                float bendX = std::sin((fy + t * 0.21f) * 12.f + sigRaw[0] * 6.f) * (warp * 8.f + sigEnv[0] * 10.f);
                float bendY = std::cos((fx - t * 0.13f) * 8.f + sigRaw[1] * 5.f) * (hold * 6.f + sigEnv[1] * 8.f);
                float pad = 0.8f + noise * 1.6f;
                NVGcolor c = blendColor(a, b, 0.5f + 0.5f * std::sin(luma * 6.5f + sigRaw[3] * 4.2f));
                float alpha = 0.05f + luma * (0.12f + sigEnv[2] * 0.24f);
                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg,
                               static_cast<float>(x) * cw + pad + bendX,
                               static_cast<float>(y) * ch + pad + bendY,
                               cw - pad * 2.f,
                               ch - pad * 2.f,
                               1.2f + noise * 1.2f);
                nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alpha));
                nvgFill(args.vg);
            }
        }
    }

    void drawFeedbackEngine(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                            float warp, float noise, float hold, float drift,
                            const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                            const NVGcolor& a, const NVGcolor& b) {
        int echoes = 4 + static_cast<int>(sceneNorm * 4.f + drift * 8.f + sigEnv[3] * 6.f);
        float baseRadius = std::min(w, h) * (0.22f + sceneNorm * 0.18f);
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        for (int e = 0; e < echoes; ++e) {
            float lag = echoes > 1 ? static_cast<float>(e) / static_cast<float>(echoes - 1) : 0.f;
            float phase = t * (1.0f - lag * (0.05f + drift * 0.11f)) - lag * (0.35f + sigRaw[3] * 1.1f);
            float offX = std::sin(phase * 1.8f + sigRaw[0] * 4.3f + lag * 5.f) * lag * (warp * 36.f + sigEnv[0] * 34.f);
            float offY = std::cos(phase * 1.4f + sigRaw[1] * 3.8f + lag * 4.f) * lag * (hold * 28.f + sigEnv[1] * 26.f);
            float radius = baseRadius + lag * (34.f + drift * 48.f);
            float detail = 20.f + sceneNorm * 30.f;
            NVGcolor c = blendColor(a, b, clamp(0.18f + lag * 0.72f + sigRaw[3] * 0.1f, 0.f, 1.f));

            nvgBeginPath(args.vg);
            for (int i = 0; i < 80; ++i) {
                float fi = static_cast<float>(i) / 79.f;
                float ang = fi * NocturneTV::TAU;
                float ring = radius
                    + std::sin(ang * (2.f + sceneNorm * 5.f) + phase * (1.4f + drift * 2.6f)) * detail
                    + std::sin(ang * (7.f + warp * 12.f) + sigRaw[0] * 5.f) * (5.f + noise * 14.f);
                float px = w * 0.5f + offX + std::cos(ang + sigRaw[3] * 0.5f) * ring;
                float py = h * 0.5f + offY + std::sin(ang + sigRaw[2] * 0.5f) * ring * (0.66f + hold * 0.22f);
                if (i == 0) {
                    nvgMoveTo(args.vg, px, py);
                } else {
                    nvgLineTo(args.vg, px, py);
                }
            }
            nvgClosePath(args.vg);
            float fillA = (0.020f + (1.f - lag) * 0.08f) * (1.f + drift * 0.8f);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, fillA));
            nvgFill(args.vg);
        }
        nvgRestore(args.vg);
    }

    void drawPhosphorBleed(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                           float warp, float noise, float hold, float drift, float chaos,
                           const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                           const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        uint32_t seed = static_cast<uint32_t>(std::fmod(t * 931.f, 65535.f)) + 9817u;
        int streaks = 16 + static_cast<int>(sceneNorm * 22.f + noise * 20.f + chaos * 16.f);
        float smearSpan = 2.0f + noise * 12.f + drift * 8.f + hold * 6.f;
        for (int i = 0; i < streaks; ++i) {
            float fy = nextRand(seed);
            float y = fy * h;
            float width = w * (0.12f + nextRand(seed) * (0.44f + sceneNorm * 0.30f));
            float x = nextRand(seed) * (w - width);
            float wobble = std::sin(t * (3.4f + nextRand(seed) * 4.5f) + fy * 18.f + sigRaw[0] * 3.2f)
                * (0.4f + warp * 3.8f);
            float bandH = 0.9f + nextRand(seed) * (1.7f + noise * 2.4f + sigEnv[1] * 2.0f);
            NVGcolor c = blendColor(a, b, clamp(fy * 0.7f + nextRand(seed) * 0.3f, 0.f, 1.f));
            float alphaCore = 0.007f + nextRand(seed) * (0.032f + noise * 0.060f + chaos * 0.045f);

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, x + wobble - smearSpan * 0.50f, y - bandH * 0.5f,
                           width + smearSpan, bandH, 0.8f + bandH * 0.4f);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alphaCore));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, x + wobble - smearSpan * 1.35f, y - bandH * 0.95f,
                           width + smearSpan * 2.7f, bandH * 1.9f, 1.2f + bandH * 0.5f);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alphaCore * (0.35f + sigEnv[3] * 0.35f)));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawGlitchEngine(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                          float warp, float noise, float hold,
                          const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                          const NVGcolor& a, const NVGcolor& b) {
        uint32_t seed = static_cast<uint32_t>(std::fmod(t * 1800.f, 65535.f)) + 3241u;
        int dots = 220 + static_cast<int>(noise * 380.f + sceneNorm * 130.f);
        for (int i = 0; i < dots; ++i) {
            float x = nextRand(seed) * w;
            float y = std::fmod(nextRand(seed) * h + t * (4.f + hold * 22.f), h);
            float s = 0.5f + nextRand(seed) * (1.8f + sigEnv[2] * 2.4f);
            NVGcolor c = blendColor(a, b, nextRand(seed));
            float alpha = 0.02f + nextRand(seed) * (0.13f + noise * 0.22f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, y, s, s);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alpha));
            nvgFill(args.vg);
        }

        int blocks = 16 + static_cast<int>(sceneNorm * 22.f + noise * 36.f);
        for (int i = 0; i < blocks; ++i) {
            float gx = nextRand(seed) * w;
            float gy = nextRand(seed) * h;
            float bw = 10.f + nextRand(seed) * (w * 0.28f);
            float bh = 2.f + nextRand(seed) * (h * 0.12f);
            float shift = (nextRand(seed) - 0.5f) * (8.f + warp * 44.f + sigEnv[0] * 34.f);
            NVGcolor c = blendColor(a, b, nextRand(seed));
            float alpha = 0.05f + nextRand(seed) * (0.07f + sigEnv[3] * 0.16f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, gx + shift, gy, bw, bh);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alpha));
            nvgFill(args.vg);
        }

        int tearBands = 4 + static_cast<int>(hold * 11.f);
        for (int i = 0; i < tearBands; ++i) {
            float yy = nextRand(seed) * h;
            float bh = 1.f + nextRand(seed) * (4.f + hold * 10.f);
            float sh = (nextRand(seed) - 0.5f) * (12.f + hold * 45.f + std::fabs(sigRaw[1]) * 20.f);
            NVGcolor c = blendColor(a, b, nextRand(seed));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, sh, yy, w, bh);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.07f + noise * 0.18f));
            nvgFill(args.vg);
        }
    }

    void drawInterferenceLattice(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                                 float warp, float noise, float hold, float drift,
                                 const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                                 const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        int vertical = 10 + static_cast<int>(sceneNorm * 24.f);
        int horizontal = 7 + static_cast<int>(sceneNorm * 18.f);
        float deflect = 1.8f + warp * 16.f + sigEnv[0] * 14.f;
        float wobble = 1.1f + noise * 6.f + hold * 8.f;

        for (int i = 0; i < vertical; ++i) {
            float fx = vertical > 1 ? static_cast<float>(i) / static_cast<float>(vertical - 1) : 0.5f;
            float x0 = fx * w + std::sin(t * (0.7f + drift * 2.8f) + fx * 11.f + sigRaw[0] * 5.7f) * deflect;
            NVGcolor c = blendColor(a, b, fx);
            nvgBeginPath(args.vg);
            for (int s = 0; s < 42; ++s) {
                float fs = static_cast<float>(s) / 41.f;
                float y = fs * h;
                float x = x0 + std::sin(fs * 25.f + t * 2.2f + sigRaw[1] * 3.5f) * wobble;
                if (s == 0) {
                    nvgMoveTo(args.vg, x, y);
                } else {
                    nvgLineTo(args.vg, x, y);
                }
            }
            nvgStrokeWidth(args.vg, 0.55f + noise * 0.9f);
            nvgStrokeColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.03f + sigEnv[2] * 0.07f));
            nvgStroke(args.vg);
        }

        for (int i = 0; i < horizontal; ++i) {
            float fy = horizontal > 1 ? static_cast<float>(i) / static_cast<float>(horizontal - 1) : 0.5f;
            float y0 = fy * h + std::sin(t * (0.9f + hold * 3.1f) + fy * 13.f + sigRaw[1] * 4.6f) * (1.4f + hold * 13.f);
            NVGcolor c = blendColor(b, a, fy);
            nvgBeginPath(args.vg);
            for (int s = 0; s < 48; ++s) {
                float fs = static_cast<float>(s) / 47.f;
                float x = fs * w;
                float y = y0 + std::cos(fs * 19.f + t * 2.6f + sigRaw[3] * 4.8f) * (1.0f + noise * 4.5f + sigEnv[3] * 5.5f);
                if (s == 0) {
                    nvgMoveTo(args.vg, x, y);
                } else {
                    nvgLineTo(args.vg, x, y);
                }
            }
            nvgStrokeWidth(args.vg, 0.50f + noise * 0.8f);
            nvgStrokeColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.025f + sigEnv[3] * 0.06f));
            nvgStroke(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawBurstOverlay(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                          float warp, float noise, float hold, float drift,
                          const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                          const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        float cx = w * (0.5f + std::sin(t * (0.4f + drift * 1.8f) + sigRaw[0] * 2.8f) * (0.05f + warp * 0.08f));
        float cy = h * (0.5f + std::cos(t * (0.33f + hold * 1.7f) + sigRaw[1] * 2.4f) * (0.05f + hold * 0.09f));
        int spokes = 16 + static_cast<int>(sceneNorm * 34.f + sigEnv[2] * 22.f);

        for (int i = 0; i < spokes; ++i) {
            float fi = static_cast<float>(i) / static_cast<float>(spokes);
            float ang = fi * NocturneTV::TAU + t * (0.5f + drift * 2.1f) + sigRaw[3] * 1.2f;
            float len = std::min(w, h) * (0.18f + fi * 0.54f) * (0.65f + warp * 0.85f);
            float jitter = std::sin(fi * 37.f + t * 4.6f + sigRaw[0] * 4.3f) * (2.5f + noise * 12.f);
            float ex = cx + std::cos(ang) * (len + jitter);
            float ey = cy + std::sin(ang) * (len * (0.62f + hold * 0.28f) + jitter * 0.5f);
            NVGcolor c = blendColor(a, b, 0.5f + 0.5f * std::sin(fi * 11.f + sigRaw[3] * 3.5f));

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx, cy);
            nvgLineTo(args.vg, ex, ey);
            nvgStrokeWidth(args.vg, 0.7f + noise * 1.5f + sigEnv[3] * 1.2f);
            nvgStrokeColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.04f + sigEnv[2] * 0.10f));
            nvgStroke(args.vg);
        }

        int rings = 3 + static_cast<int>(sceneNorm * 6.f + drift * 4.f);
        for (int r = 0; r < rings; ++r) {
            float fr = rings > 1 ? static_cast<float>(r) / static_cast<float>(rings - 1) : 0.f;
            float rr = std::min(w, h) * (0.10f + fr * 0.45f) + std::sin(t * 3.2f + fr * 20.f + sigRaw[1] * 4.2f) * (1.2f + noise * 8.f);
            NVGcolor c = blendColor(b, a, fr);
            nvgBeginPath(args.vg);
            nvgEllipse(args.vg, cx, cy, rr, rr * (0.64f + hold * 0.26f));
            nvgStrokeWidth(args.vg, 0.7f + fr * 1.4f);
            nvgStrokeColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.03f + sigEnv[3] * 0.08f));
            nvgStroke(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawVHSTapeArtifacts(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                              float warp, float noise, float hold, float drift,
                              const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                              const NVGcolor& a, const NVGcolor& b) {
        uint32_t seed = static_cast<uint32_t>(std::fmod(t * 1733.f, 65535.f)) + 15791u;
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        // Horizontal chroma smear bands emulate tape chroma delay.
        int rows = 15 + static_cast<int>(sceneNorm * 18.f + noise * 20.f);
        float rowH = h / static_cast<float>(rows);
        float chromaPush = 1.8f + noise * 6.2f + sigEnv[3] * 9.5f;
        for (int r = 0; r < rows; ++r) {
            float fy = (static_cast<float>(r) + 0.5f) / static_cast<float>(rows);
            float y = fy * h;
            float wav = std::sin(fy * 24.f + t * (5.2f + hold * 12.5f) + sigRaw[0] * 4.5f);
            float shift = wav * chromaPush + std::sin(t * 7.3f + fy * 18.f + sigRaw[3] * 5.7f) * (0.8f + warp * 3.0f);
            float width = w * (0.35f + nextRand(seed) * 0.6f);
            float x0 = nextRand(seed) * (w - width);

            NVGcolor cA = blendColor(a, b, nextRand(seed));
            NVGcolor cB = blendColor(b, a, nextRand(seed));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, x0 + shift, y - rowH * 0.45f, width, rowH * (0.34f + noise * 0.8f));
            nvgFillColor(args.vg, nvgRGBAf(clamp(cA.r + 0.18f, 0.f, 1.f), cA.g * 0.45f, cA.b * 0.55f, 0.018f + sigEnv[2] * 0.07f));
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, x0 - shift * 0.6f, y - rowH * 0.48f, width, rowH * (0.30f + noise * 0.75f));
            nvgFillColor(args.vg, nvgRGBAf(cB.r * 0.55f, cB.g * 0.52f, clamp(cB.b + 0.22f, 0.f, 1.f), 0.015f + sigEnv[3] * 0.06f));
            nvgFill(args.vg);
        }

        // Tape dropout streaks.
        int dropouts = 10 + static_cast<int>(noise * 24.f + sceneNorm * 16.f);
        for (int i = 0; i < dropouts; ++i) {
            float x = nextRand(seed) * w;
            float y = nextRand(seed) * h;
            float hh = 1.2f + nextRand(seed) * (10.f + hold * 20.f);
            float ww = 0.8f + nextRand(seed) * 1.8f;
            float alpha = 0.02f + nextRand(seed) * (0.08f + noise * 0.10f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, y, ww, hh);
            nvgFillColor(args.vg, nvgRGBAf(0.94f, 0.90f, 0.82f, alpha));
            nvgFill(args.vg);
        }

        // Head-switching noise cluster near lower scan region.
        float bandH = 7.f + hold * 16.f + sigEnv[1] * 13.f;
        float bandY = h - bandH - 1.5f + std::sin(t * (2.4f + hold * 8.f) + sigRaw[1] * 3.2f) * (1.0f + hold * 5.f);
        int segments = 12 + static_cast<int>(noise * 22.f + sceneNorm * 10.f);
        for (int i = 0; i < segments; ++i) {
            float sx = nextRand(seed) * w;
            float sw = 5.f + nextRand(seed) * (w * 0.18f);
            float jitter = (nextRand(seed) - 0.5f) * (2.f + warp * 14.f);
            NVGcolor c = blendColor(a, b, nextRand(seed));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, sx + jitter, bandY + (nextRand(seed) - 0.5f) * 3.5f, sw, 1.0f + nextRand(seed) * bandH);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.03f + noise * 0.12f));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawSynthwaveHorizon(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                              float warp, float noise, float hold, float drift,
                              const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                              const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        float horizonY = h * (0.58f + std::sin(t * (0.18f + drift * 0.5f) + sigRaw[1] * 2.5f) * (0.02f + hold * 0.04f));
        float vanX = w * (0.50f + std::sin(t * 0.16f + sigRaw[0] * 2.7f) * (0.03f + warp * 0.08f));

        NVGcolor neonA = blendColor(nvgRGBAf(0.95f, 0.22f, 0.66f, 1.f), a, 0.40f);
        NVGcolor neonB = blendColor(nvgRGBAf(0.18f, 0.86f, 0.96f, 1.f), b, 0.38f);

        // Horizon line.
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.f, horizonY);
        nvgLineTo(args.vg, w, horizonY);
        nvgStrokeWidth(args.vg, 1.0f + noise * 0.9f);
        nvgStrokeColor(args.vg, nvgRGBAf(neonA.r, neonA.g, neonA.b, 0.16f + sigEnv[2] * 0.16f));
        nvgStroke(args.vg);

        // Retro sun bloom.
        float sunR = std::min(w, h) * (0.10f + sceneNorm * 0.11f);
        float sunX = vanX + std::sin(t * 0.33f + sigRaw[3] * 2.2f) * (3.f + warp * 15.f);
        float sunY = horizonY - sunR * (0.40f + std::fabs(sigRaw[1]) * 0.20f);
        NVGpaint sun = nvgRadialGradient(args.vg, sunX, sunY, sunR * 0.12f, sunR * 1.05f,
                                         nvgRGBAf(neonA.r, neonA.g, neonA.b, 0.18f + sigEnv[3] * 0.18f),
                                         nvgRGBAf(neonB.r, neonB.g, neonB.b, 0.f));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, sunX, sunY, sunR);
        nvgFillPaint(args.vg, sun);
        nvgFill(args.vg);

        // Perspective grid.
        int radial = 8 + static_cast<int>(sceneNorm * 10.f);
        for (int i = 0; i <= radial; ++i) {
            float fx = static_cast<float>(i) / static_cast<float>(radial);
            float x = fx * w;
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, h);
            nvgLineTo(args.vg, vanX + (x - vanX) * (0.05f + hold * 0.06f), horizonY);
            nvgStrokeWidth(args.vg, 0.65f + noise * 0.7f);
            nvgStrokeColor(args.vg, nvgRGBAf(neonB.r, neonB.g, neonB.b, 0.05f + sigEnv[0] * 0.08f));
            nvgStroke(args.vg);
        }

        int lat = 7 + static_cast<int>(sceneNorm * 8.f);
        for (int i = 1; i <= lat; ++i) {
            float fi = static_cast<float>(i) / static_cast<float>(lat);
            float ease = std::pow(fi, 1.55f);
            float y = horizonY + ease * (h - horizonY);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0.f, y);
            nvgLineTo(args.vg, w, y);
            nvgStrokeWidth(args.vg, 0.55f + (1.f - fi) * 0.9f);
            nvgStrokeColor(args.vg, nvgRGBAf(neonA.r, neonA.g, neonA.b, 0.03f + (1.f - fi) * 0.09f));
            nvgStroke(args.vg);
        }

        // Twinkling stars in upper half.
        uint32_t seed = static_cast<uint32_t>(std::fmod(t * 777.f, 65535.f)) + 4291u;
        int stars = 20 + static_cast<int>(sceneNorm * 26.f);
        for (int i = 0; i < stars; ++i) {
            float x = nextRand(seed) * w;
            float y = nextRand(seed) * (horizonY * 0.92f);
            float twinkle = 0.5f + 0.5f * std::sin(t * (3.0f + nextRand(seed) * 8.f) + nextRand(seed) * NocturneTV::TAU);
            float alpha = 0.01f + twinkle * (0.03f + sigEnv[2] * 0.07f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, y, 1.0f + nextRand(seed) * 1.5f, 1.0f + nextRand(seed) * 1.5f);
            nvgFillColor(args.vg, nvgRGBAf(0.93f, 0.89f, 0.78f, alpha));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawGasFillCore(const DrawArgs& args, float cx, float cy, float baseRadius, float fill, float hold,
                         float t, const std::array<float, 4>& sigRaw, const NVGcolor& a, const NVGcolor& b) {
        if (fill <= 0.001f) {
            return;
        }

        float pulse = 0.5f + 0.5f * std::sin(t * (0.9f + fill * 1.4f) + sigRaw[2] * 2.1f + sigRaw[3] * 1.3f);
        float gas = clamp(fill * (0.70f + 0.30f * pulse), 0.f, 1.f);
        float rx = baseRadius * (0.08f + gas * 0.96f);
        float ry = rx * (0.84f + hold * 0.12f);

        NVGcolor gasA = blendColor(a, b, 0.45f + 0.35f * pulse);
        NVGcolor gasB = blendColor(b, a, 0.35f + 0.30f * (1.f - pulse));

        float jitterX = std::sin(t * 0.73f + sigRaw[0] * 2.4f) * (0.8f + fill * 2.0f);
        float jitterY = std::cos(t * 0.61f + sigRaw[1] * 2.2f) * (0.8f + fill * 2.0f);

        NVGpaint cloud = nvgRadialGradient(args.vg, cx + jitterX, cy + jitterY,
                                           rx * 0.05f, rx * 1.05f,
                                           nvgRGBAf(gasA.r, gasA.g, gasA.b, 0.04f + fill * 0.20f),
                                           nvgRGBAf(gasB.r, gasB.g, gasB.b, 0.f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, rx, ry);
        nvgFillPaint(args.vg, cloud);
        nvgFill(args.vg);

        NVGpaint core = nvgRadialGradient(args.vg, cx - jitterX * 0.6f, cy - jitterY * 0.6f,
                                          rx * 0.03f, rx * 0.55f,
                                          nvgRGBAf(gasB.r, gasB.g, gasB.b, 0.05f + fill * 0.24f),
                                          nvgRGBAf(gasA.r, gasA.g, gasA.b, 0.f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, rx * 0.78f, ry * 0.76f);
        nvgFillPaint(args.vg, core);
        nvgFill(args.vg);
    }

    void drawTronSphere(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                        float warp, float noise, float hold, float drift, float chaos, float explode, float fill,
                        const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                        const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);

        float chaosTime = t * (0.35f + drift * 0.42f + chaos * 0.28f);
        float cxNoise = fractalNoise1D(chaosTime * 0.73f + sigRaw[0] * 0.90f + sigRaw[3] * 0.25f, 0x6a09e667u);
        float cyNoise = fractalNoise1D(chaosTime * 0.61f - sigRaw[1] * 0.75f + sigRaw[2] * 0.20f, 0xbb67ae85u);
        float cx = w * (0.52f + cxNoise * (0.008f + warp * 0.042f));
        float cy = h * (0.47f + cyNoise * (0.008f + hold * 0.040f));
        float radius = std::min(w, h) * (0.27f + sigEnv[2] * 0.06f);
        float spinYNoise = fractalNoise1D(chaosTime * 0.39f + sigRaw[0] * 1.2f + 4.1f, 0x3c6ef372u);
        float spinXNoise = fractalNoise1D(chaosTime * 0.43f - sigRaw[1] * 1.1f + 9.7f, 0xa54ff53au);
        float spinY = t * (0.08f + drift * 0.30f + chaos * 0.20f)
            + spinYNoise * (1.9f + chaos * 1.1f)
            + sigRaw[0] * 1.2f;
        float spinX = t * (0.06f + hold * 0.22f + chaos * 0.13f)
            + spinXNoise * (1.6f + chaos * 1.0f)
            + sigRaw[1] * 1.0f;
        float pix = 1.6f + noise * 2.2f + chaos * 1.1f;

        auto pixelSnap = [pix](float v) {
            return std::round(v / pix) * pix;
        };

        int latBands = 6 + static_cast<int>(sceneNorm * 4.f);
        int lonBands = 9 + static_cast<int>(sceneNorm * 5.f);
        int segs = 28;

        auto projectPoint = [&](float x, float y, float z, float& sx, float& sy, float& depth, uint32_t key) {
            disintegrate3D(explode, t, key, x, y, z);

            float cyR = std::cos(spinY);
            float syR = std::sin(spinY);
            float x1 = x * cyR + z * syR;
            float z1 = -x * syR + z * cyR;

            float cxR = std::cos(spinX);
            float sxR = std::sin(spinX);
            float y1 = y * cxR - z1 * sxR;
            float z2 = y * sxR + z1 * cxR;

            float perspective = clamp(1.f / (1.16f - z2 * 0.62f), 0.74f, 1.72f);
            sx = pixelSnap(cx + x1 * radius * perspective);
            sy = pixelSnap(cy + y1 * radius * perspective * (0.92f + hold * 0.08f));
            depth = z2;
        };

        auto strokeDepthSegments = [&](const std::vector<float>& xs, const std::vector<float>& ys,
                                       const std::vector<float>& ds, bool front, float width, NVGcolor color) {
            bool drawing = false;
            int explodeStride = (explode > 0.02f) ? 3 : 1;
            for (size_t i = 0; i < xs.size(); ++i) {
                bool visible = front ? (ds[i] >= 0.f) : (ds[i] < 0.f);
                if (explodeStride > 1 && ((i / static_cast<size_t>(explodeStride)) % 2u == 1u)) {
                    visible = false;
                }
                if (visible) {
                    if (!drawing) {
                        nvgBeginPath(args.vg);
                        nvgMoveTo(args.vg, xs[i], ys[i]);
                        drawing = true;
                    } else {
                        nvgLineTo(args.vg, xs[i], ys[i]);
                    }
                } else if (drawing) {
                    nvgStrokeWidth(args.vg, width);
                    nvgStrokeColor(args.vg, color);
                    nvgStroke(args.vg);
                    drawing = false;
                }
            }
            if (drawing) {
                nvgStrokeWidth(args.vg, width);
                nvgStrokeColor(args.vg, color);
                nvgStroke(args.vg);
            }
        };

        // Soft CRT haze behind the sphere.
        NVGpaint haze = nvgRadialGradient(args.vg, cx, cy, radius * 0.16f, radius * 1.30f,
                                          nvgRGBAf(a.r, a.g, a.b, 0.06f + noise * 0.08f),
                                          nvgRGBAf(b.r, b.g, b.b, 0.f));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, radius * (1.0f + noise * 0.18f));
        nvgFillPaint(args.vg, haze);
        nvgFill(args.vg);

        // Match the cube/pyramid shell treatment so the sphere feels like part of the same family.
        NVGpaint matte = nvgRadialGradient(args.vg, cx, cy,
                                           radius * 0.08f, radius * 1.14f,
                                           nvgRGBAf(0.f, 0.f, 0.f, 0.18f + noise * 0.10f + chaos * 0.08f),
                                           nvgRGBAf(0.f, 0.f, 0.f, 0.f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, radius * 1.04f, radius * (0.92f + hold * 0.08f));
        nvgFillPaint(args.vg, matte);
        nvgFill(args.vg);

        drawGasFillCore(args, cx, cy, radius, fill, hold, t, sigRaw, a, b);

        const NVGcolor tronHighlight = nvgRGBAf(1.00f, 1.00f, 1.00f, 1.f);
        const NVGcolor tronCyan = blendColor(tronHighlight, a, 0.14f);
        const NVGcolor tronViolet = blendColor(nvgRGBAf(1.00f, 0.98f, 0.95f, 1.f), b, 0.14f);

        float blackAlphaBase = clamp(0.32f + noise * 0.14f + chaos * 0.14f + sigEnv[2] * 0.10f, 0.f, 0.74f);
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, radius * (1.0f + noise * 0.04f), radius * (0.86f + hold * 0.10f));
        nvgStrokeWidth(args.vg, 1.8f + noise * 0.60f + chaos * 0.50f);
        nvgStrokeColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, blackAlphaBase * 0.84f));
        nvgStroke(args.vg);

        // Latitude lines.
        for (int lat = -latBands; lat <= latBands; ++lat) {
            float v = static_cast<float>(lat) / static_cast<float>(std::max(1, latBands));
            float phi = v * (NocturneTV::TAU * 0.25f);
            float ringR = std::cos(phi);
            float y = std::sin(phi);

            std::vector<float> xs(segs + 1);
            std::vector<float> ys(segs + 1);
            std::vector<float> ds(segs + 1);
            float frontAccum = 0.f;
            for (int s = 0; s <= segs; ++s) {
                float u = static_cast<float>(s) / static_cast<float>(segs);
                float theta = u * NocturneTV::TAU;
                float x = std::cos(theta) * ringR;
                float z = std::sin(theta) * ringR;

                float sx = 0.f;
                float sy = 0.f;
                float depth = 0.f;
                int latKey = lat + latBands + 32;
                uint32_t key = 0x10000u + static_cast<uint32_t>(latKey * 4096 + s);
                projectPoint(x, y, z, sx, sy, depth, key);
                xs[s] = sx;
                ys[s] = sy;
                ds[s] = depth;
                frontAccum += clamp(depth * 0.5f + 0.5f, 0.f, 1.f);
            }

            float front = frontAccum / static_cast<float>(segs + 1);
            float alpha = 0.020f + front * (0.070f + sigEnv[2] * 0.08f);
            NVGcolor latBase = blendColor(tronCyan, tronHighlight, 0.48f + front * 0.22f);
            NVGcolor c = blendColor(latBase, tronHighlight, clamp(0.40f + front * 0.56f, 0.f, 1.f));
            NVGcolor cBack = blendColor(latBase, nvgRGBAf(0.f, 0.f, 0.f, 1.f), 0.36f);

            nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            strokeDepthSegments(xs, ys, ds, false,
                                0.80f + noise * 0.35f,
                                nvgRGBAf(0.f, 0.f, 0.f, blackAlphaBase * 0.46f));
            strokeDepthSegments(xs, ys, ds, false,
                                0.60f + noise * 0.25f,
                                nvgRGBAf(cBack.r, cBack.g, cBack.b, 0.14f + sigEnv[2] * 0.12f));

            strokeDepthSegments(xs, ys, ds, true,
                                1.35f + noise * 0.65f + chaos * 0.55f,
                                nvgRGBAf(0.f, 0.f, 0.f, blackAlphaBase * (0.58f + front * 0.34f)));
            nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
            strokeDepthSegments(xs, ys, ds, true,
                                2.1f + noise * 1.0f + chaos * 0.9f,
                                nvgRGBAf(c.r, c.g, c.b, alpha * 0.48f));
            nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            strokeDepthSegments(xs, ys, ds, true,
                                1.00f + noise * 0.52f,
                                nvgRGBAf(c.r, c.g, c.b, clamp(alpha * 2.00f + 0.09f, 0.f, 0.98f)));
        }

        // Longitude lines.
        for (int lon = 0; lon < lonBands; ++lon) {
            float u = static_cast<float>(lon) / static_cast<float>(std::max(1, lonBands));
            float theta = u * NocturneTV::TAU;

            std::vector<float> xs(segs + 1);
            std::vector<float> ys(segs + 1);
            std::vector<float> ds(segs + 1);
            float frontAccum = 0.f;
            for (int s = 0; s <= segs; ++s) {
                float v = static_cast<float>(s) / static_cast<float>(segs);
                float phi = (v - 0.5f) * (NocturneTV::TAU * 0.5f);
                float x = std::cos(theta) * std::cos(phi);
                float y = std::sin(phi);
                float z = std::sin(theta) * std::cos(phi);

                float sx = 0.f;
                float sy = 0.f;
                float depth = 0.f;
                uint32_t key = 0x20000u + static_cast<uint32_t>(lon * 4096 + s);
                projectPoint(x, y, z, sx, sy, depth, key);
                xs[s] = sx;
                ys[s] = sy;
                ds[s] = depth;
                frontAccum += clamp(depth * 0.5f + 0.5f, 0.f, 1.f);
            }

            float front = frontAccum / static_cast<float>(segs + 1);
            float alpha = 0.016f + front * (0.060f + sigEnv[3] * 0.08f);
            NVGcolor lonBase = blendColor(tronViolet, tronHighlight, 0.44f + front * 0.24f);
            NVGcolor c = blendColor(lonBase, tronHighlight, clamp(0.38f + front * 0.56f, 0.f, 1.f));
            NVGcolor cBack = blendColor(lonBase, nvgRGBAf(0.f, 0.f, 0.f, 1.f), 0.38f);

            nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            strokeDepthSegments(xs, ys, ds, false,
                                0.74f + noise * 0.30f,
                                nvgRGBAf(0.f, 0.f, 0.f, blackAlphaBase * 0.42f));
            strokeDepthSegments(xs, ys, ds, false,
                                0.52f + noise * 0.24f,
                                nvgRGBAf(cBack.r, cBack.g, cBack.b, 0.13f + sigEnv[3] * 0.11f));

            strokeDepthSegments(xs, ys, ds, true,
                                1.20f + noise * 0.56f + chaos * 0.46f,
                                nvgRGBAf(0.f, 0.f, 0.f, blackAlphaBase * (0.54f + front * 0.40f)));
            nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
            strokeDepthSegments(xs, ys, ds, true,
                                1.9f + noise * 0.9f + chaos * 0.8f,
                                nvgRGBAf(c.r, c.g, c.b, alpha * 0.42f));
            nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            strokeDepthSegments(xs, ys, ds, true,
                                0.92f + noise * 0.46f,
                                nvgRGBAf(c.r, c.g, c.b, clamp(alpha * 1.90f + 0.08f, 0.f, 0.96f)));
        }

        // Pixel nodes for a coarse 8-bit "vector display" feel.
        int nodeRows = 3 + static_cast<int>(sceneNorm * 2.f);
        int nodeCols = 5 + static_cast<int>(sceneNorm * 2.f);
        float nodeSize = 0.7f + pix * 0.30f + explode * 0.8f;
        for (int iy = 0; iy <= nodeRows; ++iy) {
            float v = static_cast<float>(iy) / static_cast<float>(std::max(1, nodeRows));
            float phi = (v - 0.5f) * (NocturneTV::TAU * 0.5f);
            for (int ix = 0; ix < nodeCols; ++ix) {
                float u = static_cast<float>(ix) / static_cast<float>(std::max(1, nodeCols));
                float theta = u * NocturneTV::TAU;
                float x = std::cos(theta) * std::cos(phi);
                float y = std::sin(phi);
                float z = std::sin(theta) * std::cos(phi);

                float sx = 0.f;
                float sy = 0.f;
                float depth = 0.f;
                uint32_t key = 0x30000u + static_cast<uint32_t>(iy * 2048 + ix);
                projectPoint(x, y, z, sx, sy, depth, key);
                if (depth < -0.25f) {
                    continue;
                }

                NVGcolor c = blendColor(a, b, u);
                float alpha = 0.02f + clamp(depth * 0.5f + 0.5f, 0.f, 1.f) * (0.06f + sigEnv[2] * 0.06f);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, sx - nodeSize * 0.5f, sy - nodeSize * 0.5f, nodeSize, nodeSize);
                nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, alpha));
                nvgFill(args.vg);
            }
        }

        nvgRestore(args.vg);
    }

    void drawTronPyramid(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                         float warp, float noise, float hold, float drift, float chaos, float explode, float fill,
                         const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                         const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);

        float chaosTime = t * (0.31f + drift * 0.38f + chaos * 0.30f);
        float cxNoise = fractalNoise1D(chaosTime * 0.69f + sigRaw[0] * 0.85f + sigRaw[3] * 0.31f, 0x510e527fu);
        float cyNoise = fractalNoise1D(chaosTime * 0.63f - sigRaw[1] * 0.73f + sigRaw[2] * 0.27f, 0x9b05688cu);
        float cx = w * (0.52f + cxNoise * (0.008f + warp * 0.040f));
        float cy = h * (0.48f + cyNoise * (0.008f + hold * 0.038f));
        float scale = std::min(w, h) * (0.27f + sigEnv[2] * 0.05f);
        float spinYNoise = fractalNoise1D(chaosTime * 0.37f + sigRaw[0] * 1.10f + 3.3f, 0x1f83d9abu);
        float spinXNoise = fractalNoise1D(chaosTime * 0.41f - sigRaw[1] * 1.05f + 7.4f, 0x5be0cd19u);
        float spinY = t * (0.08f + drift * 0.30f + chaos * 0.20f)
            + spinYNoise * (1.8f + chaos * 1.1f)
            + sigRaw[0] * 1.2f;
        float spinX = t * (0.06f + hold * 0.22f + chaos * 0.13f)
            + spinXNoise * (1.5f + chaos * 1.0f)
            + sigRaw[1] * 1.0f;
        float pix = 1.5f + noise * 2.0f + chaos * 1.0f;

        auto pixelSnap = [pix](float v) {
            return std::round(v / pix) * pix;
        };

        struct P3 {
            float x;
            float y;
            float z;
        };
        const std::array<P3, 5> verts = {{
            {0.f, 0.78f, 0.f},      // apex (shorter to avoid stretched look)
            {-0.88f, -0.70f, -0.88f},
            {0.88f, -0.70f, -0.88f},
            {0.88f, -0.70f, 0.88f},
            {-0.88f, -0.70f, 0.88f}
        }};
        auto explodePoint = [&](P3 p, uint32_t key) {
            disintegrate3D(explode, t, key, p.x, p.y, p.z);
            return p;
        };

        auto project = [&](const P3& p, float& sx, float& sy, float& depth) {
            float cyR = std::cos(spinY);
            float syR = std::sin(spinY);
            float x1 = p.x * cyR + p.z * syR;
            float z1 = -p.x * syR + p.z * cyR;

            float cxR = std::cos(spinX);
            float sxR = std::sin(spinX);
            float y1 = p.y * cxR - z1 * sxR;
            float z2 = p.y * sxR + z1 * cxR;

            float perspective = clamp(1.f / (1.18f - z2 * 0.62f), 0.72f, 1.70f);
            sx = pixelSnap(cx + x1 * scale * perspective);
            sy = pixelSnap(cy + y1 * scale * perspective * (0.90f + hold * 0.08f));
            depth = z2;
        };

        std::array<float, 5> px = {};
        std::array<float, 5> py = {};
        std::array<float, 5> pd = {};
        for (int i = 0; i < 5; ++i) {
            project(verts[i], px[i], py[i], pd[i]);
        }

        const NVGcolor tronCyan = nvgRGBAf(0.24f, 0.98f, 1.00f, 1.f);
        const NVGcolor tronViolet = nvgRGBAf(0.74f, 0.44f, 1.00f, 1.f);
        const NVGcolor tronHighlight = nvgRGBAf(0.98f, 1.00f, 1.00f, 1.f);

        NVGpaint haze = nvgRadialGradient(args.vg, cx, cy, scale * 0.16f, scale * 1.35f,
                                          nvgRGBAf(a.r, a.g, a.b, 0.06f + noise * 0.08f),
                                          nvgRGBAf(b.r, b.g, b.b, 0.f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, scale * 1.04f, scale * (0.92f + hold * 0.08f));
        nvgFillPaint(args.vg, haze);
        nvgFill(args.vg);

        NVGpaint matte = nvgRadialGradient(args.vg, cx, cy, scale * 0.08f, scale * 1.14f,
                                           nvgRGBAf(0.f, 0.f, 0.f, 0.18f + noise * 0.10f + chaos * 0.08f),
                                           nvgRGBAf(0.f, 0.f, 0.f, 0.f));
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, px[0], py[0]);
        nvgLineTo(args.vg, px[1], py[1]);
        nvgLineTo(args.vg, px[2], py[2]);
        nvgLineTo(args.vg, px[3], py[3]);
        nvgLineTo(args.vg, px[4], py[4]);
        nvgClosePath(args.vg);
        nvgFillPaint(args.vg, matte);
        nvgFill(args.vg);

        drawGasFillCore(args, cx, cy, scale, fill, hold, t, sigRaw, a, b);

        struct Face {
            int i0;
            int i1;
            int i2;
        };
        const std::array<Face, 6> faces = {{
            {0, 1, 2}, {0, 2, 3}, {0, 3, 4}, {0, 4, 1},
            {1, 2, 3}, {1, 3, 4}
        }};
        std::array<int, 6> faceOrder = {{0, 1, 2, 3, 4, 5}};
        std::sort(faceOrder.begin(), faceOrder.end(), [&](int lhs, int rhs) {
            const Face& fl = faces[lhs];
            const Face& fr = faces[rhs];
            float dl = (pd[fl.i0] + pd[fl.i1] + pd[fl.i2]) / 3.f;
            float dr = (pd[fr.i0] + pd[fr.i1] + pd[fr.i2]) / 3.f;
            return dl < dr; // draw back faces first
        });
        for (int oi = 0; oi < static_cast<int>(faceOrder.size()); ++oi) {
            int fi = faceOrder[oi];
            const Face& f = faces[fi];
            float depth = (pd[f.i0] + pd[f.i1] + pd[f.i2]) / 3.f;
            float front = clamp(depth * 0.5f + 0.5f, 0.f, 1.f);
            NVGcolor fc = blendColor(tronCyan, tronViolet, static_cast<float>(fi) / 5.f);
            float alpha = 0.008f + front * (0.040f + sigEnv[2] * 0.045f);
            alpha *= (1.f - explode * 0.70f);
            if (fi >= 4) {
                alpha *= 0.6f;
            }
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, px[f.i0], py[f.i0]);
            nvgLineTo(args.vg, px[f.i1], py[f.i1]);
            nvgLineTo(args.vg, px[f.i2], py[f.i2]);
            nvgClosePath(args.vg);
            nvgFillColor(args.vg, nvgRGBAf(fc.r, fc.g, fc.b, alpha));
            nvgFill(args.vg);
        }

        const std::array<std::pair<int, int>, 8> edges = {{
            {1, 2}, {2, 3}, {3, 4}, {4, 1}, // base
            {0, 1}, {0, 2}, {0, 3}, {0, 4}  // sides
        }};

        float blackAlphaBase = clamp(0.32f + noise * 0.14f + chaos * 0.14f + sigEnv[2] * 0.10f, 0.f, 0.74f);

        auto drawEdge = [&](int edgeIndex, int ia, int ib, float mix) {
            float x1 = px[ia];
            float y1 = py[ia];
            float d1 = pd[ia];
            float x2 = px[ib];
            float y2 = py[ib];
            float d2 = pd[ib];
            if (explode > 0.001f) {
                P3 p1 = explodePoint(verts[ia], 0x41000u + static_cast<uint32_t>(edgeIndex * 2));
                P3 p2 = explodePoint(verts[ib], 0x41000u + static_cast<uint32_t>(edgeIndex * 2 + 1));
                project(p1, x1, y1, d1);
                project(p2, x2, y2, d2);
            }
            float depth = (d1 + d2) * 0.5f;
            bool front = depth >= 0.f;

            NVGcolor base = blendColor(tronCyan, tronViolet, mix);
            NVGcolor edge = blendColor(base, tronHighlight, front ? 0.62f : 0.18f);

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeWidth(args.vg, front ? (1.65f + noise * 0.72f) : (1.05f + noise * 0.40f));
            nvgStrokeColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f,
                                             blackAlphaBase * (front ? 0.95f : 0.72f)));
            nvgStroke(args.vg);

            if (front) {
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, x1, y1);
                nvgLineTo(args.vg, x2, y2);
                nvgStrokeWidth(args.vg, 2.1f + noise * 0.9f + chaos * 0.8f);
                nvgStrokeColor(args.vg, nvgRGBAf(edge.r, edge.g, edge.b, 0.20f + sigEnv[2] * 0.18f));
                nvgStroke(args.vg);
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            }

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeWidth(args.vg, front ? (0.90f + noise * 0.36f) : (0.62f + noise * 0.24f));
            nvgStrokeColor(args.vg, nvgRGBAf(edge.r, edge.g, edge.b,
                                             front ? (0.42f + sigEnv[2] * 0.14f) : (0.18f + sigEnv[2] * 0.07f)));
            nvgStroke(args.vg);
        };

        for (int ei = 0; ei < static_cast<int>(edges.size()); ++ei) {
            float mix = static_cast<float>(ei) / static_cast<float>(std::max(1, static_cast<int>(edges.size()) - 1));
            drawEdge(ei, edges[ei].first, edges[ei].second, mix);
        }

        for (int i = 0; i < 5; ++i) {
            float nx = px[i];
            float ny = py[i];
            float nd = pd[i];
            if (explode > 0.001f) {
                P3 node = explodePoint(verts[i], 0x43000u + static_cast<uint32_t>(i));
                project(node, nx, ny, nd);
            }
            float front = clamp(nd * 0.5f + 0.5f, 0.f, 1.f);
            float r = 0.9f + pix * 0.28f + front * 0.5f;
            NVGcolor c = blendColor(tronCyan, tronHighlight, front);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, nx - r * 0.5f, ny - r * 0.5f, r, r);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.18f + front * 0.48f));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawTronCube(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                      float warp, float noise, float hold, float drift, float chaos, float explode, float fill,
                      const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                      const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);

        float chaosTime = t * (0.30f + drift * 0.36f + chaos * 0.28f);
        float cxNoise = fractalNoise1D(chaosTime * 0.66f + sigRaw[0] * 0.80f + sigRaw[3] * 0.26f, 0xcbbb9d5du);
        float cyNoise = fractalNoise1D(chaosTime * 0.59f - sigRaw[1] * 0.70f + sigRaw[2] * 0.22f, 0x629a292au);
        float cx = w * (0.52f + cxNoise * (0.008f + warp * 0.038f));
        float cy = h * (0.48f + cyNoise * (0.008f + hold * 0.036f));
        float scale = std::min(w, h) * (0.27f + sigEnv[2] * 0.05f);
        float spinYNoise = fractalNoise1D(chaosTime * 0.35f + sigRaw[0] * 1.05f + 2.7f, 0x9159015au);
        float spinXNoise = fractalNoise1D(chaosTime * 0.39f - sigRaw[1] * 1.00f + 6.4f, 0x152fecd8u);
        float spinY = t * (0.07f + drift * 0.26f + chaos * 0.18f)
            + spinYNoise * (1.6f + chaos * 1.0f)
            + sigRaw[0] * 1.1f;
        float spinX = t * (0.05f + hold * 0.19f + chaos * 0.12f)
            + spinXNoise * (1.35f + chaos * 0.95f)
            + sigRaw[1] * 0.9f;
        float pix = 1.45f + noise * 1.9f + chaos * 0.95f;

        auto pixelSnap = [pix](float v) {
            return std::round(v / pix) * pix;
        };

        struct P3 {
            float x;
            float y;
            float z;
        };
        const std::array<P3, 8> verts = {{
            {-0.80f, -0.80f, -0.80f}, // 0
            {0.80f, -0.80f, -0.80f},  // 1
            {0.80f, 0.80f, -0.80f},   // 2
            {-0.80f, 0.80f, -0.80f},  // 3
            {-0.80f, -0.80f, 0.80f},  // 4
            {0.80f, -0.80f, 0.80f},   // 5
            {0.80f, 0.80f, 0.80f},    // 6
            {-0.80f, 0.80f, 0.80f}    // 7
        }};
        auto explodePoint = [&](P3 p, uint32_t key) {
            disintegrate3D(explode, t, key, p.x, p.y, p.z);
            return p;
        };

        auto project = [&](const P3& p, float& sx, float& sy, float& depth) {
            float cyR = std::cos(spinY);
            float syR = std::sin(spinY);
            float x1 = p.x * cyR + p.z * syR;
            float z1 = -p.x * syR + p.z * cyR;

            float cxR = std::cos(spinX);
            float sxR = std::sin(spinX);
            float y1 = p.y * cxR - z1 * sxR;
            float z2 = p.y * sxR + z1 * cxR;

            float perspective = clamp(1.f / (1.16f - z2 * 0.60f), 0.74f, 1.72f);
            sx = pixelSnap(cx + x1 * scale * perspective);
            sy = pixelSnap(cy + y1 * scale * perspective * (0.92f + hold * 0.06f));
            depth = z2;
        };

        std::array<float, 8> px = {};
        std::array<float, 8> py = {};
        std::array<float, 8> pd = {};
        for (int i = 0; i < 8; ++i) {
            project(verts[i], px[i], py[i], pd[i]);
        }

        const NVGcolor tronCyan = nvgRGBAf(0.23f, 0.98f, 1.00f, 1.f);
        const NVGcolor tronViolet = nvgRGBAf(0.72f, 0.42f, 1.00f, 1.f);
        const NVGcolor tronHighlight = nvgRGBAf(0.98f, 1.00f, 1.00f, 1.f);

        NVGpaint haze = nvgRadialGradient(args.vg, cx, cy, scale * 0.16f, scale * 1.30f,
                                          nvgRGBAf(a.r, a.g, a.b, 0.06f + noise * 0.08f),
                                          nvgRGBAf(b.r, b.g, b.b, 0.f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, scale * 1.06f, scale * (0.92f + hold * 0.08f));
        nvgFillPaint(args.vg, haze);
        nvgFill(args.vg);

        NVGpaint matte = nvgRadialGradient(args.vg, cx, cy, scale * 0.08f, scale * 1.14f,
                                           nvgRGBAf(0.f, 0.f, 0.f, 0.18f + noise * 0.10f + chaos * 0.08f),
                                           nvgRGBAf(0.f, 0.f, 0.f, 0.f));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, cx - scale * 1.05f, cy - scale * 1.05f, scale * 2.10f, scale * 2.10f);
        nvgFillPaint(args.vg, matte);
        nvgFill(args.vg);

        drawGasFillCore(args, cx, cy, scale, fill, hold, t, sigRaw, a, b);

        struct Quad {
            int i0;
            int i1;
            int i2;
            int i3;
        };
        const std::array<Quad, 6> faces = {{
            {0, 1, 2, 3}, // back
            {4, 5, 6, 7}, // front
            {0, 1, 5, 4}, // bottom
            {3, 2, 6, 7}, // top
            {1, 2, 6, 5}, // right
            {0, 3, 7, 4}  // left
        }};

        std::array<int, 6> faceOrder = {{0, 1, 2, 3, 4, 5}};
        std::sort(faceOrder.begin(), faceOrder.end(), [&](int lhs, int rhs) {
            const Quad& fl = faces[lhs];
            const Quad& fr = faces[rhs];
            float dl = (pd[fl.i0] + pd[fl.i1] + pd[fl.i2] + pd[fl.i3]) * 0.25f;
            float dr = (pd[fr.i0] + pd[fr.i1] + pd[fr.i2] + pd[fr.i3]) * 0.25f;
            return dl < dr;
        });

        for (int oi = 0; oi < static_cast<int>(faceOrder.size()); ++oi) {
            int fi = faceOrder[oi];
            const Quad& f = faces[fi];
            float depth = (pd[f.i0] + pd[f.i1] + pd[f.i2] + pd[f.i3]) * 0.25f;
            float front = clamp(depth * 0.5f + 0.5f, 0.f, 1.f);
            NVGcolor fc = blendColor(tronCyan, tronViolet, static_cast<float>(fi) / 5.f);
            float alpha = 0.008f + front * (0.040f + sigEnv[2] * 0.045f);
            alpha *= (1.f - explode * 0.70f);

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, px[f.i0], py[f.i0]);
            nvgLineTo(args.vg, px[f.i1], py[f.i1]);
            nvgLineTo(args.vg, px[f.i2], py[f.i2]);
            nvgLineTo(args.vg, px[f.i3], py[f.i3]);
            nvgClosePath(args.vg);
            nvgFillColor(args.vg, nvgRGBAf(fc.r, fc.g, fc.b, alpha));
            nvgFill(args.vg);
        }

        const std::array<std::pair<int, int>, 12> edges = {{
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        }};

        float blackAlphaBase = clamp(0.32f + noise * 0.14f + chaos * 0.14f + sigEnv[2] * 0.10f, 0.f, 0.74f);
        auto drawEdge = [&](int edgeIndex, int ia, int ib, float mix) {
            float x1 = px[ia];
            float y1 = py[ia];
            float d1 = pd[ia];
            float x2 = px[ib];
            float y2 = py[ib];
            float d2 = pd[ib];
            if (explode > 0.001f) {
                P3 p1 = explodePoint(verts[ia], 0x51000u + static_cast<uint32_t>(edgeIndex * 2));
                P3 p2 = explodePoint(verts[ib], 0x51000u + static_cast<uint32_t>(edgeIndex * 2 + 1));
                project(p1, x1, y1, d1);
                project(p2, x2, y2, d2);
            }
            float depth = (d1 + d2) * 0.5f;
            bool front = depth >= 0.f;
            NVGcolor base = blendColor(tronCyan, tronViolet, mix);
            NVGcolor edge = blendColor(base, tronHighlight, front ? 0.60f : 0.18f);

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeWidth(args.vg, front ? (1.62f + noise * 0.68f) : (1.00f + noise * 0.38f));
            nvgStrokeColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f,
                                             blackAlphaBase * (front ? 0.94f : 0.72f)));
            nvgStroke(args.vg);

            if (front) {
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, x1, y1);
                nvgLineTo(args.vg, x2, y2);
                nvgStrokeWidth(args.vg, 2.0f + noise * 0.86f + chaos * 0.78f);
                nvgStrokeColor(args.vg, nvgRGBAf(edge.r, edge.g, edge.b, 0.20f + sigEnv[2] * 0.18f));
                nvgStroke(args.vg);
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            }

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeWidth(args.vg, front ? (0.90f + noise * 0.34f) : (0.60f + noise * 0.22f));
            nvgStrokeColor(args.vg, nvgRGBAf(edge.r, edge.g, edge.b,
                                             front ? (0.44f + sigEnv[2] * 0.14f) : (0.18f + sigEnv[2] * 0.07f)));
            nvgStroke(args.vg);
        };

        for (int ei = 0; ei < static_cast<int>(edges.size()); ++ei) {
            float mix = static_cast<float>(ei) / static_cast<float>(std::max(1, static_cast<int>(edges.size()) - 1));
            drawEdge(ei, edges[ei].first, edges[ei].second, mix);
        }

        for (int i = 0; i < 8; ++i) {
            float nx = px[i];
            float ny = py[i];
            float nd = pd[i];
            if (explode > 0.001f) {
                P3 node = explodePoint(verts[i], 0x53000u + static_cast<uint32_t>(i));
                project(node, nx, ny, nd);
            }
            float front = clamp(nd * 0.5f + 0.5f, 0.f, 1.f);
            float r = 0.85f + pix * 0.26f + front * 0.48f;
            NVGcolor c = blendColor(tronCyan, tronHighlight, front);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, nx - r * 0.5f, ny - r * 0.5f, r, r);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.18f + front * 0.46f));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    void drawTronVariantShape(const DrawArgs& args, int variantId, float w, float h, float t, float sceneNorm,
                              float warp, float noise, float hold, float drift, float chaos, float explode, float fill,
                              const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                              const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);

        float chaosTime = t * (0.30f + drift * 0.34f + chaos * 0.26f);
        float cxNoise = fractalNoise1D(chaosTime * 0.64f + sigRaw[0] * 0.70f + sigRaw[3] * 0.22f, 0x243f6a88u);
        float cyNoise = fractalNoise1D(chaosTime * 0.58f - sigRaw[1] * 0.64f + sigRaw[2] * 0.20f, 0x85a308d3u);
        float cx = w * (0.52f + cxNoise * (0.008f + warp * 0.036f));
        float cy = h * (0.48f + cyNoise * (0.008f + hold * 0.034f));
        float scale = std::min(w, h) * (0.27f + sigEnv[2] * 0.05f);
        float spinYNoise = fractalNoise1D(chaosTime * 0.34f + sigRaw[0] * 0.95f + 2.1f, 0x13198a2eu);
        float spinXNoise = fractalNoise1D(chaosTime * 0.37f - sigRaw[1] * 0.92f + 5.8f, 0x03707344u);
        float spinY = t * (0.065f + drift * 0.24f + chaos * 0.17f) + spinYNoise * (1.4f + chaos * 0.9f) + sigRaw[0] * 0.9f;
        float spinX = t * (0.050f + hold * 0.17f + chaos * 0.11f) + spinXNoise * (1.2f + chaos * 0.8f) + sigRaw[1] * 0.7f;
        float pix = 1.4f + noise * 1.8f + chaos * 0.9f;

        auto pixelSnap = [pix](float v) {
            return std::round(v / pix) * pix;
        };

        struct V3 {
            float x;
            float y;
            float z;
        };
        auto explodePoint = [&](V3 p, uint32_t key) {
            disintegrate3D(explode, t, key, p.x, p.y, p.z);
            return p;
        };

        auto project = [&](const V3& p, float& sx, float& sy, float& depth) {
            float cyR = std::cos(spinY);
            float syR = std::sin(spinY);
            float x1 = p.x * cyR + p.z * syR;
            float z1 = -p.x * syR + p.z * cyR;

            float cxR = std::cos(spinX);
            float sxR = std::sin(spinX);
            float y1 = p.y * cxR - z1 * sxR;
            float z2 = p.y * sxR + z1 * cxR;

            float perspective = clamp(1.f / (1.18f - z2 * 0.58f), 0.74f, 1.72f);
            sx = pixelSnap(cx + x1 * scale * perspective);
            sy = pixelSnap(cy + y1 * scale * perspective * (0.92f + hold * 0.06f));
            depth = z2;
        };

        const NVGcolor tronCyan = nvgRGBAf(0.22f, 0.98f, 1.00f, 1.f);
        const NVGcolor tronViolet = nvgRGBAf(0.70f, 0.40f, 1.00f, 1.f);
        const NVGcolor tronHighlight = nvgRGBAf(0.98f, 1.00f, 1.00f, 1.f);
        float blackAlphaBase = clamp(0.30f + noise * 0.14f + chaos * 0.12f + sigEnv[2] * 0.10f, 0.f, 0.72f);

        NVGpaint haze = nvgRadialGradient(args.vg, cx, cy, scale * 0.14f, scale * 1.28f,
                                          nvgRGBAf(a.r, a.g, a.b, 0.05f + noise * 0.07f),
                                          nvgRGBAf(b.r, b.g, b.b, 0.f));
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, cx, cy, scale * 1.02f, scale * (0.92f + hold * 0.07f));
        nvgFillPaint(args.vg, haze);
        nvgFill(args.vg);

        drawGasFillCore(args, cx, cy, scale, fill, hold, t, sigRaw, a, b);

        auto drawProjectedEdge = [&](float x1, float y1, float d1, float x2, float y2, float d2, float mix) {
            bool front = ((d1 + d2) * 0.5f) >= 0.f;
            NVGcolor base = blendColor(tronCyan, tronViolet, clamp(mix, 0.f, 1.f));
            NVGcolor edge = blendColor(base, tronHighlight, front ? 0.62f : 0.18f);

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeWidth(args.vg, front ? (1.56f + noise * 0.62f) : (0.98f + noise * 0.36f));
            nvgStrokeColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, blackAlphaBase * (front ? 0.93f : 0.68f)));
            nvgStroke(args.vg);

            if (front) {
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, x1, y1);
                nvgLineTo(args.vg, x2, y2);
                nvgStrokeWidth(args.vg, 1.9f + noise * 0.8f + chaos * 0.7f);
                nvgStrokeColor(args.vg, nvgRGBAf(edge.r, edge.g, edge.b, 0.18f + sigEnv[2] * 0.16f));
                nvgStroke(args.vg);
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            }

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStrokeWidth(args.vg, front ? (0.86f + noise * 0.30f) : (0.58f + noise * 0.20f));
            nvgStrokeColor(args.vg, nvgRGBAf(edge.r, edge.g, edge.b, front ? (0.40f + sigEnv[2] * 0.14f) : (0.16f + sigEnv[2] * 0.06f)));
            nvgStroke(args.vg);
        };

        uint32_t explodeEdgeCounter = 1u;
        auto projectEdgeEndpoints = [&](const V3& p0In, const V3& p1In,
                                        float& x0, float& y0, float& d0,
                                        float& x1, float& y1, float& d1) {
            V3 p0 = p0In;
            V3 p1 = p1In;
            if (explode > 0.001f) {
                uint32_t key = explodeEdgeCounter++;
                p0 = explodePoint(p0, 0x61000u + key * 2u);
                p1 = explodePoint(p1, 0x61000u + key * 2u + 1u);
            }
            project(p0, x0, y0, d0);
            project(p1, x1, y1, d1);
        };

        auto drawMesh = [&](const std::vector<V3>& verts, const std::vector<std::pair<int, int>>& edges, bool drawNodes) {
            std::vector<float> px(verts.size(), 0.f);
            std::vector<float> py(verts.size(), 0.f);
            std::vector<float> pd(verts.size(), 0.f);
            for (size_t i = 0; i < verts.size(); ++i) {
                project(verts[i], px[i], py[i], pd[i]);
            }
            for (size_t ei = 0; ei < edges.size(); ++ei) {
                int ia = edges[ei].first;
                int ib = edges[ei].second;
                float mix = static_cast<float>(ei) / static_cast<float>(std::max(1, static_cast<int>(edges.size()) - 1));
                float x0 = px[ia];
                float y0 = py[ia];
                float d0 = pd[ia];
                float x1 = px[ib];
                float y1 = py[ib];
                float d1 = pd[ib];
                if (explode > 0.001f) {
                    projectEdgeEndpoints(verts[ia], verts[ib], x0, y0, d0, x1, y1, d1);
                }
                drawProjectedEdge(x0, y0, d0, x1, y1, d1, mix);
            }
            if (drawNodes) {
                for (size_t i = 0; i < verts.size(); ++i) {
                    float nx = px[i];
                    float ny = py[i];
                    float nd = pd[i];
                    if (explode > 0.001f) {
                        V3 node = explodePoint(verts[i], 0x63000u + static_cast<uint32_t>(i * 17u));
                        project(node, nx, ny, nd);
                    }
                    float front = clamp(nd * 0.5f + 0.5f, 0.f, 1.f);
                    float r = 0.75f + pix * 0.22f + front * 0.42f;
                    NVGcolor c = blendColor(tronCyan, tronHighlight, front);
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, nx - r * 0.5f, ny - r * 0.5f, r, r);
                    nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.16f + front * 0.44f));
                    nvgFill(args.vg);
                }
            }
        };

        if (variantId == 0) {
            // Octahedron
            std::vector<V3> verts = {
                {0.f, 0.95f, 0.f}, {0.f, -0.95f, 0.f},
                {-0.95f, 0.f, 0.f}, {0.95f, 0.f, 0.f},
                {0.f, 0.f, -0.95f}, {0.f, 0.f, 0.95f}
            };
            std::vector<std::pair<int, int>> edges = {
                {0,2},{0,3},{0,4},{0,5},
                {1,2},{1,3},{1,4},{1,5},
                {2,4},{4,3},{3,5},{5,2}
            };
            drawMesh(verts, edges, true);
        } else if (variantId == 1) {
            // Tetrahedron
            std::vector<V3> verts = {
                {0.f, 0.98f, 0.f},
                {-0.90f, -0.58f, -0.52f},
                {0.90f, -0.58f, -0.52f},
                {0.f, -0.58f, 0.92f}
            };
            std::vector<std::pair<int, int>> edges = {{0,1},{0,2},{0,3},{1,2},{2,3},{3,1}};
            drawMesh(verts, edges, true);
        } else if (variantId == 2) {
            // Triangular prism
            std::vector<V3> verts = {
                {-0.75f, 0.70f, -0.55f}, {0.75f, 0.70f, -0.55f}, {0.f, 0.70f, 0.78f},
                {-0.75f, -0.70f, -0.55f}, {0.75f, -0.70f, -0.55f}, {0.f, -0.70f, 0.78f}
            };
            std::vector<std::pair<int, int>> edges = {
                {0,1},{1,2},{2,0},
                {3,4},{4,5},{5,3},
                {0,3},{1,4},{2,5}
            };
            drawMesh(verts, edges, true);
        } else if (variantId == 3) {
            // Cone
            int ring = 14;
            V3 apex = {0.f, 1.0f, 0.f};
            std::vector<V3> verts;
            verts.push_back(apex);
            for (int i = 0; i < ring; ++i) {
                float a0 = (static_cast<float>(i) / static_cast<float>(ring)) * NocturneTV::TAU;
                verts.push_back({std::cos(a0) * 0.92f, -0.82f, std::sin(a0) * 0.92f});
            }
            std::vector<std::pair<int, int>> edges;
            for (int i = 0; i < ring; ++i) {
                int j = (i + 1) % ring;
                edges.push_back({1 + i, 1 + j});
                if ((i % 2) == 0) {
                    edges.push_back({0, 1 + i});
                }
            }
            drawMesh(verts, edges, true);
        } else if (variantId == 4) {
            // Cylinder
            int ring = 14;
            std::vector<V3> verts;
            for (int i = 0; i < ring; ++i) {
                float a0 = (static_cast<float>(i) / static_cast<float>(ring)) * NocturneTV::TAU;
                verts.push_back({std::cos(a0) * 0.84f, 0.78f, std::sin(a0) * 0.84f});
            }
            for (int i = 0; i < ring; ++i) {
                float a0 = (static_cast<float>(i) / static_cast<float>(ring)) * NocturneTV::TAU;
                verts.push_back({std::cos(a0) * 0.84f, -0.78f, std::sin(a0) * 0.84f});
            }
            std::vector<std::pair<int, int>> edges;
            for (int i = 0; i < ring; ++i) {
                int j = (i + 1) % ring;
                edges.push_back({i, j});
                edges.push_back({ring + i, ring + j});
                if ((i % 2) == 0) {
                    edges.push_back({i, ring + i});
                }
            }
            drawMesh(verts, edges, false);
        } else if (variantId == 5) {
            // Torus wire
            int major = 11;
            int minor = 14;
            auto torusPoint = [&](float u, float v) -> V3 {
                float R = 0.62f;
                float r = 0.28f;
                float cu = std::cos(u), su = std::sin(u);
                float cv = std::cos(v), sv = std::sin(v);
                return {(R + r * cv) * cu, r * sv, (R + r * cv) * su};
            };
            for (int i = 0; i < major; ++i) {
                float u = (static_cast<float>(i) / static_cast<float>(major)) * NocturneTV::TAU;
                for (int s = 0; s < minor; ++s) {
                    float v0 = (static_cast<float>(s) / static_cast<float>(minor)) * NocturneTV::TAU;
                    float v1 = (static_cast<float>(s + 1) / static_cast<float>(minor)) * NocturneTV::TAU;
                    V3 p0 = torusPoint(u, v0);
                    V3 p1 = torusPoint(u, v1);
                    float x0, y0, d0, x1, y1, d1;
                    projectEdgeEndpoints(p0, p1, x0, y0, d0, x1, y1, d1);
                    drawProjectedEdge(x0, y0, d0, x1, y1, d1, static_cast<float>(i) / static_cast<float>(major));
                }
            }
            for (int j = 0; j < 7; ++j) {
                float v = (static_cast<float>(j) / 7.f) * NocturneTV::TAU;
                for (int s = 0; s < major; ++s) {
                    float u0 = (static_cast<float>(s) / static_cast<float>(major)) * NocturneTV::TAU;
                    float u1 = (static_cast<float>(s + 1) / static_cast<float>(major)) * NocturneTV::TAU;
                    V3 p0 = torusPoint(u0, v);
                    V3 p1 = torusPoint(u1, v);
                    float x0, y0, d0, x1, y1, d1;
                    projectEdgeEndpoints(p0, p1, x0, y0, d0, x1, y1, d1);
                    drawProjectedEdge(x0, y0, d0, x1, y1, d1, static_cast<float>(j) / 6.f);
                }
            }
        } else if (variantId == 6) {
            // Double helix
            int seg = 72;
            for (int hix = 0; hix < 2; ++hix) {
                for (int s = 0; s < seg; ++s) {
                    float u0 = static_cast<float>(s) / static_cast<float>(seg);
                    float u1 = static_cast<float>(s + 1) / static_cast<float>(seg);
                    float ph = hix == 0 ? 0.f : NocturneTV::TAU * 0.5f;
                    V3 p0 = {std::cos(u0 * NocturneTV::TAU * 2.f + ph) * 0.60f, (u0 - 0.5f) * 1.7f, std::sin(u0 * NocturneTV::TAU * 2.f + ph) * 0.60f};
                    V3 p1 = {std::cos(u1 * NocturneTV::TAU * 2.f + ph) * 0.60f, (u1 - 0.5f) * 1.7f, std::sin(u1 * NocturneTV::TAU * 2.f + ph) * 0.60f};
                    float x0, y0, d0, x1, y1, d1;
                    projectEdgeEndpoints(p0, p1, x0, y0, d0, x1, y1, d1);
                    drawProjectedEdge(x0, y0, d0, x1, y1, d1, hix == 0 ? 0.2f : 0.8f);
                }
            }
            for (int s = 0; s < seg; s += 4) {
                float u = static_cast<float>(s) / static_cast<float>(seg);
                V3 p0 = {std::cos(u * NocturneTV::TAU * 2.f) * 0.60f, (u - 0.5f) * 1.7f, std::sin(u * NocturneTV::TAU * 2.f) * 0.60f};
                V3 p1 = {std::cos(u * NocturneTV::TAU * 2.f + NocturneTV::TAU * 0.5f) * 0.60f, (u - 0.5f) * 1.7f, std::sin(u * NocturneTV::TAU * 2.f + NocturneTV::TAU * 0.5f) * 0.60f};
                float x0, y0, d0, x1, y1, d1;
                projectEdgeEndpoints(p0, p1, x0, y0, d0, x1, y1, d1);
                drawProjectedEdge(x0, y0, d0, x1, y1, d1, 0.5f);
            }
        } else if (variantId == 7) {
            // Lissajous knot
            int seg = 96;
            for (int s = 0; s < seg; ++s) {
                float u0 = (static_cast<float>(s) / static_cast<float>(seg)) * NocturneTV::TAU;
                float u1 = (static_cast<float>(s + 1) / static_cast<float>(seg)) * NocturneTV::TAU;
                V3 p0 = {std::sin(u0 * 3.0f) * 0.78f, std::sin(u0 * 2.0f + 0.55f) * 0.62f, std::sin(u0 * 5.0f + 1.2f) * 0.58f};
                V3 p1 = {std::sin(u1 * 3.0f) * 0.78f, std::sin(u1 * 2.0f + 0.55f) * 0.62f, std::sin(u1 * 5.0f + 1.2f) * 0.58f};
                float x0, y0, d0, x1, y1, d1;
                projectEdgeEndpoints(p0, p1, x0, y0, d0, x1, y1, d1);
                drawProjectedEdge(x0, y0, d0, x1, y1, d1, static_cast<float>(s) / static_cast<float>(seg));
            }
        } else if (variantId == 8) {
            // Crown cage: dual staggered rings with top/bottom hubs.
            std::vector<V3> verts;
            verts.push_back({0.f, 1.0f, 0.f});   // 0 top hub
            verts.push_back({0.f, -1.0f, 0.f});  // 1 bottom hub
            const int ringCount = 6;
            for (int i = 0; i < ringCount; ++i) {
                float a0 = (static_cast<float>(i) / static_cast<float>(ringCount)) * NocturneTV::TAU;
                verts.push_back({std::cos(a0) * 0.78f, 0.34f, std::sin(a0) * 0.78f}); // 2..7
            }
            for (int i = 0; i < ringCount; ++i) {
                float a0 = (static_cast<float>(i) / static_cast<float>(ringCount)) * NocturneTV::TAU + NocturneTV::TAU / 12.f;
                verts.push_back({std::cos(a0) * 0.78f, -0.34f, std::sin(a0) * 0.78f}); // 8..13
            }
            std::vector<std::pair<int, int>> edges;
            for (int i = 0; i < ringCount; ++i) {
                int up = 2 + i;
                int upN = 2 + (i + 1) % ringCount;
                int lo = 2 + ringCount + i;
                int loN = 2 + ringCount + (i + 1) % ringCount;
                edges.push_back({0, up});
                edges.push_back({1, lo});
                edges.push_back({up, upN});
                edges.push_back({lo, loN});
                edges.push_back({up, lo});
                edges.push_back({up, loN});
            }
            drawMesh(verts, edges, false);
        } else {
            // Wavy panel lattice
            const int gx = 7;
            const int gy = 5;
            std::vector<V3> verts;
            verts.reserve(gx * gy);
            for (int y = 0; y < gy; ++y) {
                for (int x = 0; x < gx; ++x) {
                    float fx = (static_cast<float>(x) / static_cast<float>(gx - 1) - 0.5f) * 1.8f;
                    float fy = (static_cast<float>(y) / static_cast<float>(gy - 1) - 0.5f) * 1.3f;
                    float z = std::sin(fx * 3.4f + t * 0.55f) * 0.24f
                            + std::cos(fy * 4.1f - t * 0.47f) * 0.16f;
                    verts.push_back({fx, fy, z});
                }
            }
            std::vector<std::pair<int, int>> edges;
            for (int y = 0; y < gy; ++y) {
                for (int x = 0; x < gx; ++x) {
                    int i = y * gx + x;
                    if (x + 1 < gx) {
                        edges.push_back({i, i + 1});
                    }
                    if (y + 1 < gy) {
                        edges.push_back({i, i + gx});
                    }
                    if (x + 1 < gx && y + 1 < gy && ((x + y) % 2 == 0)) {
                        edges.push_back({i, i + gx + 1});
                    }
                }
            }
            drawMesh(verts, edges, false);
        }

        nvgRestore(args.vg);
    }

    void drawShapeGlyph2D(const DrawArgs& args, int shapeId, float cx, float cy, float size, float t,
                          const NVGcolor& a, const NVGcolor& b) {
        NVGcolor c0 = blendColor(a, b, 0.25f + 0.55f * (0.5f + 0.5f * std::sin(t + shapeId * 0.71f)));
        NVGcolor c1 = blendColor(b, a, 0.35f + 0.45f * (0.5f + 0.5f * std::cos(t * 0.9f + shapeId * 0.61f)));
        auto stroke = [&](float width, float alpha) {
            nvgStrokeWidth(args.vg, width + 0.65f);
            nvgStrokeColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, std::min(0.90f, 0.56f + alpha * 0.42f)));
            nvgStroke(args.vg);
            nvgStrokeWidth(args.vg, width);
            nvgStrokeColor(args.vg, nvgRGBAf(c0.r, c0.g, c0.b, alpha));
            nvgStroke(args.vg);
        };

        if (shapeId == 0) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx, cy - size);
            nvgLineTo(args.vg, cx - size * 0.86f, cy + size * 0.72f);
            nvgLineTo(args.vg, cx + size * 0.86f, cy + size * 0.72f);
            nvgClosePath(args.vg);
            stroke(1.2f, 0.78f);
        } else if (shapeId == 1) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, cx - size * 0.8f, cy - size * 0.8f, size * 1.6f, size * 1.6f);
            stroke(1.2f, 0.78f);
        } else if (shapeId == 2) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx, cy, size * 0.85f);
            stroke(1.2f, 0.78f);
        } else if (shapeId == 3) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx, cy - size);
            nvgLineTo(args.vg, cx + size, cy);
            nvgLineTo(args.vg, cx, cy + size);
            nvgLineTo(args.vg, cx - size, cy);
            nvgClosePath(args.vg);
            stroke(1.2f, 0.78f);
        } else if (shapeId == 4) {
            // Tetra glyph: triangle with inner wireframe
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx - size * 0.85f, cy + size * 0.72f);
            nvgLineTo(args.vg, cx + size * 0.85f, cy + size * 0.72f);
            nvgLineTo(args.vg, cx, cy - size);
            nvgClosePath(args.vg);
            stroke(1.2f, 0.78f);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx, cy - size);
            nvgLineTo(args.vg, cx, cy + size * 0.28f);
            nvgLineTo(args.vg, cx - size * 0.42f, cy + size * 0.72f);
            nvgMoveTo(args.vg, cx, cy + size * 0.28f);
            nvgLineTo(args.vg, cx + size * 0.42f, cy + size * 0.72f);
            nvgStrokeWidth(args.vg, 0.85f);
            nvgStrokeColor(args.vg, nvgRGBAf(c1.r, c1.g, c1.b, 0.62f));
            nvgStroke(args.vg);
        } else if (shapeId == 5) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx - size * 0.9f, cy);
            nvgLineTo(args.vg, cx - size * 0.45f, cy - size * 0.78f);
            nvgLineTo(args.vg, cx + size * 0.45f, cy - size * 0.78f);
            nvgLineTo(args.vg, cx + size * 0.9f, cy);
            nvgLineTo(args.vg, cx + size * 0.45f, cy + size * 0.78f);
            nvgLineTo(args.vg, cx - size * 0.45f, cy + size * 0.78f);
            nvgClosePath(args.vg);
            stroke(1.1f, 0.74f);
        } else if (shapeId == 6) {
            nvgBeginPath(args.vg);
            for (int i = 0; i < 26; ++i) {
                float u = static_cast<float>(i) / 25.f;
                float x = cx + (u - 0.5f) * size * 1.8f;
                float y = cy + std::sin(u * NocturneTV::TAU * 1.4f + t * 0.8f) * size * 0.45f;
                if (i == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);
            }
            stroke(1.0f, 0.74f);
        } else if (shapeId == 7) {
            nvgBeginPath(args.vg);
            nvgEllipse(args.vg, cx, cy, size * 0.9f, size * 0.55f);
            stroke(1.2f, 0.76f);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx - size * 0.9f, cy);
            nvgLineTo(args.vg, cx + size * 0.9f, cy);
            stroke(0.9f, 0.58f);
        } else if (shapeId == 8) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx, cy, size * 0.88f);
            stroke(1.1f, 0.72f);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cx, cy, size * 0.46f);
            nvgStrokeWidth(args.vg, 0.9f);
            nvgStrokeColor(args.vg, nvgRGBAf(c1.r, c1.g, c1.b, 0.62f));
            nvgStroke(args.vg);
        } else if (shapeId == 9) {
            nvgBeginPath(args.vg);
            for (int i = 0; i < 30; ++i) {
                float u = static_cast<float>(i) / 29.f;
                float ph = u * NocturneTV::TAU * 2.f;
                float x = cx + std::sin(ph) * size * 0.85f;
                float y = cy + std::sin(ph * 2.f + 0.7f) * size * 0.55f;
                if (i == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);
            }
            stroke(1.0f, 0.74f);
        } else if (shapeId == 10) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, cx - size * 0.85f, cy - size * 0.85f, size * 1.7f, size * 1.7f);
            stroke(1.0f, 0.72f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, cx - size * 0.48f, cy - size * 0.48f, size * 0.96f, size * 0.96f);
            nvgStrokeWidth(args.vg, 0.9f);
            nvgStrokeColor(args.vg, nvgRGBAf(c1.r, c1.g, c1.b, 0.64f));
            nvgStroke(args.vg);
        } else if (shapeId == 11) {
            // Hourglass glyph
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx - size * 0.86f, cy - size * 0.76f);
            nvgLineTo(args.vg, cx + size * 0.86f, cy - size * 0.76f);
            nvgLineTo(args.vg, cx, cy);
            nvgClosePath(args.vg);
            stroke(1.0f, 0.74f);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx - size * 0.86f, cy + size * 0.76f);
            nvgLineTo(args.vg, cx + size * 0.86f, cy + size * 0.76f);
            nvgLineTo(args.vg, cx, cy);
            nvgClosePath(args.vg);
            stroke(1.0f, 0.74f);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, cx - size * 0.86f, cy - size * 0.76f);
            nvgLineTo(args.vg, cx + size * 0.86f, cy + size * 0.76f);
            nvgMoveTo(args.vg, cx + size * 0.86f, cy - size * 0.76f);
            nvgLineTo(args.vg, cx - size * 0.86f, cy + size * 0.76f);
            nvgStrokeWidth(args.vg, 0.75f);
            nvgStrokeColor(args.vg, nvgRGBAf(c1.r, c1.g, c1.b, 0.46f));
            nvgStroke(args.vg);
        } else {
            nvgBeginPath(args.vg);
            for (int i = 0; i < 12; ++i) {
                float u = static_cast<float>(i) / 11.f;
                float a0 = u * NocturneTV::TAU;
                float rr = (i % 2 == 0) ? size : size * 0.42f;
                float x = cx + std::cos(a0) * rr;
                float y = cy + std::sin(a0) * rr;
                if (i == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);
            }
            nvgClosePath(args.vg);
            stroke(1.0f, 0.74f);
        }
    }

    void drawShapePartyRoom(const DrawArgs& args, float w, float h, float t, float noise, float chaos,
                            float explode, const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);

        float roomX = w * 0.10f;
        float roomY = h * 0.12f;
        float roomW = w * 0.80f;
        float roomH = h * 0.72f;
        float roomCx = roomX + roomW * 0.5f;
        float roomCy = roomY + roomH * 0.5f;
        float explodeAmount = clamp(explode, 0.f, 1.f);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, roomX, roomY, roomW, roomH, 6.f);
        nvgStrokeWidth(args.vg, 1.3f);
        nvgStrokeColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, 0.45f + chaos * 0.20f));
        nvgStroke(args.vg);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, roomX + 1.0f, roomY + 1.0f, roomW - 2.0f, roomH - 2.0f, 5.f);
        nvgStrokeWidth(args.vg, 0.95f);
        nvgStrokeColor(args.vg, nvgRGBAf(a.r, a.g, a.b, 0.34f + noise * 0.16f));
        nvgStroke(args.vg);

        auto pingpong = [](float x) {
            float f = std::fmod(x, 2.f);
            if (f < 0.f) {
                f += 2.f;
            }
            return (f <= 1.f) ? f : (2.f - f);
        };

        float pad = 8.f;
        float rangeX = std::max(1.f, roomW - 2.f * pad);
        float rangeY = std::max(1.f, roomH - 2.f * pad);
        for (int shapeId = 0; shapeId < 13; ++shapeId) {
            float vx = 0.11f + 0.03f * static_cast<float>((shapeId * 3) % 5);
            float vy = 0.13f + 0.025f * static_cast<float>((shapeId * 5) % 4);
            float phx = static_cast<float>(shapeId) * 0.37f + (shapeId % 3) * 0.21f;
            float phy = static_cast<float>(shapeId) * 0.29f + (shapeId % 4) * 0.18f;
            float x = roomX + pad + pingpong(t * vx + phx) * rangeX;
            float y = roomY + pad + pingpong(t * vy + phy) * rangeY;
            float sz = 3.2f + static_cast<float>(shapeId % 4) * 0.9f + noise * 0.8f;

            float dx = x - roomCx;
            float dy = y - roomCy;
            float radialScale = 1.f + explodeAmount * (1.35f + 0.25f * std::sin(t * 0.7f + shapeId * 0.6f));
            x = roomCx + dx * radialScale;
            y = roomCy + dy * radialScale;
            sz *= 1.f + explodeAmount * (1.2f + 0.15f * static_cast<float>(shapeId % 3));

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, x, y, sz * 1.5f);
            nvgFillColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, 0.10f));
            nvgFill(args.vg);

            drawShapeGlyph2D(args, shapeId, x, y, sz, t * (0.9f + chaos * 0.4f), a, b);
        }

        nvgRestore(args.vg);
    }

    void drawHauntedCRTOverlay(const DrawArgs& args, float w, float h, float t, float sceneNorm,
                               float warp, float noise, float hold, float drift, float chaos,
                               const std::array<float, 4>& sigRaw, const std::array<float, 4>& sigEnv,
                               const NVGcolor& a, const NVGcolor& b) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);

        int apparitions = 2 + static_cast<int>(sceneNorm * 3.f + chaos * 3.f);
        float baseR = std::min(w, h) * (0.06f + sceneNorm * 0.04f);
        for (int i = 0; i < apparitions; ++i) {
            float fi = static_cast<float>(i) / static_cast<float>(std::max(1, apparitions - 1));
            float ph = t * (0.12f + fi * 0.18f + drift * 0.32f) + fi * 3.4f + sigRaw[3] * 1.2f;
            float gx = w * (0.24f + 0.52f * (0.5f + 0.5f * std::sin(ph)));
            float gy = h * (0.18f + 0.56f * (0.5f + 0.5f * std::cos(ph * 0.77f + sigRaw[1] * 1.8f)));
            float rx = baseR * (1.0f + fi * 1.4f + sigEnv[0] * 0.9f);
            float ry = rx * (1.3f + hold * 0.35f);
            NVGcolor c = blendColor(a, b, 0.25f + fi * 0.55f);

            nvgBeginPath(args.vg);
            nvgEllipse(args.vg, gx, gy, rx, ry);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.018f + chaos * 0.09f + sigEnv[3] * 0.05f));
            nvgFill(args.vg);

            for (int s = 0; s < 3; ++s) {
                float smearY = gy + static_cast<float>(s) * (1.2f + hold * 2.0f);
                nvgBeginPath(args.vg);
                nvgEllipse(args.vg, gx + std::sin(ph * 2.2f + s) * (1.2f + warp * 4.f), smearY,
                           rx * (0.88f + s * 0.09f), ry * (0.86f + s * 0.08f));
                nvgStrokeWidth(args.vg, 0.55f + s * 0.25f);
                nvgStrokeColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.02f + sigEnv[2] * 0.045f));
                nvgStroke(args.vg);
            }
        }

        float ritualPulse = 0.5f + 0.5f * std::sin(t * (0.6f + chaos * 1.6f) + sigRaw[2] * 2.8f);
        float flash = clamp((ritualPulse - (0.86f - chaos * 0.22f)) * 4.8f, 0.f, 1.f);
        if (flash > 0.001f) {
            NVGcolor c = blendColor(nvgRGBAf(0.78f, 0.98f, 0.86f, 1.f), a, 0.65f);
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 10.f);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, flash * (0.028f + chaos * 0.05f)));
            nvgFill(args.vg);
        }

        // Faint vertical "haunted phosphor memory" curtains.
        int curtains = 4 + static_cast<int>(chaos * 8.f + noise * 6.f);
        for (int i = 0; i < curtains; ++i) {
            float fx = (static_cast<float>(i) + 0.5f) / static_cast<float>(curtains);
            float x = fx * w + std::sin(t * 0.5f + fx * 9.3f + sigRaw[0] * 2.6f) * (1.0f + warp * 5.f);
            float cw = 1.2f + std::fabs(std::sin(fx * 13.0f + t * 1.6f)) * (2.0f + chaos * 5.f);
            NVGcolor c = blendColor(a, b, fx);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, x, 0.f, cw, h);
            nvgFillColor(args.vg, nvgRGBAf(c.r, c.g, c.b, 0.012f + chaos * 0.03f));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    void draw(const DrawArgs& args) override {
        float w = box.size.x;
        float h = box.size.y;
        float radius = 10.f;

        float dt = 1.f / 60.f;
        float monitorHz = 60.f;
        float uiFrameHz = 60.f;
        if (APP && APP->window) {
            double m = APP->window->getMonitorRefreshRate();
            if (std::isfinite(m) && m > 1.0) {
                monitorHz = static_cast<float>(m);
            }
            double lastFrame = APP->window->getLastFrameDuration();
            if (std::isfinite(lastFrame) && lastFrame > 1e-4) {
                dt = clamp(static_cast<float>(lastFrame), 1.f / 360.f, 0.1f);
                uiFrameHz = 1.f / dt;
            }
        }

        float refreshHz = 18.f;
        const float uiDrawableHz = std::max(1.f, std::min(monitorHz, uiFrameHz));
        float dynamicMaxRefresh = clamp(uiDrawableHz, NocturneTV::REFRESH_MIN_HZ, NocturneTV::REFRESH_MAX_HZ);
        if (module) {
            refreshHz = clamp(module->uiRefreshHz.load(std::memory_order_relaxed),
                              NocturneTV::REFRESH_MIN_HZ,
                              dynamicMaxRefresh);
        }

        snapshotTimer += dt;
        bool shouldSnapshot = !snapshotReady;
        if (!shouldSnapshot) {
            bool refreshAtFrameRate = refreshHz >= uiDrawableHz * 0.98f;
            if (refreshAtFrameRate) {
                shouldSnapshot = true;
                snapshotTimer = 0.f;
            } else {
                float refreshInterval = 1.f / refreshHz;
                while (snapshotTimer >= refreshInterval) {
                    snapshotTimer -= refreshInterval;
                    shouldSnapshot = true;
                }
            }
        }

        if (shouldSnapshot && module) {
            snapshotWarp = module->uiWarp.load(std::memory_order_relaxed);
            snapshotNoise = module->uiNoise.load(std::memory_order_relaxed);
            snapshotTear = module->uiTear.load(std::memory_order_relaxed);
            snapshotDrift = module->uiDrift.load(std::memory_order_relaxed);
            snapshotTint = module->uiTint.load(std::memory_order_relaxed);
            snapshotSignalLevel = module->uiSignalLevel.load(std::memory_order_relaxed);
            snapshotTime = module->uiClock.load(std::memory_order_relaxed);
            snapshotMode = module->uiMode.load(std::memory_order_relaxed);
            snapshotChaosGate = module->uiChaosGate.load(std::memory_order_relaxed);
            snapshotSpinCv = module->uiSpinCv.load(std::memory_order_relaxed);
            snapshotExplode = module->uiExplode.load(std::memory_order_relaxed);
            snapshotDarkness = module->uiDarkness.load(std::memory_order_relaxed);
            snapshotFill = module->uiFill.load(std::memory_order_relaxed);
            snapshotConnectedMask = module->uiConnectedMask.load(std::memory_order_relaxed);
            snapshotSceneIndex = module->uiSceneIndex.load(std::memory_order_relaxed);
            for (int i = 0; i < 4; ++i) {
                snapshotSignalRaw[i] = module->uiSignalRaw[i].load(std::memory_order_relaxed);
                snapshotSignalEnv[i] = module->uiSignalEnv[i].load(std::memory_order_relaxed);
            }

            if (!snapshotReady || displayedScene != snapshotSceneIndex) {
                displayedScene = snapshotSceneIndex;
                sceneChangeTimer = 0.9f;
            }
            snapshotReady = true;
        }
        sceneChangeTimer = std::max(0.f, sceneChangeTimer - dt);

        float warp = snapshotWarp;
        float noise = snapshotNoise;
        float tear = snapshotTear;
        float drift = snapshotDrift;
        float tint = snapshotTint;
        float t = snapshotTime;
        float signalLevel = snapshotSignalLevel;
        int mode = snapshotMode;
        float chaosGate = snapshotChaosGate;
        float explode = snapshotExplode;
        float darkness = snapshotDarkness;
        float fill = snapshotFill;
        int connectedMask = snapshotConnectedMask;
        int programIndex = clamp(snapshotSceneIndex, 0, NocturneTV::SCENE_STEP_COUNT - 1);
        int programBand = programIndex / 2;
        float sceneNorm = static_cast<float>(snapshotSceneIndex) / static_cast<float>(NocturneTV::SCENE_STEP_COUNT - 1);
        float spinRate = (0.10f + drift * 0.05f) * (1.f + snapshotSpinCv * 1.6f + snapshotSignalEnv[0] * 0.6f);
        spinRate = clamp(spinRate, 0.04f, 2.40f);
        float sphereTime = t * spinRate + 1.3f;

        // Chaos can quantize temporal motion for unstable "video hold" behavior.
        float stutterStrength = clamp((chaosGate - 0.15f) * 1.35f + snapshotSignalEnv[1] * 0.30f, 0.f, 1.f);
        if (stutterStrength > 0.001f) {
            float stepHz = 7.f + stutterStrength * 72.f + snapshotSignalEnv[3] * 38.f;
            float tStep = 1.f / std::max(1.f, stepHz);
            t = std::floor(t / tStep) * tStep;
            t += std::sin(snapshotTime * (17.f + snapshotSignalEnv[0] * 24.f)) * (0.001f + stutterStrength * 0.012f);
        }

        float programTint = std::sin(sceneNorm * NocturneTV::TAU * 3.f + t * 0.22f) * (0.03f + chaosGate * 0.09f);
        float retroTintBias = (programBand % 3 == 0) ? -0.10f : (programBand % 3 == 1 ? 0.05f : 0.14f);
        float spookyTintBias = (programBand >= 5) ? -0.06f : 0.f;
        float tintPrimary = clamp(tint + programTint + retroTintBias + spookyTintBias + snapshotSignalRaw[2] * 0.04f, 0.f, 1.f);
        float tintSecondary = clamp(tint + snapshotSignalRaw[3] * 0.22f - programTint * 0.6f + retroTintBias * 0.6f, 0.f, 1.f);
        NVGcolor primary = tintTrace(tintPrimary, false);
        NVGcolor secondary = tintTrace(tintSecondary, true);

        // Additional scene family palette push for stronger 80s/CRT identities.
        if (programBand % 2 == 0) {
            NVGcolor synthPink = nvgRGBAf(0.97f, 0.26f, 0.66f, 1.f);
            NVGcolor synthCyan = nvgRGBAf(0.22f, 0.88f, 0.96f, 1.f);
            primary = blendColor(primary, synthPink, 0.08f + sceneNorm * 0.12f);
            secondary = blendColor(secondary, synthCyan, 0.12f + sceneNorm * 0.10f);
        } else {
            NVGcolor phosphorGreen = nvgRGBAf(0.54f, 0.90f, 0.58f, 1.f);
            NVGcolor tubeAmber = nvgRGBAf(0.93f, 0.79f, 0.42f, 1.f);
            primary = blendColor(primary, phosphorGreen, 0.06f + chaosGate * 0.10f);
            secondary = blendColor(secondary, tubeAmber, 0.08f + chaosGate * 0.12f);
        }

        if (darkness > 0.001f) {
            primary = blendColor(primary, nvgRGBAf(0.08f, 0.14f, 0.12f, 1.f), darkness * 0.72f);
            secondary = blendColor(secondary, nvgRGBAf(0.10f, 0.08f, 0.16f, 1.f), darkness * 0.72f);
        }

        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
        nvgFillColor(args.vg, nvgRGBA(4, 6, 5, 255));
        nvgFill(args.vg);

        nvgScissor(args.vg, 2.f, 2.f, w - 4.f, h - 4.f);

        NVGpaint wash = nvgLinearGradient(args.vg, 0.f, 0.f, w, h,
                                          nvgRGBA(0, 154, 122, 16), nvgRGBA(111, 31, 183, 14));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.f, h - 1.f, radius - 0.5f);
        nvgFillPaint(args.vg, wash);
        nvgFill(args.vg);

        NVGpaint bulge = nvgRadialGradient(args.vg, w * 0.48f, h * 0.44f,
                                           std::min(w, h) * 0.10f, std::min(w, h) * 0.86f,
                                           nvgRGBA(230, 230, 240, 18), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.f, h - 1.f, radius - 0.5f);
        nvgFillPaint(args.vg, bulge);
        nvgFill(args.vg);

        NVGpaint edgeBowl = nvgRadialGradient(args.vg, w * 0.5f, h * 0.5f,
                                              std::min(w, h) * 0.52f, std::min(w, h) * 0.92f,
                                              nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 56));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
        nvgFillPaint(args.vg, edgeBowl);
        nvgFill(args.vg);

        NVGpaint tubeSheen = nvgLinearGradient(args.vg,
                                               w * 0.14f, h * 0.08f,
                                               w * 0.60f, h * 0.42f,
                                               nvgRGBA(255, 255, 255, 22), nvgRGBA(255, 255, 255, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.f, 1.f, w - 2.f, h - 2.f, radius - 1.f);
        nvgFillPaint(args.vg, tubeSheen);
        nvgFill(args.vg);

        if (mode == 0) {
            drawSyncEngine(args, w, h, t, sceneNorm, warp, noise, tear, drift, snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
        } else if (mode == 1) {
            drawKeyerEngine(args, w, h, t, sceneNorm, warp, noise, tear, drift, snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
        } else if (mode == 2) {
            drawFeedbackEngine(args, w, h, t, sceneNorm, warp, noise, tear, drift, snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
        } else {
            drawGlitchEngine(args, w, h, t, sceneNorm, warp, noise, tear, snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
        }

        // Program-dependent composite layers to push each program into a distinct visual territory.
        float latticeAlpha = clamp(0.13f + static_cast<float>(programBand) * 0.05f + chaosGate * 0.22f + snapshotSignalEnv[0] * 0.12f, 0.f, 0.82f) * 0.62f;
        if (latticeAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, latticeAlpha);
            drawInterferenceLattice(args, w, h, t * (1.0f + programBand * 0.05f), sceneNorm, warp, noise, tear, drift,
                                    snapshotSignalRaw, snapshotSignalEnv, secondary, primary);
            nvgRestore(args.vg);
        }

        float burstAlpha = clamp(0.11f + ((programBand + mode) % 2 == 0 ? 0.21f : 0.06f) + chaosGate * 0.19f + snapshotSignalEnv[2] * 0.19f, 0.f, 0.84f) * 0.58f;
        if (burstAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, burstAlpha);
            drawBurstOverlay(args, w, h, t * (1.08f + snapshotSignalEnv[3] * 0.3f), sceneNorm, warp, noise, tear, drift,
                             snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            nvgRestore(args.vg);
        }

        if (programBand >= 4 || chaosGate > 0.72f) {
            float ghostGlitchAlpha = clamp(0.07f + (static_cast<float>(programBand) - 3.f) * 0.04f + snapshotSignalEnv[3] * 0.20f, 0.f, 0.55f) * 0.62f;
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, ghostGlitchAlpha);
            drawGlitchEngine(args, w, h, t * 1.13f + 7.3f, sceneNorm,
                             warp * 0.55f + 0.10f, noise * 0.70f + 0.06f, tear * 0.65f + 0.08f,
                             snapshotSignalRaw, snapshotSignalEnv, secondary, primary);
            nvgRestore(args.vg);
        }

        // VHS transport personality layer.
        float vhsAlpha = clamp(0.20f + noise * 0.52f + tear * 0.24f + chaosGate * 0.18f, 0.f, 0.96f) * 0.52f;
        if (vhsAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, vhsAlpha);
            drawVHSTapeArtifacts(args, w, h, t * (1.f + drift * 0.35f), sceneNorm, warp, noise, tear, drift,
                                 snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            nvgRestore(args.vg);
        }

        // 80s synthwave horizon/grid appears strongly in selected program families.
        if ((programBand % 2 == 0) || mode == 2) {
            float synthAlpha = clamp(0.06f + sceneNorm * 0.16f + snapshotSignalEnv[2] * 0.14f + chaosGate * 0.08f, 0.f, 0.55f) * 0.70f;
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, synthAlpha);
            drawSynthwaveHorizon(args, w, h, t * 0.82f, sceneNorm, warp, noise, tear, drift,
                                 snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            nvgRestore(args.vg);
        }

        bool usePyramid = (programIndex == 0);
        bool useCube = (programIndex == 1);
        bool useSphere = (programIndex == 2);
        bool usePartyRoom = (programIndex == NocturneTV::SCENE_STEP_COUNT - 1);
        int variantId = clamp(programIndex - 3, 0, 9);
        float explodeScale = 1.f + explode * (0.85f + chaosGate * 0.35f);
        auto drawProgramShape = [&](float localTime) {
            if (usePartyRoom) {
                drawShapePartyRoom(args, w, h, localTime, noise, chaosGate, explode, primary, secondary);
                return;
            }

            bool applyExplodeScale = explodeScale > 1.0001f;
            if (applyExplodeScale) {
                nvgSave(args.vg);
                nvgTranslate(args.vg, w * 0.5f, h * 0.5f);
                nvgScale(args.vg, explodeScale, explodeScale);
                nvgTranslate(args.vg, -w * 0.5f, -h * 0.5f);
            }

            if (usePyramid) {
                drawTronPyramid(args, w, h, localTime, sceneNorm, warp, noise, tear, drift, chaosGate, explode, fill,
                                snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            } else if (useCube) {
                drawTronCube(args, w, h, localTime, sceneNorm, warp, noise, tear, drift, chaosGate, explode, fill,
                             snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            } else if (useSphere) {
                drawTronSphere(args, w, h, localTime, sceneNorm, warp, noise, tear, drift, chaosGate, explode, fill,
                               snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            } else {
                drawTronVariantShape(args, variantId, w, h, localTime, sceneNorm, warp, noise, tear, drift, chaosGate, explode, fill,
                                     snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            }

            if (applyExplodeScale) {
                nvgRestore(args.vg);
            }
        };
        float shapeBlurMix = clamp(0.16f + noise * 0.26f + drift * 0.20f + tear * 0.14f + chaosGate * 0.16f, 0.f, 0.82f);
        auto drawProgramShapeBlur = [&](float localTime, float baseAlpha, float gain) {
            if (baseAlpha <= 0.01f || gain <= 0.01f || shapeBlurMix <= 0.01f) {
                return;
            }

            float blurRadiusPx = (0.70f + shapeBlurMix * (2.1f + warp * 1.1f)) * gain;
            float tapAlpha = baseAlpha * (0.16f + shapeBlurMix * 0.34f) * gain;
            const std::array<std::array<float, 2>, 4> taps = {{
                {{-1.f, 0.f}},
                {{1.f, 0.f}},
                {{0.f, -1.f}},
                {{0.f, 1.f}}
            }};

            for (size_t i = 0; i < taps.size(); ++i) {
                nvgSave(args.vg);
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                nvgGlobalAlpha(args.vg, tapAlpha);
                float timeSkew = localTime - 0.005f * static_cast<float>(i + 1);
                nvgTranslate(args.vg, taps[i][0] * blurRadiusPx, taps[i][1] * blurRadiusPx);
                drawProgramShape(timeSkew);
                nvgRestore(args.vg);
            }
        };
        float sphereAlpha = clamp(0.34f + sceneNorm * 0.24f + noise * 0.10f + snapshotSignalEnv[2] * 0.22f + chaosGate * 0.18f, 0.f, 1.00f);
        if (sphereAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, sphereAlpha);
            drawProgramShape(sphereTime);
            nvgRestore(args.vg);
        }
        drawProgramShapeBlur(sphereTime, sphereAlpha, 1.0f);

        // Spooky old-tube apparitions and phosphor ghosts.
        float hauntAlpha = clamp(0.06f + chaosGate * 0.36f + snapshotSignalEnv[3] * 0.17f + (programBand >= 5 ? 0.15f : 0.f) + darkness * 0.34f, 0.f, 0.92f) * 0.72f;
        if (hauntAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, hauntAlpha);
            drawHauntedCRTOverlay(args, w, h, t * 0.74f + 9.1f, sceneNorm, warp, noise, tear, drift, chaosGate,
                                  snapshotSignalRaw, snapshotSignalEnv, secondary, primary);
            nvgRestore(args.vg);
        }

        // Ghost-frame echo boosts motion and old phosphor persistence.
        float frameEchoAlpha = clamp(0.05f + drift * 0.18f + snapshotSignalEnv[0] * 0.10f + noise * 0.08f, 0.f, 0.42f) * 0.62f;
        if (frameEchoAlpha > 0.01f) {
            float dx = std::sin(t * 2.3f + snapshotSignalRaw[0] * 3.1f) * (0.6f + warp * 5.2f);
            float dy = std::cos(t * 2.0f + snapshotSignalRaw[1] * 2.6f) * (0.4f + tear * 3.8f);
            nvgSave(args.vg);
            nvgTranslate(args.vg, dx, dy);
            nvgGlobalAlpha(args.vg, frameEchoAlpha);
            drawInterferenceLattice(args, w, h, t * 1.1f + 2.2f, sceneNorm, warp * 0.6f + 0.1f, noise * 0.8f, tear * 0.7f, drift,
                                    snapshotSignalRaw, snapshotSignalEnv, secondary, primary);
            nvgRestore(args.vg);
        }

        float blurAlpha = clamp(0.16f + noise * 0.40f + drift * 0.24f + tear * 0.14f + chaosGate * 0.22f, 0.f, 0.82f) * 0.78f;
        if (blurAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, blurAlpha);
            drawPhosphorBleed(args, w, h, t * (1.0f + drift * 0.3f), sceneNorm, warp, noise, tear, drift, chaosGate,
                              snapshotSignalRaw, snapshotSignalEnv, primary, secondary);
            nvgRestore(args.vg);
        }

        float contrastCrush = clamp(0.06f + chaosGate * 0.08f + noise * 0.08f + snapshotSignalEnv[2] * 0.06f + darkness * 0.16f, 0.f, 0.42f);
        NVGpaint crush = nvgRadialGradient(args.vg, w * 0.5f, h * 0.5f,
                                           std::min(w, h) * 0.10f, std::min(w, h) * 0.95f,
                                           nvgRGBA(0, 0, 0, 0), nvgRGBAf(0.f, 0.f, 0.f, contrastCrush));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
        nvgFillPaint(args.vg, crush);
        nvgFill(args.vg);

        // Autonomous CRT flicker and tube breathing.
        float slowPulse = 0.5f + 0.5f * std::sin(t * 0.72f + std::sin(t * 0.19f) * 0.95f);
        float mediumFlicker = 0.5f + 0.5f * std::sin(t * 2.6f + 1.4f + snapshotSignalRaw[1] * 2.5f);
        float fastFlicker = 0.5f + 0.5f * std::sin(t * (8.0f + 4.0f * snapshotSignalEnv[3]) + std::sin(t * 1.7f) * 0.8f);
        float glowPulse = clamp(0.62f + slowPulse * 0.36f + mediumFlicker * 0.23f + fastFlicker * 0.18f + signalLevel * 0.28f, 0.f, 2.2f);
        float phosphorIntensity = (0.44f + glowPulse * 0.40f) * (1.f - darkness * 0.52f);
        shapetaker::graphics::drawPhosphorGlow(args, Vec(w * 0.5f, h * 0.5f), std::min(w, h) * 0.61f, primary,
                                               phosphorIntensity);

        NVGpaint bloom = nvgRadialGradient(args.vg, w * 0.5f, h * 0.5f,
                                           std::min(w, h) * 0.20f, std::min(w, h) * 0.92f,
                                           nvgRGBAf(primary.r, primary.g, primary.b, 0.34f + glowPulse * 0.24f),
                                           nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
        nvgFillPaint(args.vg, bloom);
        nvgFill(args.vg);

        float flickerWashAlpha = 0.046f + mediumFlicker * 0.068f + fastFlicker * 0.068f;
        NVGpaint flickerWash = nvgLinearGradient(args.vg,
                                                 0.f, h * 0.16f,
                                                 0.f, h * 0.92f,
                                                 nvgRGBAf(primary.r, primary.g, primary.b, flickerWashAlpha),
                                                 nvgRGBAf(0.91f, 0.88f, 0.78f, flickerWashAlpha * 0.55f));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
        nvgFillPaint(args.vg, flickerWash);
        nvgFill(args.vg);

        float rollSpeed = (0.5f + tear * 14.f) * (0.2f + 0.8f * (0.35f + chaosGate * 0.65f));
        float rollBandY = std::fmod(t * rollSpeed, h + 40.f) - 20.f;
        NVGpaint rollBand = nvgLinearGradient(args.vg, 0.f, rollBandY, 0.f, rollBandY + 38.f,
                                              nvgRGBAf(primary.r, primary.g, primary.b, 0.012f + tear * 0.09f),
                                              nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, rollBandY, w, 38.f);
        nvgFillPaint(args.vg, rollBand);
        nvgFill(args.vg);

        float scanAlpha = 0.013f + noise * 0.022f + snapshotSignalEnv[1] * 0.013f + darkness * 0.012f;
        float spacing = 3.6f + (1.f - tear) * 2.2f;
        shapetaker::graphics::drawScanlines(args, 0.f, 0.f, w, h, spacing, scanAlpha);
        shapetaker::graphics::drawScanlines(args, 0.f, 0.f, w, h, spacing * 0.52f, scanAlpha * 0.36f);
        shapetaker::graphics::drawShadowMask(args, 0.f, 0.f, w, h, 3.2f, 0.030f + noise * 0.040f);

        // Final readability pass so the wireframe sphere survives dense CRT overlays.
        float sphereRevealAlpha = clamp(0.34f + sphereAlpha * (0.56f + chaosGate * 0.16f), 0.f, 1.00f);
        if (sphereRevealAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            nvgGlobalAlpha(args.vg, sphereRevealAlpha);
            drawProgramShape(sphereTime);
            nvgRestore(args.vg);
        }

        // Input activity jewels for visual feedback.
        const std::array<NVGcolor, 4> inputColors = {
            nvgRGBAf(0.00f, 0.78f, 0.60f, 1.f),
            nvgRGBAf(0.52f, 0.72f, 0.98f, 1.f),
            nvgRGBAf(0.83f, 0.34f, 0.94f, 1.f),
            nvgRGBAf(0.97f, 0.78f, 0.33f, 1.f)
        };
        for (int i = 0; i < 4; ++i) {
            float x = 11.f + static_cast<float>(i) * 10.f;
            float y = h - 10.f;
            float r = 1.8f + snapshotSignalEnv[i] * 2.4f;
            bool connected = connectedMask & (1 << i);
            float alpha = connected ? (0.30f + snapshotSignalEnv[i] * 0.60f) : 0.10f;
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, x, y, r);
            nvgFillColor(args.vg, nvgRGBAf(inputColors[i].r, inputColors[i].g, inputColors[i].b, alpha));
            nvgFill(args.vg);
        }

        if (darkness > 0.001f) {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
            nvgFillColor(args.vg, nvgRGBAf(0.f, 0.f, 0.f, 0.08f + darkness * 0.54f));
            nvgFill(args.vg);

            NVGpaint omen = nvgRadialGradient(args.vg, w * 0.50f, h * 0.46f,
                                              std::min(w, h) * 0.10f, std::min(w, h) * 0.95f,
                                              nvgRGBAf(0.16f, 0.34f, 0.24f, darkness * 0.16f),
                                              nvgRGBAf(0.08f, 0.05f, 0.14f, 0.f));
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0.f, 0.f, w, h, radius);
            nvgFillPaint(args.vg, omen);
            nvgFill(args.vg);
        }

        shapetaker::graphics::drawVignettePatinaScratches(args,
            0.f, 0.f, w, h, radius,
            26, nvgRGBA(18, 20, 14, 16), nvgRGBA(50, 40, 22, 18),
            10, 0.34f, 4, 73321u);
        shapetaker::graphics::drawGlassReflections(args, 0.f, 0.f, w, h, 0.07f);

        drawProgramShapeBlur(sphereTime + 0.01f, sphereAlpha, 0.65f);

        // Absolute top readability pass: redraw the active shape above CRT artifacts.
        float topShapeAlpha = clamp(0.34f + sphereAlpha * 0.26f, 0.f, 0.74f);
        if (topShapeAlpha > 0.01f) {
            nvgSave(args.vg);
            nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            nvgGlobalAlpha(args.vg, topShapeAlpha);
            drawProgramShape(sphereTime);
            nvgRestore(args.vg);
        }

        nvgResetScissor(args.vg);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.8f, 0.8f, w - 1.6f, h - 1.6f, radius - 0.8f);
        nvgStrokeWidth(args.vg, 1.3f);
        nvgStrokeColor(args.vg, nvgRGBA(189, 166, 116, 58));
        nvgStroke(args.vg);

        if (!font) {
            font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/FuturaLT-Bold.ttf"));
            if (!font) {
                font = APP->window->loadFont(asset::system("res/fonts/FuturaLT-Bold.ttf"));
            }
            if (!font) {
                font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
            }
            if (!font) {
                font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
            }
        }
        if (font && font->handle >= 0) {
            nvgFontFaceId(args.vg, font->handle);
            nvgFontSize(args.vg, 11.f);
            nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFillColor(args.vg, nvgRGBA(232, 224, 200, 160));
            nvgText(args.vg, 9.f, 8.f, "NOCTURNE TV", nullptr);

            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
            nvgFillColor(args.vg, nvgRGBA(170, 160, 210, 145));
            const char* modeText = (mode == 0) ? "SYNC" : (mode == 1 ? "KEYER" : (mode == 2 ? "FEEDBACK" : "GLITCH"));
            if (sceneChangeTimer > 0.f) {
                char sceneLabel[24];
                std::snprintf(sceneLabel, sizeof(sceneLabel), "PROGRAM %02d", displayedScene + 1);
                nvgText(args.vg, w - 9.f, 8.f, sceneLabel, nullptr);
            } else {
                nvgText(args.vg, w - 9.f, 8.f, modeText, nullptr);
            }

            int chaosStep = clamp(static_cast<int>(std::round(chaosGate * 4.f)), 0, 4);
            const char* chaosText = (chaosStep == 0) ? "CHAOS: STABLE"
                                   : (chaosStep == 1 ? "CHAOS: DRIFT"
                                   : (chaosStep == 2 ? "CHAOS: ACTIVE"
                                   : (chaosStep == 3 ? "CHAOS: WILD" : "CHAOS: MAX")));
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
            nvgFillColor(args.vg, nvgRGBA(232, 224, 200, 130));
            nvgText(args.vg, w * 0.5f, h - 7.f, chaosText, nullptr);
        }

        nvgRestore(args.vg);
    }
};

struct NocturneTVWidget : ModuleWidget {
    static const float PANEL_WIDTH;
    static constexpr float BG_TEXTURE_ASPECT = 2880.f / 4553.f;
    static constexpr float BG_OFFSET_OPACITY = 0.35f;
    static constexpr int BG_DARKEN_ALPHA = 18;
    static constexpr float DISPLAY_SCALE = 0.90f;

    NocturneTVWidget(NocturneTV* module) {
        setModule(module);

        auto* panel = new Widget;
        panel->box.size = Vec(PANEL_WIDTH, RACK_GRID_HEIGHT);
        setPanel(panel);
        const float kLegacyPanelWidth = 22.f * RACK_GRID_WIDTH;
        const float xScale = box.size.x / kLegacyPanelWidth;
        auto sx = [xScale](float x) {
            return x * xScale;
        };

        auto overlay = new PanelPatinaOverlay();
        overlay->box = Rect(Vec(0, 0), box.size);
        addChild(overlay);

        addChild(createWidget<ScrewJetBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewJetBlack>(Vec(box.size.x - 2.f * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewJetBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewJetBlack>(Vec(box.size.x - 2.f * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto* screen = new NocturneTVScreen(module);
        float screenInset = sx(26.f);
        const Vec baseScreenPos = Vec(screenInset, 24.f);
        const Vec baseScreenSize = Vec(box.size.x - 2.f * screenInset, 190.f);
        Vec screenSize = baseScreenSize.mult(DISPLAY_SCALE);
        Vec screenOffset = baseScreenSize.minus(screenSize).mult(0.5f);
        screen->box.pos = baseScreenPos.plus(screenOffset);
        screen->box.size = screenSize;
        addChild(screen);

        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(Vec(sx(68.f), 248.f), module, NocturneTV::WARP_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(Vec(sx(126.f), 248.f), module, NocturneTV::NOISE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(Vec(sx(184.f), 248.f), module, NocturneTV::TEAR_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(Vec(sx(242.f), 248.f), module, NocturneTV::DRIFT_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmallMedium>(Vec(sx(300.f), 248.f), module, NocturneTV::TINT_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmall>(Vec(sx(34.f), 248.f), module, NocturneTV::INPUT_GAIN_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerAttenuverterOscilloscope>(Vec(sx(184.f), 298.f), module, NocturneTV::MODE_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmall>(Vec(sx(34.f), 298.f), module, NocturneTV::REFRESH_PARAM));
        addKnobWithShadow(this, createParamCentered<ShapetakerKnobVintageSmall>(Vec(sx(34.f), 342.f), module, NocturneTV::CHANNEL_PARAM));

        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(68.f), 296.f), module, NocturneTV::WARP_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(126.f), 296.f), module, NocturneTV::NOISE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(213.f), 296.f), module, NocturneTV::FILL_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(242.f), 296.f), module, NocturneTV::DRIFT_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(271.f), 296.f), module, NocturneTV::DARKNESS_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(300.f), 296.f), module, NocturneTV::TINT_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(184.f), 296.f), module, NocturneTV::TEAR_CV_INPUT));

        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(82.f), 342.f), module, NocturneTV::SIGNAL_1_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(150.f), 342.f), module, NocturneTV::SIGNAL_2_INPUT));
        addParam(createParamCentered<ShapetakerDarkToggleFivePos>(Vec(sx(184.f), 342.f), module, NocturneTV::CHAOS_LATCH_PARAM));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(218.f), 342.f), module, NocturneTV::SIGNAL_3_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(252.f), 342.f), module, NocturneTV::EXPLODE_CV_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(Vec(sx(286.f), 342.f), module, NocturneTV::SIGNAL_4_INPUT));
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* tv = dynamic_cast<NocturneTV*>(module);
        if (!tv) {
            return;
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Display"));
        menu->addChild(shapetaker::ui::createFloatSlider(
            tv,
            [](NocturneTV* m, float v) {
                m->params[NocturneTV::REFRESH_PARAM].setValue(clamp(v, NocturneTV::REFRESH_MIN_HZ, NocturneTV::REFRESH_MAX_HZ));
            },
            [](NocturneTV* m) {
                return m->params[NocturneTV::REFRESH_PARAM].getValue();
            },
            NocturneTV::REFRESH_MIN_HZ,
            NocturneTV::REFRESH_MAX_HZ,
            18.f,
            "Refresh",
            "Hz"
        ));
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            constexpr float inset = 2.f;
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * BG_TEXTURE_ASPECT;
            float x = -inset;
            float y = -inset;

            nvgSave(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, BG_OFFSET_OPACITY);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, BG_DARKEN_ALPHA));
            nvgFill(args.vg);

            nvgRestore(args.vg);
        }

        float baseTvX = 18.f;
        float baseTvY = 16.f;
        float baseTvW = box.size.x - 36.f;
        float baseTvH = 214.f;
        float tvW = baseTvW * DISPLAY_SCALE;
        float tvH = baseTvH * DISPLAY_SCALE;
        float tvX = baseTvX + (baseTvW - tvW) * 0.5f;
        float tvY = baseTvY + (baseTvH - tvH) * 0.5f;
        float radius = 12.f * DISPLAY_SCALE;

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, tvX, tvY, tvW, tvH, radius);
        NVGpaint housing = nvgLinearGradient(args.vg, tvX, tvY, tvX, tvY + tvH,
                                             nvgRGBA(90, 66, 39, 255), nvgRGBA(45, 31, 20, 255));
        nvgFillPaint(args.vg, housing);
        nvgFill(args.vg);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, tvX + 2.f, tvY + 2.f, tvW - 4.f, tvH - 4.f, radius - 2.f);
        nvgStrokeWidth(args.vg, 1.4f * DISPLAY_SCALE);
        nvgStrokeColor(args.vg, nvgRGBA(214, 180, 128, 35));
        nvgStroke(args.vg);

        float plinthInset = 8.f * DISPLAY_SCALE;
        float plinthH = 18.f * DISPLAY_SCALE;
        float plinthRadius = 5.f * DISPLAY_SCALE;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, tvX + plinthInset, tvY + tvH + 6.f * DISPLAY_SCALE,
                       tvW - 2.f * plinthInset, plinthH, plinthRadius);
        nvgFillColor(args.vg, nvgRGBA(12, 12, 14, 180));
        nvgFill(args.vg);

        ModuleWidget::draw(args);

        constexpr float frame = 1.f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }
};

Model* modelNocturneTV = createModel<NocturneTV, NocturneTVWidget>("NocturneTV");

const float NocturneTVWidget::PANEL_WIDTH = 18.f * RACK_GRID_WIDTH;
