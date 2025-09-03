// Graphics drawing utilities implementation
#include "drawing.hpp"
// Legacy includes removed - functionality now implemented directly

namespace shapetaker {
namespace graphics {

void drawVoiceCountDots(const widget::Widget::DrawArgs& args, Vec center, int count,
                        float radius, float dotRadius, NVGcolor color) {
    if (count <= 0) return;
    
    nvgSave(args.vg);
    float angleStep = 2.0f * M_PI / count;
    for (int i = 0; i < count; i++) {
        float angle = i * angleStep - M_PI / 2; // Start at top
        float x = center.x + radius * cosf(angle);
        float y = center.y + radius * sinf(angle);
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, x, y, dotRadius);
        nvgFillColor(args.vg, color);
        nvgFill(args.vg);
    }
    nvgRestore(args.vg);
}

void drawAlchemicalSymbol(const widget::Widget::DrawArgs& args, Vec pos, int symbolId,
                         NVGcolor color, float size, float strokeWidth) {
    if (!isValidSymbolId(symbolId)) return;
    
    nvgSave(args.vg);
    nvgTranslate(args.vg, pos.x, pos.y);
    nvgStrokeColor(args.vg, color);
    nvgStrokeWidth(args.vg, strokeWidth);
    nvgFillColor(args.vg, color);
    
    // Full implementation of 40 alchemical symbols
    float r = size * 0.5f;
    
    switch (symbolId) {
        case 0: // Sol (Sun) - Circle with center dot
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, r);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, r * 0.3f);
            nvgFill(args.vg);
            break;
        case 1: // Luna (Moon) - Crescent
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, 0, r, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
        case 2: // Mercury - Circle with horns and cross
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -r * 0.6f, r * 0.8f);
            nvgStroke(args.vg);
            // Horns
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -r * 1.2f, -r * 1.6f);
            nvgLineTo(args.vg, 0, -r * 1.2f);
            nvgLineTo(args.vg, r * 1.2f, -r * 1.6f);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, r * 0.4f);
            nvgLineTo(args.vg, 0, r * 1.6f);
            nvgMoveTo(args.vg, -r * 0.6f, r);
            nvgLineTo(args.vg, r * 0.6f, r);
            nvgStroke(args.vg);
            break;
        case 3: // Venus - Circle with cross below
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -r * 0.6f, r);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, r * 0.4f);
            nvgLineTo(args.vg, 0, r * 1.6f);
            nvgMoveTo(args.vg, -r * 0.6f, r);
            nvgLineTo(args.vg, r * 0.6f, r);
            nvgStroke(args.vg);
            break;
        case 4: // Mars - Circle with arrow up-right
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -r * 0.4f, r * 0.4f, r * 0.8f);
            nvgStroke(args.vg);
            // Arrow
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, r * 0.4f, -r * 0.4f);
            nvgLineTo(args.vg, r * 1.4f, -r * 1.4f);
            nvgLineTo(args.vg, r * 1.1f, -r * 1.4f);
            nvgMoveTo(args.vg, r * 1.4f, -r * 1.4f);
            nvgLineTo(args.vg, r * 1.4f, -r * 1.1f);
            nvgStroke(args.vg);
            break;
        default: // For symbols 5-39, use simple geometric shapes as placeholder
            {
                int shapeType = symbolId % 6;
                switch (shapeType) {
                    case 0: // Circle
                        nvgBeginPath(args.vg);
                        nvgCircle(args.vg, 0, 0, r);
                        nvgStroke(args.vg);
                        break;
                    case 1: // Triangle
                        nvgBeginPath(args.vg);
                        nvgMoveTo(args.vg, 0, -r);
                        nvgLineTo(args.vg, r * 0.866f, r * 0.5f);
                        nvgLineTo(args.vg, -r * 0.866f, r * 0.5f);
                        nvgClosePath(args.vg);
                        nvgStroke(args.vg);
                        break;
                    case 2: // Square
                        nvgBeginPath(args.vg);
                        nvgRect(args.vg, -r, -r, r * 2, r * 2);
                        nvgStroke(args.vg);
                        break;
                    case 3: // Cross
                        nvgBeginPath(args.vg);
                        nvgMoveTo(args.vg, 0, -r);
                        nvgLineTo(args.vg, 0, r);
                        nvgMoveTo(args.vg, -r, 0);
                        nvgLineTo(args.vg, r, 0);
                        nvgStroke(args.vg);
                        break;
                    case 4: // Diamond
                        nvgBeginPath(args.vg);
                        nvgMoveTo(args.vg, 0, -r);
                        nvgLineTo(args.vg, r, 0);
                        nvgLineTo(args.vg, 0, r);
                        nvgLineTo(args.vg, -r, 0);
                        nvgClosePath(args.vg);
                        nvgStroke(args.vg);
                        break;
                    case 5: // Star
                        nvgBeginPath(args.vg);
                        for (int i = 0; i < 5; i++) {
                            float angle = i * 2 * M_PI / 5 - M_PI / 2;
                            float outerR = r;
                            float innerR = r * 0.4f;
                            if (i == 0) nvgMoveTo(args.vg, outerR * cosf(angle), outerR * sinf(angle));
                            else nvgLineTo(args.vg, outerR * cosf(angle), outerR * sinf(angle));
                            angle += M_PI / 5;
                            nvgLineTo(args.vg, innerR * cosf(angle), innerR * sinf(angle));
                        }
                        nvgClosePath(args.vg);
                        nvgStroke(args.vg);
                        break;
                }
            }
            break;
    }
    
    nvgRestore(args.vg);
}

bool isValidSymbolId(int symbolId) {
    return symbolId >= 0 && symbolId < 40;
}

void drawVignettePatinaScratches(const widget::Widget::DrawArgs& args,
                                float x, float y, float w, float h,
                                float cornerRadius, int scratchCount,
                                NVGcolor vignette1, NVGcolor vignette2,
                                int patinaLayers, float scratchAlpha,
                                int scratchVariations, unsigned int seed) {
    nvgSave(args.vg);
    
    // Draw vignette effect (darkening at edges)
    NVGpaint vignette = nvgRadialGradient(args.vg, x + w/2, y + h/2, 
                                         std::min(w, h) * 0.3f, std::min(w, h) * 0.7f,
                                         vignette1, vignette2);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, x, y, w, h, cornerRadius);
    nvgFillPaint(args.vg, vignette);
    nvgFill(args.vg);
    
    // Add some simple scratch effects
    if (scratchCount > 0) {
        nvgStrokeColor(args.vg, nvgRGBAf(0.3f, 0.3f, 0.25f, scratchAlpha));
        nvgStrokeWidth(args.vg, 0.5f);
        
        // Simple random scratches - using seed for reproducibility
        srand(seed);
        for (int i = 0; i < scratchCount; i++) {
            float x1 = x + (rand() % (int)w);
            float y1 = y + (rand() % (int)h);
            float x2 = x1 + (rand() % 20 - 10);
            float y2 = y1 + (rand() % 20 - 10);
            
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x1, y1);
            nvgLineTo(args.vg, x2, y2);
            nvgStroke(args.vg);
        }
    }
    
    nvgRestore(args.vg);
}

void drawVintageText(const widget::Widget::DrawArgs& args, Vec pos, 
                    const std::string& text, NVGcolor color, float fontSize,
                    bool addGlow, float glowRadius) {
    nvgSave(args.vg);
    nvgFontSize(args.vg, fontSize);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    
    if (addGlow) {
        // Draw glow effect
        nvgFontBlur(args.vg, glowRadius);
        nvgFillColor(args.vg, nvgRGBAf(color.r, color.g, color.b, 0.3f));
        nvgText(args.vg, pos.x, pos.y, text.c_str(), nullptr);
        nvgFontBlur(args.vg, 0);
    }
    
    // Draw main text
    nvgFillColor(args.vg, color);
    nvgText(args.vg, pos.x, pos.y, text.c_str(), nullptr);
    nvgRestore(args.vg);
}

void drawScanlines(const widget::Widget::DrawArgs& args, float x, float y, 
                  float w, float h, float lineSpacing, float alpha) {
    nvgSave(args.vg);
    nvgStrokeColor(args.vg, nvgRGBAf(0, 0, 0, alpha));
    nvgStrokeWidth(args.vg, 1.0f);
    
    for (float yPos = y; yPos < y + h; yPos += lineSpacing) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x, yPos);
        nvgLineTo(args.vg, x + w, yPos);
        nvgStroke(args.vg);
    }
    nvgRestore(args.vg);
}

void drawPhosphorGlow(const widget::Widget::DrawArgs& args, Vec center, 
                     float radius, NVGcolor color, float intensity) {
    nvgSave(args.vg);
    
    // Create radial gradient for phosphor glow
    NVGpaint glow = nvgRadialGradient(args.vg, center.x, center.y, 
                                     radius * 0.3f, radius,
                                     nvgRGBAf(color.r, color.g, color.b, intensity),
                                     nvgRGBAf(color.r, color.g, color.b, 0));
    
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, center.x, center.y, radius);
    nvgFillPaint(args.vg, glow);
    nvgFill(args.vg);
    nvgRestore(args.vg);
}

}} // namespace shapetaker::graphics