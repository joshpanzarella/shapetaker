#include "alchemySymbols.hpp"
#include <cmath>

namespace st {

void drawRestSymbol(const widget::Widget::DrawArgs& args, Vec pos, float halfWidth, NVGcolor color) {
    nvgStrokeColor(args.vg, color);
    nvgStrokeWidth(args.vg, 1.0f);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - halfWidth, pos.y);
    nvgLineTo(args.vg, pos.x + halfWidth, pos.y);
    nvgStroke(args.vg);
}

void drawTieSymbol(const widget::Widget::DrawArgs& args, Vec pos, float halfWidth, NVGcolor color) {
    nvgStrokeColor(args.vg, color);
    nvgStrokeWidth(args.vg, 1.0f);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - halfWidth, pos.y);
    nvgBezierTo(args.vg, pos.x - halfWidth * 0.33f, pos.y - halfWidth, pos.x + halfWidth * 0.33f, pos.y - halfWidth, pos.x + halfWidth, pos.y);
    nvgStroke(args.vg);
}

void drawAlchemicalSymbol(const widget::Widget::DrawArgs& args, Vec pos, int symbolId,
                          NVGcolor color, float size, float strokeWidth) {
    nvgSave(args.vg);
    nvgTranslate(args.vg, pos.x, pos.y);
    nvgStrokeColor(args.vg, color);
    nvgFillColor(args.vg, color);
    nvgStrokeWidth(args.vg, strokeWidth);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);

    switch (symbolId) {
        case 0: // Sun
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.3f); nvgFill(args.vg);
            break;
        case 1: // Moon
            nvgBeginPath(args.vg); nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW); nvgStroke(args.vg);
            break;
        case 2: // Mercury
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f); nvgLineTo(args.vg, 0, -size * 0.6f); nvgLineTo(args.vg, size * 0.6f, -size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size * 0.2f); nvgLineTo(args.vg, 0, size * 0.8f); nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f); nvgLineTo(args.vg, size * 0.3f, size * 0.5f); nvgStroke(args.vg);
            break;
        case 3: // Venus
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, -size * 0.3f, size * 0.5f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size * 0.2f); nvgLineTo(args.vg, 0, size * 0.8f); nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f); nvgLineTo(args.vg, size * 0.3f, size * 0.5f); nvgStroke(args.vg);
            break;
        case 4: // Mars
            nvgBeginPath(args.vg); nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, size * 0.2f, -size * 0.2f); nvgLineTo(args.vg, size * 0.7f, -size * 0.7f); nvgLineTo(args.vg, size * 0.4f, -size * 0.7f); nvgMoveTo(args.vg, size * 0.7f, -size * 0.7f); nvgLineTo(args.vg, size * 0.7f, -size * 0.4f); nvgStroke(args.vg);
            break;
        case 5: // Jupiter
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, 0); nvgLineTo(args.vg, size * 0.2f, 0); nvgMoveTo(args.vg, 0, -size * 0.6f); nvgLineTo(args.vg, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgArc(args.vg, size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 0.5f, M_PI * 1.5f, NVG_CCW); nvgStroke(args.vg);
            break;
        case 6: // Saturn
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.2f, 0); nvgLineTo(args.vg, size * 0.6f, 0); nvgMoveTo(args.vg, 0, -size * 0.6f); nvgLineTo(args.vg, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgArc(args.vg, -size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 1.5f, M_PI * 0.5f, NVG_CCW); nvgStroke(args.vg);
            break;
        case 7: // Fire
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size); nvgLineTo(args.vg, -size * 0.8f, size * 0.6f); nvgLineTo(args.vg, size * 0.8f, size * 0.6f); nvgClosePath(args.vg); nvgStroke(args.vg);
            break;
        case 8: // Water
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size); nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f); nvgLineTo(args.vg, size * 0.8f, -size * 0.6f); nvgClosePath(args.vg); nvgStroke(args.vg);
            break;
        case 9: // Air
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size); nvgLineTo(args.vg, -size * 0.8f, size * 0.6f); nvgLineTo(args.vg, size * 0.8f, size * 0.6f); nvgClosePath(args.vg); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.4f, 0); nvgLineTo(args.vg, size * 0.4f, 0); nvgStroke(args.vg);
            break;
        case 10: // Earth
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size); nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f); nvgLineTo(args.vg, size * 0.8f, -size * 0.6f); nvgClosePath(args.vg); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.4f, 0); nvgLineTo(args.vg, size * 0.4f, 0); nvgStroke(args.vg);
            break;
        case 11: // Quintessence
            nvgBeginPath(args.vg); nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f); nvgStroke(args.vg);
            break;
        case 12: // Pentagram
            nvgBeginPath(args.vg);
            for (int i = 0; i < 5; i++) { int p = (i * 2) % 5; float a = p * 2.0f * M_PI / 5.0f - M_PI / 2.0f; float x = cosf(a) * size; float y = sinf(a) * size; if (i == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);} nvgClosePath(args.vg); nvgStroke(args.vg);
            break;
        case 13: // Hexagram
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size); nvgLineTo(args.vg, -size * 0.866f, size * 0.5f); nvgLineTo(args.vg, size * 0.866f, size * 0.5f); nvgClosePath(args.vg); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size); nvgLineTo(args.vg, -size * 0.866f, -size * 0.5f); nvgLineTo(args.vg, size * 0.866f, -size * 0.5f); nvgClosePath(args.vg); nvgStroke(args.vg);
            break;
        case 14: // Ankh
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.2f); nvgLineTo(args.vg, 0, size); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.5f, size * 0.2f); nvgLineTo(args.vg, size * 0.5f, size * 0.2f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgArc(args.vg, 0, -size * 0.4f, size * 0.3f, 0, M_PI, NVG_CW); nvgStroke(args.vg);
            break;
        case 15: // Eye of Horus
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.8f, 0); nvgBezierTo(args.vg, -size * 0.8f, -size * 0.5f, size * 0.8f, -size * 0.5f, size * 0.8f, 0); nvgBezierTo(args.vg, size * 0.8f, size * 0.5f, -size * 0.8f, size * 0.5f, -size * 0.8f, 0); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.2f); nvgFill(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.3f, size * 0.2f); nvgLineTo(args.vg, -size * 0.3f, size * 0.8f); nvgStroke(args.vg);
            break;
        case 16: // Ouroboros
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, size * 0.8f, 0, size * 0.15f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, size * 0.65f, 0); nvgLineTo(args.vg, size * 0.5f, 0); nvgStroke(args.vg);
            break;
        case 17: // Triskele
            nvgBeginPath(args.vg);
            for (int i = 0; i < 3; i++) { float ang = i * 2.0f * M_PI / 3.0f; nvgMoveTo(args.vg, 0, 0); for (int j = 1; j <= 8; j++) { float r = (j / 8.0f) * size; float a = ang + (j / 8.0f) * M_PI; nvgLineTo(args.vg, cosf(a) * r, sinf(a) * r);} }
            nvgStroke(args.vg);
            break;
        case 18: // Caduceus
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size); nvgLineTo(args.vg, 0, size); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.6f); nvgBezierTo(args.vg, -size * 0.4f, -size * 0.2f, -size * 0.4f, size * 0.2f, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.6f); nvgBezierTo(args.vg, size * 0.4f, -size * 0.2f, size * 0.4f, size * 0.2f, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.3f, -size * 0.8f); nvgLineTo(args.vg, 0, -size * 0.6f); nvgLineTo(args.vg, size * 0.3f, -size * 0.8f); nvgStroke(args.vg);
            break;
        case 19: // Yin Yang
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgArc(args.vg, 0, -size * 0.5f, size * 0.5f, 0, M_PI, NVG_CW); nvgArc(args.vg, 0, size * 0.5f, size * 0.5f, M_PI, 2 * M_PI, NVG_CCW); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, -size * 0.5f, size * 0.15f); nvgFill(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, size * 0.5f, size * 0.15f); nvgStroke(args.vg);
            break;
        case 20: // Seal of Solomon
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.7f); nvgLineTo(args.vg, -size * 0.6f, size * 0.35f); nvgLineTo(args.vg, size * 0.6f, size * 0.35f); nvgClosePath(args.vg); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size * 0.7f); nvgLineTo(args.vg, -size * 0.6f, -size * 0.35f); nvgLineTo(args.vg, size * 0.6f, -size * 0.35f); nvgClosePath(args.vg); nvgStroke(args.vg);
            break;
        case 21: // Sulfur
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.5f); nvgLineTo(args.vg, -size * 0.6f, size * 0.1f); nvgLineTo(args.vg, size * 0.6f, size * 0.1f); nvgClosePath(args.vg); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size * 0.1f); nvgLineTo(args.vg, 0, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.3f, size * 0.45f); nvgLineTo(args.vg, size * 0.3f, size * 0.45f); nvgStroke(args.vg);
            break;
        case 22: // Salt
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.8f, 0); nvgLineTo(args.vg, size * 0.8f, 0); nvgStroke(args.vg);
            break;
        case 23: // Antimony
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size * 0.1f); nvgLineTo(args.vg, 0, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.3f, size * 0.45f); nvgLineTo(args.vg, size * 0.3f, size * 0.45f); nvgStroke(args.vg);
            break;
        case 24: // Philosopher's Stone
            nvgBeginPath(args.vg); nvgRect(args.vg, -size * 0.7f, -size * 0.7f, size * 1.4f, size * 1.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.5f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.1f); nvgFill(args.vg);
            break;
        case 25: // Arsenic
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f); nvgLineTo(args.vg, 0, size * 0.8f); nvgLineTo(args.vg, size * 0.6f, -size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.4f, 0); nvgLineTo(args.vg, size * 0.4f, 0); nvgStroke(args.vg);
            break;
        case 26: // Copper
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, -size * 0.2f, size * 0.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, size * 0.2f); nvgLineTo(args.vg, 0, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgArc(args.vg, -size * 0.2f, size * 0.5f, size * 0.2f, 0, M_PI, NVG_CW); nvgArc(args.vg, size * 0.2f, size * 0.5f, size * 0.2f, M_PI, 2 * M_PI, NVG_CW); nvgStroke(args.vg);
            break;
        case 27: // Iron
            nvgBeginPath(args.vg); nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.3f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, size * 0.1f, -size * 0.1f); nvgLineTo(args.vg, size * 0.8f, -size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, size * 0.6f, -size * 0.8f); nvgLineTo(args.vg, size * 0.8f, -size * 0.8f); nvgLineTo(args.vg, size * 0.8f, -size * 0.6f); nvgStroke(args.vg);
            break;
        case 28: // Lead
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.3f); nvgLineTo(args.vg, size * 0.6f, -size * 0.3f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.8f); nvgLineTo(args.vg, 0, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgArc(args.vg, 0, size * 0.2f, size * 0.3f, 0, M_PI, NVG_CW); nvgStroke(args.vg);
            break;
        case 29: // Silver
            nvgBeginPath(args.vg); nvgArc(args.vg, size * 0.2f, 0, size * 0.8f, M_PI * 0.6f, M_PI * 1.4f, NVG_CW); nvgStroke(args.vg);
            break;
        case 30: // Zinc
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, 0); nvgLineTo(args.vg, size * 0.6f, 0); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.6f); nvgLineTo(args.vg, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.4f, -size * 0.4f); nvgLineTo(args.vg, size * 0.4f, size * 0.4f); nvgStroke(args.vg);
            break;
        case 31: // Tin
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.2f); nvgLineTo(args.vg, size * 0.6f, -size * 0.2f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, size * 0.2f); nvgLineTo(args.vg, size * 0.6f, size * 0.2f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.8f); nvgLineTo(args.vg, 0, size * 0.8f); nvgStroke(args.vg);
            break;
        case 32: // Bismuth
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, -size * 0.3f, size * 0.3f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, 0); nvgLineTo(args.vg, 0, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.4f, size * 0.5f); nvgLineTo(args.vg, size * 0.4f, size * 0.5f); nvgStroke(args.vg);
            break;
        case 33: // Magnesium
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f); nvgLineTo(args.vg, -size * 0.6f, size * 0.8f); nvgLineTo(args.vg, size * 0.6f, size * 0.8f); nvgLineTo(args.vg, size * 0.6f, -size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, 0); nvgLineTo(args.vg, size * 0.6f, 0); nvgStroke(args.vg);
            break;
        case 34: // Platinum
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.6f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.8f, -size * 0.8f); nvgLineTo(args.vg, size * 0.8f, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, size * 0.8f, -size * 0.8f); nvgLineTo(args.vg, -size * 0.8f, size * 0.8f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.2f); nvgFill(args.vg);
            break;
        case 35: // Aether
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.8f); nvgLineTo(args.vg, -size * 0.7f, size * 0.4f); nvgLineTo(args.vg, size * 0.7f, size * 0.4f); nvgClosePath(args.vg); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.8f, -size * 0.9f); nvgLineTo(args.vg, size * 0.8f, -size * 0.9f); nvgStroke(args.vg);
            break;
        case 36: // Void
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.7f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size, 0); nvgLineTo(args.vg, size, 0); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size); nvgLineTo(args.vg, 0, size); nvgStroke(args.vg);
            break;
        case 37: // Chaos Star
            nvgBeginPath(args.vg); for (int i = 0; i < 8; i++) { float ang = i * M_PI / 4.0f; nvgMoveTo(args.vg, 0, 0); nvgLineTo(args.vg, cosf(ang) * size, sinf(ang) * size);} nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.2f); nvgFill(args.vg);
            break;
        case 38: // Tree of Life
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.9f); nvgLineTo(args.vg, 0, size * 0.9f); nvgStroke(args.vg);
            for (int i = 0; i < 3; i++) { float y = -size * 0.6f + i * size * 0.6f; nvgBeginPath(args.vg); nvgCircle(args.vg, 0, y, size * 0.15f); nvgStroke(args.vg);} 
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.3f); nvgLineTo(args.vg, size * 0.6f, -size * 0.3f); nvgStroke(args.vg);
            break;
        case 39: // Leviathan Cross
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, 0, -size * 0.8f); nvgLineTo(args.vg, 0, size * 0.4f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.6f, -size * 0.2f); nvgLineTo(args.vg, size * 0.6f, -size * 0.2f); nvgStroke(args.vg);
            nvgBeginPath(args.vg); nvgMoveTo(args.vg, -size * 0.4f, -size * 0.5f); nvgLineTo(args.vg, size * 0.4f, -size * 0.5f); nvgStroke(args.vg);
            // Infinity loop
            nvgBeginPath(args.vg);
            for (float t = 0; t < 2 * M_PI; t += 0.1f) { float x = size * 0.4f * sinf(t) / (1 + cosf(t) * cosf(t)); float y = size * 0.6f + size * 0.2f * sinf(t) * cosf(t) / (1 + cosf(t) * cosf(t)); if (t == 0) nvgMoveTo(args.vg, x, y); else nvgLineTo(args.vg, x, y);} nvgStroke(args.vg);
            break;
        default:
            nvgBeginPath(args.vg); nvgCircle(args.vg, 0, 0, size * 0.5f); nvgStroke(args.vg);
            break;
    }

    nvgRestore(args.vg);
}

} // namespace st
