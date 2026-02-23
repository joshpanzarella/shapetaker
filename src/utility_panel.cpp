#include "plugin.hpp"

#include <cmath>

struct UtilityPanel : Module {
    static constexpr int MIN_WIDTH_HP = 3;
    static constexpr int MAX_WIDTH_HP = 64;
    static constexpr int DEFAULT_WIDTH_HP = 12;

    int panelWidthHp = DEFAULT_WIDTH_HP;

    UtilityPanel() {
        config(0, 0, 0, 0);
    }

    void setPanelWidthHp(int hp) {
        panelWidthHp = clamp(hp, MIN_WIDTH_HP, MAX_WIDTH_HP);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "panelWidthHp", json_integer(panelWidthHp));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* widthJ = json_object_get(rootJ, "panelWidthHp");
        if (widthJ) {
            setPanelWidthHp(json_integer_value(widthJ));
        }
    }
};

struct UtilityPanelWidget;

struct UtilityPanelCenterScrew : ScrewJetBlack {
    UtilityPanelWidget* moduleWidget = nullptr;
    bool bottom = false;

    UtilityPanelCenterScrew(UtilityPanelWidget* moduleWidget, bool bottom)
        : moduleWidget(moduleWidget), bottom(bottom) {
    }

    void step() override;
};

struct UtilityPanelResizeHandle : widget::OpaqueWidget {
    UtilityPanelWidget* moduleWidget = nullptr;
    bool right = false;

    UtilityPanelResizeHandle(UtilityPanelWidget* moduleWidget, bool right)
        : moduleWidget(moduleWidget), right(right) {
        box.size = Vec(8.f, RACK_GRID_HEIGHT);
    }

    void step() override;

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
        nvgFill(args.vg);

        const float gripX = right ? 2.2f : box.size.x - 2.2f;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, gripX, 0.f);
        nvgLineTo(args.vg, gripX, box.size.y);
        nvgStrokeColor(args.vg, nvgRGBA(235, 216, 170, 34));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);
    }

    void onButton(const event::Button& e) override {
        if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
            e.consume(this);
        }
    }

    void onDragMove(const event::DragMove& e) override;
};

struct UtilityPanelWidget : ModuleWidget {
    static constexpr float BG_TEXTURE_ASPECT = 2880.f / 4553.f;
    static constexpr float BG_OFFSET_OPACITY = 0.35f;
    static constexpr int BG_DARKEN_ALPHA = 18;

    UtilityPanelWidget(UtilityPanel* module) {
        setModule(module);
        applyPanelWidthHp(module ? module->panelWidthHp : UtilityPanel::DEFAULT_WIDTH_HP, false, false);

        addChild(new UtilityPanelCenterScrew(this, false));
        addChild(new UtilityPanelCenterScrew(this, true));

        addChild(new UtilityPanelResizeHandle(this, false));
        addChild(new UtilityPanelResizeHandle(this, true));
    }

    bool applyPanelWidthHp(int hp, bool keepRightEdge, bool updateModule) {
        hp = clamp(hp, UtilityPanel::MIN_WIDTH_HP, UtilityPanel::MAX_WIDTH_HP);
        const float newWidth = hp * RACK_GRID_WIDTH;
        const float oldWidth = box.size.x;

        if (std::fabs(newWidth - oldWidth) < 0.001f && box.size.y > 0.f) {
            if (updateModule) {
                if (auto* utility = dynamic_cast<UtilityPanel*>(module)) {
                    utility->setPanelWidthHp(hp);
                }
            }
            return true;
        }

        const Vec oldPos = box.pos;
        const Vec oldSize = box.size;
        const float rightEdge = oldPos.x + oldWidth;
        Vec newPos = oldPos;
        if (keepRightEdge && oldWidth > 0.f) {
            newPos.x = rightEdge - newWidth;
        }

        setSize(Vec(newWidth, RACK_GRID_HEIGHT));
        setPosition(newPos);

        app::RackWidget* rackWidget = (APP && APP->scene) ? APP->scene->rack : nullptr;
        if (rackWidget && !rackWidget->requestModulePos(this, newPos)) {
            setSize(oldSize);
            setPosition(oldPos);
            return false;
        }

        if (updateModule) {
            if (auto* utility = dynamic_cast<UtilityPanel*>(module)) {
                utility->setPanelWidthHp(hp);
            }
        }
        return true;
    }

    bool resizeByDelta(float deltaX, bool dragRightEdge) {
        const float currentWidth = box.size.x > 0.f ? box.size.x : UtilityPanel::DEFAULT_WIDTH_HP * RACK_GRID_WIDTH;
        const float proposedWidth = currentWidth + (dragRightEdge ? deltaX : -deltaX);
        const int targetHp = clamp(static_cast<int>(std::round(proposedWidth / RACK_GRID_WIDTH)),
            UtilityPanel::MIN_WIDTH_HP, UtilityPanel::MAX_WIDTH_HP);
        return applyPanelWidthHp(targetHp, !dragRightEdge, true);
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<Image> bg = APP->window->loadImage(asset::plugin(pluginInstance, "res/panels/panel_background.png"));
        if (bg) {
            constexpr float inset = 2.0f;
            constexpr float textureAspect = BG_TEXTURE_ASPECT;
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
            NVGpaint paintB = nvgImagePattern(args.vg, x + tileW * 0.5f, y, tileW, tileH, 0.f, bg->handle, BG_OFFSET_OPACITY);
            nvgFillPaint(args.vg, paintB);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, BG_DARKEN_ALPHA));
            nvgFill(args.vg);

            nvgRestore(args.vg);
        }

        ModuleWidget::draw(args);

        constexpr float frame = 1.0f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgRect(args.vg, frame, frame, box.size.x - 2.f * frame, box.size.y - 2.f * frame);
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);
    }
};

void UtilityPanelCenterScrew::step() {
    if (moduleWidget) {
        const float x = std::round((moduleWidget->box.size.x - 2.f * RACK_GRID_WIDTH) * 0.5f);
        const float y = bottom ? (RACK_GRID_HEIGHT - RACK_GRID_WIDTH) : 0.f;
        if (box.pos.x != x || box.pos.y != y) {
            box.pos = Vec(x, y);
        }
    }
    ScrewJetBlack::step();
}

void UtilityPanelResizeHandle::step() {
    if (moduleWidget) {
        box.pos = right ? Vec(moduleWidget->box.size.x - box.size.x, 0.f) : Vec(0.f, 0.f);
        box.size.y = moduleWidget->box.size.y;
    }
    widget::OpaqueWidget::step();
}

void UtilityPanelResizeHandle::onDragMove(const event::DragMove& e) {
    if (!moduleWidget || e.button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    moduleWidget->resizeByDelta(e.mouseDelta.x, right);
}

Model* modelUtilityPanel = createModel<UtilityPanel, UtilityPanelWidget>("UtilityPanel");
