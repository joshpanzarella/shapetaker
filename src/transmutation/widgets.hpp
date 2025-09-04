#pragma once

#include "../plugin.hpp"

namespace shapetaker {
namespace transmutation {

// Colored Jewel LED Widgets - standalone widgets with no external dependencies
struct TealJewelLEDSmall : ModuleLightWidget {
    TealJewelLEDSmall();
    void draw(const DrawArgs& args) override;
};

struct PurpleJewelLEDSmall : ModuleLightWidget {
    PurpleJewelLEDSmall();
    void draw(const DrawArgs& args) override;
};

struct TealJewelLEDMedium : ModuleLightWidget {
    TealJewelLEDMedium();
    void draw(const DrawArgs& args) override;
};

struct PurpleJewelLEDMedium : ModuleLightWidget {
    PurpleJewelLEDMedium();
    void draw(const DrawArgs& args) override;
};

} // namespace transmutation
} // namespace shapetaker