// ChaosVisualizer implementation
// This file is included in involution.cpp after the Involution struct is fully defined

// Chaos visualizer screen color theme palette
struct ChaosParticleRange {
    float rBase, rT, rAura;
    float gBase, gT, gAura;
    float bBase, bT, bAura;
};

struct ChaosThemePalette {
    NVGcolor bgInner;          // Screen background radial gradient inner
    NVGcolor bgOuter;          // Screen background radial gradient outer
    NVGcolor hotspotInner;     // Center hotspot glow (with alpha)
    NVGcolor gridColor;        // Grid line color
    NVGcolor outerGlowInner;   // CRT outer glow inner
    NVGcolor outerGlowOuter;   // CRT outer glow outer
    NVGcolor innerGlowInner;   // CRT inner glow inner
    NVGcolor innerGlowOuter;   // CRT inner glow outer
    ChaosParticleRange hueRanges[3]; // Particle color per hue sector
    NVGcolor ledBaseColors[3]; // Jewel LED base colors (aura, orbit, tide)
    NVGcolor ledGlowColor;     // Rate glow halo/core color
};

static const ChaosThemePalette CHAOS_THEMES[] = {
    // 0: Phosphor - Green terminal CRT spectrum (default)
    {
        nvgRGB(16, 24, 16),                  // bgInner
        nvgRGB(8, 11, 8),                    // bgOuter
        nvgRGBA(22, 35, 22, 120),            // hotspotInner
        nvgRGBA(0, 200, 60, 20),             // gridColor
        nvgRGBA(0, 140, 40, 60),             // outerGlowInner
        nvgRGBA(0, 40, 10, 0),              // outerGlowOuter
        nvgRGBA(0, 200, 60, 120),            // innerGlowInner
        nvgRGBA(0, 60, 18, 0),              // innerGlowOuter
        {   // hueRanges
            {0, 0, 0,     255, 0, 0,       80, 80, 40},   // Range 0: green w/ blue
            {0, 80, 30,   255, 0, 0,      160, -80, 0},   // Range 1: green→teal
            {80, -80, 30, 200, -60, 0,     40, 0, 0}      // Range 2: teal→green
        },
        {nvgRGB(80, 220, 60), nvgRGB(40, 200, 120), nvgRGB(60, 180, 160)}, // ledBaseColors
        nvgRGB(0, 255, 80)     // ledGlowColor
    },
    // 1: Ice - Cool cyan spectrum (more cyan, less blue for better visibility)
    {
        nvgRGB(18, 22, 30),                  // bgInner
        nvgRGB(8, 10, 14),                   // bgOuter
        nvgRGBA(30, 35, 45, 120),            // hotspotInner
        nvgRGBA(0, 200, 255, 20),            // gridColor (more cyan)
        nvgRGBA(0, 140, 200, 60),            // outerGlowInner (more cyan)
        nvgRGBA(0, 35, 50, 0),              // outerGlowOuter (more cyan)
        nvgRGBA(0, 200, 240, 120),           // innerGlowInner (bright cyan)
        nvgRGBA(0, 50, 70, 0),              // innerGlowOuter (cyan tint)
        {   // hueRanges
            {0, 120, 20,  200, 60, 20,   255, 0, 0},     // Range 0: cyan→aqua
            {100, -40, 20, 230, -60, 0,   255, 0, 0},     // Range 1: aqua→ice
            {0, 80, 20, 200, -60, 0,   255, 0, 0}      // Range 2: ice→cyan
        },
        {nvgRGB(0, 200, 255), nvgRGB(100, 220, 255), nvgRGB(170, 210, 255)}, // ledBaseColors
        nvgRGB(0, 230, 255)  // ledGlowColor (bright cyan)
    },
    // 2: Solar - Warm yellow/gold spectrum
    {
        nvgRGB(28, 26, 14),                  // bgInner (yellow-tinted)
        nvgRGB(12, 11, 6),                   // bgOuter
        nvgRGBA(40, 38, 18, 120),            // hotspotInner
        nvgRGBA(220, 200, 60, 20),           // gridColor (bright yellow)
        nvgRGBA(180, 160, 40, 60),           // outerGlowInner (yellow glow)
        nvgRGBA(50, 45, 10, 0),             // outerGlowOuter
        nvgRGBA(220, 200, 80, 120),          // innerGlowInner (bright yellow)
        nvgRGBA(70, 60, 20, 0),             // innerGlowOuter
        {   // hueRanges
            {255, 0, 0,   220, 100, 40,    80, 80, 0},    // Range 0: gold→yellow
            {255, -50, 30, 255, 0, 0,     120, -60, 0},   // Range 1: yellow→amber
            {220, -80, 30, 180, 0, 0,      60, 0, 0}      // Range 2: amber→gold
        },
        {nvgRGB(255, 230, 80), nvgRGB(255, 210, 100), nvgRGB(255, 200, 120)}, // ledBaseColors
        nvgRGB(255, 230, 80)   // ledGlowColor
    },
    // 3: Amber - Warm CRT orange/amber spectrum
    {
        nvgRGB(28, 20, 14),                  // bgInner
        nvgRGB(12, 9, 6),                    // bgOuter
        nvgRGBA(40, 30, 18, 120),            // hotspotInner
        nvgRGBA(200, 120, 0, 20),            // gridColor
        nvgRGBA(140, 80, 0, 60),             // outerGlowInner
        nvgRGBA(40, 20, 0, 0),              // outerGlowOuter
        nvgRGBA(200, 120, 0, 120),           // innerGlowInner
        nvgRGBA(60, 35, 0, 0),              // innerGlowOuter
        {   // hueRanges
            {255, 0, 0,    80, 120, 40,    0, 20, 0},     // Range 0: red→amber
            {255, 0, 0,   200, -100, 0,   20, -20, 0},    // Range 1: gold→orange
            {255, -105, 30, 40, 0, 0,      0, 0, 0}       // Range 2: orange→red
        },
        {nvgRGB(220, 100, 40), nvgRGB(200, 170, 50), nvgRGB(255, 140, 60)}, // ledBaseColors
        nvgRGB(255, 160, 0)    // ledGlowColor
    }
};

static const int NUM_CHAOS_THEMES = 4;
static const char* const CHAOS_THEME_NAMES[] = {"Phosphor", "Ice", "Solar", "Amber"};

inline void ChaosVisualizer::step() {
    Widget::step();
    float deltaTime = 1.0f / APP->window->getMonitorRefreshRate();
    time += deltaTime;

    if (module) {
        float rawChaosRate = clamp(module->smoothedChaosRate, Involution::CHAOS_RATE_MIN_HZ, Involution::CHAOS_RATE_MAX_HZ);
        float smoothedChaosRate = visualChaosRateSmoother.process(rawChaosRate, deltaTime);
        chaosPhase += smoothedChaosRate * deltaTime;

        float smoothedFilterMorph = visualFilterMorphSmoother.process(
            module->params[Involution::AURA_PARAM].getValue(), deltaTime);
        filterMorphPhase += (smoothedFilterMorph + 0.1f) * 0.5f * deltaTime;

        visualOrbitSmoother.process(module->params[Involution::ORBIT_PARAM].getValue(), deltaTime);
        visualTideSmoother.process(module->params[Involution::TIDE_PARAM].getValue(), deltaTime);

        float smoothedCutoffA = visualCutoffASmoother.process(module->effectiveCutoffA, deltaTime);
        float smoothedCutoffB = visualCutoffBSmoother.process(module->effectiveCutoffB, deltaTime);
        cutoffPhase += (smoothedCutoffA + smoothedCutoffB) * 0.2f * deltaTime;

        float smoothedResonanceA = visualResonanceASmoother.process(module->effectiveResonanceA, deltaTime);
        float smoothedResonanceB = visualResonanceBSmoother.process(module->effectiveResonanceB, deltaTime);
        float avgResonance = (smoothedResonanceA + smoothedResonanceB) * 0.5f;
        float resonanceActivity = (avgResonance - 0.707f) * 2.0f;
        resonanceActivity = std::max(resonanceActivity, 0.0f);
        resonancePhase += resonanceActivity * 0.4f * deltaTime;
    }
}

inline void ChaosVisualizer::drawLayer(const DrawArgs& args, int layer) {
    if (layer != 1) return;

    // Resolve current color theme
    int themeIdx = module ? clamp(module->chaosTheme, 0, NUM_CHAOS_THEMES - 1) : 0;
    const ChaosThemePalette& t = CHAOS_THEMES[themeIdx];

    NVGcontext* vg = args.vg;
    float width = box.size.x;
    float height = box.size.y;
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float diamondSize = std::min(width, height) * 0.9f;

    // Draw diamond-shaped oscilloscope bezel
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - diamondSize/2);
    nvgLineTo(vg, centerX + diamondSize/2, centerY);
    nvgLineTo(vg, centerX, centerY + diamondSize/2);
    nvgLineTo(vg, centerX - diamondSize/2, centerY);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(40, 40, 45));
    nvgFill(vg);

    // Inner diamond shadow
    float innerSize = diamondSize * 0.97f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - innerSize/2);
    nvgLineTo(vg, centerX + innerSize/2, centerY);
    nvgLineTo(vg, centerX, centerY + innerSize/2);
    nvgLineTo(vg, centerX - innerSize/2, centerY);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(25, 25, 30));
    nvgFill(vg);

    // Diamond screen background with backlit effect
    float screenSize = innerSize * 0.85f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2);
    nvgLineTo(vg, centerX + screenSize/2, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2);
    nvgLineTo(vg, centerX - screenSize/2, centerY);
    nvgClosePath(vg);

    NVGpaint backlitPaint = nvgRadialGradient(vg, centerX, centerY, 0, screenSize * 0.6f,
                                             t.bgInner, t.bgOuter);
    nvgFillPaint(vg, backlitPaint);
    nvgFill(vg);

    // Add center hotspot
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/4);
    nvgLineTo(vg, centerX + screenSize/4, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/4);
    nvgLineTo(vg, centerX - screenSize/4, centerY);
    nvgClosePath(vg);
    NVGcolor hotspotOuter = t.hotspotInner;
    hotspotOuter.a = 0.0f;
    NVGpaint centerGlow = nvgRadialGradient(vg, centerX, centerY, 0, screenSize * 0.25f,
                                           t.hotspotInner, hotspotOuter);
    nvgFillPaint(vg, centerGlow);
    nvgFill(vg);

    // Draw diamond grid lines
    nvgStrokeColor(vg, t.gridColor);
    nvgStrokeWidth(vg, 0.5f);

    float halfSize = screenSize / 2.0f;

    // Horizontal lines
    for (int i = -2; i <= 2; i++) {
        if (i == 0) continue;
        float y = centerY + i * screenSize * 0.15f;
        float width = halfSize * (1.0f - abs(y - centerY) / halfSize);

        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX - width, y);
        nvgLineTo(vg, centerX + width, y);
        nvgStroke(vg);
    }

    // Vertical lines
    for (int i = -2; i <= 2; i++) {
        if (i == 0) continue;
        float x = centerX + i * screenSize * 0.15f;
        float height = halfSize * (1.0f - abs(x - centerX) / halfSize);

        nvgBeginPath(vg);
        nvgMoveTo(vg, x, centerY - height);
        nvgLineTo(vg, x, centerY + height);
        nvgStroke(vg);
    }

    if (module) {
        // Only show pattern if output cables are connected (module has power)
        bool outputsConnected = module->outputs[Involution::AUDIO_A_OUTPUT].isConnected() ||
                               module->outputs[Involution::AUDIO_B_OUTPUT].isConnected();

        if (outputsConnected) {
            float deltaTime = 1.0f / APP->window->getMonitorRefreshRate();

            float chaosAmount = visualChaosAmountSmoother.process(
                module->params[Involution::CHAOS_AMOUNT_PARAM].getValue(), deltaTime);
            float filterMorph = visualFilterMorphSmoother.getValue();
            float orbitAmount = visualOrbitSmoother.getValue();
            float tideAmount = visualTideSmoother.getValue();
            float cutoffA = visualCutoffASmoother.process(module->effectiveCutoffA, deltaTime);
            float cutoffB = visualCutoffBSmoother.process(module->effectiveCutoffB, deltaTime);
            float resonanceA = visualResonanceASmoother.process(module->effectiveResonanceA, deltaTime);
            float resonanceB = visualResonanceBSmoother.process(module->effectiveResonanceB, deltaTime);

            drawSquareChaos(vg, centerX, centerY, screenSize * 0.4f, chaosAmount, chaosPhase,
                          filterMorph, orbitAmount, tideAmount,
                          cutoffA, cutoffB, resonanceA, resonanceB,
                          filterMorphPhase, cutoffPhase, resonancePhase);
        }
    }

    // CRT Effects
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2 * 1.2f);
    nvgLineTo(vg, centerX + screenSize/2 * 1.2f, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2 * 1.2f);
    nvgLineTo(vg, centerX - screenSize/2 * 1.2f, centerY);
    nvgClosePath(vg);
    NVGpaint outerGlow = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.35f, screenSize * 0.55f,
                                          t.outerGlowInner, t.outerGlowOuter);
    nvgFillPaint(vg, outerGlow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2 * 1.05f);
    nvgLineTo(vg, centerX + screenSize/2 * 1.05f, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2 * 1.05f);
    nvgLineTo(vg, centerX - screenSize/2 * 1.05f, centerY);
    nvgClosePath(vg);
    NVGpaint innerGlow = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.25f, screenSize * 0.38f,
                                          t.innerGlowInner, t.innerGlowOuter);
    nvgFillPaint(vg, innerGlow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2 * 0.9f);
    nvgLineTo(vg, centerX + screenSize/2 * 0.9f, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2 * 0.9f);
    nvgLineTo(vg, centerX - screenSize/2 * 0.9f, centerY);
    nvgClosePath(vg);
    NVGpaint bulgeHighlight = nvgRadialGradient(vg,
        centerX - screenSize * 0.15f, centerY - screenSize * 0.15f,
        screenSize * 0.05f, screenSize * 0.4f,
        nvgRGBA(255, 255, 255, 25), nvgRGBA(255, 255, 255, 0));
    nvgFillPaint(vg, bulgeHighlight);
    nvgFill(vg);

    // Scanlines
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 40));
    nvgStrokeWidth(vg, 0.5f);
    for (int i = 0; i < 20; i++) {
        float y = centerY - screenSize/2 + (i / 19.0f) * screenSize;
        float lineWidth = screenSize * (1.0f - 2.0f * abs(y - centerY) / screenSize);
        if (lineWidth > 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, centerX - lineWidth/2, y);
            nvgLineTo(vg, centerX + lineWidth/2, y);
            nvgStroke(vg);
        }
    }

    // Vignette
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2);
    nvgLineTo(vg, centerX + screenSize/2, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2);
    nvgLineTo(vg, centerX - screenSize/2, centerY);
    nvgClosePath(vg);
    NVGpaint vignette = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.2f, screenSize * 0.5f,
                                         nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 30));
    nvgFillPaint(vg, vignette);
    nvgFill(vg);
}

inline void ChaosVisualizer::drawSquareChaos(NVGcontext* vg, float cx, float cy, float maxRadius,
                    float chaosAmount, float chaosPhase, float auraAmount,
                    float orbitAmount, float tideAmount,
                    float cutoffA, float cutoffB, float resonanceA, float resonanceB,
                    float auraPhase, float cutoffPhase, float resonancePhase) {

    float totalActivity = chaosAmount + (cutoffA + cutoffB) * 0.2f;

    float avgResonance = (resonanceA + resonanceB) * 0.5f;
    float resonanceActivity = (avgResonance - 0.707f) * 2.0f;
    resonanceActivity = std::max(resonanceActivity, 0.0f);
    totalActivity += resonanceActivity * 0.3f;

    totalActivity = std::max(totalActivity, 0.35f);

    int baseSquares = 45 + (int)(auraAmount * 20);
    int resonanceSquares = (int)(resonanceActivity * 80);
    int activitySquares = (int)(totalActivity * 120);
    int numSquares = baseSquares + activitySquares + resonanceSquares;
    numSquares = clamp(numSquares, 45, 220);

    for (int i = 0; i < numSquares; i++) {
        float angle = (i / (float)numSquares) * 2.0f * M_PI * (3.4f + orbitAmount * 0.6f);

        angle += time * 0.3f;
        angle += chaosPhase * 1.0f;
        angle += chaosPhase * 0.8f;
        angle += filterMorphPhase;
        angle += cutoffPhase;
        angle += resonancePhase;

        float baseRadius = (i / (float)numSquares) * maxRadius;
        float radiusVar = sinf(time * (3.0f + tideAmount * 1.5f) + i * 0.2f) * maxRadius * 0.2f * chaosAmount * (1.f + orbitAmount * 0.4f);
        float resonancePulse = sinf(time * 4.0f + i * 0.5f) * maxRadius * 0.15f * resonanceActivity;
        float radius = baseRadius + radiusVar + resonancePulse;
        radius *= (0.8f + cutoffA * 0.2f + cutoffB * 0.2f + resonanceActivity * 0.1f);

        float x = cx + cosf(angle) * radius;
        float y = cy + sinf(angle) * radius;

        float distanceFromCenterX = abs(x - cx);
        float distanceFromCenterY = abs(y - cy);
        float diamondDistance = distanceFromCenterX / maxRadius + distanceFromCenterY / maxRadius;

        if (diamondDistance > 0.9f) {
            float scale = 0.9f / diamondDistance;
            x = cx + (x - cx) * scale;
            y = cy + (y - cy) * scale;
        }

        float baseSize = 0.35f + chaosAmount * 0.6f + orbitAmount * 0.2f;
        float sizeVar = sinf(time * (4.0f + tideAmount * 1.5f) + i * 0.32f + auraPhase * 0.5f) * (0.18f + orbitAmount * 0.1f);
        float resonanceScale = 0.2f + 0.4f * (1.f - resonanceActivity);
        float resonanceSize = resonanceScale * 0.45f + sinf(time * 5.5f + i * 0.3f) * resonanceScale * 0.32f;
        float dotRadius = clamp(baseSize + sizeVar + resonanceSize, 0.18f, 1.4f);

        float hue = fmodf(time * (30.0f + tideAmount * 12.f) + i * (15.0f + auraAmount * 6.f)
                          + auraAmount * 140.0f + orbitAmount * 150.0f + resonanceActivity * 120.0f, 360.0f);

        float baseBrightness = 0.55f;
        float activityBrightness = chaosAmount * 0.85f;
        float filterBrightness = (cutoffA + cutoffB) * 0.12f;
        float resonanceBrightness = resonanceActivity * 0.22f;
        float orbitBrightness = orbitAmount * 0.45f;

        float brightness = baseBrightness + activityBrightness + filterBrightness + resonanceBrightness + orbitBrightness;
        brightness = clamp(brightness, 0.7f, 2.2f);
        float radiusNorm = radius / maxRadius;
        brightness *= (1.0f - radiusNorm * 0.18f);
        if (radiusNorm < 0.25f) {
            float boost = (0.25f - radiusNorm);
            brightness += boost * (2.7f + orbitAmount * 1.4f + auraAmount * 0.9f + tideAmount * 0.9f);
            dotRadius += boost * 0.9f;
        }
        brightness = clamp(brightness, 0.75f, 2.3f);

        // Resolve particle color from theme hue ranges
        int themeIdx = module ? clamp(module->chaosTheme, 0, NUM_CHAOS_THEMES - 1) : 0;
        const ChaosParticleRange* ranges = CHAOS_THEMES[themeIdx].hueRanges;

        NVGcolor color;
        int rangeIdx;
        float t;
        if (hue < 120.0f) {
            rangeIdx = 0;
            t = hue / 120.0f;
        } else if (hue < 240.0f) {
            rangeIdx = 1;
            t = (hue - 120.0f) / 120.0f;
        } else {
            rangeIdx = 2;
            t = (hue - 240.0f) / 120.0f;
        }
        const ChaosParticleRange& r = ranges[rangeIdx];
        color = nvgRGBA(
            (int)((r.rBase + t * r.rT + auraAmount * r.rAura) * brightness),
            (int)((r.gBase + t * r.gT + auraAmount * r.gAura) * brightness),
            (int)((r.bBase + t * r.bT + auraAmount * r.bAura) * brightness),
            (int)(brightness * 255));

        nvgBeginPath(vg);
        nvgCircle(vg, x, y, dotRadius);
        nvgFillColor(vg, color);
        nvgFill(vg);

        NVGcolor coreColor = color;
        coreColor.a = clamp(coreColor.a + 0.4f, 0.0f, 1.0f);
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, dotRadius * 0.42f);
        nvgFillColor(vg, coreColor);
        nvgFill(vg);

        nvgSave(vg);
        nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
        NVGcolor haloInner = color;
        haloInner.a = clamp(color.a * 0.65f + 0.25f, 0.25f, 0.95f);
        NVGcolor haloOuter = color;
        haloOuter.a = 0.0f;
        NVGpaint halo = nvgRadialGradient(vg, x, y, dotRadius * 0.3f, dotRadius * 2.4f, haloInner, haloOuter);
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, dotRadius * 2.3f);
        nvgFillPaint(vg, halo);
        nvgFill(vg);
        nvgRestore(vg);
    }
}
