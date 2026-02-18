#include "plugin.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <cmath>

struct Specula : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        LEFT_INPUT,
        RIGHT_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        LEFT_OUTPUT,
        RIGHT_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        LEFT_VU_LIGHT,
        RIGHT_VU_LIGHT,
        NUM_LIGHTS
    };

    dsp::VuMeter2 vuMeterLeft;
    dsp::VuMeter2 vuMeterRight;
    float leftNeedleDisplay = 0.f;
    float rightNeedleDisplay = 0.f;

    Specula() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        vuMeterLeft.mode = dsp::VuMeter2::PEAK;
        vuMeterRight.mode = dsp::VuMeter2::PEAK;
        // Slower ballistic response for analog-style needle movement.
        vuMeterLeft.lambda = 5.f;
        vuMeterRight.lambda = 5.f;

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void process(const ProcessArgs& args) override {
        passThroughAudio(LEFT_INPUT, LEFT_OUTPUT);
        passThroughAudio(RIGHT_INPUT, RIGHT_OUTPUT);

        float leftPeak = getPeakVoltage(inputs[LEFT_INPUT]);
        float rightPeak = getPeakVoltage(inputs[RIGHT_INPUT]);

        constexpr float calibration = 1.125f;
        float leftNeedle = computeNeedleNormalized(
            args.sampleTime, leftPeak, calibration, vuMeterLeft);
        float rightNeedle = computeNeedleNormalized(
            args.sampleTime, rightPeak, calibration, vuMeterRight);

        lights[LEFT_VU_LIGHT].setBrightness(applyNeedleBallistics(args.sampleTime, leftNeedle, leftNeedleDisplay));
        lights[RIGHT_VU_LIGHT].setBrightness(applyNeedleBallistics(args.sampleTime, rightNeedle, rightNeedleDisplay));
    }

private:
    void passThroughAudio(int inputId, int outputId) {
        int channels = inputs[inputId].getChannels();
        outputs[outputId].setChannels(channels);
        for (int c = 0; c < channels; ++c) {
            outputs[outputId].setVoltage(inputs[inputId].getVoltage(c), c);
        }
    }

    float getPeakVoltage(Input& input) {
        int channels = input.getChannels();
        float peak = 0.f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::fabs(input.getVoltage(c)));
        }
        return peak;
    }

    float computeNeedleNormalized(float deltaTime,
                                  float peakVoltage,
                                  float calibration,
                                  dsp::VuMeter2& meter) {
        // Calibrate for standard Rack audio levels: 10 Vpp (5 V peak) ~= 0 VU at default calibration.
        float cal = rack::math::clamp(calibration, 0.5f, 2.f);
        float reference = 5.f / cal;
        meter.process(deltaTime, peakVoltage / reference);

        float amplitude = std::max(meter.v, 1e-6f);
        float db = rack::dsp::amplitudeToDb(amplitude);
        return dbToNeedleNormalized(db);
    }

    float dbToNeedleNormalized(float db) const {
        // Dial model: -20dB (left) -> 0dB mark (center) -> +3dB clip edge (right).
        constexpr float kDbMin = -20.f;
        constexpr float kDbZero = 0.f;
        constexpr float kDbClip = 3.f;
        float clampedDb = rack::math::clamp(db, kDbMin, kDbClip);
        if (clampedDb <= kDbZero) {
            return rack::math::rescale(clampedDb, kDbMin, kDbZero, 0.f, 0.5f);
        }
        return rack::math::rescale(clampedDb, kDbZero, kDbClip, 0.5f, 1.f);
    }

    float applyNeedleBallistics(float deltaTime, float target, float& state) const {
        // Fast attack, slower release.
        constexpr float attackTau = 0.015f;
        constexpr float releaseTau = 0.45f;
        float tau = (target > state) ? attackTau : releaseTau;
        float alpha = 1.f - std::exp(-deltaTime / std::max(tau, 1e-4f));
        state += (target - state) * alpha;
        return rack::math::clamp(state, 0.f, 1.f);
    }
};

struct SpeculaWidget : ModuleWidget {
    // Match the uniform Clairaudient/Tessellation/Transmutation/Torsion leather treatment
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            // Keep leather grain density consistent across panel widths via fixed-height tiling.
            constexpr float inset = 2.0f;
            constexpr float textureAspect = 2880.f / 4553.f;  // panel_background.png
            float tileH = box.size.y + inset * 2.f;
            float tileW = tileH * textureAspect;
            float x = -inset;
            float y = -inset;

            nvgSave(args.vg);

            // Base tile pass
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintA = nvgImagePattern(args.vg, x, y, tileW, tileH, 0.f, bg->handle, 1.0f);
            nvgFillPaint(args.vg, paintA);
            nvgFill(args.vg);

            // Offset low-opacity pass to soften seam visibility
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, 0.35f);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            // Slight darkening to match existing module tone
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

    SpeculaWidget(Specula* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Specula.svg")));

        using LayoutHelper = shapetaker::ui::LayoutHelper;
        LayoutHelper::ScrewPositions::addStandardScrews<ScrewJetBlack>(this, box.size.x);

        // Parse SVG panel for precise positioning
        shapetaker::ui::LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/Specula.svg"));
        auto centerPx = shapetaker::ui::LayoutHelper::createCenterPxHelper(parser);

        Rect leftMeterRect = parser.rectMm("left_vu_meter", 6.367703f, 14.433204f, 38.064594f, 39.764595f);
        Rect rightMeterRect = parser.rectMm("right_vu_meter", 6.367703f, 62.969048f, 38.064594f, 39.764595f);
        constexpr float kMeterScale = 1.10f;
        auto scaleRectFromCenter = [](const Rect& rect, float scale) {
            Vec center = rect.pos.plus(rect.size.div(2.f));
            Vec scaledSize = rect.size.mult(scale);
            return Rect(center.minus(scaledSize.div(2.f)), scaledSize);
        };
        leftMeterRect = scaleRectFromCenter(leftMeterRect, kMeterScale);
        rightMeterRect = scaleRectFromCenter(rightMeterRect, kMeterScale);

        auto* leftMeter = new shapetaker::ui::VintageVUMeterWidget(
            module, Specula::LEFT_VU_LIGHT, asset::plugin(pluginInstance, "res/meters/vintage_vu.svg"));
        leftMeter->box.size = mm2px(leftMeterRect.size);
        leftMeter->box.pos = mm2px(leftMeterRect.pos);
        addChild(leftMeter);

        auto* rightMeter = new shapetaker::ui::VintageVUMeterWidget(
            module, Specula::RIGHT_VU_LIGHT, asset::plugin(pluginInstance, "res/meters/vintage_vu.svg"));
        rightMeter->box.size = mm2px(rightMeterRect.size);
        rightMeter->box.pos = mm2px(rightMeterRect.pos);
        addChild(rightMeter);

        // Use SVG positioning for inputs and outputs
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("left_input", 9.3099117f, 114.73895f), module, Specula::LEFT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("right_input", 20.391472f, 114.73895f), module, Specula::RIGHT_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("left_output", 31.473032f, 114.73895f), module, Specula::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("right_output", 42.554592f, 114.73895f), module, Specula::RIGHT_OUTPUT));
    }
};

Model* modelSpecula = createModel<Specula, SpeculaWidget>("Specula");
