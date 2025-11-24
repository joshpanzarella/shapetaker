// Transmutation UI helpers implementation
#include "ui.hpp"
#include "../plugin.hpp"
// Legacy helpers are now in utilities.hpp via plugin.hpp
#include <sstream>

using namespace shapetaker;

void PanelPatinaOverlay::draw(const DrawArgs& args) {
    float w = box.size.x;
    float h = box.size.y;
    // Vignette
    NVGpaint vignette = nvgRadialGradient(args.vg, w * 0.5f, h * 0.5f,
                                          std::min(w, h) * 0.6f, std::min(w, h) * 0.95f,
                                          nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 20));
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, w, h);
    nvgFillPaint(args.vg, vignette);
    nvgFill(args.vg);

    // Gentle patina wash
    NVGpaint wash = nvgLinearGradient(args.vg, 0, 0, w, h,
                                      nvgRGBA(22, 28, 18, 8), nvgRGBA(50, 40, 22, 6));
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, w, h);
    nvgFillPaint(args.vg, wash);
    nvgFill(args.vg);

    // Sparse micro-scratches
    unsigned seed = 99173u;
    auto rnd = [&]() {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (seed & 0xFFFF) / 65535.f; };
    nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 8));
    nvgStrokeWidth(args.vg, 0.7f);
    for (int i = 0; i < 8; ++i) {
        float x1 = rnd() * w;
        float y1 = rnd() * h;
        float dx = (rnd() - 0.5f) * (w * 0.15f);
        float dy = (rnd() - 0.5f) * (h * 0.15f);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x1, y1);
        nvgLineTo(args.vg, x1 + dx, y1 + dy);
        nvgStroke(args.vg);
    }
}

void TransmutationDisplayWidget::draw(const DrawArgs& args) {
    if (!view) return;

    if (!font) {
        font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font)
            font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    }
    if (!font) return;

    nvgSave(args.vg);
    // CRT-like mini screen with subtle bezel and glass depth
    bool spooky = view->getSpookyTvMode();
    float w = box.size.x, h = box.size.y;
    float r = 4.0f; // corner radius

    // Base near-black fill (neutral to match spooky preview palette)
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, r);
    nvgFillColor(args.vg, nvgRGBA(8, 8, 10, 255));
    nvgFill(args.vg);

    // Subtle center bulge glow
    NVGpaint centerGlow = nvgRadialGradient(args.vg,
        w * 0.5f, h * 0.5f,
        std::min(w, h) * 0.20f,
        std::min(w, h) * 0.85f,
        nvgRGBA(36, 36, 40, 64), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, r - 0.5f);
    nvgFillPaint(args.vg, centerGlow);
    nvgFill(args.vg);

    // Inset edge shadow for seating
    NVGpaint inset = nvgBoxGradient(args.vg, 1.5f, 1.5f, w - 3.0f, h - 3.0f,
                                    r - 2.5f, 6.0f, nvgRGBA(0, 0, 0, 55), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1.0f, 1.0f, w - 2.0f, h - 2.0f, r - 1.0f);
    nvgRoundedRect(args.vg, 3.5f, 3.5f, w - 7.0f, h - 7.0f, std::max(0.0f, r - 3.5f));
    nvgPathWinding(args.vg, NVG_HOLE);
    nvgFillPaint(args.vg, inset);
    nvgFill(args.vg);

    // Bezel ring for depth (very subtle)
    float bezel = 3.0f;
    NVGpaint bezelPaint = nvgLinearGradient(args.vg, 0.f, 0.f, 0.f, h,
        nvgRGBA(26, 26, 32, 200), nvgRGBA(10, 10, 14, 200));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, r - 0.5f);
    nvgRoundedRect(args.vg, bezel + 0.5f, bezel + 0.5f,
                   w - 2.f * bezel - 1.0f, h - 2.f * bezel - 1.0f,
                   std::max(0.0f, r - bezel - 0.5f));
    nvgPathWinding(args.vg, NVG_HOLE);
    nvgFillPaint(args.vg, bezelPaint);
    nvgFill(args.vg);

    // Bezel highlight and shadow strokes
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, bezel + 0.8f, bezel + 0.8f, w - 2.f * (bezel + 0.8f), h - 2.f * (bezel + 0.8f), std::max(0.0f, r - bezel - 0.8f));
    nvgStrokeWidth(args.vg, 1.0f);
    nvgStrokeColor(args.vg, nvgRGBA(210, 210, 225, 30));
    nvgStroke(args.vg);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, bezel - 0.4f, bezel - 0.4f, w - 2.f * (bezel - 0.4f), h - 2.f * (bezel - 0.4f), std::max(0.0f, r - bezel + 0.4f));
    nvgStrokeWidth(args.vg, 1.0f);
    nvgStrokeColor(args.vg, nvgRGBA(5, 5, 8, 80));
    nvgStroke(args.vg);

    // Screen rect (inside bezel)
    float sx = bezel + 0.5f;
    float sy = bezel + 0.5f;
    float sw = w - 2.0f * bezel - 1.0f;
    float sh = h - 2.0f * bezel - 1.0f;

    // Softer, sparser scanlines on the mini display too
    float scanAlpha = spooky ? 0.008f : 0.006f;
    float lineSpacing = spooky ? 4.0f : 3.0f;
    shapetaker::graphics::drawScanlines(args, sx, sy, sw, sh, lineSpacing, scanAlpha);

    // Glass reflections to sell curvature
    shapetaker::graphics::drawGlassReflections(args, sx, sy, sw, sh, 0.10f);

    nvgFontSize(args.vg, 10);
    if (font && font->handle >= 0)
        nvgFontFaceId(args.vg, font->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    auto drawVintageText = [&](float x, float y, NVGcolor color, const std::string& s) {
        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 120));
        nvgText(args.vg, x + 1, y + 1, s.c_str(), NULL);
        nvgFillColor(args.vg, color);
        nvgText(args.vg, x, y, s.c_str(), NULL);
    };

    NVGcolor ink = nvgRGBA(232, 224, 200, 230);
    NVGcolor tealInk = nvgRGBA(150, 230, 210, 230);
    NVGcolor purpleInk = nvgRGBA(210, 160, 250, 230);
    NVGcolor yellowInk = nvgRGBA(240, 230, 140, 230);

    float y = 5;
    // BPM
    float baseBPM = view->getInternalClockBpm();
    int multIdx = view->getBpmMultiplier();
    float multipliers[] = {1.0f, 2.0f, 4.0f, 8.0f};
    const char* multiplierLabels[] = {"1x", "2x", "4x", "8x"};
    float effectiveBPM = baseBPM * multipliers[multIdx];
    std::string bpmText = "BPM: " + std::to_string((int)baseBPM) + " (" + multiplierLabels[multIdx] + " = " + std::to_string((int)effectiveBPM) + ")";
    drawVintageText(5, y, ink, bpmText);
    y += 12;

    // A status
    std::string statusA = std::string("A: ") + (view->isSeqARunning() ? "RUN" : "STOP") +
                          " [" + std::to_string(view->getSeqACurrentStep() + 1) +
                          "/" + std::to_string(view->getSeqALength()) + "]";
    drawVintageText(5, y, tealInk, statusA);
    y += 12;

    // B status
    const char* modeNames[] = {"IND", "HAR", "LOK"};
    int bMode = view->getSeqBMode();
    std::string statusB = std::string("B: ") + (view->isSeqBRunning() ? "RUN" : "STOP") +
                          " [" + std::to_string(view->getSeqBCurrentStep() + 1) +
                          "/" + std::to_string(view->getSeqBLength()) + "] " +
                          modeNames[std::max(0, std::min(bMode, 2))];
    drawVintageText(5, y, purpleInk, statusB);
    y += 12;

    // Edit mode
    std::string editStatus = std::string("EDIT: ") + (view->isEditModeA() ? "A" : (view->isEditModeB() ? "B" : "OFF"));
    drawVintageText(5, y, yellowInk, editStatus);

    // Clock sources
    float rightX = box.size.x - 25;
    NVGcolor smallInk = nvgRGBA(210, 210, 210, 200);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 120));
    nvgFontSize(args.vg, 8);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    std::string clockAText = view->isClockAConnected() ? "EXT" : "INT";
    nvgText(args.vg, rightX + 1, 18, clockAText.c_str(), NULL);
    nvgFillColor(args.vg, smallInk);
    nvgText(args.vg, rightX, 17, clockAText.c_str(), NULL);

    std::string clockBText = view->isClockBConnected() ? "EXT" : "INT";
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 120));
    nvgText(args.vg, rightX + 1, 30, clockBText.c_str(), NULL);
    nvgFillColor(args.vg, smallInk);
    nvgText(args.vg, rightX, 29, clockBText.c_str(), NULL);

    // Vintage micro-scratches overlay (match matrix spooky palette)
    nvgSave(args.vg);
    shapetaker::graphics::drawVignettePatinaScratches(args,
        0, 0, w, h, r,
        /*scratchCount*/ 26,
        /*vignette1*/ nvgRGBA(24,30,20,10),
        /*vignette2*/ nvgRGBA(50,40,22,12),
        /*patinaLayers*/ 8,
        /*scratchAlpha*/ 0.30f,
        /*scratchVariations*/ 3,
        /*seed*/ 73321u);
    nvgRestore(args.vg);

    nvgRestore(args.vg);
}

// ---- HighResMatrixWidget implementation ----

void HighResMatrixWidget::onButton(const event::Button& e) {
    if (e.action != GLFW_PRESS) return;
    if (!view) return;
    Vec pos = e.pos;
    int cols = 8, rows = 8;
    int gs = view->getGridSteps();
    if (gs == 16) { cols = rows = 4; }
    else if (gs == 32) { cols = rows = 6; }
    int x = (int)(pos.x / box.size.x * cols);
    int y = (int)(pos.y / box.size.y * rows);
    x = rack::clamp(x, 0, cols - 1);
    y = rack::clamp(y, 0, rows - 1);
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) onMatrixClick(x, y);
    else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) onMatrixRightClick(x, y);
    e.consume(this);
}

void HighResMatrixWidget::onMatrixClick(int x, int y) {
    if (!view || !ctrl) return;
    int gs = view->getGridSteps();
    int stepIndex = -1;
    if (gs == 16) stepIndex = y * 4 + x;
    else if (gs == 32) { if (y < 5) stepIndex = y * 6 + x; else if (y == 5 && x >= 2 && x <= 3) stepIndex = 30 + (x - 2); }
    else stepIndex = y * 8 + x;
    if (stepIndex < 0) return;
    if (view->isEditModeA() && stepIndex < view->getSeqALength()) { ctrl->setEditCursorA(stepIndex); ctrl->programStepA(stepIndex); }
    if (view->isEditModeB() && stepIndex < view->getSeqBLength()) { ctrl->setEditCursorB(stepIndex); ctrl->programStepB(stepIndex); }
}

void HighResMatrixWidget::onMatrixRightClick(int x, int y) {
    if (!view || !ctrl) return;
    int gs = view->getGridSteps();
    int stepIndex = -1;
    if (gs == 16) stepIndex = y * 4 + x;
    else if (gs == 32) { if (y < 5) stepIndex = y * 6 + x; else if (y == 5 && x >= 2 && x <= 3) stepIndex = 30 + (x - 2); }
    else stepIndex = y * 8 + x;
    if (stepIndex < 0) return;
    if (view->isEditModeA() && stepIndex < view->getSeqALength()) { ctrl->setEditCursorA(stepIndex); ctrl->cycleVoiceCountA(stepIndex); }
    if (view->isEditModeB() && stepIndex < view->getSeqBLength()) { ctrl->setEditCursorB(stepIndex); ctrl->cycleVoiceCountB(stepIndex); }
}

void HighResMatrixWidget::drawLayer(const DrawArgs& args, int layer) {
    if (layer == 1) drawMatrix(args);
    Widget::drawLayer(args, layer);
}

static std::vector<std::string> wrapTextLocal(const std::string& text, float maxWidth, NVGcontext* vg) {
    std::vector<std::string> lines; if (text.empty()) return lines;
    float w = nvgTextBounds(vg, 0, 0, text.c_str(), NULL, NULL);
    if (w <= maxWidth) { lines.push_back(text); return lines; }
    std::vector<std::string> words; std::stringstream ss(text); std::string word;
    while (std::getline(ss, word, ' ')) if (!word.empty()) words.push_back(word);
    if (words.empty()) return lines;
    std::string cur = words[0];
    for (size_t i = 1; i < words.size(); ++i) {
        std::string test = cur + " " + words[i];
        float tw = nvgTextBounds(vg, 0, 0, test.c_str(), NULL, NULL);
        if (tw <= maxWidth) cur = test; else { lines.push_back(cur); cur = words[i]; }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

void HighResMatrixWidget::drawMatrix(const DrawArgs& args) {
    nvgSave(args.vg);
    if (!view) { nvgRestore(args.vg); return; }
    // Base screen background (vintage TV look: deep black + neutral depth)
    {
        float radius = 8.0f;
        // Base fill: near-black for CRT glass
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, radius);
        nvgFillColor(args.vg, nvgRGBA(6, 6, 8, 255));
        nvgFill(args.vg);

        // Subtle center bulge glow (neutral gray, matches spooky preview palette)
        NVGpaint centerGlow = nvgRadialGradient(args.vg,
            box.size.x * 0.5f, box.size.y * 0.5f,
            std::min(box.size.x, box.size.y) * 0.20f,
            std::min(box.size.x, box.size.y) * 0.72f,
            nvgRGBA(36, 36, 40, 64), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, radius - 0.5f);
        nvgFillPaint(args.vg, centerGlow);
        nvgFill(args.vg);

        // Inset edge shadow to seat the screen into bezel
        NVGpaint inset = nvgBoxGradient(args.vg,
            1.5f, 1.5f, box.size.x - 3.0f, box.size.y - 3.0f,
            radius - 3.0f, 7.0f,
            nvgRGBA(0, 0, 0, 55), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, 1.0f, box.size.x - 2.0f, box.size.y - 2.0f, radius - 1.0f);
        nvgRoundedRect(args.vg, 4.0f, 4.0f, box.size.x - 8.0f, box.size.y - 8.0f, std::max(0.0f, radius - 4.0f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, inset);
        nvgFill(args.vg);

        // Curvature vignette to darken corners
        NVGpaint vignette = nvgRadialGradient(args.vg,
            box.size.x * 0.5f, box.size.y * 0.5f,
            std::min(box.size.x, box.size.y) * 0.45f,
            std::min(box.size.x, box.size.y) * 0.85f,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 38));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, radius - 0.5f);
        nvgFillPaint(args.vg, vignette);
        nvgFill(args.vg);

        // Glass reflection: soft diagonal highlight band (top-left to center)
        NVGpaint glassHi = nvgLinearGradient(args.vg,
            box.size.x * 0.12f, box.size.y * 0.10f,
            box.size.x * 0.55f, box.size.y * 0.45f,
            nvgRGBA(255, 255, 255, 14), nvgRGBA(255, 255, 255, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, 1.0f, box.size.x - 2.0f, box.size.y - 2.0f, radius - 1.0f);
        nvgFillPaint(args.vg, glassHi);
        nvgFill(args.vg);

        // (Removed fine gray outline to let bezel + glow define edges)

        // Subtle bezel ring for added depth
        float bezel = 5.5f; // ring thickness
        NVGpaint bezelPaint = nvgLinearGradient(args.vg,
            0.f, 0.f, 0.f, box.size.y,
            nvgRGBA(26, 26, 32, 220), nvgRGBA(10, 10, 14, 220));
        nvgBeginPath(args.vg);
        // Outer path
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, radius - 0.5f);
        // Inner hole (screen area)
        nvgRoundedRect(args.vg, bezel + 0.5f, bezel + 0.5f,
                       box.size.x - 2.f * bezel - 1.0f,
                       box.size.y - 2.f * bezel - 1.0f,
                       std::max(0.0f, radius - bezel - 0.5f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, bezelPaint);
        nvgFill(args.vg);

        // Bezel highlight (top-left) and shadow (bottom-right)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, bezel + 1.0f, bezel + 1.0f,
                       box.size.x - 2.f * (bezel + 1.0f),
                       box.size.y - 2.f * (bezel + 1.0f),
                       std::max(0.0f, radius - bezel - 1.0f));
        nvgStrokeWidth(args.vg, 1.2f);
        nvgStrokeColor(args.vg, nvgRGBA(210, 210, 225, 35)); // faint highlight
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, bezel - 0.5f, bezel - 0.5f,
                       box.size.x - 2.f * (bezel - 0.5f),
                       box.size.y - 2.f * (bezel - 0.5f),
                       std::max(0.0f, radius - bezel + 0.5f));
        nvgStrokeWidth(args.vg, 1.2f);
        nvgStrokeColor(args.vg, nvgRGBA(5, 5, 8, 90)); // faint shadow
        nvgStroke(args.vg);

        // Compute screen (inside bezel) rect for screen-space overlays
        float screenX = bezel + 0.5f;
        float screenY = bezel + 0.5f;
        float screenW = box.size.x - 2.0f * bezel - 1.0f;
        float screenH = box.size.y - 2.0f * bezel - 1.0f;

        // Very light scanlines overlay confined to screen area
        bool spookyLocal = view && view->getSpookyTvMode();
        // Softer, sparser scanlines
        float scanAlpha = spookyLocal ? 0.007f : 0.006f;
        float lineSpacing = spookyLocal ? 4.5f : 3.0f;
        graphics::drawScanlines(args, screenX, screenY, screenW, screenH, lineSpacing, scanAlpha);

        // Stronger perceived depth via neutral inner vignettes and bevels (no bright whites)
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        NVGpaint edgeGlow = nvgRadialGradient(args.vg,
            box.size.x * 0.5f, box.size.y * 0.5f,
            std::min(box.size.x, box.size.y) * 0.46f,
            std::min(box.size.x, box.size.y) * 0.54f,
            nvgRGBA(40,40,40,18), nvgRGBA(0,0,0,0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, radius - 0.5f);
        nvgFillPaint(args.vg, edgeGlow);
        nvgFill(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        nvgRestore(args.vg);

        // Inner bevel: top-left subtle highlight (neutral gray) and bottom-right subtle shadow
        NVGpaint innerHi = nvgLinearGradient(args.vg,
            0.5f, 0.5f, 0.5f, 8.0f,
            nvgRGBA(60, 60, 60, 20), nvgRGBA(60,60,60,0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, 1.0f, box.size.x - 2.0f, 6.0f, radius - 2.0f);
        nvgFillPaint(args.vg, innerHi);
        nvgFill(args.vg);

        NVGpaint innerShadow = nvgLinearGradient(args.vg,
            0.5f, box.size.y - 6.5f, 0.5f, box.size.y - 0.5f,
            nvgRGBA(0, 0, 0, 50), nvgRGBA(0,0,0,0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, box.size.y - 7.0f, box.size.x - 2.0f, 6.5f, radius - 2.0f);
        nvgFillPaint(args.vg, innerShadow);
        nvgFill(args.vg);
    }

    // Preview display moved later to render above grid

    // Grid (constrained to the inner screen inside the bezel)
    int cols = 8, rows = 8; int gs = view->getGridSteps();
    if (gs == 16) { cols = rows = 4; }
    else if (gs == 32) { cols = rows = 6; }

    // Compute screen rect (inside bezel) and use it as the drawing area for the grid
    const float bezel = 5.5f;
    const float screenX = bezel + 0.5f;
    const float screenY = bezel + 0.5f;
    const float screenW = box.size.x - 2.0f * bezel - 1.0f;
    const float screenH = box.size.y - 2.0f * bezel - 1.0f;

    // Pad within the screen area
    float pad = std::max(2.0f, std::min(screenW, screenH) * 0.02f);
    float innerW = screenW - pad * 2.0f, innerH = screenH - pad * 2.0f;
    float cellWidth = innerW / cols, cellHeight = innerH / rows;

    // Draw grid content positioned within the screen area (no global scissor)

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int stepIndex = -1;
            if (gs == 16) stepIndex = y * 4 + x;
            else if (gs == 32) { if (y < 5) stepIndex = y * 6 + x; else if (y == 5 && x >= 2 && x <= 3) stepIndex = 30 + (x - 2); }
            else stepIndex = y * cols + x;

            // Skip drawing cells that don't correspond to valid steps
            if (stepIndex < 0) continue;

            Vec cellPos = Vec(screenX + pad + x * cellWidth, screenY + pad + y * cellHeight);
            Vec cellCenter = Vec(cellPos.x + cellWidth/2, cellPos.y + cellHeight/2);

            bool inA = (stepIndex < view->getSeqALength());
            bool inB = (stepIndex < view->getSeqBLength());
            auto sA = view->getStepA(stepIndex);
            auto sB = view->getStepB(stepIndex);
            bool hasA = inA && (sA.chordIndex >= -2);
            bool hasB = inB && (sB.chordIndex >= -2);
            bool playheadA = (view->isSeqARunning() && view->getSeqACurrentStep() == stepIndex) || (view->isEditModeA() && inA && view->getSeqACurrentStep() == stepIndex);
            bool playheadB = (view->isSeqBRunning() && view->getSeqBCurrentStep() == stepIndex) || (view->isEditModeB() && inB && view->getSeqBCurrentStep() == stepIndex);

            // Unified cell circle size across grid modes; slightly larger for a touch less space
            float radiusFactor = 0.46f;
            float cellRadius = std::min(cellWidth, cellHeight) * radiusFactor;

            // Auto-split when both sequencers occupy the step; otherwise single/blended
            bool doubleOcc = hasA && hasB; // per-step automatic split
            if (!doubleOcc) {
                // Single occupancy: blended background indicating membership/length
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
                bool editModeHighlight = (view->isEditModeA() && hasA) || (view->isEditModeB() && hasB);
                if (playheadA && playheadB) {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(120,160,255,255), nvgRGBA(60,80,200,255)); nvgFillPaint(args.vg, p);
                } else if (playheadA) {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(0,255,180,255), nvgRGBA(0,180,120,255)); nvgFillPaint(args.vg, p);
                } else if (playheadB) {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(180,0,255,255), nvgRGBA(120,0,180,255)); nvgFillPaint(args.vg, p);
                } else if (editModeHighlight) {
                    if (hasA && view->isEditModeA()) { NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(0,150,120,200), nvgRGBA(0,80,60,200)); nvgFillPaint(args.vg, p);} 
                    else if (hasB && view->isEditModeB()) { NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(120,0,150,200), nvgRGBA(60,0,80,200)); nvgFillPaint(args.vg, p);} 
                } else if (inA && inB) {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(60,80,120,255), nvgRGBA(30,40,60,255)); nvgFillPaint(args.vg, p);
                } else if (inA) {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(0,100,70,255), nvgRGBA(0,50,35,255)); nvgFillPaint(args.vg, p);
                } else if (inB) {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(70,0,100,255), nvgRGBA(35,0,50,255)); nvgFillPaint(args.vg, p);
                } else {
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(25,25,30,255), nvgRGBA(15,15,20,255)); nvgFillPaint(args.vg, p);
                }
                nvgFill(args.vg);
            } else {
                // Double occupancy: darker neutral base + subtle separator for higher contrast
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
                // Darken the base so the active arcs "light up" more clearly
                nvgFillColor(args.vg, nvgRGBA(14,14,18,235));
                nvgFill(args.vg);

                // Softer separator
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cellCenter.x, cellCenter.y - cellRadius * 0.80f);
                nvgLineTo(args.vg, cellCenter.x, cellCenter.y + cellRadius * 0.80f);
                nvgStrokeColor(args.vg, nvgRGBA(100,100,110,70));
                nvgStrokeWidth(args.vg, 1.0f);
                nvgStroke(args.vg);
            }

            // Color the edge with sequencer colors: teal for A, purple for B (only in double occupancy)
            auto strokeArc = [&](bool left, NVGcolor col, float width){
                nvgBeginPath(args.vg);
                if (left) nvgArc(args.vg, cellCenter.x, cellCenter.y, cellRadius, (float)M_PI/2.f, (float)M_PI*1.5f, NVG_CW);
                else      nvgArc(args.vg, cellCenter.x, cellCenter.y, cellRadius, (float)-M_PI/2.f, (float)M_PI/2.f, NVG_CW);
                nvgStrokeColor(args.vg, col);
                nvgStrokeWidth(args.vg, width);
                nvgStroke(args.vg);
            };
            // Dimmer inactive arcs; brighter active arcs with subtle glow
            NVGcolor colAInactive = nvgRGBA(0,180,120,130);
            NVGcolor colBInactive = nvgRGBA(120,0,180,130);
            NVGcolor colAActive   = nvgRGBA(0,255,190,255);
            NVGcolor colBActive   = nvgRGBA(190,0,255,255);
            NVGcolor colAActiveGlow = nvgRGBA(0,255,200,70);
            NVGcolor colBActiveGlow = nvgRGBA(200,0,255,70);

            if (doubleOcc && hasA && hasB) {
                // A side
                if (playheadA) {
                    // Glow underlay then bright stroke
                    strokeArc(true,  colAActiveGlow, 5.0f);
                    strokeArc(true,  colAActive,     3.6f);
                } else {
                    strokeArc(true,  colAInactive,   1.6f);
                }
                // B side
                if (playheadB) {
                    strokeArc(false, colBActiveGlow, 5.0f);
                    strokeArc(false, colBActive,     3.6f);
                } else {
                    strokeArc(false, colBInactive,   1.6f);
                }
            }

            // Alchemical symbols (vintage off-white) and REST/TIE glyphs styled like symbols
            NVGcolor vintage = nvgRGBA(232,224,200,230);
            float minDimCell = std::min(cellWidth, cellHeight);
            // Symbol stroke matches in-cell symbol stroke weight
            float symbolStroke = rack::clamp(minDimCell * 0.020f, 1.0f, 2.0f);
            float symbolSize = minDimCell * (doubleOcc ? ((gs == 16) ? 0.13f : (gs == 32) ? 0.12f : 0.11f)
                                                      : ((gs == 16) ? 0.34f : (gs == 32) ? 0.32f : 0.30f));
            auto drawRest = [&](const Vec& c){
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, c.x - symbolSize*0.35f, c.y);
                nvgLineTo(args.vg, c.x + symbolSize*0.35f, c.y);
                nvgStrokeColor(args.vg, vintage);
                nvgStrokeWidth(args.vg, symbolStroke);
                nvgStroke(args.vg);
            };
            auto drawTie = [&](const Vec& c){
                // Flip the tie arc (draw below the center for contrast)
                nvgBeginPath(args.vg);
                float r = symbolSize * 0.45f;
                // Draw lower arc from ~205deg to ~335deg
                nvgArc(args.vg, c.x, c.y, r, M_PI * 1.15f, M_PI * 1.85f, NVG_CW);
                nvgStrokeColor(args.vg, vintage);
                nvgStrokeWidth(args.vg, symbolStroke);
                nvgStroke(args.vg);
            };

            if (doubleOcc) {
                // Draw symbols offset to the sides to avoid overlap
                // Slightly more inboard from the dots to avoid crowding
                Vec leftPos  = Vec(cellCenter.x - cellRadius * 0.36f, cellCenter.y);
                Vec rightPos = Vec(cellCenter.x + cellRadius * 0.36f, cellCenter.y);
                if (hasA) {
                    if (sA.symbolId >= 0) drawAlchemicalSymbol(args, leftPos, sA.symbolId, vintage, 0.42f);
                    else if (sA.chordIndex == -1) drawRest(leftPos);
                    else if (sA.chordIndex == -2) drawTie(leftPos);
                }
                if (hasB) {
                    if (sB.symbolId >= 0) drawAlchemicalSymbol(args, rightPos, sB.symbolId, vintage, 0.42f);
                    else if (sB.chordIndex == -1) drawRest(rightPos);
                    else if (sB.chordIndex == -2) drawTie(rightPos);
                }
            } else {
                // Single occupancy: center symbols
                if (hasA && sA.symbolId >= 0) drawAlchemicalSymbol(args, cellCenter, sA.symbolId, vintage);
                if (hasB && sB.symbolId >= 0) drawAlchemicalSymbol(args, cellCenter, sB.symbolId, vintage);
                if (hasA && sA.chordIndex == -1) drawRest(cellCenter);
                if (hasA && sA.chordIndex == -2) drawTie(cellCenter);
                if (hasB && sB.chordIndex == -1) drawRest(cellCenter);
                if (hasB && sB.chordIndex == -2) drawTie(cellCenter);
            }

            // Voice dots along side arcs (double) or centered ring (single)
            auto drawSideDots = [&](bool left, int count, NVGcolor color){
                if (count <= 0) return;
                // Left side: 120° to 240°; Right side: -60° to 60°
                float start = left ? (float)M_PI * 2.0f/3.0f : (float)-M_PI/3.0f;
                float end   = left ? (float)M_PI * 4.0f/3.0f : (float)M_PI/3.0f;
                int n = std::min(count, 6);
                float rr = cellRadius * 0.82f;
                float stepA = (n > 1) ? (end - start) / (float)(n - 1) : 0.f;
                for (int iDot = 0; iDot < n; ++iDot) {
                    float a = start + stepA * iDot;
                    float dx = rr * cosf(a);
                    float dy = rr * sinf(a);
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, cellCenter.x + dx, cellCenter.y + dy, (gs == 16) ? 2.2f : (gs == 32) ? 1.8f : 1.5f);
                    nvgFillColor(args.vg, color);
                    nvgFill(args.vg);
                }
            };
            if (doubleOcc) {
                if (hasA) drawSideDots(true,  sA.voiceCount, vintage);
                if (hasB) drawSideDots(false, sB.voiceCount, vintage);
            } else {
                // Single: reuse existing centered voice dots
                if (hasA) drawVoiceCount(args, cellCenter, sA.voiceCount, vintage);
                if (hasB) drawVoiceCount(args, cellCenter, sB.voiceCount, vintage);
            }

            // Subtle outline
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
            nvgStrokeColor(args.vg, nvgRGBA(60,60,70,80));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        }
    // (no scissor used; rely on geometry to stay within the bezel)
    }

    // Preview display (drawn above grid)
    if (!view->getDisplayChordName().empty()) {
        bool spooky = view->getSpookyTvMode();
        nvgSave(args.vg);
        float time = APP->engine->getFrame() * 0.0009f;
        float waveA = sinf(time * 0.30f) * 0.10f + sinf(time * 0.50f) * 0.06f;
        float waveB = cosf(time * 0.25f) * 0.08f + cosf(time * 0.45f) * 0.05f;
        float tapeWarp = sinf(time * 0.15f) * 0.04f + cosf(time * 0.22f) * 0.025f;
        float deepWarp = sinf(time * 0.09f) * 0.06f;
        if (spooky) {
            // Constrain spooky TV preview overlays to screen area (inside bezel)
            float radius = 8.0f; float bezel = 5.5f;
            float screenX = bezel + 0.5f;
            float screenY = bezel + 0.5f;
            float screenW = box.size.x - 2.0f * bezel - 1.0f;
            float screenH = box.size.y - 2.0f * bezel - 1.0f;
            float innerR = std::max(0.0f, radius - bezel - 0.5f);
            // Save and clip strictly to screen rect for entire spooky block
            nvgSave(args.vg);
            nvgIntersectScissor(args.vg, screenX, screenY, screenW, screenH);

            // Neutral grayscale base to avoid green tint
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, innerR);
            int base = 10 + (int)((waveA + waveB) * 10.0f);
            base = std::max(0, std::min(48, base));
            nvgFillColor(args.vg, nvgRGBA(base, base, base, 255));
            nvgFill(args.vg);

            // VHS-style noise: random thin horizontal bars and speckles (confined by scissor)
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, 0.06f);
            for (int i = 0; i < 6; ++i) {
                float y = screenY + fmodf((i * 13.37f + time * 90.0f), screenH);
                float h = 0.6f + fmodf(i * 1.7f, 1.2f);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, screenX, y, screenW, h);
                int g = 180 + (i * 11) % 50;
                nvgFillColor(args.vg, nvgRGBA(g, g, g, 255));
                nvgFill(args.vg);
            }
            for (int i = 0; i < 320; ++i) {
                float x = screenX + fmodf((i * 37.1f + time * 300.0f), screenW);
                float y = screenY + fmodf((i * 21.7f + time * 220.0f), screenH);
                nvgBeginPath(args.vg);
                float w = 0.6f + fmodf(i * 0.91f, 0.8f);
                float h = 0.6f + fmodf(i * 1.13f, 0.8f);
                nvgRect(args.vg, x, y, w, h);
                int g = 130 + (i * 19) % 120;
                nvgFillColor(args.vg, nvgRGBA(g, g, g, 150));
                nvgFill(args.vg);
            }
            nvgRestore(args.vg); // alpha
            nvgRestore(args.vg); // scissor
        }
        nvgSave(args.vg);
        // Center preview within the inner screen area (inside bezel)
        float bezel = 5.5f;
        float screenX = bezel + 0.5f;
        float screenY = bezel + 0.5f;
        float screenW = box.size.x - 2.0f * bezel - 1.0f;
        float screenH = box.size.y - 2.0f * bezel - 1.0f;
        // Reduce jumpiness by lowering wobble amplitudes and speeds
        float shakeX = sinf(time * 0.35f) * 0.18f + tapeWarp * 0.60f + deepWarp * 0.45f;
        float shakeY = cosf(time * 0.28f) * 0.14f + waveA   * 0.40f + waveB    * 0.28f;
        nvgTranslate(args.vg, (screenX + screenW * 0.5f) + shakeX, (screenY + screenH * 0.40f) + shakeY);
        nvgScale(args.vg, 5.0f, 5.0f);
        float colorCycle = sin(time * 0.3f) * 0.5f + 0.5f;
        int symbolR = 140, symbolG = 140, symbolB = 150;
        if (colorCycle < 0.25f) { symbolR = 0; symbolG = 180 + (int)(waveA * 50); symbolB = 180 + (int)(waveB * 50); }
        else if (colorCycle < 0.5f) { symbolR = 180 + (int)(waveA * 50); symbolG = 0; symbolB = 255; }
        else if (colorCycle < 0.75f) { symbolR = 60 + (int)(waveB * 30); symbolG = 120 + (int)(waveA * 40); symbolB = 80 + (int)(tapeWarp * 80); }
        // Draw preview symbol or REST/TIE glyphs
        int dispId = view->getDisplaySymbolId();
        NVGcolor mainCol = nvgRGBA(symbolR, symbolG, symbolB, 220);
        auto drawRestPreview = [&](NVGcolor col, float stroke){
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -6.0f, 0.0f);
            nvgLineTo(args.vg, 6.0f, 0.0f);
            nvgStrokeColor(args.vg, col);
            nvgStrokeWidth(args.vg, stroke);
            nvgStroke(args.vg);
        };
        auto drawTiePreview = [&](NVGcolor col, float stroke){
            nvgBeginPath(args.vg);
            // Flipped tie (lower arc)
            nvgArc(args.vg, 0.0f, 0.0f, 7.0f, M_PI * 1.15f, M_PI * 1.85f, NVG_CW);
            nvgStrokeColor(args.vg, col);
            nvgStrokeWidth(args.vg, stroke);
            nvgStroke(args.vg);
        };
        if (dispId >= 0) {
            if (spooky) {
                // Additive halos, RGB ghosts, and soft multi-pass blur with minimal drift
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                // Base halo
                shapetaker::graphics::drawAlchemicalSymbol(args, Vec(-0.5f, -0.25f), dispId, nvgRGBA(255, 255, 255, 22), 10.3f, 1.00f);
                // RGB ghosts
                shapetaker::graphics::drawAlchemicalSymbol(args, Vec(-0.6f, 0.0f),  dispId, nvgRGBA(255,  30,  30, 70), 10.1f, 1.02f);
                shapetaker::graphics::drawAlchemicalSymbol(args, Vec( 0.6f, 0.0f),  dispId, nvgRGBA( 30, 255,  30, 70), 10.1f, 1.02f);
                shapetaker::graphics::drawAlchemicalSymbol(args, Vec( 0.0f, 0.6f),  dispId, nvgRGBA( 30, 130, 255, 70), 10.1f, 1.02f);
                // Static blur ring (8 directions) with slight, slow drift to reduce jumpiness
                const int passes = 10;
                float baseR = 0.7f; // base blur radius
                float slow = sinf(time * 0.15f) * 0.12f; // very slow micro‑movement
                for (int i = 0; i < passes; ++i) {
                    float ang = (2.f * M_PI * i) / passes;
                    float rr = baseR + slow; // tiny drift
                    float jx = cosf(ang) * rr;
                    float jy = sinf(ang) * rr;
                    NVGcolor haze = nvgRGBA(255, 255, 255, 20);
                    shapetaker::graphics::drawAlchemicalSymbol(args, Vec(jx, jy), dispId, haze, 10.2f, 0.98f);
                }
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            }
            // Main readable symbol (kept smaller stroke; blur above does the softening)
            shapetaker::graphics::drawAlchemicalSymbol(args, Vec(0, 0), dispId, mainCol, 10.0f, 1.06f);
        } else if (dispId == -1) { // REST
            if (spooky) {
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                drawRestPreview(nvgRGBA(255,255,255,40), 1.1f);
                drawRestPreview(nvgRGBA(255,0,0,90), 1.05f);
                drawRestPreview(nvgRGBA(0,255,0,90), 1.05f);
                drawRestPreview(nvgRGBA(0,128,255,90), 1.05f);
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            }
            drawRestPreview(mainCol, 1.2f);
        } else if (dispId == -2) { // TIE
            if (spooky) {
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                drawTiePreview(nvgRGBA(255,255,255,40), 1.1f);
                drawTiePreview(nvgRGBA(255,0,0,90), 1.05f);
                drawTiePreview(nvgRGBA(0,255,0,90), 1.05f);
                drawTiePreview(nvgRGBA(0,128,255,90), 1.05f);
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
            }
            drawTiePreview(mainCol, 1.2f);
        }
        nvgRestore(args.vg);
        // Text
        // Larger, more readable chord name (auto-fit to inner screen)
        float baseFont = 50.f;
        nvgFontSize(args.vg, baseFont);
        // Keep titles well inside the inner screen bounds to avoid spillover
        float bezelT = 5.5f;
        float screenXT = bezelT + 0.5f;
        float screenYT = bezelT + 0.5f;
        float screenWT = box.size.x - 2.0f * bezelT - 1.0f;
        float screenHT = box.size.y - 2.0f * bezelT - 1.0f;
        float maxTextWidth = screenWT * 0.72f;
        std::string title = view->getDisplayChordName();

        auto measureWidth = [&](const std::string& s) {
            float b[4] = {0};
            nvgTextBounds(args.vg, 0, 0, s.c_str(), NULL, b);
            return b[2] - b[0];
        };
        auto maxLineWidth = [&](const std::vector<std::string>& ls){
            float m = 0.f; for (auto& t : ls) m = std::max(m, measureWidth(t)); return m; };

        // Determine allowed text block height (smaller if symbol is shown)
        float allowedH = (dispId == -999) ? (screenHT * 0.54f) : (screenHT * 0.34f);

        // Iteratively fit font size so longest line <= maxTextWidth and total height <= allowedH
        std::vector<std::string> lines = wrapTextLocal(title, maxTextWidth, args.vg);
        float asc = 0.f, desc = 0.f, lineh = 0.f;
        nvgTextMetrics(args.vg, &asc, &desc, &lineh);
        float lineH = lineh * 1.35f;
        auto totalH = [&](){ return (float)std::max<size_t>(1, lines.size()) * lineH; };
        int it = 0; const int maxIt = 3;
        while (it++ < maxIt) {
            float w = maxLineWidth(lines);
            float scaleW = (w > 1.f) ? (maxTextWidth / w) : 1.f;
            float scaleH = (totalH() > 1.f) ? (allowedH / totalH()) : 1.f;
            float scale = std::min(1.f, std::min(scaleW, scaleH));
            if (scale >= 0.999f) break;
            baseFont *= scale;
            baseFont = std::max(16.f, baseFont);
            nvgFontSize(args.vg, baseFont);
            nvgTextMetrics(args.vg, &asc, &desc, &lineh);
            lineH = lineh * 1.35f;
            lines = wrapTextLocal(title, maxTextWidth, args.vg);
        }

        // Final vertical anchor: center multi-line block around target y
        float textY = (dispId == -999) ? (screenYT + screenHT * 0.52f) : (screenYT + screenHT * 0.79f);
        float blockOffset = ((float)lines.size() - 1.f) * lineH * 0.5f;
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        // Helper to draw letter-spaced, centered text (used only for "TIE")
        auto drawSpacedCentered = [&](const std::string& s, float cx, float cy, float tracking, NVGcolor color) {
            // Measure per-character widths
            std::vector<float> w; w.reserve(s.size());
            float total = 0.f;
            for (char c : s) {
                char buf[2] = {c, '\0'};
                float bounds[4] = {};
                nvgTextBounds(args.vg, 0, 0, buf, NULL, bounds);
                float cw = bounds[2] - bounds[0];
                w.push_back(cw);
                total += cw;
            }
            float totalWithTracking = total + tracking * std::max<int>(0, (int)s.size() - 1);
            float x = cx - totalWithTracking * 0.5f;
            nvgFillColor(args.vg, color);
            for (size_t idx = 0; idx < s.size(); ++idx) {
                char buf[2] = {s[idx], '\0'};
                nvgText(args.vg, x, cy, buf, NULL);
                x += w[idx] + tracking;
            }
        };

        // Clip text strictly to inner screen to prevent spillover
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, screenXT, screenYT, screenWT, screenHT);
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string s = lines[i];
            float cx = screenXT + screenWT / 2.0f;
            float cy = textY - blockOffset + i * lineH;
            float tracking = 6.0f; // used only when s == "TIE"
            if (spooky) {
                // Stronger blur glow and dynamic RGB ghosting to match symbol fuzziness
                nvgSave(args.vg);
                float t = time + i * 0.13f;
                float jx = sinf(t * 3.3f) * 0.8f;
                float jy = cosf(t * 2.7f) * 0.6f;
                nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
                // Wide glow
                nvgFontBlur(args.vg, 3.1f);
                if (s == "TIE") drawSpacedCentered(s, cx + jx, cy + jy, tracking, nvgRGBA(255, 255, 255, 85));
                else { nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 85)); nvgText(args.vg, cx + jx, cy + jy, s.c_str(), NULL); }
                // RGB ghosts with small animated offsets
                nvgFontBlur(args.vg, 1.6f);
                if (s == "TIE") {
                    drawSpacedCentered(s, cx - 1.1f + jx * 0.6f, cy + jy * 0.3f, tracking, nvgRGBA(255, 0, 0, 150));
                    drawSpacedCentered(s, cx + 1.1f + jx * 0.6f, cy + jy * 0.3f, tracking, nvgRGBA(0, 255, 0, 130));
                    drawSpacedCentered(s, cx + jx * 0.3f, cy + 1.1f + jy * 0.6f, tracking, nvgRGBA(0, 128, 255, 130));
                } else {
                    nvgFillColor(args.vg, nvgRGBA(255, 0, 0, 150)); nvgText(args.vg, cx - 1.1f + jx * 0.6f, cy + jy * 0.3f, s.c_str(), NULL);
                    nvgFillColor(args.vg, nvgRGBA(0, 255, 0, 130)); nvgText(args.vg, cx + 1.1f + jx * 0.6f, cy + jy * 0.3f, s.c_str(), NULL);
                    nvgFillColor(args.vg, nvgRGBA(0, 128, 255, 130)); nvgText(args.vg, cx + jx * 0.3f, cy + 1.1f + jy * 0.6f, s.c_str(), NULL);
                }
                nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
                nvgRestore(args.vg);
            }
            // Main readable text (slightly blurred so it doesn't look crisp)
            nvgFontBlur(args.vg, spooky ? 0.9f : 0.0f);
            if (s == "TIE") drawSpacedCentered(s, cx, cy, tracking, nvgRGBA(232, 224, 200, spooky ? 205 : 235));
            else { nvgFillColor(args.vg, nvgRGBA(232, 224, 200, spooky ? 205 : 235)); nvgText(args.vg, cx, cy, s.c_str(), NULL); }
            if (spooky) nvgFontBlur(args.vg, 0.0f);
        }
        nvgRestore(args.vg); // scissor
        // end text
        if (spooky) {
            nvgSave(args.vg);
            graphics::drawVignettePatinaScratches(args, 0, 0, box.size.x, box.size.y, 8.0f,
                                            26, nvgRGBA(24,30,20,10), nvgRGBA(50,40,22,12), 8, 0.30f, 3, 73321u);
            nvgRestore(args.vg);
        }
        nvgRestore(args.vg);
    }

    // Edit-mode border glow
    if (view->isEditModeA() || view->isEditModeB()) {
        nvgSave(args.vg); nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        float time = system::getTime(); float pulse = 0.4f + 0.3f * sin(time * 3.0f);
        NVGcolor glow  = view->isEditModeA() ? nvgRGBA(0,255,180, pulse*150) : nvgRGBA(180,0,255, pulse*150);
        NVGcolor halo1 = view->isEditModeA() ? nvgRGBA(0,255,180, pulse*70)  : nvgRGBA(180,0,255, pulse*70);
        NVGcolor halo2 = view->isEditModeA() ? nvgRGBA(0,255,180, pulse*40)  : nvgRGBA(180,0,255, pulse*40);
        // Draw the glow around the inner screen (inside the bezel), not the full widget
        float radius = 8.0f; float bezel = 5.5f;
        float screenX = bezel + 0.5f;
        float screenY = bezel + 0.5f;
        float screenW = box.size.x - 2.0f * bezel - 1.0f;
        float screenH = box.size.y - 2.0f * bezel - 1.0f;
        float innerR  = std::max(0.0f, radius - bezel - 0.5f);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, screenX, screenY, screenW, screenH, innerR);
        // Outer halo layers (broad, soft)
        nvgStrokeColor(args.vg, halo2); nvgStrokeWidth(args.vg, 10.0f); nvgStroke(args.vg);
        nvgStrokeColor(args.vg, halo1); nvgStrokeWidth(args.vg, 6.0f);  nvgStroke(args.vg);
        // Inner crisp glow
        nvgStrokeColor(args.vg, glow);  nvgStrokeWidth(args.vg, 2.0f);  nvgStroke(args.vg);
        nvgRestore(args.vg);
    }

    // Vintage overlay (skip in spooky mode to preserve deep blacks)
    if (!view->getSpookyTvMode()) {
        nvgSave(args.vg);
        graphics::drawVignettePatinaScratches(args, 0, 0, box.size.x, box.size.y, 8.0f, 26,
                                        nvgRGBA(24,30,20,10), nvgRGBA(50,40,22,12), 8, 0.30f, 3, 73321u);
        nvgRestore(args.vg);
    }

    nvgRestore(args.vg);
}

void HighResMatrixWidget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color, float scale) {
    // Scale symbol to cell size for better visibility across grid densities
    int cols = 8, rows = 8; int gs = view ? view->getGridSteps() : 64;
    if (gs == 16) { cols = rows = 4; }
    else if (gs == 32) { cols = rows = 6; }
    float cellW = box.size.x / cols;
    float cellH = box.size.y / rows;
    float minDim = std::min(cellW, cellH);
    // Match cell circle radius computation used in drawMatrix()
    // Match unified circle sizing used in drawMatrix
    float radiusFactor = 0.46f;
    float circleR = minDim * radiusFactor;
    // Target the symbol to occupy a safe portion of the inner circle to avoid voice dots
    float symbolRadius = circleR * 0.58f * scale; // allow caller to shrink for double occupancy
    float strokeW = rack::clamp(minDim * 0.020f, 1.0f, 2.0f); // tuned stroke for clarity
    shapetaker::graphics::drawAlchemicalSymbol(args, pos, symbolId, color, symbolRadius, strokeW);
}

void HighResMatrixWidget::drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor) {
    nvgSave(args.vg);
    int cols = 8, rows = 8; int gs = view ? view->getGridSteps() : 64;
    if (gs == 16) { cols = rows = 4; } else if (gs == 32) { cols = rows = 6; }
    float cellWidth = box.size.x / cols; float cellHeight = box.size.y / rows;
    // Match unified circle sizing used in drawMatrix
    float radiusFactor = 0.46f;
    float circleR = std::min(cellWidth, cellHeight) * radiusFactor;
    // Slightly smaller dots so they don't collide with the symbol
    float dotR = (gs == 16) ? 2.2f : (gs == 32) ? 1.8f : 1.5f;
    // Keep dots on an inner ring proportionally inside the cell circle
    float ringR = circleR * 0.80f;
    graphics::drawVoiceCountDots(args, pos, voiceCount, ringR, dotR, dotColor);
    nvgRestore(args.vg);
}

// AlchemicalSymbolWidget implementation

int AlchemicalSymbolWidget::getSymbolId() {
    if (!view) return buttonPosition; // Fallback to button position if no view
    return view->getButtonSymbol(buttonPosition);
}

void AlchemicalSymbolWidget::draw(const DrawArgs& args) {
    int symbolId = getSymbolId();
    bool isSelected = view && view->getSelectedSymbol() == symbolId;
    bool inEditMode = view && (view->isEditModeA() || view->isEditModeB());
    float press = view ? view->getButtonPressAnim(buttonPosition) : 0.0f;
    
    // Draw button background with enhanced states
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
    
    // Check which sequence(s) are playing this symbol for background color
    bool playheadA = false, playheadB = false;
    if (view) {
        int currentChordA = view->getCurrentChordIndex(true);  // seqA = true
        int currentChordB = view->getCurrentChordIndex(false); // seqB = false
        
        if (view->isSeqARunning() && currentChordA == symbolId) {
            playheadA = true;
        }
        if (view->isSeqBRunning() && currentChordB == symbolId) {
            playheadB = true;
        }
    }
    
    if (playheadA && playheadB) {
        // Both sequences playing - mixed color background
        nvgFillColor(args.vg, nvgRGBA(90, 127, 217, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(90, 127, 217, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (playheadA) {
        // Sequence A playing - teal background
        nvgFillColor(args.vg, nvgRGBA(0, 154, 122, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(0, 154, 122, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (playheadB) {
        // Sequence B playing - purple background
        nvgFillColor(args.vg, nvgRGBA(111, 31, 183, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(111, 31, 183, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (isSelected && inEditMode) {
        // Selected for editing — blink to indicate "awaiting placement"
        double t = system::getTime();
        float pulse01 = 0.5f + 0.5f * std::sin(t * 6.0f); // ~3Hz blink
        int fillA = (int)(90 + pulse01 * 80);  // 90..170
        int strokeA = (int)(140 + pulse01 * 115); // 140..255
        nvgFillColor(args.vg, nvgRGBA(0, 200, 255, fillA));
        nvgFill(args.vg);
        // Outer stroke with pulsating alpha
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 255, strokeA));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
        // Soft additive glow ring
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 4);
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 255, (int)(50 + pulse01 * 60))); // 50..110
        nvgStrokeWidth(args.vg, 1.5f);
        nvgStroke(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        nvgRestore(args.vg);
    } else if (inEditMode) {
        // In edit mode but not selected - subtle highlight
        nvgFillColor(args.vg, nvgRGBA(60, 60, 80, 120));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(140, 140, 160, 200));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
    } else {
        // Normal state
        nvgFillColor(args.vg, nvgRGBA(40, 40, 40, 100));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 150));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
    }
    
    // Bezel/depth for button to feel integrated with panel
    {
        float inset = 1.0f;
        float rOuter = 3.0f;
        float rInner = std::max(0.0f, rOuter - 1.0f);
        // Inner shadow ring
        NVGpaint innerShadow = nvgBoxGradient(
            args.vg,
            inset, inset,
            box.size.x - inset * 2.0f,
            box.size.y - inset * 2.0f,
            rInner, 3.5f,
            nvgRGBA(0, 0, 0, 50),
            nvgRGBA(0, 0, 0, 0)
        );
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        nvgFillPaint(args.vg, innerShadow);
        nvgFill(args.vg);
        
        // Top highlight
        nvgSave(args.vg);
        nvgScissor(args.vg, 0, 0, box.size.x, std::min(6.0f, box.size.y));
        NVGpaint topHi = nvgLinearGradient(args.vg, 0, 0, 0, 6.0f, nvgRGBA(255, 255, 255, 28), nvgRGBA(255, 255, 255, 0));
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset + 0.5f, inset + 0.5f, box.size.x - (inset + 1.0f), 5.0f, rInner);
        nvgFillPaint(args.vg, topHi);
        nvgFill(args.vg);
        nvgRestore(args.vg);
        
        // Left and right inner highlights (very subtle)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint leftHi = nvgLinearGradient(args.vg, inset - 0.5f, 0, inset + 4.5f, 0, nvgRGBA(255, 255, 255, 18), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, leftHi);
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
        nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
        nvgPathWinding(args.vg, NVG_HOLE);
        NVGpaint rightHi = nvgLinearGradient(args.vg, box.size.x - (inset - 0.5f), 0, box.size.x - (inset + 4.5f), 0, nvgRGBA(255, 255, 255, 12), nvgRGBA(255, 255, 255, 0));
        nvgFillPaint(args.vg, rightHi);
        nvgFill(args.vg);
    }

    // Vintage face treatment: vignette + patina + micro-scratches
    {
        float r = 3.0f;
        // Subtle vignette to darken edges
        NVGpaint vignette = nvgRadialGradient(
            args.vg,
            box.size.x * 0.5f, box.size.y * 0.5f,
            std::min(box.size.x, box.size.y) * 0.2f,
            std::min(box.size.x, box.size.y) * 0.6f,
            nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 28)
        );
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, r);
        nvgFillPaint(args.vg, vignette);
        nvgFill(args.vg);

        // Patina tint (very subtle greenish/sepia film)
        NVGpaint patina = nvgLinearGradient(
            args.vg,
            0, 0, box.size.x, box.size.y,
            nvgRGBA(20, 30, 18, 12), nvgRGBA(50, 40, 20, 10)
        );
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, 1.0f, box.size.x - 2.0f, box.size.y - 2.0f, r - 1.0f);
        nvgFillPaint(args.vg, patina);
        nvgFill(args.vg);

        // Micro-scratches (static, low alpha)
        unsigned seed = 14621u + (unsigned)buttonPosition * 9283u;
        auto rnd = [&]() {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (seed & 0xFFFF) / 65535.f; };
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 14));
        nvgStrokeWidth(args.vg, 0.6f);
        for (int i = 0; i < 3; ++i) {
            float x1 = rnd() * (box.size.x * 0.7f) + box.size.x * 0.15f;
            float y1 = rnd() * (box.size.y * 0.7f) + box.size.y * 0.15f;
            float dx = (rnd() - 0.5f) * (box.size.x * 0.25f);
            float dy = (rnd() - 0.5f) * (box.size.y * 0.25f);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x1 + dx, y1 + dy);
            nvgStroke(args.vg);
        }
    }
    
    // Draw the alchemical symbol with a more vintage look
    // Slight "depress" on press animation
    nvgSave(args.vg);
    nvgTranslate(args.vg, 0, press * 1.0f);
    drawAlchemicalSymbol(args, Vec(box.size.x/2, box.size.y/2), symbolId);
    nvgRestore(args.vg);
}

void AlchemicalSymbolWidget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId) {
    nvgSave(args.vg);
    // Clip to the inner button area so strokes never bleed outside
    float clipMargin = 1.0f;
    nvgIntersectScissor(args.vg, clipMargin, clipMargin, box.size.x - 2.f * clipMargin, box.size.y - 2.f * clipMargin);
    nvgTranslate(args.vg, pos.x, pos.y);
    
    // Set drawing properties for button symbols (vintage ink look)
    double t = system::getTime();
    // Warm off-white ink tones
    NVGcolor ink = nvgRGBA(232, 224, 200, 255);
    NVGcolor inkFill = nvgRGBA(232, 224, 200, 190);
    // Slight stroke width wobble for hand-drawn feel
    float wobble = 1.2f * (1.0f + 0.08f * std::sin(t * 7.0 + buttonPosition * 1.37f));
    // Tiny rotation jitter to simulate imperfect stamp
    float jitter = 0.010f * std::sin(t * 2.5 + buttonPosition * 0.77f);
    nvgRotate(args.vg, jitter);
    nvgStrokeColor(args.vg, ink);
    nvgFillColor(args.vg, inkFill);
    nvgStrokeWidth(args.vg, wobble);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);
    
    // Scale symbol to button size while keeping margins
    float size = std::min(box.size.x, box.size.y) * 0.40f;
    
    // Delegate actual symbol geometry to shared utility
    shapetaker::graphics::drawAlchemicalSymbol(args, Vec(0, 0), symbolId, ink, size, wobble);
    nvgRestore(args.vg);
}

void AlchemicalSymbolWidget::onButton(const event::Button& e) {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && ctrl) {
        int symbolId = getSymbolId();
        ctrl->onSymbolPressed(symbolId);
        e.consume(this);
    }
    Widget::onButton(e);
}

// ---- RestTieMomentary implementation ----

void RestTieMomentary::draw(const DrawArgs& args) {
    // Determine playhead active states based on current chord indices
    bool playA = false, playB = false;
    if (view) {
        int target = isRest ? -1 : -2;
        if (view->isSeqARunning() && view->getCurrentChordIndex(true) == target)
            playA = true;
        if (view->isSeqBRunning() && view->getCurrentChordIndex(false) == target)
            playB = true;
    }

    // Draw button background styled like alchemical symbols
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);

    if (playA && playB) {
        nvgFillColor(args.vg, nvgRGBA(90, 127, 217, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(90, 127, 217, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (playA) {
        nvgFillColor(args.vg, nvgRGBA(0, 154, 122, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(0, 154, 122, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (playB) {
        nvgFillColor(args.vg, nvgRGBA(111, 31, 183, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(111, 31, 183, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else {
        nvgFillColor(args.vg, nvgRGBA(40, 40, 40, 100));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 150));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
    }

    // Inner shadow and highlights for depth
    float inset = 1.0f; float rOuter = 3.0f; float rInner = std::max(0.0f, rOuter - 1.0f);
    NVGpaint innerShadow = nvgBoxGradient(
        args.vg,
        inset, inset,
        box.size.x - inset * 2.0f,
        box.size.y - inset * 2.0f,
        rInner, 3.5f,
        nvgRGBA(0, 0, 0, 50), nvgRGBA(0, 0, 0, 0)
    );
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
    nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
    nvgPathWinding(args.vg, NVG_HOLE);
    nvgFillPaint(args.vg, innerShadow);
    nvgFill(args.vg);

    nvgSave(args.vg);
    nvgScissor(args.vg, 0, 0, box.size.x, std::min(6.0f, box.size.y));
    NVGpaint topHi = nvgLinearGradient(args.vg, 0, 0, 0, 6.0f, nvgRGBA(255, 255, 255, 28), nvgRGBA(255, 255, 255, 0));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
    nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
    nvgPathWinding(args.vg, NVG_HOLE);
    nvgFillPaint(args.vg, topHi);
    nvgFill(args.vg);
    nvgRestore(args.vg);

    // Side highlights
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
    nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
    nvgPathWinding(args.vg, NVG_HOLE);
    NVGpaint leftHi = nvgLinearGradient(args.vg, inset - 0.5f, 0, inset + 4.5f, 0, nvgRGBA(255, 255, 255, 18), nvgRGBA(255, 255, 255, 0));
    nvgFillPaint(args.vg, leftHi);
    nvgFill(args.vg);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, inset - 0.5f, inset - 0.5f, box.size.x - (inset - 0.5f) * 2.0f, box.size.y - (inset - 0.5f) * 2.0f, rInner + 0.5f);
    nvgRoundedRect(args.vg, inset + 0.8f, inset + 0.8f, box.size.x - (inset + 0.8f) * 2.0f, box.size.y - (inset + 0.8f) * 2.0f, std::max(0.0f, rInner - 0.8f));
    nvgPathWinding(args.vg, NVG_HOLE);
    NVGpaint rightHi = nvgLinearGradient(args.vg, box.size.x - (inset - 0.5f), 0, box.size.x - (inset + 4.5f), 0, nvgRGBA(255, 255, 255, 12), nvgRGBA(255, 255, 255, 0));
    nvgFillPaint(args.vg, rightHi);
    nvgFill(args.vg);

    // Vintage glyph (REST line or TIE arc)
    NVGcolor ink = nvgRGBA(232, 224, 200, 230);
    if (isRest) {
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float w = std::min(box.size.x, box.size.y) * 0.60f;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, cx - w * 0.5f, cy);
        nvgLineTo(args.vg, cx + w * 0.5f, cy);
        nvgStrokeColor(args.vg, ink);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgStrokeWidth(args.vg, rack::clamp(w * 0.10f, 1.0f, 2.0f));
        nvgStroke(args.vg);
    } else {
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.52f;
        float r = std::min(box.size.x, box.size.y) * 0.32f;
        nvgBeginPath(args.vg);
        nvgArc(args.vg, cx, cy, r, M_PI * 1.15f, M_PI * 1.85f, NVG_CW);
        nvgStrokeColor(args.vg, ink);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgStrokeWidth(args.vg, rack::clamp(r * 0.28f, 1.0f, 2.0f));
        nvgStroke(args.vg);
    }

    // Additive outer glow when active
    if (playA || playB) {
        nvgSave(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        NVGcolor glow = playA && playB ? nvgRGBA(90,127,217,90) : playA ? nvgRGBA(0,255,180,90) : nvgRGBA(180,0,255,90);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -1.0f, -1.0f, box.size.x + 2.0f, box.size.y + 2.0f, 4);
        nvgStrokeColor(args.vg, glow);
        nvgStrokeWidth(args.vg, 1.6f);
        nvgStroke(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
        nvgRestore(args.vg);
    }

    // Pressed overlay for feedback
    bool pressed = false;
    if (getParamQuantity()) pressed = getParamQuantity()->getValue() > 0.5f;
    if (pressed) {
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 40));
        nvgFill(args.vg);
        nvgRestore(args.vg);
    }
}
