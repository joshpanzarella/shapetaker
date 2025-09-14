# Shapetaker VCV Rack Plugin

## Overview
Shapetaker is a VCV Rack plugin containing 7 modules focused on advanced audio processing and synthesis. The plugin is written in C++ and uses the VCV Rack SDK.

## Build System
- **Build**: `make -j4` 
- **Clean**: `make clean`
- **Platform**: macOS (ARM64)
- **SDK**: `/Users/joshpanzarella/Documents/src/Rack-SDK`

## Modules

### 1. Clairaudient (14HP)
Dual sigmoid oscillator (V/Z) with morphing waveforms and stereo crossfading. Features hardware-realistic control sizing with 18mm medium knobs for frequency controls and 8mm attenuverters.

### 2. Chiaroscuro (18HP) 
Stereo VCA with sidechain-controlled distortion engine. 6 distortion algorithms with full CV control. Refactored to use modern utility system.

### 3. Involution (18HP)
Dual 6th order morphing filters with cross-feedback, chaos modulation, shimmer processing, and stereo effects. Uses modular DSP architecture.

### 4. Fatebinder
Strange attractor chaotic LFO with chaos/order morphing.

### 5. Evocation  
Gesture-based envelope generator with multi-speed outputs.

### 6. Incantation
Sophisticated animated filter bank inspired by the Moog MF-105 MuRF pedal. 8-band filter system with polyphonic processing.

### 7. Transmutation (26HP) - Flagship Module
Dual chord sequencer with 8x8 LED matrix, 40 alchemical symbols, JSON chord packs, and polyphonic outputs. Uses clean modular architecture with view/controller interfaces.

## Project Structure
```
shapetaker/
├── src/
│   ├── [module].cpp           # Module implementations
│   ├── dsp/                   # DSP utilities (polyphony, parameters, effects, filters)
│   ├── graphics/              # Drawing system (40 alchemical symbols)
│   ├── ui/                    # Theme management, layout helpers
│   ├── voice/                 # Polyphonic voice assignment 
│   ├── transmutation/         # Modular Transmutation components
│   ├── involution/            # Module-specific DSP
│   └── utilities.hpp          # Unified utility access
├── res/                       # SVG panels, knobs, graphics
├── chord_packs/               # JSON chord libraries
└── plugin.json
```

## Code Architecture

### Modern Utility System
All modules use the unified utility system organized by function:

**Parameter Configuration:**
```cpp
shapetaker::ParameterHelper::configGain(this, GAIN_PARAM, "Gain");
shapetaker::ParameterHelper::configAttenuverter(this, ATT_PARAM, "Attenuverter");
```

**Polyphonic Processing:**
```cpp
shapetaker::PolyphonicProcessor polyProcessor;
shapetaker::VoiceArray<MyDSPClass> voices;
int channels = polyProcessor.updateChannels(input, {output});
```

**UI Layout & Theming:**
```cpp
shapetaker::ui::LayoutHelper::PanelSVGParser parser(asset::plugin(pluginInstance, "res/panels/MyPanel.svg"));
Vec pos = parser.centerPx("element_id", fallback_x, fallback_y);
shapetaker::ui::ThemeManager::BrandColors::tealRGB(); // Channel A
```

**Voice Assignment:**
```cpp
std::vector<float> intervals = {0, 4, 7, 11};
std::vector<float> targets;
stx::poly::buildTargetsFromIntervals(intervals, voiceCount, harmonyMode, targets);
stx::poly::assignNearest(targets, lastCV, voiceCount, outputCV);
```

### Transmutation Modular Architecture
Uses view/controller interfaces for clean separation:
- **TransmutationView** - Read-only access for UI widgets
- **TransmutationController** - Action interface for user interactions
- Components separated into `src/transmutation/` directory

### Hardware-First Design Philosophy
All controls sized to match real Eurorack hardware:
- **Large knobs**: 22mm (63px) for main parameters
- **Medium knobs**: 18mm (52px) for secondary controls  
- **Small attenuverters**: 8mm (26px) for CV scaling
- **Jacks**: 6-8mm for professional appearance

## Key Features

### Visual System
- **Color Coding**: Teal (#00ffb4) for Channel A, Purple (#b400ff) for Channel B
- **40 Alchemical Symbols**: Complete drawing system for Transmutation
- **LED Matrix**: 8x8 matrix with color-coded sequence states
- **Theme Management**: Consistent styling across all modules

### Technical Implementation
- **C++11 Compatible**: Maintains VCV Rack requirements
- **6-Voice Polyphony**: Consistent across all modules
- **Sample Rate Adaptive**: All DSP adjusts to current sample rate
- **SVG-Based Layout**: Precise control positioning from panel designs
- **Modular DSP**: Module-specific components in dedicated directories

## Development Guidelines

### Utility System Usage
1. **Always use `ParameterHelper`** for parameter configuration
2. **Use `PolyphonicProcessor` and `VoiceArray<T>`** for voice management
3. **Use `ThemeManager`** for consistent colors and styling
4. **Use `LayoutHelper::PanelSVGParser`** for SVG-based positioning
5. **Check utilities first** before implementing new functionality

### Common Development Tasks
- **Build Verification**: `make clean && make -j4`
- **Parameter Updates**: Use semantic helpers instead of raw `configParam()`
- **Panel Changes**: Update both SVG and code coordinates
- **New Modules**: Leverage existing utilities from day one

### Quality Standards
- **Type Safety**: Use templates and strong typing
- **Semantic Clarity**: Descriptive function names
- **Error Prevention**: Automatic bounds checking and validation
- **Consistent Sizing**: Hardware-realistic control dimensions
- **Professional Appearance**: Proper spacing and visual hierarchy

## Refactoring Status
✅ **Utility System** - Complete DSP, graphics, UI, and voice utilities
✅ **Parameter Helpers** - Standardized configuration system
✅ **Theme Management** - Comprehensive visual styling system
✅ **Layout System** - SVG-based positioning with panel parsing
✅ **Transmutation Modular Architecture** - View/controller separation
✅ **Hardware-First Sizing** - Realistic control dimensions
🟨 **Module Decomposition** - Transmutation complete, others pending
⏳ **Widget Extraction** - Common widget patterns pending

## Notes for AI Assistants

### Essential Patterns
- **Namespace**: Access utilities through `shapetaker::`
- **Parameter Config**: Always use `ParameterHelper` methods
- **Polyphony**: Use `PolyphonicProcessor` + `VoiceArray<T>`
- **UI Layout**: Parse SVG panels with `PanelSVGParser`
- **Colors**: Use `ThemeManager` brand colors consistently
- **Voice Assignment**: Use `stx::poly` for chord building

### Critical Requirements
- **C++11 Only**: Avoid C++17+ features
- **Hardware Sizing**: Follow established control dimensions
- **Build System**: Always `make clean` before major changes
- **Documentation**: Update CLAUDE.md for major features
- **Consistency**: Maintain 6-voice polyphony and standard ranges

### Architecture Focus
- **Transmutation**: Uses modular view/controller interfaces
- **Color System**: Teal/Purple throughout for dual channels
- **Utility-First**: Check existing utilities before implementing
- **SVG Integration**: All controls positioned from panel coordinates

The codebase emphasizes professional hardware emulation, clean architecture, and reusable utilities for efficient development.