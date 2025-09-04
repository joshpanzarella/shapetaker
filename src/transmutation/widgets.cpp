#include "widgets.hpp"

namespace shapetaker {
namespace transmutation {

// TealJewelLEDSmall Implementation
TealJewelLEDSmall::TealJewelLEDSmall() {
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

void TealJewelLEDSmall::draw(const DrawArgs& args) {
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

// PurpleJewelLEDSmall Implementation
PurpleJewelLEDSmall::PurpleJewelLEDSmall() {
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

void PurpleJewelLEDSmall::draw(const DrawArgs& args) {
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

// TealJewelLEDMedium Implementation
TealJewelLEDMedium::TealJewelLEDMedium() {
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

void TealJewelLEDMedium::draw(const DrawArgs& args) {
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

// PurpleJewelLEDMedium Implementation
PurpleJewelLEDMedium::PurpleJewelLEDMedium() {
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

void PurpleJewelLEDMedium::draw(const DrawArgs& args) {
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

} // namespace transmutation
} // namespace shapetaker