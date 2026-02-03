#include "plugin.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <cmath>

struct Specula : Module {
    enum ParamIds {
        LEFT_CALIBRATION_PARAM,
        RIGHT_CALIBRATION_PARAM,
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

    // Smoothed needle response (normalized 0-1)
    dsp::ExponentialFilter leftNeedleFilter;
    dsp::ExponentialFilter rightNeedleFilter;

    Specula() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LEFT_CALIBRATION_PARAM, 0.5f, 2.f, 1.0f, "Left Calibration");
        configParam(RIGHT_CALIBRATION_PARAM, 0.5f, 2.f, 1.0f, "Right Calibration");

        vuMeterLeft.mode = dsp::VuMeter2::RMS;
        vuMeterRight.mode = dsp::VuMeter2::RMS;

        leftNeedleFilter.setTau(0.3f);
        rightNeedleFilter.setTau(0.3f);

        shapetaker::ui::LabelFormatter::normalizeModuleControls(this);
    }

    void process(const ProcessArgs& args) override {
        float left_input = inputs[LEFT_INPUT].getVoltage();
        float right_input = inputs[RIGHT_INPUT].getVoltage();

        outputs[LEFT_OUTPUT].setVoltage(left_input);
        outputs[RIGHT_OUTPUT].setVoltage(right_input);

        float left_cal = params[LEFT_CALIBRATION_PARAM].getValue();
        float right_cal = params[RIGHT_CALIBRATION_PARAM].getValue();

        float leftNeedle = computeNeedleValue(args.sampleTime, left_input, left_cal, vuMeterLeft, leftNeedleFilter);
        float rightNeedle = computeNeedleValue(args.sampleTime, right_input, right_cal, vuMeterRight, rightNeedleFilter);

        lights[LEFT_VU_LIGHT].setBrightness(leftNeedle);
        lights[RIGHT_VU_LIGHT].setBrightness(rightNeedle);
    }

private:
    float computeNeedleValue(float deltaTime,
                             float voltage,
                             float calibration,
                             dsp::VuMeter2& meter,
                             dsp::ExponentialFilter& smoother) {
        float cal = rack::math::clamp(calibration, 0.25f, 4.f);
        float reference = 10.f / cal;
        meter.process(deltaTime, voltage / reference);

        float amplitude = (meter.mode == dsp::VuMeter2::RMS) ? std::sqrt(std::max(meter.v, 0.f)) : std::fabs(meter.v);
        float dbFs = rack::dsp::amplitudeToDb(std::max(amplitude, 1e-6f));

        float normalized;
        if (dbFs <= -20.f) {
            normalized = 0.f;
        } else if (dbFs < -6.f) {
            normalized = rack::math::rescale(dbFs, -20.f, -6.f, 0.f, 0.5f);
        } else if (dbFs < 3.f) {
            normalized = rack::math::rescale(dbFs, -6.f, 3.f, 0.5f, 1.f);
        } else {
            normalized = 1.f;
        }

        return smoother.process(deltaTime, normalized);
    }
};

struct SpeculaWidget : ModuleWidget {
    // Draw leather texture background
    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/black_leather_seamless.jpg"));
        if (bg) {
            // Scale < 1.0 = finer grain appearance
            float scale = 0.4f;
            float textureHeight = box.size.y * scale;
            float textureWidth = textureHeight * (1.f);
            float xOffset = 0.f;  // Unique offset for Specula (left edge)
            NVGpaint paint = nvgImagePattern(args.vg, -xOffset, 0.f, textureWidth, box.size.y, 0.f, bg->handle, 1.0f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        }
        ModuleWidget::draw(args);
    }

    SpeculaWidget(Specula* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Specula.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Parse SVG panel for precise positioning
        shapetaker::ui::LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/Specula.svg"));
        auto centerPx = shapetaker::ui::LayoutHelper::createCenterPxHelper(parser);

        if (module) {
            // VU meters with correct aspect ratio (259.09:271.04 ≈ 0.956)
            // Set height to 46mm, calculate width from aspect ratio
            float aspectRatio = 259.0896f / 271.0356f; // 0.956
            float meterHeight = 46.0f;
            float meterWidth = meterHeight * aspectRatio; // 43.97mm

            // Left VU meter: center using SVG rectangle
            Vec leftVUCenter = parser.centerMm("left_vu_meter", 25.40, 35.67);
            shapetaker::ui::VintageVUMeterWidget* vuMeter1 = new shapetaker::ui::VintageVUMeterWidget(module, Specula::LEFT_VU_LIGHT, asset::plugin(pluginInstance, "res/meters/vintage_vu.svg"));
            vuMeter1->box.size = Vec(mm2px(meterWidth), mm2px(meterHeight));
            vuMeter1->box.pos = mm2px(Vec(leftVUCenter.x - meterWidth/2, leftVUCenter.y - meterHeight/2));
            addChild(vuMeter1);

            // Right VU meter: center using SVG rectangle
            Vec rightVUCenter = parser.centerMm("right_vu_meter", 25.40, 84.13);
            shapetaker::ui::VintageVUMeterWidget* vuMeter2 = new shapetaker::ui::VintageVUMeterWidget(module, Specula::RIGHT_VU_LIGHT, asset::plugin(pluginInstance, "res/meters/vintage_vu.svg"));
            vuMeter2->box.size = Vec(mm2px(meterWidth), mm2px(meterHeight));
            vuMeter2->box.pos = mm2px(Vec(rightVUCenter.x - meterWidth/2, rightVUCenter.y - meterHeight/2));
            addChild(vuMeter2);

            addParam(createParamCentered<shapetaker::ui::Trimpot>(mm2px(Vec(25.4, 52)), module, Specula::LEFT_CALIBRATION_PARAM));
            addParam(createParamCentered<shapetaker::ui::Trimpot>(mm2px(Vec(25.4, 102)), module, Specula::RIGHT_CALIBRATION_PARAM));
        }

        // Use SVG positioning for inputs and outputs
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("left_input", mm2px(9.31), mm2px(116.59)), module, Specula::LEFT_INPUT));
        addInput(createInputCentered<ShapetakerBNCPort>(centerPx("right_input", mm2px(20.39), mm2px(116.59)), module, Specula::RIGHT_INPUT));

        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("left_output", mm2px(31.47), mm2px(116.59)), module, Specula::LEFT_OUTPUT));
        addOutput(createOutputCentered<ShapetakerBNCPort>(centerPx("right_output", mm2px(42.55), mm2px(116.59)), module, Specula::RIGHT_OUTPUT));
    }
};

Model* modelSpecula = createModel<Specula, SpeculaWidget>("Specula");
