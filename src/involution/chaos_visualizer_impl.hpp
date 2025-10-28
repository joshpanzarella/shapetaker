// ChaosVisualizer implementation
// This file is included in involution.cpp after the Involution struct is fully defined

inline void ChaosVisualizer::step() {
    Widget::step();
    float deltaTime = 1.0f / APP->window->getMonitorRefreshRate();
    time += deltaTime;

    if (module) {
        float rawChaosRate = module->params[Involution::CHAOS_RATE_PARAM].getValue();
        if (module->inputs[Involution::CHAOS_RATE_CV_INPUT].isConnected()) {
            float rateCv = module->inputs[Involution::CHAOS_RATE_CV_INPUT].getVoltage();
            rawChaosRate += rateCv * 0.5f;
        }
        rawChaosRate = clamp(rawChaosRate, 0.001f, 20.0f);
        float smoothedChaosRate = visualChaosRateSmoother.process(rawChaosRate, deltaTime);
        chaosPhase += smoothedChaosRate * deltaTime;

        float smoothedFilterMorph = visualFilterMorphSmoother.process(
            module->params[Involution::FILTER_MORPH_PARAM].getValue(), deltaTime);
        filterMorphPhase += (smoothedFilterMorph + 0.1f) * 0.5f * deltaTime;

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
    float innerSize = diamondSize * 0.9f;
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
                                             nvgRGB(18, 22, 28), nvgRGB(8, 10, 12));
    nvgFillPaint(vg, backlitPaint);
    nvgFill(vg);

    // Add center hotspot
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/4);
    nvgLineTo(vg, centerX + screenSize/4, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/4);
    nvgLineTo(vg, centerX - screenSize/4, centerY);
    nvgClosePath(vg);
    NVGpaint centerGlow = nvgRadialGradient(vg, centerX, centerY, 0, screenSize * 0.25f,
                                           nvgRGBA(25, 30, 40, 120), nvgRGBA(25, 30, 40, 0));
    nvgFillPaint(vg, centerGlow);
    nvgFill(vg);

    // Draw diamond grid lines
    nvgStrokeColor(vg, nvgRGBA(0, 100, 255, 20));
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
        float deltaTime = 1.0f / APP->window->getMonitorRefreshRate();

        float chaosAmount = visualChaosAmountSmoother.process(
            module->params[Involution::CHAOS_AMOUNT_PARAM].getValue(), deltaTime);
        float filterMorph = visualFilterMorphSmoother.process(
            module->params[Involution::FILTER_MORPH_PARAM].getValue(), deltaTime);
        float cutoffA = visualCutoffASmoother.process(module->effectiveCutoffA, deltaTime);
        float cutoffB = visualCutoffBSmoother.process(module->effectiveCutoffB, deltaTime);
        float resonanceA = visualResonanceASmoother.process(module->effectiveResonanceA, deltaTime);
        float resonanceB = visualResonanceBSmoother.process(module->effectiveResonanceB, deltaTime);

        drawSquareChaos(vg, centerX, centerY, screenSize * 0.4f, chaosAmount, chaosPhase,
                      filterMorph, cutoffA, cutoffB, resonanceA, resonanceB,
                      filterMorphPhase, cutoffPhase, resonancePhase);
    }

    // CRT Effects
    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2 * 1.2f);
    nvgLineTo(vg, centerX + screenSize/2 * 1.2f, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2 * 1.2f);
    nvgLineTo(vg, centerX - screenSize/2 * 1.2f, centerY);
    nvgClosePath(vg);
    NVGpaint outerGlow = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.35f, screenSize * 0.55f,
                                          nvgRGBA(0, 110, 140, 60), nvgRGBA(0, 30, 40, 0));
    nvgFillPaint(vg, outerGlow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, centerX, centerY - screenSize/2 * 1.05f);
    nvgLineTo(vg, centerX + screenSize/2 * 1.05f, centerY);
    nvgLineTo(vg, centerX, centerY + screenSize/2 * 1.05f);
    nvgLineTo(vg, centerX - screenSize/2 * 1.05f, centerY);
    nvgClosePath(vg);
    NVGpaint innerGlow = nvgRadialGradient(vg, centerX, centerY, screenSize * 0.25f, screenSize * 0.38f,
                                          nvgRGBA(0, 150, 200, 120), nvgRGBA(0, 45, 60, 0));
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
                    float chaosAmount, float chaosPhase, float filterMorph,
                    float cutoffA, float cutoffB, float resonanceA, float resonanceB,
                    float filterMorphPhase, float cutoffPhase, float resonancePhase) {

    float totalActivity = chaosAmount + (cutoffA + cutoffB) * 0.2f;

    float avgResonance = (resonanceA + resonanceB) * 0.5f;
    float resonanceActivity = (avgResonance - 0.707f) * 2.0f;
    resonanceActivity = std::max(resonanceActivity, 0.0f);
    totalActivity += resonanceActivity * 0.3f;

    totalActivity = std::max(totalActivity, 0.35f);

    int baseSquares = 45 + (int)(filterMorph * 20);
    int resonanceSquares = (int)(resonanceActivity * 80);
    int activitySquares = (int)(totalActivity * 120);
    int numSquares = baseSquares + activitySquares + resonanceSquares;
    numSquares = clamp(numSquares, 45, 220);

    for (int i = 0; i < numSquares; i++) {
        float angle = (i / (float)numSquares) * 2.0f * M_PI * 3.7f;

        angle += time * 0.3f;
        angle += chaosPhase * 1.0f;
        angle += chaosPhase * 0.8f;
        angle += filterMorphPhase;
        angle += cutoffPhase;
        angle += resonancePhase;

        float baseRadius = (i / (float)numSquares) * maxRadius;
        float radiusVar = sinf(time * 3.0f + i * 0.2f) * maxRadius * 0.2f * chaosAmount;
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

        float baseSize = 1.5f + chaosAmount * 3.0f;
        float sizeVar = sinf(time * 4.0f + i * 0.3f + filterMorph * 5.0f) * 1.0f;
        float resonanceSize = resonanceActivity * 2.0f + sinf(time * 6.0f + i * 0.4f) * resonanceActivity * 1.5f;
        float squareSize = baseSize + sizeVar + resonanceSize;
        squareSize = clamp(squareSize, 0.5f, 6.0f);

        float hue = fmodf(time * 30.0f + i * 15.0f + filterMorph * 180.0f + resonanceActivity * 120.0f, 360.0f);

        float baseBrightness = 0.3f;
        float activityBrightness = chaosAmount * 0.7f;
        float filterBrightness = (cutoffA + cutoffB) * 0.1f;
        float resonanceBrightness = resonanceActivity * 0.5f + sinf(time * 8.0f + i * 0.6f) * resonanceActivity * 0.3f;

        float brightness = baseBrightness + activityBrightness + filterBrightness + resonanceBrightness;
        brightness = clamp(brightness, 0.2f, 1.2f);
        brightness *= (1.0f - (radius / maxRadius) * 0.3f);

        NVGcolor color;
        if (hue < 120.0f) {
            float t = hue / 120.0f;
            color = nvgRGBA((int)(0 * brightness), (int)((100 + t * 155) * brightness),
                           (int)(255 * brightness), (int)(brightness * 255));
        } else if (hue < 240.0f) {
            float t = (hue - 120.0f) / 120.0f;
            color = nvgRGBA((int)(t * 100 * brightness), (int)((255 - t * 100) * brightness),
                           (int)(255 * brightness), (int)(brightness * 255));
        } else {
            float t = (hue - 240.0f) / 120.0f;
            color = nvgRGBA((int)((150 - t * 150) * brightness), (int)(0 * brightness),
                           (int)(255 * brightness), (int)(brightness * 255));
        }

        nvgBeginPath(vg);
        nvgRect(vg, x - squareSize/2, y - squareSize/2, squareSize, squareSize);
        nvgFillColor(vg, color);
        nvgFill(vg);
    }
}
