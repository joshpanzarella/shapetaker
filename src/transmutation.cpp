#include "plugin.hpp"
#include <vector>
#include <array>
#include <string>
#include <random>
#include <algorithm>
#include <fstream>

// Forward declarations
struct Transmutation;

// Custom Shapetaker Widgets (using proper background/indicator setup like Clairaudient)
struct STKnobLarge : app::SvgKnob {
    widget::SvgWidget* bg;
    
    STKnobLarge() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_large.svg")));
        
        // Add background as first child (will be drawn behind the rotating part)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_large_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct STKnobMedium : app::SvgKnob {
    widget::SvgWidget* bg;
    
    STKnobMedium() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_medium.svg")));
        
        // Add background as first child (will be drawn behind the rotating part)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_medium_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct STKnobSmall : app::SvgKnob {
    widget::SvgWidget* bg;
    
    STKnobSmall() {
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        
        // Use the indicator as the rotating part
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/indicators/st_knob_oscilloscope_indicator_small.svg")));
        
        // Add background as first child (will be drawn behind the rotating part)
        bg = new widget::SvgWidget;
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knobs/backgrounds/st_knob_small_bg_light.svg")));
        addChild(bg);
        
        // Move the background to the back by reordering children
        removeChild(bg);
        children.insert(children.begin(), bg);
    }
};

struct STToggleSwitch : app::SvgSwitch {
    STToggleSwitch() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/safety_toggle_switch_OFF.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/safety_toggle_switch_ON.svg")));
    }
};

struct STToggleSwitchSmall : app::SvgSwitch {
    STToggleSwitchSmall() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/safety_toggle_switch_OFF_small.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/safety_toggle_switch_ON_small.svg")));
    }
};

struct STSelector : app::SvgSwitch {
    STSelector() {
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_0.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_1.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_2.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_3.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_4.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/switches/st_vintage_selector_5.svg")));
        box.size = Vec(35, 35);
    }
};

struct STPort : app::SvgPort {
    STPort() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/ports/st_bnc_connector.svg")));
    }
};

// Vintage 1940s Push Button Widget
struct Vintage1940sButton : app::SvgSwitch {
    Vintage1940sButton() {
        momentary = true; // Makes it a momentary push button
        
        // We'll draw this custom rather than using SVG for more control
        box.size = Vec(18, 18);
    }
    
    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        
        // Get the current parameter value to determine if pressed
        bool pressed = false;
        if (getParamQuantity()) {
            pressed = getParamQuantity()->getValue() > 0.5f;
        }
        
        nvgSave(vg);
        
        float centerX = box.size.x * 0.5f;
        float centerY = box.size.y * 0.5f;
        float outerRadius = box.size.x * 0.45f;
        float innerRadius = outerRadius * 0.7f;
        
        // Draw the outer bezel (chrome/steel look)
        nvgBeginPath(vg);
        nvgCircle(vg, centerX, centerY, outerRadius);
        
        // Create a radial gradient for the chrome bezel
        NVGpaint bezelGradient = nvgRadialGradient(vg, 
            centerX - outerRadius * 0.3f, centerY - outerRadius * 0.3f, // Light source from top-left
            outerRadius * 0.2f, outerRadius * 1.2f,
            nvgRGB(220, 220, 230), // Bright chrome highlight
            nvgRGB(120, 120, 130)  // Darker chrome shadow
        );
        nvgFillPaint(vg, bezelGradient);
        nvgFill(vg);
        
        // Add a darker outer ring for depth
        nvgBeginPath(vg);
        nvgCircle(vg, centerX, centerY, outerRadius);
        nvgStrokeColor(vg, nvgRGB(80, 80, 90));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        
        // Draw the button face
        nvgBeginPath(vg);
        nvgCircle(vg, centerX, centerY + (pressed ? 1.0f : 0.0f), innerRadius);
        
        // Button face color - bakelite/phenolic resin look
        NVGcolor buttonColor;
        if (pressed) {
            // Darker when pressed
            buttonColor = nvgRGB(45, 35, 25); // Dark brown bakelite
        } else {
            // Normal state - warm brown bakelite
            buttonColor = nvgRGB(65, 50, 35);
        }
        
        // Create gradient for the button face
        NVGpaint buttonGradient = nvgRadialGradient(vg,
            centerX - innerRadius * 0.4f, centerY - innerRadius * 0.4f + (pressed ? 1.0f : 0.0f),
            innerRadius * 0.1f, innerRadius * 1.1f,
            nvgRGB(85, 65, 45), // Lighter highlight
            buttonColor // Base color
        );
        nvgFillPaint(vg, buttonGradient);
        nvgFill(vg);
        
        // Add inner shadow when pressed
        if (pressed) {
            nvgBeginPath(vg);
            nvgCircle(vg, centerX, centerY + 1.0f, innerRadius);
            NVGpaint shadowGradient = nvgRadialGradient(vg,
                centerX, centerY + 1.0f,
                0, innerRadius,
                nvgRGBA(0, 0, 0, 60), // Dark center
                nvgRGBA(0, 0, 0, 0)   // Transparent edge
            );
            nvgFillPaint(vg, shadowGradient);
            nvgFill(vg);
        } else {
            // Add highlight when not pressed
            nvgBeginPath(vg);
            nvgCircle(vg, centerX - innerRadius * 0.3f, centerY - innerRadius * 0.3f, innerRadius * 0.4f);
            NVGpaint highlightGradient = nvgRadialGradient(vg,
                centerX - innerRadius * 0.3f, centerY - innerRadius * 0.3f,
                0, innerRadius * 0.4f,
                nvgRGBA(255, 255, 255, 40), // White highlight
                nvgRGBA(255, 255, 255, 0)   // Transparent edge
            );
            nvgFillPaint(vg, highlightGradient);
            nvgFill(vg);
        }
        
        // Add a subtle inner ring
        nvgBeginPath(vg);
        nvgCircle(vg, centerX, centerY + (pressed ? 1.0f : 0.0f), innerRadius);
        nvgStrokeColor(vg, pressed ? nvgRGB(30, 25, 20) : nvgRGB(95, 75, 55));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);
        
        nvgRestore(vg);
    }
};

// Custom colored jewel LEDs for sequence identification
struct TealJewelLEDSmall : ModuleLightWidget {
    TealJewelLEDSmall() {
        box.size = Vec(15, 15);
        
        // Try to load the jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_small.svg"));
        
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up teal color (single color, not RGB)
        addBaseColor(nvgRGB(64, 224, 208)); // Teal color
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 7.5, 7.5, 7.2);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 7.5, 7.5, 4.8);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct PurpleJewelLEDSmall : ModuleLightWidget {
    PurpleJewelLEDSmall() {
        box.size = Vec(15, 15);
        
        // Try to load the jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_small.svg"));
        
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up purple color (single color, not RGB)
        addBaseColor(nvgRGB(180, 64, 255)); // Purple color
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 7.5, 7.5, 7.2);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 7.5, 7.5, 4.8);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct TealJewelLEDMedium : ModuleLightWidget {
    TealJewelLEDMedium() {
        box.size = Vec(20, 20);
        
        // Try to load the jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));
        
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up teal color (single color, not RGB)
        addBaseColor(nvgRGB(64, 224, 208)); // Teal color
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.6);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 6.4);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

struct PurpleJewelLEDMedium : ModuleLightWidget {
    PurpleJewelLEDMedium() {
        box.size = Vec(20, 20);
        
        // Try to load the jewel SVG
        widget::SvgWidget* sw = new widget::SvgWidget;
        std::shared_ptr<Svg> svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/leds/jewel_led_medium.svg"));
        
        if (svg) {
            sw->setSvg(svg);
            addChild(sw);
        }
        
        // Set up purple color (single color, not RGB)
        addBaseColor(nvgRGB(180, 64, 255)); // Purple color
    }
    
    void draw(const DrawArgs& args) override {
        if (children.empty()) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 9.6);
            nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
            nvgFill(args.vg);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 10, 10, 6.4);
            nvgFillColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
            nvgFill(args.vg);
        }
        
        ModuleLightWidget::draw(args);
    }
};

// Use the jewel LEDs from plugin.hpp - they're already properly defined there
// No need to redefine them here since they're in the global header

struct ChordData {
    std::string name;
    std::vector<float> intervals;
    int preferredVoices;
    std::string category;
};

struct ChordPack {
    std::string name;
    std::string key;
    std::vector<ChordData> chords;
    std::string description;
};

struct SequenceStep {
    int chordIndex;
    int voiceCount;
    int alchemySymbolId;
    
    SequenceStep() : chordIndex(-999), voiceCount(1), alchemySymbolId(-999) {} // -999 = uninitialized, -1 = REST
};

struct Sequence {
    std::array<SequenceStep, 64> steps;
    int length;
    int currentStep;
    bool running;
    float clockPhase;
    
    Sequence() : length(16), currentStep(0), running(false), clockPhase(0.0f) {}
};

// Matrix8x8Widget - declaration only, implementation after Transmutation class
// High-Resolution Matrix Widget - 512x512 pixel canvas with 8x8 logical grid
struct HighResMatrixWidget : Widget {
    Transmutation* module;
    static constexpr int MATRIX_SIZE = 8;
    static constexpr float CANVAS_SIZE = 512.0f;  // High resolution canvas
    static constexpr float CELL_SIZE = CANVAS_SIZE / MATRIX_SIZE;  // 64x64 pixels per cell
    
    HighResMatrixWidget(Transmutation* module);
    void onButton(const event::Button& e) override;
    void onMatrixClick(int x, int y);
    void onMatrixRightClick(int x, int y);
    void programStep(Sequence& seq, int stepIndex);
    void drawLayer(const DrawArgs& args, int layer) override;
    void drawMatrix(const DrawArgs& args);
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color = nvgRGBA(255, 255, 255, 255));
    void drawRestSymbol(const DrawArgs& args, Vec pos);
    void drawTieSymbol(const DrawArgs& args, Vec pos);
    void drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor = nvgRGBA(255, 255, 255, 255));
};

// Legacy Matrix Widget (keeping for reference during transition)
struct Matrix8x8Widget : Widget {
    Transmutation* module;
    static constexpr int MATRIX_SIZE = 8;
    static constexpr float LED_SIZE = 16.0f;
    static constexpr float LED_SPACING = 20.0f;
    
    Matrix8x8Widget(Transmutation* module);
    void onButton(const event::Button& e) override;
    void onMatrixClick(int x, int y);
    void onMatrixRightClick(int x, int y);
    void programStep(Sequence& seq, int stepIndex);
    void drawLayer(const DrawArgs& args, int layer) override;
    void drawMatrix(const DrawArgs& args);
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color = nvgRGBA(255, 255, 255, 255));
    void drawRestSymbol(const DrawArgs& args, Vec pos);
    void drawTieSymbol(const DrawArgs& args, Vec pos);
    void drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor = nvgRGBA(255, 255, 255, 255));
};

struct Transmutation : Module {
    enum ParamId {
        // Edit mode controls
        EDIT_A_PARAM,
        EDIT_B_PARAM,
        
        // Sequence controls
        LENGTH_A_PARAM,
        LENGTH_B_PARAM,
        START_A_PARAM,
        STOP_A_PARAM,
        RESET_A_PARAM,
        START_B_PARAM,
        STOP_B_PARAM,
        RESET_B_PARAM,
        
        // Clock controls
        INTERNAL_CLOCK_PARAM,
        BPM_MULTIPLIER_PARAM,
        
        // Sequence B mode
        SEQ_B_MODE_PARAM,
        
        // Chord pack selection
        CHORD_PACK_PARAM,
        
        // Alchemical symbol buttons (12)
        SYMBOL_1_PARAM,
        SYMBOL_2_PARAM,
        SYMBOL_3_PARAM,
        SYMBOL_4_PARAM,
        SYMBOL_5_PARAM,
        SYMBOL_6_PARAM,
        SYMBOL_7_PARAM,
        SYMBOL_8_PARAM,
        SYMBOL_9_PARAM,
        SYMBOL_10_PARAM,
        SYMBOL_11_PARAM,
        SYMBOL_12_PARAM,
        
        // Rest and tie buttons
        REST_PARAM,
        TIE_PARAM,
        
        PARAMS_LEN
    };
    
    enum InputId {
        CLOCK_A_INPUT,
        CLOCK_B_INPUT,
        RESET_A_INPUT,
        RESET_B_INPUT,
        START_A_INPUT,
        STOP_A_INPUT,
        START_B_INPUT,
        STOP_B_INPUT,
        INPUTS_LEN
    };
    
    enum OutputId {
        // Sequence A polyphonic outputs (6 voices)
        CV_A_OUTPUT,
        GATE_A_OUTPUT,
        
        // Sequence B polyphonic outputs (6 voices)  
        CV_B_OUTPUT,
        GATE_B_OUTPUT,
        
        OUTPUTS_LEN
    };
    
    enum LightId {
        EDIT_A_LIGHT,
        EDIT_B_LIGHT,
        RUNNING_A_LIGHT,
        RUNNING_B_LIGHT,
        
        // Alchemical symbol lights (12 symbols × 3 colors each = 36 lights)
        SYMBOL_1_LIGHT,
        SYMBOL_1_LIGHT_GREEN = SYMBOL_1_LIGHT + 1,
        SYMBOL_1_LIGHT_BLUE = SYMBOL_1_LIGHT + 2,
        SYMBOL_2_LIGHT,
        SYMBOL_2_LIGHT_GREEN = SYMBOL_2_LIGHT + 1,
        SYMBOL_2_LIGHT_BLUE = SYMBOL_2_LIGHT + 2,
        SYMBOL_3_LIGHT,
        SYMBOL_3_LIGHT_GREEN = SYMBOL_3_LIGHT + 1,
        SYMBOL_3_LIGHT_BLUE = SYMBOL_3_LIGHT + 2,
        SYMBOL_4_LIGHT,
        SYMBOL_4_LIGHT_GREEN = SYMBOL_4_LIGHT + 1,
        SYMBOL_4_LIGHT_BLUE = SYMBOL_4_LIGHT + 2,
        SYMBOL_5_LIGHT,
        SYMBOL_5_LIGHT_GREEN = SYMBOL_5_LIGHT + 1,
        SYMBOL_5_LIGHT_BLUE = SYMBOL_5_LIGHT + 2,
        SYMBOL_6_LIGHT,
        SYMBOL_6_LIGHT_GREEN = SYMBOL_6_LIGHT + 1,
        SYMBOL_6_LIGHT_BLUE = SYMBOL_6_LIGHT + 2,
        SYMBOL_7_LIGHT,
        SYMBOL_7_LIGHT_GREEN = SYMBOL_7_LIGHT + 1,
        SYMBOL_7_LIGHT_BLUE = SYMBOL_7_LIGHT + 2,
        SYMBOL_8_LIGHT,
        SYMBOL_8_LIGHT_GREEN = SYMBOL_8_LIGHT + 1,
        SYMBOL_8_LIGHT_BLUE = SYMBOL_8_LIGHT + 2,
        SYMBOL_9_LIGHT,
        SYMBOL_9_LIGHT_GREEN = SYMBOL_9_LIGHT + 1,
        SYMBOL_9_LIGHT_BLUE = SYMBOL_9_LIGHT + 2,
        SYMBOL_10_LIGHT,
        SYMBOL_10_LIGHT_GREEN = SYMBOL_10_LIGHT + 1,
        SYMBOL_10_LIGHT_BLUE = SYMBOL_10_LIGHT + 2,
        SYMBOL_11_LIGHT,
        SYMBOL_11_LIGHT_GREEN = SYMBOL_11_LIGHT + 1,
        SYMBOL_11_LIGHT_BLUE = SYMBOL_11_LIGHT + 2,
        SYMBOL_12_LIGHT,
        SYMBOL_12_LIGHT_GREEN = SYMBOL_12_LIGHT + 1,
        SYMBOL_12_LIGHT_BLUE = SYMBOL_12_LIGHT + 2,
        
        LIGHTS_LEN
    };
    
    // Sequencer state
    Sequence sequenceA;
    Sequence sequenceB;
    
    // Edit mode state
    bool editModeA = false;
    bool editModeB = false;
    int selectedSymbol = -1;
    
    // Symbol preview display system (8-bit retro style)
    std::string displayChordName = "";
    int displaySymbolId = -999; // -999 means no symbol display
    float symbolPreviewTimer = 0.0f;
    static constexpr float SYMBOL_PREVIEW_DURATION = 0.40f; // Show for 400ms
    
    // Chord pack system
    ChordPack currentChordPack;
    std::array<int, 20> symbolToChordMapping; // Expanded from 12 to 20 symbols
    std::array<int, 12> buttonToSymbolMapping; // Maps button positions 0-11 to symbol IDs 0-19
    
    // Clock system
    float internalClock = 0.0f;
    float clockRate = 120.0f; // BPM
    
    // Triggers
    dsp::SchmittTrigger editATrigger;
    dsp::SchmittTrigger editBTrigger;
    dsp::SchmittTrigger startATrigger;
    dsp::SchmittTrigger stopATrigger;
    dsp::SchmittTrigger resetATrigger;
    dsp::SchmittTrigger startBTrigger;
    dsp::SchmittTrigger stopBTrigger;
    dsp::SchmittTrigger resetBTrigger;
    dsp::SchmittTrigger symbolTriggers[12];
    dsp::SchmittTrigger restTrigger;
    dsp::SchmittTrigger tieTrigger;
    dsp::SchmittTrigger clockATrigger;
    dsp::SchmittTrigger clockBTrigger;
    dsp::SchmittTrigger resetAInputTrigger;
    dsp::SchmittTrigger resetBInputTrigger;
    dsp::SchmittTrigger startAInputTrigger;
    dsp::SchmittTrigger stopAInputTrigger;
    dsp::SchmittTrigger startBInputTrigger;
    dsp::SchmittTrigger stopBInputTrigger;
    
    Transmutation() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Edit mode buttons
        configParam(EDIT_A_PARAM, 0.f, 1.f, 0.f, "Edit Transmutation A");
        configParam(EDIT_B_PARAM, 0.f, 1.f, 0.f, "Edit Transmutation B");
        
        // Transmutation controls
        configParam(LENGTH_A_PARAM, 1.f, 64.f, 16.f, "Transmutation A Length");
        paramQuantities[LENGTH_A_PARAM]->snapEnabled = true;
        configParam(LENGTH_B_PARAM, 1.f, 64.f, 16.f, "Transmutation B Length");
        paramQuantities[LENGTH_B_PARAM]->snapEnabled = true;
        configParam(START_A_PARAM, 0.f, 1.f, 0.f, "Start Transmutation A");
        configParam(STOP_A_PARAM, 0.f, 1.f, 0.f, "Stop Transmutation A");
        configParam(RESET_A_PARAM, 0.f, 1.f, 0.f, "Reset Transmutation A");
        configParam(START_B_PARAM, 0.f, 1.f, 0.f, "Start Transmutation B");
        configParam(STOP_B_PARAM, 0.f, 1.f, 0.f, "Stop Transmutation B");
        configParam(RESET_B_PARAM, 0.f, 1.f, 0.f, "Reset Transmutation B");
        
        // Clock control
        configParam(INTERNAL_CLOCK_PARAM, 20.f, 200.f, 120.f, "Internal Clock", " BPM");
        paramQuantities[INTERNAL_CLOCK_PARAM]->snapEnabled = true;
        
        // BPM Multiplier
        configParam(BPM_MULTIPLIER_PARAM, 0.f, 3.f, 0.f, "BPM Multiplier");
        paramQuantities[BPM_MULTIPLIER_PARAM]->snapEnabled = true;
        
        // Transmutation B mode (0=Independent, 1=Harmony, 2=Lock)
        configSwitch(SEQ_B_MODE_PARAM, 0.f, 2.f, 0.f, "Transmutation B Mode", {"Independent", "Harmony", "Lock"});
        
        // Chord pack selection
        configParam(CHORD_PACK_PARAM, 0.f, 1.f, 0.f, "Load Chord Pack");
        
        // Alchemical symbol buttons
        for (int i = 0; i < 12; i++) {
            configParam(SYMBOL_1_PARAM + i, 0.f, 1.f, 0.f, "Alchemical Symbol " + std::to_string(i + 1));
        }
        
        // Rest and tie
        configParam(REST_PARAM, 0.f, 1.f, 0.f, "Rest");
        configParam(TIE_PARAM, 0.f, 1.f, 0.f, "Tie");
        
        // Inputs
        configInput(CLOCK_A_INPUT, "Clock A");
        configInput(CLOCK_B_INPUT, "Clock B");
        configInput(RESET_A_INPUT, "Reset A");
        configInput(RESET_B_INPUT, "Reset B");
        configInput(START_A_INPUT, "Start A Trigger");
        configInput(STOP_A_INPUT, "Stop A Trigger");
        configInput(START_B_INPUT, "Start B Trigger");
        configInput(STOP_B_INPUT, "Stop B Trigger");
        
        // Outputs
        configOutput(CV_A_OUTPUT, "CV A (Polyphonic)");
        configOutput(GATE_A_OUTPUT, "Gate A (Polyphonic)");
        configOutput(CV_B_OUTPUT, "CV B (Polyphonic)");
        configOutput(GATE_B_OUTPUT, "Gate B (Polyphonic)");
        
        // Initialize symbol mapping to -1 (no chord assigned)
        symbolToChordMapping.fill(-1);
        
        // Initialize button mapping to default 0-11 sequence
        for (int i = 0; i < 12; i++) {
            buttonToSymbolMapping[i] = i;
        }
        
        // Load default chord pack
        loadDefaultChordPack();
    }
    
    void process(const ProcessArgs& args) override {
        // Handle edit mode toggles - ensure one mode is always active
        if (editATrigger.process(params[EDIT_A_PARAM].getValue())) {
            if (!editModeA) {
                // Switching to Edit A mode
                editModeA = true;
                editModeB = false;
            } else {
                // Leaving Edit A mode - enter play mode
                editModeA = false;
                editModeB = false;
            }
        }
        
        if (editBTrigger.process(params[EDIT_B_PARAM].getValue())) {
            if (!editModeB) {
                // Switching to Edit B mode
                editModeB = true;
                editModeA = false;
            } else {
                // Leaving Edit B mode - enter play mode
                editModeB = false;
                editModeA = false;
            }
        }
        
        // Ensure at least one mode is active (default to Edit A if somehow none are active)
        if (!editModeA && !editModeB) {
            // This is play mode - explicitly do nothing as this is a valid state
        }
        
        // Update sequence lengths from parameters
        sequenceA.length = (int)params[LENGTH_A_PARAM].getValue();
        sequenceB.length = (int)params[LENGTH_B_PARAM].getValue();
        
        // Handle sequence controls
        if (startATrigger.process(params[START_A_PARAM].getValue())) {
            sequenceA.running = true;
        }
        if (stopATrigger.process(params[STOP_A_PARAM].getValue())) {
            sequenceA.running = false;
        }
        if (resetATrigger.process(params[RESET_A_PARAM].getValue())) {
            sequenceA.currentStep = 0;
            sequenceA.clockPhase = 0.0f;
        }
        
        if (startBTrigger.process(params[START_B_PARAM].getValue())) {
            sequenceB.running = true;
        }
        if (stopBTrigger.process(params[STOP_B_PARAM].getValue())) {
            sequenceB.running = false;
        }
        if (resetBTrigger.process(params[RESET_B_PARAM].getValue())) {
            sequenceB.currentStep = 0;
            sequenceB.clockPhase = 0.0f;
        }
        
        // Handle external reset inputs
        if (inputs[RESET_A_INPUT].isConnected()) {
            if (resetAInputTrigger.process(inputs[RESET_A_INPUT].getVoltage())) {
                sequenceA.currentStep = 0;
                sequenceA.clockPhase = 0.0f;
            }
        }
        
        if (inputs[RESET_B_INPUT].isConnected()) {
            if (resetBInputTrigger.process(inputs[RESET_B_INPUT].getVoltage())) {
                sequenceB.currentStep = 0;
                sequenceB.clockPhase = 0.0f;
            }
        }
        
        // Handle external start/stop trigger inputs
        if (inputs[START_A_INPUT].isConnected()) {
            if (startAInputTrigger.process(inputs[START_A_INPUT].getVoltage())) {
                sequenceA.running = true;
            }
        }
        
        if (inputs[STOP_A_INPUT].isConnected()) {
            if (stopAInputTrigger.process(inputs[STOP_A_INPUT].getVoltage())) {
                sequenceA.running = false;
            }
        }
        
        if (inputs[START_B_INPUT].isConnected()) {
            if (startBInputTrigger.process(inputs[START_B_INPUT].getVoltage())) {
                sequenceB.running = true;
            }
        }
        
        if (inputs[STOP_B_INPUT].isConnected()) {
            if (stopBInputTrigger.process(inputs[STOP_B_INPUT].getVoltage())) {
                sequenceB.running = false;
            }
        }
        
        // Handle chord pack loading
        if (params[CHORD_PACK_PARAM].getValue() > 0.5f) {
            params[CHORD_PACK_PARAM].setValue(0.0f); // Reset button
            // File browser will be handled by widget
        }
        
        // Handle symbol button presses
        for (int i = 0; i < 12; i++) {
            if (symbolTriggers[i].process(params[SYMBOL_1_PARAM + i].getValue())) {
                // Map button position to actual symbol ID
                int symbolId = buttonToSymbolMapping[i];
                onSymbolPressed(symbolId);
            }
        }
        
        // Handle rest/tie buttons
        if (restTrigger.process(params[REST_PARAM].getValue())) {
            onSymbolPressed(-1); // Rest symbol
        }
        if (tieTrigger.process(params[TIE_PARAM].getValue())) {
            onSymbolPressed(-2); // Tie symbol
        }
        
        // Update internal clock
        float baseBPM = params[INTERNAL_CLOCK_PARAM].getValue();
        int multiplierIndex = (int)params[BPM_MULTIPLIER_PARAM].getValue();
        float multipliers[] = {1.0f, 2.0f, 4.0f, 8.0f};
        float multiplier = multipliers[multiplierIndex];
        clockRate = baseBPM * multiplier;
        float clockFreq = clockRate / 60.0f; // Convert BPM to Hz
        internalClock += args.sampleTime * clockFreq;
        
        // Generate internal clock pulse when needed
        bool internalClockTrigger = false;
        if (internalClock >= 1.0f) {
            internalClock -= 1.0f;
            internalClockTrigger = true;
        }
        
        // Process sequences
        processSequence(sequenceA, CLOCK_A_INPUT, CV_A_OUTPUT, GATE_A_OUTPUT, args, internalClockTrigger);
        processSequenceB(args, internalClockTrigger);
        
        // Update chord name display timer
        // Update symbol preview timer
        if (symbolPreviewTimer > 0.0f) {
            symbolPreviewTimer -= args.sampleTime;
            if (symbolPreviewTimer <= 0.0f) {
                displayChordName = "";
                displaySymbolId = -999;
                symbolPreviewTimer = 0.0f;
            }
        }
        
        // Update lights
        lights[EDIT_A_LIGHT].setBrightness(editModeA ? 1.0f : 0.0f);
        lights[EDIT_B_LIGHT].setBrightness(editModeB ? 1.0f : 0.0f);
        lights[RUNNING_A_LIGHT].setBrightness(sequenceA.running ? 1.0f : 0.0f);
        lights[RUNNING_B_LIGHT].setBrightness(sequenceB.running ? 1.0f : 0.0f);
        
        // Update symbol lights with color coding for sequences
        for (int i = 0; i < 12; i++) {
            bool symbolActiveA = false;
            bool symbolActiveB = false;
            
            if (sequenceA.running && getCurrentChordIndex(sequenceA) == i) {
                symbolActiveA = true;
            }
            if (sequenceB.running && getCurrentChordIndex(sequenceB) == i) {
                symbolActiveB = true;
            }
            
            // RGB Light indices: Red=0, Green=1, Blue=2
            int lightIndex = SYMBOL_1_LIGHT + i * 3;
            
            if (symbolActiveA && symbolActiveB) {
                // Both sequences - mix teal and purple = cyan-magenta
                lights[lightIndex + 0].setBrightness(0.5f); // Red
                lights[lightIndex + 1].setBrightness(1.0f); // Green (for teal component)
                lights[lightIndex + 2].setBrightness(1.0f); // Blue (for both teal and purple)
            } else if (symbolActiveA) {
                // Sequence A - Teal color (#00ffb4)
                lights[lightIndex + 0].setBrightness(0.0f); // Red
                lights[lightIndex + 1].setBrightness(1.0f); // Green
                lights[lightIndex + 2].setBrightness(0.7f); // Blue
            } else if (symbolActiveB) {
                // Sequence B - Purple color (#b400ff)
                lights[lightIndex + 0].setBrightness(0.7f); // Red
                lights[lightIndex + 1].setBrightness(0.0f); // Green
                lights[lightIndex + 2].setBrightness(1.0f); // Blue
            } else {
                // No sequence active - off
                lights[lightIndex + 0].setBrightness(0.0f);
                lights[lightIndex + 1].setBrightness(0.0f);
                lights[lightIndex + 2].setBrightness(0.0f);
            }
        }
    }
    
    void processSequence(Sequence& seq, int clockInputId, int cvOutputId, int gateOutputId, const ProcessArgs& args, bool internalClockTrigger) {
        if (!seq.running) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
            return;
        }
        
        // Get clock source (external overrides internal)
        bool useExternalClock = inputs[clockInputId].isConnected();
        bool clockTrigger = false;
        
        if (useExternalClock) {
            if (clockInputId == CLOCK_A_INPUT) {
                clockTrigger = clockATrigger.process(inputs[clockInputId].getVoltage());
            } else {
                clockTrigger = clockBTrigger.process(inputs[clockInputId].getVoltage());
            }
        } else {
            // Use the global internal clock trigger
            clockTrigger = internalClockTrigger;
        }
        
        // Advance sequence on clock
        if (clockTrigger) {
            seq.currentStep = (seq.currentStep + 1) % seq.length;
        }
        
        // Get current step data
        const SequenceStep& currentStep = seq.steps[seq.currentStep];
        
        // Output CV and gates based on current step
        if (currentStep.chordIndex >= 0 && currentStep.chordIndex < 20 && 
            symbolToChordMapping[currentStep.chordIndex] >= 0) {
            outputChord(currentStep, cvOutputId, gateOutputId);
        } else {
            // Rest or tie
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
        }
    }
    
    void processSequenceB(const ProcessArgs& args, bool internalClockTrigger) {
        int bMode = (int)params[SEQ_B_MODE_PARAM].getValue();
        
        switch (bMode) {
            case 0: // Independent mode
                processSequence(sequenceB, CLOCK_B_INPUT, CV_B_OUTPUT, GATE_B_OUTPUT, args, internalClockTrigger);
                break;
                
            case 1: // Harmony mode
                processSequenceBHarmony(args, internalClockTrigger);
                break;
                
            case 2: // Lock mode  
                processSequenceBLock(args, internalClockTrigger);
                break;
        }
    }
    
    void processSequenceBHarmony(const ProcessArgs& args, bool internalClockTrigger) {
        if (!sequenceB.running) {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
            return;
        }
        
        // In harmony mode, sequence B follows A's timing and chord but plays harmony notes
        if (!sequenceA.running) {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
            return;
        }
        
        // Get clock from sequence A or external B clock
        bool useExternalClock = inputs[CLOCK_B_INPUT].isConnected();
        bool clockTrigger = false;
        
        if (useExternalClock) {
            clockTrigger = clockBTrigger.process(inputs[CLOCK_B_INPUT].getVoltage());
        } else {
            // Follow sequence A's timing using internal clock
            clockTrigger = internalClockTrigger && sequenceA.running;
        }
        
        // Advance sequence B step
        if (clockTrigger) {
            sequenceB.currentStep = (sequenceB.currentStep + 1) % sequenceB.length;
        }
        
        // Get current chord from sequence A
        const SequenceStep& stepA = sequenceA.steps[sequenceA.currentStep];
        const SequenceStep& stepB = sequenceB.steps[sequenceB.currentStep];
        
        if (stepA.chordIndex >= 0 && stepA.chordIndex < 20 && 
            symbolToChordMapping[stepA.chordIndex] >= 0) {
            // Generate harmony based on sequence A's chord
            outputHarmony(stepA, stepB, CV_B_OUTPUT, GATE_B_OUTPUT);
        } else {
            // If A is resting, B rests too
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
        }
    }
    
    void processSequenceBLock(const ProcessArgs& args, bool internalClockTrigger) {
        if (!sequenceB.running) {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
            return;
        }
        
        // In lock mode, sequence B uses the same chord pack as A but has independent timing/progression
        // Standard clock handling for B
        bool useExternalClock = inputs[CLOCK_B_INPUT].isConnected();
        bool clockTrigger = false;
        
        if (useExternalClock) {
            clockTrigger = clockBTrigger.process(inputs[CLOCK_B_INPUT].getVoltage());
        } else {
            // Use the global internal clock trigger
            clockTrigger = internalClockTrigger;
        }
        
        // Advance sequence B
        if (clockTrigger) {
            sequenceB.currentStep = (sequenceB.currentStep + 1) % sequenceB.length;
        }
        
        // Output sequence B's programmed progression using same chord pack as A
        const SequenceStep& currentStep = sequenceB.steps[sequenceB.currentStep];
        if (currentStep.chordIndex >= 0 && currentStep.chordIndex < 20 && 
            symbolToChordMapping[currentStep.chordIndex] >= 0) {
            outputChord(currentStep, CV_B_OUTPUT, GATE_B_OUTPUT);
        } else {
            outputs[CV_B_OUTPUT].setChannels(0);
            outputs[GATE_B_OUTPUT].setChannels(0);
        }
    }
    
    void outputHarmony(const SequenceStep& stepA, const SequenceStep& stepB, int cvOutputId, int gateOutputId) {
        if (stepA.chordIndex < 0 || stepA.chordIndex >= (int)currentChordPack.chords.size()) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
            return;
        }
        
        const ChordData& chordA = currentChordPack.chords[symbolToChordMapping[stepA.chordIndex]];
        int voiceCount = std::min(stepB.voiceCount, 6);
        
        // Set up polyphonic outputs
        outputs[cvOutputId].setChannels(voiceCount);
        outputs[gateOutputId].setChannels(voiceCount);
        
        // Generate harmony notes based on the root chord
        // This creates harmony by using upper chord tones and inversions
        float rootNote = 0.0f; // C4 = 0V as standard
        
        for (int voice = 0; voice < voiceCount; voice++) {
            float harmonyInterval = 0.0f;
            
            if (voice < (int)chordA.intervals.size()) {
                // Use chord tones but transpose up an octave for harmony
                harmonyInterval = chordA.intervals[voice] + 12.0f; // +1 octave
                
                // Add some variation based on voice number for richer harmony
                if (voice % 2 == 1) {
                    harmonyInterval += 7.0f; // Add fifth for more harmonic interest
                }
            } else {
                // If more voices requested, cycle through intervals with additional octaves
                int intervalIndex = voice % chordA.intervals.size();
                int octaveOffset = (voice / chordA.intervals.size()) + 1; // Start at +1 octave for harmony
                harmonyInterval = chordA.intervals[intervalIndex] + octaveOffset * 12.0f;
                
                // Add fifth variation for odd voices
                if (voice % 2 == 1) {
                    harmonyInterval += 7.0f;
                }
            }
            
            float noteCV = rootNote + harmonyInterval / 12.0f; // Convert semitones to V/oct
            outputs[cvOutputId].setVoltage(noteCV, voice);
            outputs[gateOutputId].setVoltage(10.0f, voice); // Standard gate voltage
        }
    }
    
    void outputChord(const SequenceStep& step, int cvOutputId, int gateOutputId) {
        if (step.chordIndex < 0 || step.chordIndex >= (int)currentChordPack.chords.size()) {
            outputs[cvOutputId].setChannels(0);
            outputs[gateOutputId].setChannels(0);
            return;
        }
        
        const ChordData& chord = currentChordPack.chords[symbolToChordMapping[step.chordIndex]];
        int voiceCount = std::min(step.voiceCount, 6);
        
        // Set up polyphonic outputs
        outputs[cvOutputId].setChannels(voiceCount);
        outputs[gateOutputId].setChannels(voiceCount);
        
        // Calculate root note (C4 = 0V as standard in VCV Rack)
        float rootNote = 0.0f; // C4 = 0V, will be configurable later
        
        // Output chord tones with proper voice allocation
        for (int voice = 0; voice < voiceCount; voice++) {
            float noteCV = rootNote;
            
            if (voice < (int)chord.intervals.size()) {
                // Use chord intervals directly
                noteCV = rootNote + chord.intervals[voice] / 12.0f; // Convert semitones to V/oct
            } else {
                // If more voices requested than chord intervals, cycle through intervals in higher octaves
                int intervalIndex = voice % chord.intervals.size();
                int octaveOffset = voice / chord.intervals.size();
                noteCV = rootNote + (chord.intervals[intervalIndex] + octaveOffset * 12.0f) / 12.0f;
            }
            
            outputs[cvOutputId].setVoltage(noteCV, voice);
            outputs[gateOutputId].setVoltage(10.0f, voice); // Standard gate voltage
        }
    }
    
    int getCurrentChordIndex(const Sequence& seq) {
        return seq.steps[seq.currentStep].chordIndex;
    }
    
    void onSymbolPressed(int symbolIndex) {
        selectedSymbol = symbolIndex;
        
        // Display chord name on LED matrix
        // Trigger 8-bit symbol preview when selecting a symbol
        if (symbolIndex >= 0 && symbolIndex < 20 && 
            symbolToChordMapping[symbolIndex] >= 0 && symbolToChordMapping[symbolIndex] < (int)currentChordPack.chords.size()) {
            const ChordData& chord = currentChordPack.chords[symbolToChordMapping[symbolIndex]];
            displayChordName = chord.name;
            displaySymbolId = symbolIndex;
            symbolPreviewTimer = SYMBOL_PREVIEW_DURATION;
        } else if (symbolIndex == -1) {
            displayChordName = "REST";
            displaySymbolId = -1;
            symbolPreviewTimer = SYMBOL_PREVIEW_DURATION;
        } else if (symbolIndex == -2) {
            displayChordName = "TIE";
            displaySymbolId = -2;
            symbolPreviewTimer = SYMBOL_PREVIEW_DURATION;
        }
        
        // Audition the chord if we're in edit mode
        if ((editModeA || editModeB) && symbolIndex >= 0 && symbolIndex < 20) {
            auditionChord(symbolIndex);
        }
    }
    
    void auditionChord(int symbolIndex) {
        if (symbolIndex < 0 || symbolIndex >= 20 || 
            symbolToChordMapping[symbolIndex] < 0 || symbolToChordMapping[symbolIndex] >= (int)currentChordPack.chords.size()) {
            return;
        }
        
        const ChordData& chord = currentChordPack.chords[symbolToChordMapping[symbolIndex]];
        
        // Trigger a brief chord preview on the appropriate output
        if (editModeA) {
            outputChordAudition(chord, CV_A_OUTPUT, GATE_A_OUTPUT);
        } else if (editModeB) {
            outputChordAudition(chord, CV_B_OUTPUT, GATE_B_OUTPUT);
        }
    }
    
    void outputChordAudition(const ChordData& chord, int cvOutputId, int gateOutputId) {
        int voiceCount = std::min(chord.preferredVoices, 6);
        
        // Set up polyphonic outputs
        outputs[cvOutputId].setChannels(voiceCount);
        outputs[gateOutputId].setChannels(voiceCount);
        
        // Calculate root note (C4 = 0V as standard in VCV Rack)
        float rootNote = 0.0f; // C4 = 0V
        
        // Output chord tones with proper voice allocation (same as outputChord)
        for (int voice = 0; voice < voiceCount; voice++) {
            float noteCV = rootNote;
            
            if (voice < (int)chord.intervals.size()) {
                // Use chord intervals directly
                noteCV = rootNote + chord.intervals[voice] / 12.0f; // Convert semitones to V/oct
            } else {
                // If more voices requested than chord intervals, cycle through intervals in higher octaves
                int intervalIndex = voice % chord.intervals.size();
                int octaveOffset = voice / chord.intervals.size();
                noteCV = rootNote + (chord.intervals[intervalIndex] + octaveOffset * 12.0f) / 12.0f;
            }
            
            outputs[cvOutputId].setVoltage(noteCV, voice);
            outputs[gateOutputId].setVoltage(10.0f, voice); // Standard gate voltage
        }
        
        // Set a timer to turn off the audition after a short time
        // (This would need a proper gate generator for timing, keeping simple for now)
    }
    
    bool loadChordPackFromFile(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) return false;
            
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            
            json_error_t error;
            json_t* rootJ = json_loads(content.c_str(), 0, &error);
            if (!rootJ) return false;
            
            // Parse chord pack
            json_t* nameJ = json_object_get(rootJ, "name");
            json_t* keyJ = json_object_get(rootJ, "key");
            json_t* descJ = json_object_get(rootJ, "description");
            json_t* chordsJ = json_object_get(rootJ, "chords");
            
            if (!nameJ || !keyJ || !chordsJ) {
                json_decref(rootJ);
                return false;
            }
            
            currentChordPack.name = json_string_value(nameJ);
            currentChordPack.key = json_string_value(keyJ);
            currentChordPack.description = descJ ? json_string_value(descJ) : "";
            currentChordPack.chords.clear();
            
            // Parse chords array
            size_t chordIndex;
            json_t* chordJ;
            json_array_foreach(chordsJ, chordIndex, chordJ) {
                json_t* chordNameJ = json_object_get(chordJ, "name");
                json_t* intervalsJ = json_object_get(chordJ, "intervals");
                json_t* voicesJ = json_object_get(chordJ, "preferredVoices");
                json_t* categoryJ = json_object_get(chordJ, "category");
                
                if (!chordNameJ || !intervalsJ) continue;
                
                ChordData chord;
                chord.name = json_string_value(chordNameJ);
                chord.preferredVoices = voicesJ ? (int)json_integer_value(voicesJ) : 3;
                chord.category = categoryJ ? json_string_value(categoryJ) : "unknown";
                
                // Parse intervals array
                size_t intervalIndex;
                json_t* intervalJ;
                json_array_foreach(intervalsJ, intervalIndex, intervalJ) {
                    chord.intervals.push_back((float)json_real_value(intervalJ));
                }
                
                currentChordPack.chords.push_back(chord);
            }
            
            json_decref(rootJ);
            randomizeSymbolAssignment();
            return true;
            
        } catch (...) {
            return false;
        }
    }
    
    void randomizeSymbolAssignment() {
        if (currentChordPack.chords.empty()) return;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        
        // Create a pool of all available symbol IDs (0-19)
        std::vector<int> availableSymbolIds;
        for (int i = 0; i < 20; i++) {
            availableSymbolIds.push_back(i);
        }
        
        // Shuffle the symbol IDs to randomize which symbols appear on which buttons
        std::shuffle(availableSymbolIds.begin(), availableSymbolIds.end(), gen);
        
        // Clear all mappings first
        symbolToChordMapping.fill(-1);
        
        // Assign the first 12 shuffled symbols to the 12 button positions
        for (int buttonPos = 0; buttonPos < 12; buttonPos++) {
            buttonToSymbolMapping[buttonPos] = availableSymbolIds[buttonPos];
        }
        
        // Assign random chord indices to all 12 button symbols
        std::uniform_int_distribution<> dis(0, currentChordPack.chords.size() - 1);
        
        for (int buttonPos = 0; buttonPos < 12; buttonPos++) {
            int symbolId = buttonToSymbolMapping[buttonPos];
            int randomChordIndex = dis(gen);
            symbolToChordMapping[symbolId] = randomChordIndex;
        }
    }
    
    void loadDefaultChordPack() {
        currentChordPack.name = "Basic Major";
        currentChordPack.key = "C";
        currentChordPack.description = "Basic major chord progressions";
        
        // Add some basic chords
        ChordData cmaj = {"Cmaj", {0, 4, 7}, 3, "major"};
        ChordData dmin = {"Dmin", {2, 5, 9}, 3, "minor"};
        ChordData emin = {"Emin", {4, 7, 11}, 3, "minor"};
        ChordData fmaj = {"Fmaj", {5, 9, 0}, 3, "major"};
        ChordData gmaj = {"Gmaj", {7, 11, 2}, 3, "major"};
        ChordData amin = {"Amin", {9, 0, 4}, 3, "minor"};
        ChordData gmaj7 = {"Gmaj7", {7, 11, 2, 5}, 4, "major7"};
        ChordData fmaj7 = {"Fmaj7", {5, 9, 0, 4}, 4, "major7"};
        ChordData dmin7 = {"Dmin7", {2, 5, 9, 0}, 4, "minor7"};
        ChordData cmaj7 = {"Cmaj7", {0, 4, 7, 11}, 4, "major7"};
        ChordData amin7 = {"Amin7", {9, 0, 4, 7}, 4, "minor7"};
        ChordData emin7 = {"Emin7", {4, 7, 11, 2}, 4, "minor7"};
        
        currentChordPack.chords = {cmaj, dmin, emin, fmaj, gmaj, amin, gmaj7, fmaj7, dmin7, cmaj7, amin7, emin7};
        
        randomizeSymbolAssignment();
    }
};

// Matrix8x8Widget Implementation
// High-Resolution Matrix Widget Implementation
HighResMatrixWidget::HighResMatrixWidget(Transmutation* module) : module(module) {
    // Set widget size slightly larger to give symbols and dots more breathing room
    // Just a bit bigger than the original matrix for better spacing and visibility
    box.size = Vec(210.0f, 210.0f); // Slightly larger size for better spacing (26.25px per cell)
}

void HighResMatrixWidget::onButton(const event::Button& e) {
    if (e.action == GLFW_PRESS) {
        Vec pos = e.pos;
        
        // Convert click position to 8x8 grid coordinates
        // Scale from widget size to logical grid
        int x = (int)(pos.x / box.size.x * MATRIX_SIZE);
        int y = (int)(pos.y / box.size.y * MATRIX_SIZE);
        
        // Clamp to valid range
        x = clamp(x, 0, MATRIX_SIZE - 1);
        y = clamp(y, 0, MATRIX_SIZE - 1);
        
        if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
            onMatrixClick(x, y);
        } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            onMatrixRightClick(x, y);
        }
        e.consume(this);
    }
}

void HighResMatrixWidget::onMatrixClick(int x, int y) {
    if (!module) return;
    
    int stepIndex = y * MATRIX_SIZE + x;
    
    if (module->editModeA && stepIndex < 64) {
        programStep(module->sequenceA, stepIndex);
    }
    if (module->editModeB && stepIndex < 64) {
        programStep(module->sequenceB, stepIndex);
    }
}

void HighResMatrixWidget::onMatrixRightClick(int x, int y) {
    if (!module) return;
    
    int stepIndex = y * MATRIX_SIZE + x;
    
    // Right click cycles through voice counts (1-6)
    if (module->editModeA && stepIndex < module->sequenceA.length) {
        SequenceStep& step = module->sequenceA.steps[stepIndex];
        if (step.chordIndex >= 0 && step.chordIndex < 20 && 
            module->symbolToChordMapping[step.chordIndex] >= 0) {
            step.voiceCount = (step.voiceCount % 6) + 1;
        }
    }
    if (module->editModeB && stepIndex < module->sequenceB.length) {
        SequenceStep& step = module->sequenceB.steps[stepIndex];
        if (step.chordIndex >= 0 && step.chordIndex < 20 && 
            module->symbolToChordMapping[step.chordIndex] >= 0) {
            step.voiceCount = (step.voiceCount % 6) + 1;
        }
    }
}

void HighResMatrixWidget::programStep(Sequence& seq, int stepIndex) {
    if (stepIndex >= 64) return;
    
    SequenceStep& step = seq.steps[stepIndex];
    
    if (module->selectedSymbol >= 0 && module->selectedSymbol < 20 && 
        module->symbolToChordMapping[module->selectedSymbol] >= 0) {
        // Assign chord to step
        step.chordIndex = module->selectedSymbol;
        step.alchemySymbolId = module->selectedSymbol;
        // Set default voice count based on chord's preferred voices
        if (module->symbolToChordMapping[module->selectedSymbol] < (int)module->currentChordPack.chords.size()) {
            const ChordData& chord = module->currentChordPack.chords[module->symbolToChordMapping[module->selectedSymbol]];
            step.voiceCount = std::min(chord.preferredVoices, 6);
        }
    } else if (module->selectedSymbol == -1) {
        // Rest
        step.chordIndex = -1;
        step.alchemySymbolId = -1;
        step.voiceCount = 1;
    } else if (module->selectedSymbol == -2) {
        // Tie
        step.chordIndex = -2;
        step.alchemySymbolId = -2;
        step.voiceCount = 1;
    }
}

void HighResMatrixWidget::drawLayer(const DrawArgs& args, int layer) {
    if (layer == 1) {
        drawMatrix(args);
    }
    Widget::drawLayer(args, layer);
}

void HighResMatrixWidget::drawMatrix(const DrawArgs& args) {
    // Spooky 80s horror movie TV preview mode
    if (module && !module->displayChordName.empty() && module->displaySymbolId != -999) {
        nvgSave(args.vg);
        
        // Calculate VHS tape warping effects based on time - ultra slow and warped
        float time = APP->engine->getFrame() * 0.003f; // Slightly slower for more languid tape drag
        float waveA = sin(time * 0.6f) * 0.12f + sin(time * 0.9f) * 0.08f; // Even slower, more intense waves
        float waveB = cos(time * 0.4f) * 0.10f + cos(time * 0.7f) * 0.06f; // Deeper secondary waves
        float tapeWarp = sin(time * 0.25f) * 0.05f + cos(time * 0.35f) * 0.03f; // Slower, more intense warping
        float deepWarp = sin(time * 0.15f) * 0.08f; // Even slower deep warping layer
        
        // Dark VHS horror movie background with slight blue tint and warping - rounded corners
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8.0f); // Match LED matrix corner radius
        int bgBrightness = 8 + (int)(waveA * 12); // Slower brightness variation
        nvgFillColor(args.vg, nvgRGBA(bgBrightness, bgBrightness * 0.8f, bgBrightness * 1.3f, 255));
        nvgFill(args.vg);
        
        // Set up clipping to keep all effects within the rounded rectangle
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, 0, 0, box.size.x, box.size.y);
        
        // Add intense VHS tape warping scanlines with multiple distortion layers (clipped)
        for (int i = 0; i < box.size.y; i += 2) {
            float warpOffset = sin((i * 0.015f) + time * 1.2f) * 6.0f; // More intense warping
            warpOffset += cos((i * 0.008f) + time * 0.8f) * 4.0f; // Additional warp layer
            warpOffset += deepWarp * sin(i * 0.03f) * 3.0f; // Deep distortion based on position
            nvgBeginPath(args.vg);
            nvgRect(args.vg, warpOffset, i, box.size.x, 1);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 25 + (int)(waveB * 30)));
            nvgFill(args.vg);
        }
        
        // Add ultra slow moving VHS tape distortion bands with warping (clipped)
        for (int i = 0; i < 3; i++) {
            float distortionY = fmod(time * 8 + i * 160, box.size.y); // Ultra slow movement
            float warpWidth = sin(time * 0.7f + i) * 4.0f + deepWarp * 6.0f; // More intense warping
            float bandHeight = 2 + sin(time * 0.5f + i) * 1.0f; // Variable height
            nvgBeginPath(args.vg);
            nvgRect(args.vg, warpWidth, distortionY, box.size.x, bandHeight);
            nvgFillColor(args.vg, nvgRGBA(80 + (int)(waveA * 40), 100 + (int)(waveB * 40), 120, 40 + (int)(tapeWarp * 200)));
            nvgFill(args.vg);
        }
        
        nvgRestore(args.vg); // End clipping
        
        // Draw the symbol with Shapetaker colors and VHS warping
        nvgSave(args.vg);
        float shakeX = sin(time * 1.8f) * 0.8f + tapeWarp * 3.0f + deepWarp * 2.5f; // Much slower, intense warping
        float shakeY = cos(time * 1.4f) * 0.6f + waveA * 2.0f + waveB * 1.5f; // Slower, more layered warping
        nvgTranslate(args.vg, box.size.x / 2 + shakeX, box.size.y * 0.40f + shakeY); // More centered vertically
        nvgScale(args.vg, 5.0f, 5.0f); // Much bigger and more pixelated
        
        // Cycle through Shapetaker colors with ultra slow VHS color shift
        float colorCycle = sin(time * 0.3f) * 0.5f + 0.5f; // Even slower color transitions like old tape
        int symbolR, symbolG, symbolB;
        
        if (colorCycle < 0.25f) {
            // Teal with VHS warping
            symbolR = 0;
            symbolG = 180 + (int)(waveA * 50);
            symbolB = 180 + (int)(waveB * 50);
        } else if (colorCycle < 0.5f) {
            // Purple with VHS warping  
            symbolR = 180 + (int)(waveA * 50);
            symbolG = 0;
            symbolB = 255;
        } else if (colorCycle < 0.75f) {
            // Muted green with VHS warping
            symbolR = 60 + (int)(waveB * 30);
            symbolG = 120 + (int)(waveA * 40);
            symbolB = 80 + (int)(tapeWarp * 80);
        } else {
            // Gray with VHS warping
            symbolR = 140 + (int)(waveA * 40);
            symbolG = 140 + (int)(waveB * 40);
            symbolB = 150 + (int)(tapeWarp * 60);
        }
        
        // Draw symbol with intense glow layers
        for (int glow = 0; glow < 6; glow++) {
            float glowAlpha = 80 / (glow + 1);
            float glowOffset = glow * 0.8f;
            
            // Create blur effect with multiple offset draws
            for (int blur = 0; blur < 2; blur++) {
                float blurOffset = blur * 0.3f;
                drawAlchemicalSymbol(args, Vec(glowOffset + blurOffset, glowOffset + blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                drawAlchemicalSymbol(args, Vec(-glowOffset + blurOffset, glowOffset - blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                drawAlchemicalSymbol(args, Vec(glowOffset - blurOffset, -glowOffset + blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                drawAlchemicalSymbol(args, Vec(-glowOffset - blurOffset, -glowOffset - blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
            }
        }
        
        // Main symbol with bright core
        drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGBA(symbolR, symbolG, symbolB, 255));
        
        // Extra bright white core for VHS warping intensity
        drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGBA(255, 255, 255, 50 + (int)(waveA * 30)));
        nvgRestore(args.vg);
        
        // Set up horror movie text with much more pixelated chunky font
        nvgFontSize(args.vg, 36.0f); // Even bigger text for more pixelated look
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        
        // Text position with slow, intense VHS warping movement, closer to symbol
        float textX = box.size.x / 2 + sin(time * 1.1f) * 0.6f + tapeWarp * 2.5f + deepWarp * 2.0f;
        float textY = box.size.y * 0.65f + cos(time * 0.9f) * 0.4f + waveB * 1.2f + waveA * 0.8f; // More warped movement
        
        // Multiple chunky shadows for extreme pixelated horror effect
        nvgFillColor(args.vg, nvgRGBA(40, 40, 50, 220)); // Dark gray shadow
        nvgText(args.vg, textX + 3, textY + 3, module->displayChordName.c_str(), NULL);
        nvgText(args.vg, textX + 2, textY + 4, module->displayChordName.c_str(), NULL);
        nvgText(args.vg, textX + 4, textY + 2, module->displayChordName.c_str(), NULL);
        
        // Main text with ultra slowly cycling Shapetaker colors
        int textR, textG, textB;
        float textColorCycle = sin(time * 0.25f + 1.5f) * 0.5f + 0.5f; // Even slower text color transitions
        
        if (textColorCycle < 0.25f) {
            // Bright teal with VHS warping
            textR = 0;
            textG = 255;
            textB = 200 + (int)(waveA * 40);
        } else if (textColorCycle < 0.5f) {
            // Bright purple with VHS warping
            textR = 220 + (int)(waveB * 25);
            textG = 60 + (int)(tapeWarp * 100);
            textB = 255;
        } else if (textColorCycle < 0.75f) {
            // Bright muted green with VHS warping
            textR = 120 + (int)(waveA * 40);
            textG = 200 + (int)(waveB * 35);
            textB = 140 + (int)(tapeWarp * 80);
        } else {
            // Bright gray-white with VHS warping
            textR = 220 + (int)(waveA * 25);
            textG = 220 + (int)(waveB * 25);
            textB = 240 + (int)(tapeWarp * 15);
        }
        
        nvgFillColor(args.vg, nvgRGBA(textR, textG, textB, 255));
        nvgText(args.vg, textX, textY, module->displayChordName.c_str(), NULL);
        
        // Intense multi-layer glow effects with blur
        for (int glow = 0; glow < 8; glow++) {
            float glowRadius = (glow + 1) * 1.5f;
            float glowAlpha = 120 / (glow + 1); // Fade out each layer
            
            // Blur effect by drawing at slightly offset positions
            for (int blur = 0; blur < 3; blur++) {
                float blurOffset = blur * 0.5f;
                nvgFillColor(args.vg, nvgRGBA(textR * 0.8f, textG * 0.8f, textB * 0.8f, glowAlpha));
                nvgText(args.vg, textX + blurOffset, textY + blurOffset, module->displayChordName.c_str(), NULL);
                nvgText(args.vg, textX - blurOffset, textY + blurOffset, module->displayChordName.c_str(), NULL);
                nvgText(args.vg, textX + blurOffset, textY - blurOffset, module->displayChordName.c_str(), NULL);
                nvgText(args.vg, textX - blurOffset, textY - blurOffset, module->displayChordName.c_str(), NULL);
            }
        }
        
        // Extra intense core glow with VHS warping
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 80 + (int)(waveA * 40)));
        nvgText(args.vg, textX, textY, module->displayChordName.c_str(), NULL);
        
        nvgRestore(args.vg);
        return;
    }

    // High-resolution matrix rendering
    nvgSave(args.vg);
    
    // Set up high-quality rendering
    nvgShapeAntiAlias(args.vg, true);
    
    // Draw matrix background with subtle gradient
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 8);
    NVGpaint bg = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y, nvgRGBA(15, 15, 20, 255), nvgRGBA(5, 5, 10, 255));
    nvgFillPaint(args.vg, bg);
    nvgFill(args.vg);
    
    // Calculate cell size in widget coordinates
    float cellWidth = box.size.x / MATRIX_SIZE;
    float cellHeight = box.size.y / MATRIX_SIZE;
    
    // Draw each matrix cell at high resolution
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            int stepIndex = y * MATRIX_SIZE + x;
            
            // Calculate cell position and center
            Vec cellPos = Vec(x * cellWidth, y * cellHeight);
            Vec cellCenter = Vec(cellPos.x + cellWidth/2, cellPos.y + cellHeight/2);
            
            // Get step data from both sequences
            bool hasA = false, hasB = false;
            bool playheadA = false, playheadB = false;
            int symbolId = -999; // Default to "no symbol"
            
            if (module && stepIndex < 64) {
                // Check if this step is within sequence lengths
                if (stepIndex < module->sequenceA.length) {
                    hasA = true;
                    if (module->sequenceA.steps[stepIndex].chordIndex >= -2) {
                        symbolId = module->sequenceA.steps[stepIndex].alchemySymbolId;
                    }
                }
                
                if (stepIndex < module->sequenceB.length) {
                    hasB = true;
                    if (symbolId == -999 && module->sequenceB.steps[stepIndex].chordIndex >= -2) {
                        symbolId = module->sequenceB.steps[stepIndex].alchemySymbolId;
                    }
                }
                
                // Check for playhead position
                playheadA = (module->sequenceA.running && module->sequenceA.currentStep == stepIndex);
                playheadB = (module->sequenceB.running && module->sequenceB.currentStep == stepIndex);
            }
            
            // Draw high-resolution cell background
            nvgBeginPath(args.vg);
            
            // Use larger, smoother circles with bigger matrix size
            float cellRadius = std::min(cellWidth, cellHeight) * 0.42f; // Slightly larger radius
            nvgCircle(args.vg, cellCenter.x, cellCenter.y, cellRadius);
            
            // LED color logic with smooth gradients
            NVGcolor ledColor = nvgRGBA(25, 25, 30, 255); // Default off
            
            // Check for edit mode highlighting
            bool editModeHighlight = false;
            if (module && ((module->editModeA && hasA) || (module->editModeB && hasB))) {
                editModeHighlight = true;
            }
            
            if (playheadA && playheadB) {
                // Both playheads - create gradient mix
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(120, 160, 255, 255), nvgRGBA(60, 80, 200, 255));
                nvgFillPaint(args.vg, paint);
            } else if (playheadA) {
                // Sequence A - teal gradient
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(0, 255, 180, 255), nvgRGBA(0, 180, 120, 255));
                nvgFillPaint(args.vg, paint);
            } else if (playheadB) {
                // Sequence B - purple gradient  
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(180, 0, 255, 255), nvgRGBA(120, 0, 180, 255));
                nvgFillPaint(args.vg, paint);
            } else if (editModeHighlight) {
                // Edit mode - subtle glow
                if (hasA && module->editModeA) {
                    NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                      nvgRGBA(0, 150, 120, 200), nvgRGBA(0, 80, 60, 200));
                    nvgFillPaint(args.vg, paint);
                } else if (hasB && module->editModeB) {
                    NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                      nvgRGBA(120, 0, 150, 200), nvgRGBA(60, 0, 80, 200));
                    nvgFillPaint(args.vg, paint);
                }
            } else if (hasA && hasB) {
                // Both sequences - subtle mix
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(60, 80, 120, 255), nvgRGBA(30, 40, 60, 255));
                nvgFillPaint(args.vg, paint);
            } else if (hasA) {
                // Sequence A only
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(0, 100, 70, 255), nvgRGBA(0, 50, 35, 255));
                nvgFillPaint(args.vg, paint);
            } else if (hasB) {
                // Sequence B only
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(70, 0, 100, 255), nvgRGBA(35, 0, 50, 255));
                nvgFillPaint(args.vg, paint);
            } else {
                // Empty - subtle gradient
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(25, 25, 30, 255), nvgRGBA(15, 15, 20, 255));
                nvgFillPaint(args.vg, paint);
            }
            
            // Add edit mode glow for empty steps
            if (module && (module->editModeA || module->editModeB) && !hasA && !hasB) {
                NVGpaint paint = nvgRadialGradient(args.vg, cellCenter.x, cellCenter.y, 0, cellRadius,
                                                  nvgRGBA(40, 40, 60, 100), nvgRGBA(20, 20, 30, 50));
                nvgFillPaint(args.vg, paint);
            }
            
            nvgFill(args.vg);
            
            // Draw alchemical symbol at high resolution
            if (symbolId >= 0 && symbolId < 20) {
                // Determine symbol color - black when LED is lit, white otherwise
                NVGcolor symbolColor = nvgRGBA(255, 255, 255, 255); // Default white
                if (playheadA || playheadB) {
                    symbolColor = nvgRGBA(0, 0, 0, 255); // Black for contrast
                }
                drawAlchemicalSymbol(args, cellCenter, symbolId, symbolColor);
            } else if (symbolId == -1) {
                drawRestSymbol(args, cellCenter);
            } else if (symbolId == -2) {
                drawTieSymbol(args, cellCenter);
            }
            
            // Draw voice count indicators at high resolution
            if (module && stepIndex < 64 && (hasA || hasB)) {
                int voiceCount = 1;
                if (hasA && stepIndex < module->sequenceA.length) {
                    voiceCount = module->sequenceA.steps[stepIndex].voiceCount;
                } else if (hasB && stepIndex < module->sequenceB.length) {
                    voiceCount = module->sequenceB.steps[stepIndex].voiceCount;
                }
                
                if (symbolId >= 0 && symbolId < 20) {
                    NVGcolor dotColor = nvgRGBA(255, 255, 255, 255); // Default white
                    if (playheadA || playheadB) {
                        dotColor = nvgRGBA(0, 0, 0, 255); // Black for contrast
                    }
                    drawVoiceCount(args, cellCenter, voiceCount, dotColor);
                }
            }
            
            // Draw subtle cell border for definition
            nvgStrokeColor(args.vg, nvgRGBA(60, 60, 70, 100));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        }
    }
    
    // Draw edit mode matrix border glow at high resolution
    if (module && (module->editModeA || module->editModeB)) {
        nvgSave(args.vg);
        
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        
        float time = system::getTime();
        float pulse = 0.4f + 0.3f * sin(time * 3.0f);
        
        NVGcolor glowColor;
        if (module->editModeA) {
            glowColor = nvgRGBA(0, 255, 180, pulse * 150);
        } else {
            glowColor = nvgRGBA(180, 0, 255, pulse * 150);
        }
        
        // Larger, smoother glow
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -6, -6, box.size.x + 12, box.size.y + 12, 12);
        nvgStrokeColor(args.vg, glowColor);
        nvgStrokeWidth(args.vg, 4.0f);
        nvgStroke(args.vg);
        
        nvgRestore(args.vg);
    }
    
    nvgRestore(args.vg);
}

void HighResMatrixWidget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color) {
    nvgSave(args.vg);
    nvgTranslate(args.vg, pos.x, pos.y);
    
    // Set drawing properties for high-resolution symbols
    nvgStrokeColor(args.vg, color);
    nvgFillColor(args.vg, color);
    nvgStrokeWidth(args.vg, 1.0f); // Thinner stroke for lighter appearance
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);
    
    float size = 6.5f; // Slightly larger symbol size for better visibility
    
    switch (symbolId) {
        case 0: // Sol (Sun) - Circle with center dot (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.3f);
            nvgFill(args.vg);
            break;
            
        case 1: // Luna (Moon) - Crescent (match original)
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 2: // Mercury - Circle with horns and cross (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
            nvgStroke(args.vg);
            // Horns
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 3: // Venus - Circle with cross below (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 4: // Mars - Circle with arrow up-right (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.2f, -size * 0.2f);
            nvgLineTo(args.vg, size * 0.7f, -size * 0.7f);
            nvgLineTo(args.vg, size * 0.4f, -size * 0.7f);
            nvgMoveTo(args.vg, size * 0.7f, -size * 0.7f);
            nvgLineTo(args.vg, size * 0.7f, -size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        case 5: // Jupiter
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, 0);
            nvgLineTo(args.vg, size * 0.2f, 0);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 0.5f, M_PI * 1.5f, NVG_CCW);
            nvgStroke(args.vg);
            break;
            
        case 6: // Saturn
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.2f, 0);
            nvgLineTo(args.vg, size * 0.6f, 0);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, -size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 1.5f, M_PI * 0.5f, NVG_CCW);
            nvgStroke(args.vg);
            break;
            
        case 7: // Fire
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 8: // Water
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 9: // Air - Triangle up with line through (match original)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 10: // Earth - Triangle down with line through (match original)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 11: // Quintessence - Interwoven circles (match original)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        // Additional Occult Symbols
        case 12: // Pentagram - Five-pointed star
            nvgBeginPath(args.vg);
            // Draw pentagram by connecting every second point
            for (int i = 0; i < 5; i++) {
                int pointIndex = (i * 2) % 5;
                float angle = pointIndex * 2.0f * M_PI / 5.0f - M_PI / 2.0f;
                float x = cosf(angle) * size;
                float y = sinf(angle) * size;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 13: // Hexagram - Six-pointed star (Star of David)
            nvgBeginPath(args.vg);
            // First triangle (up)
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.866f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.866f, size * 0.5f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            // Second triangle (down)  
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.866f, -size * 0.5f);
            nvgLineTo(args.vg, size * 0.866f, -size * 0.5f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 14: // Ankh - Egyptian symbol of life
            nvgBeginPath(args.vg);
            // Cross vertical line
            nvgMoveTo(args.vg, 0, -size * 0.2f);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            // Cross horizontal line
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.5f, size * 0.2f);
            nvgLineTo(args.vg, size * 0.5f, size * 0.2f);
            nvgStroke(args.vg);
            // Loop at top
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, -size * 0.4f, size * 0.3f, 0, M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 15: // Eye of Horus - Egyptian protection symbol
            nvgBeginPath(args.vg);
            // Eye outline
            nvgMoveTo(args.vg, -size * 0.8f, 0);
            nvgBezierTo(args.vg, -size * 0.8f, -size * 0.5f, size * 0.8f, -size * 0.5f, size * 0.8f, 0);
            nvgBezierTo(args.vg, size * 0.8f, size * 0.5f, -size * 0.8f, size * 0.5f, -size * 0.8f, 0);
            nvgStroke(args.vg);
            // Pupil
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.2f);
            nvgFill(args.vg);
            // Tear line
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.2f);
            nvgLineTo(args.vg, -size * 0.3f, size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 16: // Ouroboros - Snake eating its tail
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.8f);
            nvgStroke(args.vg);
            // Snake head
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.8f, 0, size * 0.15f);
            nvgStroke(args.vg);
            // Tail into mouth
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.65f, 0);
            nvgLineTo(args.vg, size * 0.5f, 0);
            nvgStroke(args.vg);
            break;
            
        case 17: // Triskele - Triple spiral
            nvgBeginPath(args.vg);
            for (int i = 0; i < 3; i++) {
                float angle = i * 2.0f * M_PI / 3.0f;
                nvgMoveTo(args.vg, 0, 0);
                for (int j = 1; j <= 8; j++) {
                    float r = (j / 8.0f) * size;
                    float a = angle + (j / 8.0f) * M_PI;
                    nvgLineTo(args.vg, cosf(a) * r, sinf(a) * r);
                }
            }
            nvgStroke(args.vg);
            break;
            
        case 18: // Caduceus - Staff of Hermes
            nvgBeginPath(args.vg);
            // Central staff
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            // Left spiral
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgBezierTo(args.vg, -size * 0.4f, -size * 0.2f, -size * 0.4f, size * 0.2f, 0, size * 0.6f);
            nvgStroke(args.vg);
            // Right spiral
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgBezierTo(args.vg, size * 0.4f, -size * 0.2f, size * 0.4f, size * 0.2f, 0, size * 0.6f);
            nvgStroke(args.vg);
            // Wings
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, -size * 0.8f);
            nvgLineTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.3f, -size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 19: // Yin Yang - Balance symbol
            nvgBeginPath(args.vg);
            // Outer circle
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            // Dividing line
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, -size * 0.5f, size * 0.5f, 0, M_PI, NVG_CW);
            nvgArc(args.vg, 0, size * 0.5f, size * 0.5f, M_PI, 2 * M_PI, NVG_CCW);
            nvgStroke(args.vg);
            // Small circles
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.5f, size * 0.15f);
            nvgFill(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, size * 0.5f, size * 0.15f);
            nvgStroke(args.vg);
            break;
            
        // Even More Occult/Alchemical Symbols
        case 20: // Seal of Solomon - Hexagram with circle
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            // Star of David inside
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.7f);
            nvgLineTo(args.vg, -size * 0.6f, size * 0.35f);
            nvgLineTo(args.vg, size * 0.6f, size * 0.35f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.7f);
            nvgLineTo(args.vg, -size * 0.6f, -size * 0.35f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.35f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 21: // Sulfur - Triangle over cross
            nvgBeginPath(args.vg);
            // Triangle
            nvgMoveTo(args.vg, 0, -size * 0.5f);
            nvgLineTo(args.vg, -size * 0.6f, size * 0.1f);
            nvgLineTo(args.vg, size * 0.6f, size * 0.1f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.1f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.45f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.45f);
            nvgStroke(args.vg);
            break;
            
        case 22: // Salt - Circle with horizontal line
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.8f, 0);
            nvgLineTo(args.vg, size * 0.8f, 0);
            nvgStroke(args.vg);
            break;
            
        case 23: // Antimony - Circle with cross below
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.1f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.45f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.45f);
            nvgStroke(args.vg);
            break;
            
        case 24: // Philosopher's Stone - Square with inscribed circle and dot
            nvgBeginPath(args.vg);
            nvgRect(args.vg, -size * 0.7f, -size * 0.7f, size * 1.4f, size * 1.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.5f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.1f);
            nvgFill(args.vg);
            break;
            
        case 25: // Arsenic - Stylized As
            nvgBeginPath(args.vg);
            // Zigzag pattern
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgStroke(args.vg);
            // Horizontal line through middle
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 26: // Copper - Venus symbol variant
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.2f, size * 0.4f);
            nvgStroke(args.vg);
            // Cross with curved arms
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, -size * 0.2f, size * 0.5f, size * 0.2f, 0, M_PI, NVG_CW);
            nvgArc(args.vg, size * 0.2f, size * 0.5f, size * 0.2f, M_PI, 2 * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 27: // Iron - Mars symbol variant with double arrow
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.3f);
            nvgStroke(args.vg);
            // Double arrow
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.1f, -size * 0.1f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.8f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgStroke(args.vg);
            break;
            
        case 28: // Lead - Saturn symbol with additional cross
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.3f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.3f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.8f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, size * 0.2f, size * 0.3f, 0, M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 29: // Silver - Crescent moon
            nvgBeginPath(args.vg);
            nvgArc(args.vg, size * 0.2f, 0, size * 0.8f, M_PI * 0.6f, M_PI * 1.4f, NVG_CW);
            nvgStroke(args.vg);
            break;
    }
    
    nvgRestore(args.vg);
}

void HighResMatrixWidget::drawRestSymbol(const DrawArgs& args, Vec pos) {
    nvgStrokeColor(args.vg, nvgRGBA(180, 180, 180, 255));
    nvgStrokeWidth(args.vg, 1.0f); // Match symbol stroke width
    
    // Draw rest symbol (horizontal line)
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - 6, pos.y);
    nvgLineTo(args.vg, pos.x + 6, pos.y);
    nvgStroke(args.vg);
}

void HighResMatrixWidget::drawTieSymbol(const DrawArgs& args, Vec pos) {
    nvgStrokeColor(args.vg, nvgRGBA(255, 220, 120, 255));
    nvgStrokeWidth(args.vg, 1.0f); // Match symbol stroke width
    
    // Draw tie symbol (curved line)
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - 6, pos.y);
    nvgBezierTo(args.vg, pos.x - 2, pos.y - 6, pos.x + 2, pos.y - 6, pos.x + 6, pos.y);
    nvgStroke(args.vg);
}

void HighResMatrixWidget::drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor) {
    nvgSave(args.vg);
    
    // Now with larger matrix size, we can position dots further from symbol center
    float radius = 9.0f; // Better spacing around symbols with larger matrix size
    
    for (int i = 0; i < std::min(voiceCount, 6); i++) {
        float angle = (float)i / 6.0f * 2.0f * M_PI - M_PI/2; // Start from top
        float dotX = pos.x + cos(angle) * radius;
        float dotY = pos.y + sin(angle) * radius;
        
        nvgBeginPath(args.vg);
        nvgFillColor(args.vg, dotColor);
        nvgStrokeWidth(args.vg, 0.0f);
        nvgCircle(args.vg, dotX, dotY, 1.2f); // Slightly smaller dots for better proportion
        nvgFill(args.vg);
    }
    
    nvgRestore(args.vg);
}

Matrix8x8Widget::Matrix8x8Widget(Transmutation* module) : module(module) {
    box.size = Vec(LED_SPACING * MATRIX_SIZE, LED_SPACING * MATRIX_SIZE);
}

void Matrix8x8Widget::onButton(const event::Button& e) {
    if (e.action == GLFW_PRESS) {
        Vec pos = e.pos;
        int x = (int)(pos.x / LED_SPACING);
        int y = (int)(pos.y / LED_SPACING);
        
        if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE) {
            if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
                onMatrixClick(x, y);
            } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
                onMatrixRightClick(x, y);
            }
            e.consume(this);
        }
    }
    Widget::onButton(e);
}

void Matrix8x8Widget::onMatrixClick(int x, int y) {
    if (!module) return;
    
    int stepIndex = y * MATRIX_SIZE + x; // Convert 2D to 1D
    if (stepIndex >= 64) return;
    
    // Only allow programming in edit mode
    if (module->editModeA) {
        programStep(module->sequenceA, stepIndex);
    } else if (module->editModeB) {
        programStep(module->sequenceB, stepIndex);
    }
}

void Matrix8x8Widget::onMatrixRightClick(int x, int y) {
    if (!module) return;
    
    int stepIndex = y * MATRIX_SIZE + x;
    if (stepIndex >= 64) return;
    
    // Only allow voice count editing in edit mode and on steps with chords
    if (module->editModeA && stepIndex < module->sequenceA.length) {
        SequenceStep& step = module->sequenceA.steps[stepIndex];
        if (step.chordIndex >= 0) {
            // Cycle through voice counts 1-6
            step.voiceCount = (step.voiceCount % 6) + 1;
        }
    } else if (module->editModeB && stepIndex < module->sequenceB.length) {
        SequenceStep& step = module->sequenceB.steps[stepIndex];
        if (step.chordIndex >= 0) {
            // Cycle through voice counts 1-6
            step.voiceCount = (step.voiceCount % 6) + 1;
        }
    }
}

void Matrix8x8Widget::programStep(Sequence& seq, int stepIndex) {
    if (!module || stepIndex >= 64) return;
    
    SequenceStep& step = seq.steps[stepIndex];
    
    if (module->selectedSymbol >= 0 && module->selectedSymbol < 20 && 
        module->symbolToChordMapping[module->selectedSymbol] >= 0) {
        // Assign chord to step
        step.chordIndex = module->selectedSymbol;
        step.alchemySymbolId = module->selectedSymbol;
        // Set default voice count based on chord's preferred voices
        if (module->symbolToChordMapping[module->selectedSymbol] < (int)module->currentChordPack.chords.size()) {
            const ChordData& chord = module->currentChordPack.chords[module->symbolToChordMapping[module->selectedSymbol]];
            step.voiceCount = std::min(chord.preferredVoices, 6);
        }
    } else if (module->selectedSymbol == -1) {
        // Rest
        step.chordIndex = -1;
        step.alchemySymbolId = -1;
        step.voiceCount = 1;
    } else if (module->selectedSymbol == -2) {
        // Tie
        step.chordIndex = -2;
        step.alchemySymbolId = -2;
        step.voiceCount = 1;
    }
}

void Matrix8x8Widget::drawLayer(const DrawArgs& args, int layer) {
    if (layer == 1) {
        drawMatrix(args);
    }
    Widget::drawLayer(args, layer);
}

void Matrix8x8Widget::drawMatrix(const DrawArgs& args) {
    // Spooky 80s horror movie TV preview mode (8x8 version)
    if (module && !module->displayChordName.empty() && module->displaySymbolId != -999) {
        nvgSave(args.vg);
        
        // Calculate VHS tape warping effects based on time - ultra slow and warped
        float time = APP->engine->getFrame() * 0.003f; // Slightly slower for more languid tape drag
        float waveA = sin(time * 0.6f) * 0.12f + sin(time * 0.9f) * 0.08f; // Even slower, more intense waves
        float waveB = cos(time * 0.4f) * 0.10f + cos(time * 0.7f) * 0.06f; // Deeper secondary waves
        float tapeWarp = sin(time * 0.25f) * 0.05f + cos(time * 0.35f) * 0.03f; // Slower, more intense warping
        float deepWarp = sin(time * 0.15f) * 0.08f; // Even slower deep warping layer
        
        float matrixSize = MATRIX_SIZE * LED_SPACING;
        
        // Dark VHS horror movie background with slight blue tint and warping (8x8 version)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, matrixSize, matrixSize, 8);
        int bgBrightness = 8 + (int)(waveA * 8); // Slower brightness variation
        nvgFillColor(args.vg, nvgRGBA(bgBrightness, bgBrightness * 0.8f, bgBrightness * 1.3f, 255));
        nvgFill(args.vg);
        
        // Set up clipping to keep all effects within the rounded rectangle (8x8 version)
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, 0, 0, matrixSize, matrixSize);
        
        // Add intense VHS tape warping scanlines (8x8 version, clipped) 
        for (int i = 0; i < matrixSize; i += 2) {
            float warpOffset = sin((i * 0.025f) + time * 1.2f) * 3.0f; // More intense warping for 8x8
            warpOffset += cos((i * 0.012f) + time * 0.8f) * 2.0f; // Additional warp layer
            warpOffset += deepWarp * sin(i * 0.05f) * 2.0f; // Deep distortion
            nvgBeginPath(args.vg);
            nvgRect(args.vg, warpOffset, i, matrixSize, 1);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 25 + (int)(waveB * 25)));
            nvgFill(args.vg);
        }
        
        // Add ultra slow moving VHS tape distortion bands (8x8 version, clipped)
        for (int i = 0; i < 2; i++) {
            float distortionY = fmod(time * 6 + i * 100, matrixSize); // Ultra slow movement
            float warpWidth = sin(time * 0.6f + i) * 2.5f + deepWarp * 3.0f; // More intense warping
            float bandHeight = 1 + sin(time * 0.4f + i) * 0.5f; // Variable height
            nvgBeginPath(args.vg);
            nvgRect(args.vg, warpWidth, distortionY, matrixSize, bandHeight);
            nvgFillColor(args.vg, nvgRGBA(70 + (int)(waveA * 30), 90 + (int)(waveB * 30), 110, 30 + (int)(tapeWarp * 150)));
            nvgFill(args.vg);
        }
        
        nvgRestore(args.vg); // End clipping (8x8 version)
        
        // Draw the symbol with Shapetaker colors and VHS warping (8x8 version)
        nvgSave(args.vg);
        float shakeX = sin(time * 1.6f) * 0.6f + tapeWarp * 2.5f + deepWarp * 2.0f; // Much slower, intense warping for 8x8
        float shakeY = cos(time * 1.2f) * 0.4f + waveA * 1.5f + waveB * 1.0f; // Slower, more layered warping
        nvgTranslate(args.vg, matrixSize / 2 + shakeX, matrixSize * 0.40f + shakeY); // More centered vertically
        nvgScale(args.vg, 3.0f, 3.0f); // Much bigger and more pixelated for 8x8
        
        // Cycle through Shapetaker colors with ultra slow VHS color shift
        float colorCycle = sin(time * 0.3f) * 0.5f + 0.5f; // Even slower color transitions like old tape
        int symbolR, symbolG, symbolB;
        
        if (colorCycle < 0.25f) {
            // Teal with VHS warping
            symbolR = 0;
            symbolG = 180 + (int)(waveA * 50);
            symbolB = 180 + (int)(waveB * 50);
        } else if (colorCycle < 0.5f) {
            // Purple with VHS warping  
            symbolR = 180 + (int)(waveA * 50);
            symbolG = 0;
            symbolB = 255;
        } else if (colorCycle < 0.75f) {
            // Muted green with VHS warping
            symbolR = 60 + (int)(waveB * 30);
            symbolG = 120 + (int)(waveA * 40);
            symbolB = 80 + (int)(tapeWarp * 80);
        } else {
            // Gray with VHS warping
            symbolR = 140 + (int)(waveA * 40);
            symbolG = 140 + (int)(waveB * 40);
            symbolB = 150 + (int)(tapeWarp * 60);
        }
        
        // Draw symbol with intense glow layers
        for (int glow = 0; glow < 6; glow++) {
            float glowAlpha = 80 / (glow + 1);
            float glowOffset = glow * 0.8f;
            
            // Create blur effect with multiple offset draws
            for (int blur = 0; blur < 2; blur++) {
                float blurOffset = blur * 0.3f;
                drawAlchemicalSymbol(args, Vec(glowOffset + blurOffset, glowOffset + blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                drawAlchemicalSymbol(args, Vec(-glowOffset + blurOffset, glowOffset - blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                drawAlchemicalSymbol(args, Vec(glowOffset - blurOffset, -glowOffset + blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
                drawAlchemicalSymbol(args, Vec(-glowOffset - blurOffset, -glowOffset - blurOffset), module->displaySymbolId, nvgRGBA(symbolR * 0.9f, symbolG * 0.9f, symbolB * 0.9f, glowAlpha));
            }
        }
        
        // Main symbol with bright core
        drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGBA(symbolR, symbolG, symbolB, 255));
        
        // Extra bright white core for VHS warping intensity
        drawAlchemicalSymbol(args, Vec(0, 0), module->displaySymbolId, nvgRGBA(255, 255, 255, 50 + (int)(waveA * 30)));
        nvgRestore(args.vg);
        
        // Set up horror movie text with much more pixelated chunky font
        nvgFontSize(args.vg, 20.0f); // Much bigger text for 8x8 matrix
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        
        // Text position with slow, intense VHS warping movement, closer to symbol (8x8 version)
        float textX = matrixSize / 2 + sin(time * 1.0f) * 0.5f + tapeWarp * 2.0f + deepWarp * 1.5f;
        float textY = matrixSize * 0.65f + cos(time * 0.8f) * 0.3f + waveB * 1.0f + waveA * 0.6f; // More warped movement
        
        // Multiple chunky shadows for extreme pixelated horror effect
        nvgFillColor(args.vg, nvgRGBA(40, 40, 50, 220)); // Dark gray shadow
        nvgText(args.vg, textX + 2, textY + 2, module->displayChordName.c_str(), NULL);
        nvgText(args.vg, textX + 1, textY + 3, module->displayChordName.c_str(), NULL);
        nvgText(args.vg, textX + 3, textY + 1, module->displayChordName.c_str(), NULL);
        
        // Main text with ultra slowly cycling Shapetaker colors
        int textR, textG, textB;
        float textColorCycle = sin(time * 0.25f + 1.5f) * 0.5f + 0.5f; // Even slower text color transitions
        
        if (textColorCycle < 0.25f) {
            // Bright teal with VHS warping
            textR = 0;
            textG = 255;
            textB = 200 + (int)(waveA * 40);
        } else if (textColorCycle < 0.5f) {
            // Bright purple with VHS warping
            textR = 220 + (int)(waveB * 25);
            textG = 60 + (int)(tapeWarp * 100);
            textB = 255;
        } else if (textColorCycle < 0.75f) {
            // Bright muted green with VHS warping
            textR = 120 + (int)(waveA * 40);
            textG = 200 + (int)(waveB * 35);
            textB = 140 + (int)(tapeWarp * 80);
        } else {
            // Bright gray-white with VHS warping
            textR = 220 + (int)(waveA * 25);
            textG = 220 + (int)(waveB * 25);
            textB = 240 + (int)(tapeWarp * 15);
        }
        
        nvgFillColor(args.vg, nvgRGBA(textR, textG, textB, 255));
        nvgText(args.vg, textX, textY, module->displayChordName.c_str(), NULL);
        
        // Intense multi-layer glow effects with blur
        for (int glow = 0; glow < 8; glow++) {
            float glowRadius = (glow + 1) * 1.5f;
            float glowAlpha = 120 / (glow + 1); // Fade out each layer
            
            // Blur effect by drawing at slightly offset positions
            for (int blur = 0; blur < 3; blur++) {
                float blurOffset = blur * 0.5f;
                nvgFillColor(args.vg, nvgRGBA(textR * 0.8f, textG * 0.8f, textB * 0.8f, glowAlpha));
                nvgText(args.vg, textX + blurOffset, textY + blurOffset, module->displayChordName.c_str(), NULL);
                nvgText(args.vg, textX - blurOffset, textY + blurOffset, module->displayChordName.c_str(), NULL);
                nvgText(args.vg, textX + blurOffset, textY - blurOffset, module->displayChordName.c_str(), NULL);
                nvgText(args.vg, textX - blurOffset, textY - blurOffset, module->displayChordName.c_str(), NULL);
            }
        }
        
        // Extra intense core glow with VHS warping
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 80 + (int)(waveA * 40)));
        nvgText(args.vg, textX, textY, module->displayChordName.c_str(), NULL);
        
        nvgRestore(args.vg);
        return; // Skip normal LED matrix drawing
    }
    
    for (int x = 0; x < MATRIX_SIZE; x++) {
        for (int y = 0; y < MATRIX_SIZE; y++) {
            Vec ledPos = Vec(x * LED_SPACING + LED_SPACING/2, y * LED_SPACING + LED_SPACING/2);
            int stepIndex = y * MATRIX_SIZE + x;
            
            // Get step data from both sequences
            bool hasA = false, hasB = false;
            bool playheadA = false, playheadB = false;
            int symbolId = -999; // Default to "no symbol" - different from REST (-1)
            
            if (module && stepIndex < 64) {
                // Check if this step is within sequence lengths (for LED color)
                if (stepIndex < module->sequenceA.length) {
                    hasA = true;
                    // Only set symbol if step has actual data
                    if (module->sequenceA.steps[stepIndex].chordIndex >= -2) {
                        symbolId = module->sequenceA.steps[stepIndex].alchemySymbolId;
                    }
                }
                
                if (stepIndex < module->sequenceB.length) {
                    hasB = true;
                    // Only set symbol if step has actual data and no symbol from A
                    if (symbolId == -999 && module->sequenceB.steps[stepIndex].chordIndex >= -2) {
                        symbolId = module->sequenceB.steps[stepIndex].alchemySymbolId;
                    }
                }
                
                // Check for playhead position
                playheadA = (module->sequenceA.running && module->sequenceA.currentStep == stepIndex);
                playheadB = (module->sequenceB.running && module->sequenceB.currentStep == stepIndex);
            }
            
            // Draw LED background
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, ledPos.x, ledPos.y, LED_SIZE/2);
            
            // LED color logic with edit mode feedback
            NVGcolor ledColor = nvgRGBA(20, 20, 20, 255); // Default off
            
            // Check for edit mode highlighting
            bool editModeHighlight = false;
            if (module && ((module->editModeA && hasA) || (module->editModeB && hasB))) {
                editModeHighlight = true;
            }
            
            if (playheadA && playheadB) {
                // Both playheads - mix colors
                ledColor = nvgRGBA(90, 127, 217, 255); // Teal + Purple mix
            } else if (playheadA) {
                ledColor = nvgRGBA(0, 255, 180, 255); // Bright teal
            } else if (playheadB) {
                ledColor = nvgRGBA(180, 0, 255, 255); // Bright purple
            } else if (editModeHighlight) {
                // Edit mode - make editable steps more visible
                if (hasA && module->editModeA) {
                    ledColor = nvgRGBA(0, 200, 140, 200); // Brighter teal for editing
                } else if (hasB && module->editModeB) {
                    ledColor = nvgRGBA(140, 0, 200, 200); // Brighter purple for editing
                }
            } else if (hasA && hasB) {
                // Both sequences have data - dim mix
                ledColor = nvgRGBA(45, 63, 108, 255); // Dim mix
            } else if (hasA) {
                ledColor = nvgRGBA(0, 127, 90, 255); // Dim teal
            } else if (hasB) {
                ledColor = nvgRGBA(90, 0, 127, 255); // Dim purple
            }
            
            // Add edit mode matrix border glow for empty steps
            if (module && (module->editModeA || module->editModeB)) {
                // Subtle edit mode indication on empty steps
                if (!hasA && !hasB) {
                    ledColor = nvgRGBA(40, 40, 60, 100); // Subtle highlight for programmable steps
                }
            }
            
            nvgFillColor(args.vg, ledColor);
            nvgFill(args.vg);
            
            // Draw alchemical symbol if assigned (but not if it's the default "no symbol" value)
            if (symbolId >= 0 && symbolId < 20) {
                // Determine symbol color - black when LED is lit, white otherwise
                NVGcolor symbolColor = nvgRGBA(255, 255, 255, 255); // Default white
                if (playheadA || playheadB) {
                    // LED is lit - make symbol black for contrast
                    symbolColor = nvgRGBA(0, 0, 0, 255); // Black
                }
                drawAlchemicalSymbol(args, ledPos, symbolId, symbolColor);
            } else if (symbolId == -1) {
                // Draw rest symbol (only when explicitly programmed)
                drawRestSymbol(args, ledPos);
            } else if (symbolId == -2) {
                // Draw tie symbol
                drawTieSymbol(args, ledPos);
            }
            // symbolId == -999 means empty step - draw nothing
            
            // Draw voice count indicators
            if (module && stepIndex < 64 && (hasA || hasB)) {
                int voiceCount = 1;
                if (hasA && stepIndex < module->sequenceA.length) {
                    voiceCount = module->sequenceA.steps[stepIndex].voiceCount;
                } else if (hasB && stepIndex < module->sequenceB.length) {
                    voiceCount = module->sequenceB.steps[stepIndex].voiceCount;
                }
                
                // Only draw voice dots for actual chords (not REST/TIE)
                if (symbolId >= 0 && symbolId < 20) {
                    // Use same color logic as symbols - black when LED is lit, white otherwise
                    NVGcolor dotColor = nvgRGBA(255, 255, 255, 255); // Default white
                    if (playheadA || playheadB) {
                        // LED is lit - make dots black for contrast
                        dotColor = nvgRGBA(0, 0, 0, 255); // Black
                    }
                    drawVoiceCount(args, ledPos, voiceCount, dotColor);
                }
            }
            
            // LED border
            nvgStrokeColor(args.vg, nvgRGBA(80, 80, 80, 255));
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStroke(args.vg);
        }
    }
    
    // Draw edit mode matrix border glow
    if (module && (module->editModeA || module->editModeB)) {
        nvgSave(args.vg);
        
        // Set up glow effect
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        
        // Draw animated border glow
        float time = system::getTime();
        float pulse = 0.3f + 0.2f * sin(time * 3.0f);
        
        NVGcolor glowColor;
        if (module->editModeA) {
            glowColor = nvgRGBA(0, 255, 180, pulse * 100);
        } else {
            glowColor = nvgRGBA(180, 0, 255, pulse * 100);
        }
        
        // Draw outer glow border
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -4, -4, 
                      MATRIX_SIZE * LED_SPACING + 8, 
                      MATRIX_SIZE * LED_SPACING + 8, 6);
        nvgStrokeColor(args.vg, glowColor);
        nvgStrokeWidth(args.vg, 3.0f);
        nvgStroke(args.vg);
        
        nvgRestore(args.vg);
    }
}

void Matrix8x8Widget::drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId, NVGcolor color) {
    nvgSave(args.vg);
    nvgTranslate(args.vg, pos.x, pos.y);
    
    // Set drawing properties - use the provided color for symbol
    nvgStrokeColor(args.vg, color);
    nvgFillColor(args.vg, color);
    nvgStrokeWidth(args.vg, 1.2f); // Slightly thicker stroke for visibility
    nvgLineCap(args.vg, NVG_ROUND);
    nvgLineJoin(args.vg, NVG_ROUND);
    
    float size = 4.0f; // Bigger symbol size
    
    switch (symbolId) {
        case 0: // Sol (Sun) - Circle with center dot
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.3f);
            nvgFill(args.vg);
            break;
            
        case 1: // Luna (Moon) - Crescent
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 2: // Mercury - Circle with horns and cross
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
            nvgStroke(args.vg);
            // Horns
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
            nvgLineTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
            nvgStroke(args.vg);
            // Cross below
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 3: // Venus - Circle with cross below
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size * 0.2f);
            nvgLineTo(args.vg, 0, size * 0.8f);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
            nvgStroke(args.vg);
            break;
            
        case 4: // Mars - Circle with arrow up-right
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.2f, -size * 0.2f);
            nvgLineTo(args.vg, size * 0.7f, -size * 0.7f);
            nvgLineTo(args.vg, size * 0.4f, -size * 0.7f);
            nvgMoveTo(args.vg, size * 0.7f, -size * 0.7f);
            nvgLineTo(args.vg, size * 0.7f, -size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        case 5: // Jupiter - Cross with curved line
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.6f, 0);
            nvgLineTo(args.vg, size * 0.2f, 0);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 0.5f, M_PI * 1.5f, NVG_CCW);
            nvgStroke(args.vg);
            break;
            
        case 6: // Saturn - Cross with curved line (flipped)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.2f, 0);
            nvgLineTo(args.vg, size * 0.6f, 0);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, -size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 1.5f, M_PI * 0.5f, NVG_CCW);
            nvgStroke(args.vg);
            break;
            
        case 7: // Fire - Triangle pointing up
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 8: // Water - Triangle pointing down  
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 9: // Air - Triangle up with line through
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 10: // Earth - Triangle down with line through
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.4f, 0);
            nvgLineTo(args.vg, size * 0.4f, 0);
            nvgStroke(args.vg);
            break;
            
        case 11: // Quintessence - Interwoven circles
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f);
            nvgStroke(args.vg);
            break;
            
        // Additional Occult Symbols (Matrix8x8 version)
        case 12: // Pentagram - Five-pointed star
            nvgBeginPath(args.vg);
            for (int i = 0; i < 5; i++) {
                int pointIndex = (i * 2) % 5;
                float angle = pointIndex * 2.0f * M_PI / 5.0f - M_PI / 2.0f;
                float x = cosf(angle) * size;
                float y = sinf(angle) * size;
                if (i == 0) nvgMoveTo(args.vg, x, y);
                else nvgLineTo(args.vg, x, y);
            }
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 13: // Hexagram - Six-pointed star
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, -size * 0.866f, size * 0.5f);
            nvgLineTo(args.vg, size * 0.866f, size * 0.5f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, size);
            nvgLineTo(args.vg, -size * 0.866f, -size * 0.5f);
            nvgLineTo(args.vg, size * 0.866f, -size * 0.5f);
            nvgClosePath(args.vg);
            nvgStroke(args.vg);
            break;
            
        case 14: // Ankh - Egyptian symbol
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.2f);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.5f, size * 0.2f);
            nvgLineTo(args.vg, size * 0.5f, size * 0.2f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, -size * 0.4f, size * 0.3f, 0, M_PI, NVG_CW);
            nvgStroke(args.vg);
            break;
            
        case 15: // Eye of Horus
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.8f, 0);
            nvgBezierTo(args.vg, -size * 0.8f, -size * 0.5f, size * 0.8f, -size * 0.5f, size * 0.8f, 0);
            nvgBezierTo(args.vg, size * 0.8f, size * 0.5f, -size * 0.8f, size * 0.5f, -size * 0.8f, 0);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.2f);
            nvgFill(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, size * 0.2f);
            nvgLineTo(args.vg, -size * 0.3f, size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 16: // Ouroboros
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size * 0.8f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, size * 0.8f, 0, size * 0.15f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, size * 0.65f, 0);
            nvgLineTo(args.vg, size * 0.5f, 0);
            nvgStroke(args.vg);
            break;
            
        case 17: // Triskele
            nvgBeginPath(args.vg);
            for (int i = 0; i < 3; i++) {
                float angle = i * 2.0f * M_PI / 3.0f;
                nvgMoveTo(args.vg, 0, 0);
                for (int j = 1; j <= 8; j++) {
                    float r = (j / 8.0f) * size;
                    float a = angle + (j / 8.0f) * M_PI;
                    nvgLineTo(args.vg, cosf(a) * r, sinf(a) * r);
                }
            }
            nvgStroke(args.vg);
            break;
            
        case 18: // Caduceus
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size);
            nvgLineTo(args.vg, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgBezierTo(args.vg, -size * 0.4f, -size * 0.2f, -size * 0.4f, size * 0.2f, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, -size * 0.6f);
            nvgBezierTo(args.vg, size * 0.4f, -size * 0.2f, size * 0.4f, size * 0.2f, 0, size * 0.6f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -size * 0.3f, -size * 0.8f);
            nvgLineTo(args.vg, 0, -size * 0.6f);
            nvgLineTo(args.vg, size * 0.3f, -size * 0.8f);
            nvgStroke(args.vg);
            break;
            
        case 19: // Yin Yang
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, 0, size);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgArc(args.vg, 0, -size * 0.5f, size * 0.5f, 0, M_PI, NVG_CW);
            nvgArc(args.vg, 0, size * 0.5f, size * 0.5f, M_PI, 2 * M_PI, NVG_CCW);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, -size * 0.5f, size * 0.15f);
            nvgFill(args.vg);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0, size * 0.5f, size * 0.15f);
            nvgStroke(args.vg);
            break;
    }
    
    nvgRestore(args.vg);
}

void Matrix8x8Widget::drawRestSymbol(const DrawArgs& args, Vec pos) {
    nvgStrokeColor(args.vg, nvgRGBA(150, 150, 150, 255));
    nvgStrokeWidth(args.vg, 1.0f);
    
    // Draw rest symbol (small horizontal line)
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - 3, pos.y);
    nvgLineTo(args.vg, pos.x + 3, pos.y);
    nvgStroke(args.vg);
}

void Matrix8x8Widget::drawTieSymbol(const DrawArgs& args, Vec pos) {
    nvgStrokeColor(args.vg, nvgRGBA(255, 200, 100, 255));
    nvgStrokeWidth(args.vg, 1.0f);
    
    // Draw tie symbol (curved line)
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pos.x - 3, pos.y);
    nvgBezierTo(args.vg, pos.x - 1, pos.y - 3, pos.x + 1, pos.y - 3, pos.x + 3, pos.y);
    nvgStroke(args.vg);
}

void Matrix8x8Widget::drawVoiceCount(const DrawArgs& args, Vec pos, int voiceCount, NVGcolor dotColor) {
    // Draw smaller dots positioned to not interfere with symbols
    nvgSave(args.vg);
    
    float radius = 7.0f; // Position dots around the symbol
    
    for (int i = 0; i < std::min(voiceCount, 6); i++) {
        float angle = (float)i / 6.0f * 2.0f * M_PI - M_PI/2; // Start from top
        float dotX = pos.x + cos(angle) * radius;
        float dotY = pos.y + sin(angle) * radius;
        
        nvgBeginPath(args.vg);
        nvgFillColor(args.vg, dotColor);
        nvgStrokeWidth(args.vg, 0.0f); // Disable stroke completely
        nvgCircle(args.vg, dotX, dotY, 0.8f);
        nvgFill(args.vg);
    }
    
    nvgRestore(args.vg);
}

// Alchemical Symbol Button Widget
struct AlchemicalSymbolWidget : Widget {
    Transmutation* module;
    int buttonPosition; // Button position (0-11)
    
    AlchemicalSymbolWidget(Transmutation* module, int buttonPosition) : module(module), buttonPosition(buttonPosition) {
        box.size = Vec(20, 20);
    }
    
    int getSymbolId() {
        if (!module) return buttonPosition; // Fallback to button position if no module
        return module->buttonToSymbolMapping[buttonPosition];
    }
    
    void draw(const DrawArgs& args) override {
        int symbolId = getSymbolId();
        bool isSelected = module && module->selectedSymbol == symbolId;
        bool inEditMode = module && (module->editModeA || module->editModeB);
        bool isCurrentlyPlaying = false;
        
        // Check if this symbol's chord is currently playing
        if (module) {
            int currentChordA = module->getCurrentChordIndex(module->sequenceA);
            int currentChordB = module->getCurrentChordIndex(module->sequenceB);
            
            // Check if this symbol maps to a currently playing chord
            for (int i = 0; i < 20; i++) {
                if (module->symbolToChordMapping[i] == symbolId) {
                    if ((module->sequenceA.running && currentChordA == i) ||
                        (module->sequenceB.running && currentChordB == i)) {
                        isCurrentlyPlaying = true;
                        break;
                    }
                }
            }
        }
        
        // Draw button background with enhanced states
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        
        // Check which sequence(s) are playing this symbol for background color
        bool playheadA = false, playheadB = false;
        if (module) {
            int currentChordA = module->getCurrentChordIndex(module->sequenceA);
            int currentChordB = module->getCurrentChordIndex(module->sequenceB);
            
            if (module->sequenceA.running && currentChordA == symbolId) {
                playheadA = true;
            }
            if (module->sequenceB.running && currentChordB == symbolId) {
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
        
        // Draw the alchemical symbol (always white)
        drawAlchemicalSymbol(args, Vec(box.size.x/2, box.size.y/2), symbolId);
    }
    
    void drawAlchemicalSymbol(const DrawArgs& args, Vec pos, int symbolId) {
        nvgSave(args.vg);
        nvgTranslate(args.vg, pos.x, pos.y);
        
        // Set drawing properties for button symbols (larger)
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 255));
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(args.vg, 1.2f);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgLineJoin(args.vg, NVG_ROUND);
        
        float size = 6.0f; // Larger symbols for buttons
        
        switch (symbolId) {
            case 0: // Sol (Sun)
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size * 0.3f);
                nvgFill(args.vg);
                break;
                
            case 1: // Luna (Moon)
                nvgBeginPath(args.vg);
                nvgArc(args.vg, 0, 0, size, 0.3f * M_PI, 1.7f * M_PI, NVG_CW);
                nvgStroke(args.vg);
                break;
                
            case 2: // Mercury
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, -size * 0.3f, size * 0.4f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.6f, -size * 0.8f);
                nvgLineTo(args.vg, 0, -size * 0.6f);
                nvgLineTo(args.vg, size * 0.6f, -size * 0.8f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, size * 0.2f);
                nvgLineTo(args.vg, 0, size * 0.8f);
                nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
                nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
                nvgStroke(args.vg);
                break;
                
            case 3: // Venus
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, -size * 0.3f, size * 0.5f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, size * 0.2f);
                nvgLineTo(args.vg, 0, size * 0.8f);
                nvgMoveTo(args.vg, -size * 0.3f, size * 0.5f);
                nvgLineTo(args.vg, size * 0.3f, size * 0.5f);
                nvgStroke(args.vg);
                break;
                
            case 4: // Mars
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, -size * 0.2f, size * 0.2f, size * 0.4f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, size * 0.2f, -size * 0.2f);
                nvgLineTo(args.vg, size * 0.7f, -size * 0.7f);
                nvgLineTo(args.vg, size * 0.4f, -size * 0.7f);
                nvgMoveTo(args.vg, size * 0.7f, -size * 0.7f);
                nvgLineTo(args.vg, size * 0.7f, -size * 0.4f);
                nvgStroke(args.vg);
                break;
                
            case 5: // Jupiter
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.6f, 0);
                nvgLineTo(args.vg, size * 0.2f, 0);
                nvgMoveTo(args.vg, 0, -size * 0.6f);
                nvgLineTo(args.vg, 0, size * 0.6f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgArc(args.vg, size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 0.5f, M_PI * 1.5f, NVG_CCW);
                nvgStroke(args.vg);
                break;
                
            case 6: // Saturn
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.2f, 0);
                nvgLineTo(args.vg, size * 0.6f, 0);
                nvgMoveTo(args.vg, 0, -size * 0.6f);
                nvgLineTo(args.vg, 0, size * 0.6f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgArc(args.vg, -size * 0.4f, -size * 0.3f, size * 0.3f, M_PI * 1.5f, M_PI * 0.5f, NVG_CCW);
                nvgStroke(args.vg);
                break;
                
            case 7: // Fire
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size);
                nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
                nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                break;
                
            case 8: // Water
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, size);
                nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
                nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                break;
                
            case 9: // Air
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size);
                nvgLineTo(args.vg, -size * 0.8f, size * 0.6f);
                nvgLineTo(args.vg, size * 0.8f, size * 0.6f);
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.4f, 0);
                nvgLineTo(args.vg, size * 0.4f, 0);
                nvgStroke(args.vg);
                break;
                
            case 10: // Earth
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, size);
                nvgLineTo(args.vg, -size * 0.8f, -size * 0.6f);
                nvgLineTo(args.vg, size * 0.8f, -size * 0.6f);
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.4f, 0);
                nvgLineTo(args.vg, size * 0.4f, 0);
                nvgStroke(args.vg);
                break;
                
            case 11: // Quintessence
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, -size * 0.3f, 0, size * 0.4f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, size * 0.3f, 0, size * 0.4f);
                nvgStroke(args.vg);
                break;
                
            // Additional Occult Symbols (Button version)
            case 12: // Pentagram
                nvgBeginPath(args.vg);
                for (int i = 0; i < 5; i++) {
                    int pointIndex = (i * 2) % 5;
                    float angle = pointIndex * 2.0f * M_PI / 5.0f - M_PI / 2.0f;
                    float x = cosf(angle) * size;
                    float y = sinf(angle) * size;
                    if (i == 0) nvgMoveTo(args.vg, x, y);
                    else nvgLineTo(args.vg, x, y);
                }
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                break;
                
            case 13: // Hexagram
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size);
                nvgLineTo(args.vg, -size * 0.866f, size * 0.5f);
                nvgLineTo(args.vg, size * 0.866f, size * 0.5f);
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, size);
                nvgLineTo(args.vg, -size * 0.866f, -size * 0.5f);
                nvgLineTo(args.vg, size * 0.866f, -size * 0.5f);
                nvgClosePath(args.vg);
                nvgStroke(args.vg);
                break;
                
            case 14: // Ankh
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size * 0.2f);
                nvgLineTo(args.vg, 0, size);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.5f, size * 0.2f);
                nvgLineTo(args.vg, size * 0.5f, size * 0.2f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgArc(args.vg, 0, -size * 0.4f, size * 0.3f, 0, M_PI, NVG_CW);
                nvgStroke(args.vg);
                break;
                
            case 15: // Eye of Horus
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.8f, 0);
                nvgBezierTo(args.vg, -size * 0.8f, -size * 0.5f, size * 0.8f, -size * 0.5f, size * 0.8f, 0);
                nvgBezierTo(args.vg, size * 0.8f, size * 0.5f, -size * 0.8f, size * 0.5f, -size * 0.8f, 0);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size * 0.2f);
                nvgFill(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.3f, size * 0.2f);
                nvgLineTo(args.vg, -size * 0.3f, size * 0.8f);
                nvgStroke(args.vg);
                break;
                
            case 16: // Ouroboros
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size * 0.8f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, size * 0.8f, 0, size * 0.15f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, size * 0.65f, 0);
                nvgLineTo(args.vg, size * 0.5f, 0);
                nvgStroke(args.vg);
                break;
                
            case 17: // Triskele
                nvgBeginPath(args.vg);
                for (int i = 0; i < 3; i++) {
                    float angle = i * 2.0f * M_PI / 3.0f;
                    nvgMoveTo(args.vg, 0, 0);
                    for (int j = 1; j <= 8; j++) {
                        float r = (j / 8.0f) * size;
                        float a = angle + (j / 8.0f) * M_PI;
                        nvgLineTo(args.vg, cosf(a) * r, sinf(a) * r);
                    }
                }
                nvgStroke(args.vg);
                break;
                
            case 18: // Caduceus
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size);
                nvgLineTo(args.vg, 0, size);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size * 0.6f);
                nvgBezierTo(args.vg, -size * 0.4f, -size * 0.2f, -size * 0.4f, size * 0.2f, 0, size * 0.6f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, 0, -size * 0.6f);
                nvgBezierTo(args.vg, size * 0.4f, -size * 0.2f, size * 0.4f, size * 0.2f, 0, size * 0.6f);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, -size * 0.3f, -size * 0.8f);
                nvgLineTo(args.vg, 0, -size * 0.6f);
                nvgLineTo(args.vg, size * 0.3f, -size * 0.8f);
                nvgStroke(args.vg);
                break;
                
            case 19: // Yin Yang
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, 0, size);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgArc(args.vg, 0, -size * 0.5f, size * 0.5f, 0, M_PI, NVG_CW);
                nvgArc(args.vg, 0, size * 0.5f, size * 0.5f, M_PI, 2 * M_PI, NVG_CCW);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, -size * 0.5f, size * 0.15f);
                nvgFill(args.vg);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, 0, size * 0.5f, size * 0.15f);
                nvgStroke(args.vg);
                break;
        }
        
        nvgRestore(args.vg);
    }
    
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            int symbolId = getSymbolId();
            module->onSymbolPressed(symbolId);
            e.consume(this);
        }
        Widget::onButton(e);
    }
};

struct ChordPackButton : Widget {
    Transmutation* module;
    
    ChordPackButton(Transmutation* module) : module(module) {
        box.size = Vec(30, 15);
    }
    
    void draw(const DrawArgs& args) override {
        // Draw button background
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(60, 60, 80, 180));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(120, 120, 140, 255));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
        
        // Draw text
        nvgFontSize(args.vg, 8);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 255));
        nvgText(args.vg, box.size.x/2, box.size.y/2, "LOAD", NULL);
        
        // Show chord pack name below button if loaded
        if (module && !module->currentChordPack.name.empty()) {
            nvgFontSize(args.vg, 6);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(args.vg, nvgRGBA(200, 200, 255, 200));
            nvgText(args.vg, box.size.x/2, box.size.y + 2, module->currentChordPack.name.c_str(), NULL);
        }
    }
    
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            // For now, cycle through example chord packs
            static int packIndex = 0;
            std::vector<std::string> packFiles = {
                asset::plugin(pluginInstance, "chord_packs/80s_pop_d_sharp.json"),
                asset::plugin(pluginInstance, "chord_packs/jazz_standards_bb.json")
            };
            
            if (module->loadChordPackFromFile(packFiles[packIndex])) {
                packIndex = (packIndex + 1) % packFiles.size();
            }
            e.consume(this);
        }
        Widget::onButton(e);
    }
};

// Custom Display Widget - shows BPM, sequence status, steps, and mode information
struct TransmutationDisplayWidget : TransparentWidget {
    Transmutation* module;
    std::shared_ptr<Font> font;
    
    TransmutationDisplayWidget(Transmutation* module) {
        this->module = module;
        box.size = mm2px(Vec(40, 20)); // 40mm x 20mm display area
        font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) {
            // Fallback to a default font if ShareTech isn't available
            font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
        }
    }
    
    void draw(const DrawArgs& args) override {
        if (!module) return;
        if (!font) return; // Safety check
        
        // Additional safety check for module initialization
        try {
        
        // Background
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
        nvgFillColor(args.vg, nvgRGBA(20, 25, 30, 200));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(80, 90, 100, 150));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
        
        // Set up text properties
        nvgFontSize(args.vg, 10);
        if (font && font->handle >= 0) {
            nvgFontFaceId(args.vg, font->handle);
        }
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        
        float y = 5;
        
        // BPM Display
        nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 200));
        float baseBPM = module->params[Transmutation::INTERNAL_CLOCK_PARAM].getValue();
        int multiplierIndex = (int)module->params[Transmutation::BPM_MULTIPLIER_PARAM].getValue();
        float multipliers[] = {1.0f, 2.0f, 4.0f, 8.0f};
        const char* multiplierLabels[] = {"1x", "2x", "4x", "8x"};
        float effectiveBPM = baseBPM * multipliers[multiplierIndex];
        std::string bpmText = "BPM: " + std::to_string((int)baseBPM) + " (" + multiplierLabels[multiplierIndex] + " = " + std::to_string((int)effectiveBPM) + ")"; 
        nvgText(args.vg, 5, y, bpmText.c_str(), NULL);
        y += 12;
        
        // Sequence A Status
        nvgFillColor(args.vg, nvgRGBA(0, 255, 200, 255)); // Teal for A
        std::string statusA = std::string("A: ") + (module->sequenceA.running ? "RUN" : "STOP") + 
                              " [" + std::to_string(module->sequenceA.currentStep + 1) + 
                              "/" + std::to_string(module->sequenceA.length) + "]";
        nvgText(args.vg, 5, y, statusA.c_str(), NULL);
        y += 12;
        
        // Sequence B Status with Mode
        nvgFillColor(args.vg, nvgRGBA(200, 100, 255, 255)); // Purple for B
        int bMode = (int)module->params[Transmutation::SEQ_B_MODE_PARAM].getValue();
        std::string modeNames[] = {"IND", "HAR", "LOK"}; // Independent, Harmony, Lock
        std::string statusB = std::string("B: ") + (module->sequenceB.running ? "RUN" : "STOP") + 
                              " [" + std::to_string(module->sequenceB.currentStep + 1) + 
                              "/" + std::to_string(module->sequenceB.length) + "] " + 
                              modeNames[bMode];
        nvgText(args.vg, 5, y, statusB.c_str(), NULL);
        y += 12;
        
        // Edit Mode Status
        nvgFillColor(args.vg, nvgRGBA(255, 255, 100, 255)); // Yellow for edit mode
        std::string editStatus = "EDIT: ";
        if (module->editModeA) {
            editStatus += "A";
        } else if (module->editModeB) {
            editStatus += "B";
        } else {
            editStatus += "OFF";
        }
        nvgText(args.vg, 5, y, editStatus.c_str(), NULL);
        
        // Clock source indicators (small icons on the right)
        float rightX = box.size.x - 25;
        nvgFillColor(args.vg, nvgRGBA(150, 150, 150, 200));
        nvgFontSize(args.vg, 8);
        nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        
        // Clock A source
        std::string clockAText = module->inputs[Transmutation::CLOCK_A_INPUT].isConnected() ? "EXT" : "INT";
        nvgText(args.vg, rightX, 17, clockAText.c_str(), NULL);
        
        // Clock B source  
        std::string clockBText = module->inputs[Transmutation::CLOCK_B_INPUT].isConnected() ? "EXT" : "INT";
        nvgText(args.vg, rightX, 29, clockBText.c_str(), NULL);
        
        nvgRestore(args.vg);
        
        } catch (...) {
            // Silently handle any drawing errors to prevent crashes
            if (args.vg) {
                nvgRestore(args.vg);
            }
        }
    }
};

struct TransmutationWidget : ModuleWidget {
    HighResMatrixWidget* matrix;
    
    TransmutationWidget(Transmutation* module) {
        setModule(module);
        
        // 26HP = 131.318mm width
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Transmutation.svg")));
        
        // Add screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // High-Resolution 8x8 Matrix (center of panel) - updated position from SVG
        matrix = new HighResMatrixWidget(module);
        matrix->box.pos = Vec(mm2px(29.89035), mm2px(44.700367)); // Updated position from SVG matrix area
        addChild(matrix);
        
        // Edit mode buttons (above matrix) - updated positions from SVG
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(56.692692, 30.109375)), module, Transmutation::EDIT_A_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(74.692688, 30.109375)), module, Transmutation::EDIT_B_PARAM));
        
        // Edit mode lights (updated positions from SVG)
        addChild(createLightCentered<TealJewelLEDSmall>(mm2px(Vec(56.692692, 30.109375)), module, Transmutation::EDIT_A_LIGHT));
        addChild(createLightCentered<PurpleJewelLEDSmall>(mm2px(Vec(74.692688, 30.109375)), module, Transmutation::EDIT_B_LIGHT));
        
        // Left side controls - Sequence A (updated positions from SVG)
        addParam(createParamCentered<STKnobMedium>(mm2px(Vec(15.950587, 37.849998)), module, Transmutation::LENGTH_A_PARAM));
        addParam(createParamCentered<STKnobMedium>(mm2px(Vec(15.950587, 17.386507)), module, Transmutation::INTERNAL_CLOCK_PARAM));
        
        // BPM Multiplier knob (positioned near BPM knob)
        addParam(createParamCentered<STKnobSmall>(mm2px(Vec(30, 17.386507)), module, Transmutation::BPM_MULTIPLIER_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(15.950587, 67.512939)), module, Transmutation::START_A_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(15.950587, 75.119118)), module, Transmutation::STOP_A_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(15.950587, 82.725296)), module, Transmutation::RESET_A_PARAM));
        
        // Right side controls - Sequence B (updated positions from SVG)
        addParam(createParamCentered<STKnobMedium>(mm2px(Vec(115.02555, 37.849998)), module, Transmutation::LENGTH_B_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(115.02555, 67.512939)), module, Transmutation::START_B_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(115.02555, 74.704277)), module, Transmutation::STOP_B_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(115.02555, 82.725296)), module, Transmutation::RESET_B_PARAM));
        
        // Sequence B mode switch (right side) - updated position from SVG  
        addParam(createParamCentered<STSelector>(mm2px(Vec(114.66154, 16.509369)), module, Transmutation::SEQ_B_MODE_PARAM));
        
        // Custom Display Widget - temporarily commented out for debugging
        // TransmutationDisplayWidget* display = new TransmutationDisplayWidget(module);
        // display->box.pos = mm2px(Vec(10, 115)); // Position in lower left area
        // addChild(display);
        
        // Chord pack loader button (custom widget) - centered on new panel
        ChordPackButton* chordPackButton = new ChordPackButton(module);
        chordPackButton->box.pos = mm2px(Vec(48.659, 12)); // Match SVG position
        addChild(chordPackButton);
        
        // Left side I/O - Sequence A (updated positions from SVG)
        addInput(createInputCentered<STPort>(mm2px(Vec(15.950587, 90.831467)), module, Transmutation::CLOCK_A_INPUT));
        addInput(createInputCentered<STPort>(mm2px(Vec(15.950587, 99.437645)), module, Transmutation::RESET_A_INPUT));
        addInput(createInputCentered<STPort>(mm2px(Vec(7.5, 67.512939)), module, Transmutation::START_A_INPUT));
        addInput(createInputCentered<STPort>(mm2px(Vec(7.5, 75.119118)), module, Transmutation::STOP_A_INPUT));
        addOutput(createOutputCentered<STPort>(mm2px(Vec(15.950587, 108.04382)), module, Transmutation::CV_A_OUTPUT));
        addOutput(createOutputCentered<STPort>(mm2px(Vec(15.950587, 116.65)), module, Transmutation::GATE_A_OUTPUT));
        
        // Right side I/O - Sequence B (updated positions from SVG)
        addInput(createInputCentered<STPort>(mm2px(Vec(115.02555, 90.831467)), module, Transmutation::CLOCK_B_INPUT));
        addInput(createInputCentered<STPort>(mm2px(Vec(115.02555, 99.437645)), module, Transmutation::RESET_B_INPUT));
        addInput(createInputCentered<STPort>(mm2px(Vec(123.5, 67.512939)), module, Transmutation::START_B_INPUT));
        addInput(createInputCentered<STPort>(mm2px(Vec(123.5, 74.704277)), module, Transmutation::STOP_B_INPUT));
        addOutput(createOutputCentered<STPort>(mm2px(Vec(115.02555, 108.04382)), module, Transmutation::CV_B_OUTPUT));
        addOutput(createOutputCentered<STPort>(mm2px(Vec(115.02555, 116.65)), module, Transmutation::GATE_B_OUTPUT));
        
        // Alchemical Symbol Buttons - updated positions from SVG
        // Top row of symbols (above matrix) - updated positions from SVG
        float topSymbolPositions[] = {36.510941, 46.963669, 57.416397, 67.869125, 78.321854, 88.774582}; // Updated from SVG
        for (int i = 0; i < 6; i++) {
            AlchemicalSymbolWidget* symbolWidget = new AlchemicalSymbolWidget(module, i); // i is button position
            symbolWidget->box.pos = mm2px(Vec(topSymbolPositions[i], 35.67366)); // Updated position from SVG
            addChild(symbolWidget);
        }
        
        // Bottom row of symbols (below matrix) - updated positions from SVG  
        float bottomSymbolPositions[] = {36.510941, 46.963669, 57.416397, 67.869125, 78.321854, 88.774582}; // Same X positions as top row
        for (int i = 6; i < 12; i++) {
            AlchemicalSymbolWidget* symbolWidget = new AlchemicalSymbolWidget(module, i); // i is button position
            symbolWidget->box.pos = mm2px(Vec(bottomSymbolPositions[i - 6], 117.56416)); // Updated position from SVG
            addChild(symbolWidget);
        }
        
        // Rest and Tie buttons - updated positions (if they exist in SVG, otherwise keep for functionality)
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(15.950587, 53.27956)), module, Transmutation::REST_PARAM));
        addParam(createParamCentered<Vintage1940sButton>(mm2px(Vec(115.02555, 53.27956)), module, Transmutation::TIE_PARAM));
        
        // Running lights - positioned with sequence controls  
        addChild(createLightCentered<TealJewelLEDMedium>(mm2px(Vec(32.739845, 28.08626)), module, Transmutation::RUNNING_A_LIGHT));
        addChild(createLightCentered<PurpleJewelLEDMedium>(mm2px(Vec(98.721703, 26.886547)), module, Transmutation::RUNNING_B_LIGHT));
    }
};

Model* modelTransmutation = createModel<Transmutation, TransmutationWidget>("Transmutation");