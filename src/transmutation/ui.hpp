// Transmutation UI helpers
#pragma once
#include <rack.hpp>
#include "view.hpp"
// Forward declare helpers; implementation includes them

using namespace rack;

// Full-module subtle vignette and patina for cohesive vintage look
struct PanelPatinaOverlay : TransparentWidget {
    void draw(const DrawArgs& args) override;
};

// Small status display that reads via TransmutationView
struct TransmutationDisplayWidget : TransparentWidget {
    stx::transmutation::TransmutationView* view;
    std::shared_ptr<Font> font;
    explicit TransmutationDisplayWidget(stx::transmutation::TransmutationView* v) : view(v) {}
    void draw(const DrawArgs& args) override;
};

// Alchemical Symbol Button Widget - now uses view/controller pattern
struct AlchemicalSymbolWidget : Widget {
    stx::transmutation::TransmutationView* view = nullptr;
    stx::transmutation::TransmutationController* ctrl = nullptr;
    int buttonPosition; // Button position (0-11)
    
    AlchemicalSymbolWidget(stx::transmutation::TransmutationView* v,
                          stx::transmutation::TransmutationController* c,
                          int buttonPosition)
        : view(v), ctrl(c), buttonPosition(buttonPosition) {
        box.size = Vec(20, 20);
    }
    
    int getSymbolId();
    void draw(const DrawArgs& args) override;
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId);
    void onButton(const event::Button& e) override;
};

// Rest/Tie momentary buttons styled like alchemical buttons, with playhead glow
struct RestTieMomentary : app::SvgSwitch {
    stx::transmutation::TransmutationView* view = nullptr;
    bool isRest = true; // true = REST, false = TIE

    RestTieMomentary() {
        momentary = true;
        if (shadow) shadow->visible = false;
        box.size = Vec(18.f, 18.f);
    }

    void setView(stx::transmutation::TransmutationView* v) { view = v; }
    void setIsRest(bool rest) { isRest = rest; }

    void draw(const DrawArgs& args) override;
};

// High-Resolution Matrix Widget - now consumes view/controller
struct HighResMatrixWidget : Widget {
    stx::transmutation::TransmutationView* view = nullptr;
    stx::transmutation::TransmutationController* ctrl = nullptr;
    static constexpr int MATRIX_COLS = 8;
    static constexpr float CANVAS_SIZE = 512.0f;
    static constexpr float CELL_SIZE = CANVAS_SIZE / MATRIX_COLS;

    HighResMatrixWidget(stx::transmutation::TransmutationView* v,
                        stx::transmutation::TransmutationController* c)
        : view(v), ctrl(c) {
        box.size = Vec(231.0f, 231.0f);
    }

    void onButton(const event::Button& e) override;
    void onMatrixClick(int x, int y);
    void onMatrixRightClick(int x, int y);
    void drawLayer(const DrawArgs& args, int layer) override;
    void drawMatrix(const DrawArgs& args);
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color = nvgRGBA(255,255,255,255), float scale = 1.0f);
    void drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor = nvgRGBA(255,255,255,255));
};
