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
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
    nvgFillColor(args.vg, nvgRGBA(20, 25, 30, 200));
    nvgFill(args.vg);
    nvgStrokeColor(args.vg, nvgRGBA(80, 90, 100, 150));
    nvgStrokeWidth(args.vg, 1.0f);
    nvgStroke(args.vg);

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
    int cols = 8, rows = 8;
    int gs = view->getGridSteps();
    if (gs == 16) { cols = rows = 4; }
    else if (gs == 32) { cols = rows = 6; }
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
    int cols = 8, rows = 8;
    int gs = view->getGridSteps();
    if (gs == 16) { cols = rows = 4; }
    else if (gs == 32) { cols = rows = 6; }
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
    
    // Preview display
    if (view->getDisplaySymbolId() != -999 && !view->getDisplayChordName().empty()) {
        bool spooky = view->getSpookyTvMode();
        nvgSave(args.vg);
        float time = APP->engine->getFrame() * 0.0009f;
        float waveA = sinf(time * 0.30f) * 0.10f + sinf(time * 0.50f) * 0.06f;
        float waveB = cosf(time * 0.25f) * 0.08f + cosf(time * 0.45f) * 0.05f;
        float tapeWarp = sinf(time * 0.15f) * 0.04f + cosf(time * 0.22f) * 0.025f;
        float deepWarp = sinf(time * 0.09f) * 0.06f;
        if (spooky) {
            nvgBeginPath(args.vg); nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8.0f);
            nvgFillColor(args.vg, nvgRGBA(8 + (int)(waveA * 12), 6 + (int)(waveA * 10), 12 + (int)(waveB * 20), 255)); nvgFill(args.vg);
            nvgSave(args.vg); nvgIntersectScissor(args.vg, 0, 0, box.size.x, box.size.y);
            // VHS state structure needs to be defined in graphics namespace
            // For now, draw a simple spooky effect without the complex VHS state
            nvgSave(args.vg);
            nvgGlobalAlpha(args.vg, 0.1f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGB(50, 255, 50));
            nvgFill(args.vg);
            nvgRestore(args.vg);
            nvgRestore(args.vg);
        }
        nvgSave(args.vg);
        float shakeX = sinf(time * 0.55f) * 0.5f + tapeWarp * 1.8f + deepWarp * 1.4f;
        float shakeY = cosf(time * 0.40f) * 0.4f + waveA * 1.2f + waveB * 0.8f;
        nvgTranslate(args.vg, box.size.x / 2 + shakeX, box.size.y * 0.40f + shakeY);
        nvgScale(args.vg, 5.0f, 5.0f);
        float colorCycle = sin(time * 0.3f) * 0.5f + 0.5f;
        int symbolR = 140, symbolG = 140, symbolB = 150;
        if (colorCycle < 0.25f) { symbolR = 0; symbolG = 180 + (int)(waveA * 50); symbolB = 180 + (int)(waveB * 50); }
        else if (colorCycle < 0.5f) { symbolR = 180 + (int)(waveA * 50); symbolG = 0; symbolB = 255; }
        else if (colorCycle < 0.75f) { symbolR = 60 + (int)(waveB * 30); symbolG = 120 + (int)(waveA * 40); symbolB = 80 + (int)(tapeWarp * 80); }
        graphics::drawAlchemicalSymbol(args, Vec(0, 0), view->getDisplaySymbolId(), nvgRGBA(symbolR, symbolG, symbolB, 255));
        nvgRestore(args.vg);
        // Text
        nvgFontSize(args.vg, 11);
        float maxTextWidth = box.size.x * 0.80f;
        auto lines = wrapTextLocal(view->getDisplayChordName(), maxTextWidth, args.vg);
        float textY = box.size.y * 0.68f; float lineH = 12.f;
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (size_t i = 0; i < lines.size(); ++i) {
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 120));
            nvgText(args.vg, box.size.x/2 + 1, textY + i * lineH + 1, lines[i].c_str(), NULL);
            nvgFillColor(args.vg, nvgRGBA(232, 224, 200, 230));
            nvgText(args.vg, box.size.x/2, textY + i * lineH, lines[i].c_str(), NULL);
        }
        if (spooky) {
            nvgSave(args.vg);
            graphics::drawVignettePatinaScratches(args, 0, 0, box.size.x, box.size.y, 8.0f,
                                            26, nvgRGBA(24,30,20,10), nvgRGBA(50,40,22,12), 8, 0.5f, 3, 73321u);
            nvgRestore(args.vg);
        }
        nvgRestore(args.vg);
    }

    // Grid
    int cols = 8, rows = 8; int gs = view->getGridSteps();
    if (gs == 16) { cols = rows = 4; }
    else if (gs == 32) { cols = rows = 6; }
    float pad = std::max(2.0f, std::min(box.size.x, box.size.y) * 0.02f);
    float innerW = box.size.x - pad * 2.0f, innerH = box.size.y - pad * 2.0f;
    float cellWidth = innerW / cols, cellHeight = innerH / rows;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int stepIndex = -1;
            if (gs == 16) stepIndex = y * 4 + x;
            else if (gs == 32) { if (y < 5) stepIndex = y * 6 + x; else if (y == 5 && x >= 2 && x <= 3) stepIndex = 30 + (x - 2); }
            else stepIndex = y * cols + x;

            // Skip drawing cells that don't correspond to valid steps
            if (stepIndex < 0) continue;

            Vec cellPos = Vec(pad + x * cellWidth, pad + y * cellHeight);
            Vec cellCenter = Vec(cellPos.x + cellWidth/2, cellPos.y + cellHeight/2);

            bool inA = (stepIndex < view->getSeqALength());
            bool inB = (stepIndex < view->getSeqBLength());
            auto sA = view->getStepA(stepIndex);
            auto sB = view->getStepB(stepIndex);
            bool hasA = inA && (sA.chordIndex >= -2);
            bool hasB = inB && (sB.chordIndex >= -2);
            bool playheadA = (view->isSeqARunning() && view->getSeqACurrentStep() == stepIndex) || (view->isEditModeA() && inA && view->getSeqACurrentStep() == stepIndex);
            bool playheadB = (view->isSeqBRunning() && view->getSeqBCurrentStep() == stepIndex) || (view->isEditModeB() && inB && view->getSeqBCurrentStep() == stepIndex);

            nvgBeginPath(args.vg);
            float radiusFactor = 0.42f; if (gs == 32) radiusFactor = 0.44f; else if (gs == 16) radiusFactor = 0.40f;
            float cellRadius = std::min(cellWidth, cellHeight) * radiusFactor;
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

            if (view->isEditModeA() || view->isEditModeB()) {
                if (view->isEditModeA() && inA && !hasA && !playheadA) { 
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(0,180,140,120), nvgRGBA(0,80,60,60)); 
                    nvgFillPaint(args.vg, p);
                    nvgFill(args.vg);
                } 
                if (view->isEditModeB() && inB && !hasB && !playheadB) { 
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
                    NVGpaint p = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius, nvgRGBA(140,0,180,120), nvgRGBA(60,0,80,60)); 
                    nvgFillPaint(args.vg, p);
                    nvgFill(args.vg);
                } 
            }

            // Alchemical symbols
            if (hasA && sA.symbolId >= 0) {
                drawAlchemicalSymbol(args, cellCenter, sA.symbolId, nvgRGBA(0,255,180,180));
            }
            if (hasB && sB.symbolId >= 0) {
                drawAlchemicalSymbol(args, cellCenter, sB.symbolId, nvgRGBA(180,0,255,180));
            }

            // Voice dots
            if (hasA) drawVoiceCount(args, cellCenter, sA.voiceCount, nvgRGBA(0,255,180,220));
            if (hasB) drawVoiceCount(args, cellCenter, sB.voiceCount, nvgRGBA(180,0,255,220));

            // Playhead ring
            if (playheadA || playheadB) {
                float ringR = cellRadius + 1.0f;
                if (playheadA && playheadB) {
                    nvgBeginPath(args.vg); nvgArc(args.vg, cellCenter.x, cellCenter.y, ringR, -M_PI, 0, NVG_CW);
                    nvgStrokeColor(args.vg, nvgRGBA(0,255,180,220)); nvgStrokeWidth(args.vg, 2.0f); nvgStroke(args.vg);
                    nvgBeginPath(args.vg); nvgArc(args.vg, cellCenter.x, cellCenter.y, ringR, 0, M_PI, NVG_CW);
                    nvgStrokeColor(args.vg, nvgRGBA(180,0,255,220)); nvgStrokeWidth(args.vg, 2.0f); nvgStroke(args.vg);
                } else {
                    nvgBeginPath(args.vg); nvgCircle(args.vg, cellCenter.x, cellCenter.y, ringR);
                    nvgStrokeColor(args.vg, playheadA ? nvgRGBA(0,255,180,220) : nvgRGBA(180,0,255,220)); nvgStrokeWidth(args.vg, 2.0f); nvgStroke(args.vg);
                }
            }

            // Draw circle outline
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
            nvgStrokeColor(args.vg, nvgRGBA(60,60,70,100)); 
            nvgStrokeWidth(args.vg, 1.0f); 
            nvgStroke(args.vg);
        }
    }

    // Edit-mode border glow
    if (view->isEditModeA() || view->isEditModeB()) {
        nvgSave(args.vg); nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        float time = system::getTime(); float pulse = 0.4f + 0.3f * sin(time * 3.0f);
        NVGcolor glow = view->isEditModeA() ? nvgRGBA(0,255,180, pulse*150) : nvgRGBA(180,0,255, pulse*150);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.0f, box.size.y - 1.0f, 7.5f);
        nvgStrokeColor(args.vg, glow); nvgStrokeWidth(args.vg, 1.25f); nvgStroke(args.vg);
        nvgRestore(args.vg);
    }

    // Vintage overlay
    nvgSave(args.vg);
    graphics::drawVignettePatinaScratches(args, 0, 0, box.size.x, box.size.y, 8.0f, 26,
                                    nvgRGBA(24,30,20,10), nvgRGBA(50,40,22,12), 8, 0.5f, 3, 73321u);
    nvgRestore(args.vg);

    nvgRestore(args.vg);
}

void HighResMatrixWidget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color) {
    graphics::drawAlchemicalSymbol(args, pos, symbolId, color, 6.5f, 1.0f);
}

void HighResMatrixWidget::drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor) {
    nvgSave(args.vg);
    int cols = 8, rows = 8; int gs = view ? view->getGridSteps() : 64;
    if (gs == 16) { cols = rows = 4; } else if (gs == 32) { cols = rows = 6; }
    float cellWidth = box.size.x / cols; float cellHeight = box.size.y / rows;
    float radiusFactor = (gs == 32) ? 0.44f : (gs == 16) ? 0.40f : 0.42f;
    float circleR = std::min(cellWidth, cellHeight) * radiusFactor;
    float ringR = std::max(0.0f, circleR - 1.4f);
    graphics::drawVoiceCountDots(args, pos, voiceCount, ringR, 1.2f, dotColor);
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
        nvgFillColor(args.vg, nvgRGBA(0, 255, 180, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 180, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (playheadB) {
        // Sequence B playing - purple background
        nvgFillColor(args.vg, nvgRGBA(180, 0, 255, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(180, 0, 255, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
    } else if (isSelected && inEditMode) {
        // Selected for editing - bright blue
        nvgFillColor(args.vg, nvgRGBA(0, 200, 255, 150));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(0, 255, 255, 255));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
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
    float size = std::min(box.size.x, box.size.y) * 0.36f;
    
    // Delegate actual symbol geometry to shared utility
    graphics::drawAlchemicalSymbol(args, Vec(0, 0), symbolId, ink, size, wobble);
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
