#pragma once
#include <rack.hpp>

namespace st {
using namespace rack;

// Draw a single alchemical symbol at pos with given size and color.
// size is the nominal radius/scale used by the primitives.
void drawAlchemicalSymbol(const widget::Widget::DrawArgs& args, Vec pos, int symbolId,
                          NVGcolor color = nvgRGBA(255, 255, 255, 255), float size = 6.5f,
                          float strokeWidth = 1.0f);

// Draw REST and TIE helper glyphs
void drawRestSymbol(const widget::Widget::DrawArgs& args, Vec pos, float halfWidth = 6.0f, NVGcolor color = nvgRGBA(180,180,180,255));
void drawTieSymbol(const widget::Widget::DrawArgs& args, Vec pos, float halfWidth = 6.0f, NVGcolor color = nvgRGBA(255,220,120,255));

} // namespace st

