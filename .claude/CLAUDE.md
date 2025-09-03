# Shapetaker VCV Rack Plugin

## Overview

Shapetaker is a VCV Rack plugin containing 7 modules focused on advanced audio processing and synthesis. The plugin is written in C++ and uses the VCV Rack SDK.

## Project Structure

```
shapetaker/
├── src/                    # Source code
│   ├── plugin.cpp         # Plugin initialization and module registration
│   ├── plugin.hpp         # Plugin header and shared components
│   ├── chiaroscuro.cpp    # Chiaroscuro module
│   ├── clairaudient.cpp   # Clairaudient module
│   ├── evocation.cpp      # Evocation module
│   ├── fatebinder.cpp     # Fatebinder module
│   ├── incantation.cpp    # Incantation module (MuRF-inspired filter bank)
│   ├── involution.cpp     # Involution module
│   └── sequencer.cpp      # Transmutation module (dual chord sequencer)
├── res/                   # Resources
│   ├── panels/           # SVG panel designs
│   ├── buttons/         # Button graphics (vintage 1940s style)
│   ├── knobs/           # Custom knob graphics
│   ├── switches/        # Switch graphics
│   ├── leds/            # LED graphics
│   ├── meters/          # VU meter graphics
│   └── ...              # Other UI elements
├── chord_packs/          # JSON chord pack files for Transmutation
│   ├── jazz_standards_bb.json
│   └── 80s_pop_d_sharp.json
├── plugin.json           # Plugin metadata
└── Makefile              # Build configuration
```

## Build System

- **Build command**: `make -j4`
- **Clean command**: `make clean`
- **Platform**: macOS (ARM64)
- **Dependencies**: VCV Rack SDK (located at `/Users/joshpanzarella/Documents/src/Rack-SDK`)

## Module Overview

### 1. Clairaudient
Dual sigmoid oscillator with morphing waveforms and stereo crossfading.

### 2. Chiaroscuro
Stereo VCA with sidechain-controlled distortion engine. Named after the artistic technique of dramatic light/dark contrast, features 6 intense distortion algorithms with full CV control.

### 3. Fatebinder
Strange attractor chaotic LFO with chaos/order morphing.

### 4. Involution
Dual 6th order morphing filters with cross-feedback, chaos modulation, shimmer processing, and magical stereo effects.

### 5. Evocation
Gesture-based envelope generator with multi-speed outputs.

### 6. Incantation (Primary Module)
The Incantation module is a sophisticated animated filter bank inspired by the Moog MF-105 MuRF pedal, with significant enhancements:

#### Features
- **12 Authentic MuRF Patterns**: Exact replications of the original MF-105 patterns
- **8-Band Filter System**: 
  - BASS mode: 110Hz-1.8kHz with lowpass + bandpass filters
  - MIDS mode: 200Hz-3.4kHz all bandpass filters
- **Polyphonic Processing**: Up to 6 voices
- **Stereo I/O**: Dedicated L/R inputs and outputs
- **Real-time CV Control**: ±5V range for all 8 filter levels
- **Preset Buttons**: Quick 0%, 50%, 100% settings
- **CV Bypass Switch**: Global disable for all filter CV inputs
- **Variable Q Factor**: Normal/High Q switch for filter resonance
- **LFO Sweep**: Frequency modulation of all filters
- **Tap Tempo**: External clock input support

### 7. Transmutation (Flagship Module)
The Transmutation module is a sophisticated dual chord sequencer with advanced alchemical-themed interface:

#### Features
- **8x8 LED Matrix**: Main step programming interface with large, visible LEDs (10mm size, 14mm spacing)
- **Dual Independent Sequences**: A & B with separate clocks, controls, and outputs
- **Alchemical Symbols**: 40 unique symbols representing different chords, with real-time RGB lighting and 12 displayed on buttons at any time
- **Chord Pack System**: JSON-based chord libraries (Jazz, Pop, etc.) with contextual assignments
- **Three Sequence B Modes**:
  - **Independent**: Completely separate chord pack and progression
  - **Harmony**: Uses Sequence A's chord pack with harmony intervals (+1 octave + variations)
  - **Lock**: Same chord pack as A, different progression
- **Polyphonic Outputs**: Up to 6 voices per sequence with intelligent voice allocation
- **Per-Step Voice Count**: 1-6 voices configurable for each step
- **Edit Modes**: Visual programming by clicking symbols then matrix positions
- **Rest & Tie Functions**: Full sequencer control with musical articulation
- **Real-time Visual Feedback**: 
  - Matrix LEDs show sequence states with color coding
  - Symbol lights show current playing chords:
    - **Teal** (#00ffb4) for Sequence A
    - **Purple** (#b400ff) for Sequence B  
    - **Cyan-Magenta Mix** when both sequences play same symbol
- **Configurable Chord Display Modes**:
  - **Spooky TV Effect**: Dramatic VHS-style display with warping, scanlines, and color cycling
  - **Clean Mode**: Simple, professional display with clear symbol and text (toggle via context menu)
- **Compact 26HP Design**: Professional layout with proper control spacing

#### Technical Implementation
- **Sample Rate Adaptive**: All timing and filters adjust to current sample rate
- **JSON Chord Packs**: Extensible chord library system
- **Voice Allocation**: Intelligent polyphonic voice distribution with octave cycling
- **Clock System**: Internal clock with external override (separate for A & B)
- **External Trigger Inputs**: Start/Stop/Reset triggers for both sequences with CV control
- **CV Integration**: Full CV control integration throughout
- **Visual Feedback**: Real-time LED matrix and symbol lighting updates
- **Configurable Display System**: `bool spookyTvMode` toggle for chord name presentation
- **Precise Panel Alignment**: All controls use exact SVG coordinates with named IDs

#### Chord Pack Format
```json
{
  "name": "Jazz Standards in Bb",
  "key": "Bb",
  "description": "Classic jazz progressions in Bb major",
  "chords": [
    {
      "name": "BbMaj7",
      "intervals": [0, 4, 7, 11],
      "preferredVoices": 4,
      "category": "major"
    }
  ]
}
```

#### Panel Layout (26HP)
- **Center**: 8x8 LED matrix with surrounding alchemical symbols
- **Left Side**: Sequence A controls (length, transport, rest, I/O)
- **Right Side**: Sequence B controls (length, transport, tie, mode, I/O)
- **Top**: Chord pack selector and edit mode buttons
- **Bottom**: Internal clock control

#### I/O Configuration
**Sequence A (Left Side):**
- Clock Input: External clock override
- Start/Stop/Reset Trigger Inputs: CV trigger control for transport functions
- CV Output: Polyphonic pitch output (up to 6 voices)
- Gate Output: Polyphonic gate output (up to 6 voices)

**Sequence B (Right Side):**
- Clock Input: External clock override (independent from A)
- Start/Stop/Reset Trigger Inputs: CV trigger control for transport functions
- CV Output: Polyphonic pitch output (up to 6 voices) 
- Gate Output: Polyphonic gate output (up to 6 voices)

**Control Integration:**
- All I/O ports positioned using exact SVG coordinates with named IDs
- External triggers work alongside front panel transport buttons
- Black screws for professional aesthetic matching panel design

## Code Architecture

### Module Structure
Each module follows the VCV Rack module pattern:
- **Parameter Enums**: Define all knobs, switches, and buttons
- **Input/Output Enums**: Define all audio and CV connections  
- **Constructor**: Configure parameters and initialize state
- **process()**: Main audio processing loop (called per sample)
- **Widget Class**: UI layout and control positioning

### Key Classes in Transmutation

#### ChordData & ChordPack
```cpp
struct ChordData {
    std::string name;
    std::vector<float> intervals;  // Semitone intervals from root
    int preferredVoices;
    std::string category;
};

struct ChordPack {
    std::string name;
    std::string key;
    std::vector<ChordData> chords;
    std::string description;
};
```

#### Sequence Engine
```cpp
struct SequenceStep {
    int chordIndex;        // Which chord from pack (-1=rest, -2=tie)
    int voiceCount;        // 1-6 voices for this step
    int alchemySymbolId;   // Visual symbol association
};

struct Sequence {
    std::array<SequenceStep, 64> steps;
    int length;            // 1-64 steps (user configurable)
    int currentStep;
    bool running;
    float clockPhase;
};
```

#### Custom Widgets
```cpp
struct Matrix8x8Widget : Widget {
    static constexpr int MATRIX_SIZE = 8;
    static constexpr float LED_SIZE = 10.0f;
    static constexpr float LED_SPACING = 14.0f;
    
    void drawMatrix(const DrawArgs& args);
    void onMatrixClick(int x, int y);
    void programStep(Sequence& seq, int stepIndex);
};

struct AlchemicalSymbolWidget : Widget {
    void drawSymbol(const DrawArgs& args, int symbolId);
    // Renders geometric planetary/alchemical symbols
};

struct ChordPackButton : Widget {
    void loadChordPack();
    // Cycles through available chord pack JSON files
};
```

## UI Components

### Panel Design
- **26HP Compact Size**: 131.318mm width (reduced from 38HP for efficiency)
- **SVG-based panels**: Located in `res/panels/`
- **Professional Typography**: Clear hierarchy with section labels
- **Color Coding**: Teal for Sequence A, Purple for Sequence B
- **Visual Guides**: Proper spacing and grouping of related controls

### Matrix LED System
- **Large LEDs**: 10mm diameter with 14mm spacing for visibility
- **Color States**:
  - **Dark**: Empty step
  - **Teal**: Sequence A programmed step
  - **Purple**: Sequence B programmed step  
  - **Mixed**: Both sequences programmed
  - **Bright**: Current playhead position
  - **Edit Glow**: Animated border during edit mode

### Alchemical Symbol System
- **40 Unique Symbols**: Hand-drawn geometric representations including classical planetary symbols and extended occult/alchemical designs
- **Dynamic Button Mapping**: 12 symbols displayed on buttons at any time, randomized from the full 40-symbol collection when loading chord packs
- **Symbol Collection**: Includes Sun, Moon, Mercury, Venus, Mars, Jupiter, Saturn, Earth, Fire, Water, Air, Aether, Sulfur, Salt, Antimony, Arsenic, Bismuth, Copper, Gold, Iron, Lead, Magnesium, Platinum, Silver, Tin, Zinc, Void, Chaos Star, Tree of Life, Leviathan Cross, and more
- **RGB Lighting**: Real-time feedback based on sequence playback
- **Symbol-to-Chord Mapping**: Each symbol represents a chord from current pack with intelligent randomization
- **Visual Programming**: Click symbol → click matrix step to program

## Development History

The Transmutation module underwent extensive development phases:

1. **Initial Architecture**: Basic dual sequencer structure
2. **Matrix Implementation**: 8x8 LED grid with click handling
3. **Symbol System**: 12 alchemical symbols with custom graphics
4. **Chord Pack Integration**: JSON loading system with example packs
5. **Edit Mode**: Visual feedback system with animated glows
6. **Polyphonic System**: Voice allocation with octave cycling
7. **Sequence B Modes**: Independent/Harmony/Lock functionality
8. **Visual Polish**: RGB symbol lighting and improved matrix display
9. **Layout Refinement**: Compact 26HP design with proper spacing
10. **Symbol System Expansion**: Extended from 30 to 40 alchemical symbols with improved randomization
11. **Final Polish**: Color-coded lighting system and professional panel design
12. **Display Mode Options**: Added configurable spooky TV effect toggle with clean alternative display mode

## Common Development Tasks

### Adding New Parameters
1. Add to Parameter enum in module header
2. Configure in constructor with `configParam()` or `configSwitch()`
3. Add processing logic in `process()` method
4. Position widget in ModuleWidget constructor
5. Update panel SVG if needed

### Creating New Chord Packs
1. Create JSON file in `chord_packs/` directory
2. Use existing format with name, key, description, and chords array
3. Each chord needs name, intervals array, preferredVoices, and category
4. Chord pack will automatically be discoverable by the module

### Extending Matrix Functionality
- **New Modes**: Add to sequence processing logic
- **Visual States**: Update `drawMatrix()` method color logic
- **Interaction**: Modify `onMatrixClick()` for new behaviors
- **Size Changes**: Update `MATRIX_SIZE`, `LED_SIZE`, `LED_SPACING` constants

### Symbol System Modifications
- **New Symbols**: Add to `drawAlchemicalSymbol()` method with cases 0-39
- **Symbol Range Updates**: Ensure all drawing functions use `< 40` instead of older `< 20` limits
- **Lighting Changes**: Update RGB lighting logic in `process()`
- **Randomization**: Modify `randomizeSymbolAssignment()` for new symbol distribution
- **Button Mapping**: Update `buttonToSymbolMapping` array to include symbols 0-39

## Testing and Debugging

### Build Verification
```bash
make clean && make -j4
```

### Common Issues
- **Compilation errors**: Check enum consistency and parameter counts
- **Module loading**: Verify plugin.json includes all modules
- **Audio dropouts**: Check sample rate handling in processing loops
- **UI misalignment**: Verify mm2px() conversions in widget positioning
- **JSON loading**: Ensure chord pack files are valid JSON format
- **Symbol Display Issues**: Ensure all drawing functions use correct symbol range (0-39), not legacy ranges
- **Symbol Assignment Problems**: Verify `symbolToChordMapping` array covers all 40 symbols and button mappings are valid

### Performance Considerations
- **Matrix Updates**: Only redraw when state changes
- **Polyphonic Processing**: Limit to 6 voices maximum per sequence
- **Visual Updates**: Throttle parameter updates to avoid jitter
- **Chord Pack Loading**: Cache loaded packs to avoid file I/O during audio processing

## Plugin Integration

The plugin follows VCV Rack conventions:
- **Consistent Naming**: All parameters and ports clearly labeled
- **Standard Voltages**: ±5V CV, ±10V audio, standard 1V/oct
- **Polyphonic Support**: Handles multiple channels appropriately  
- **Resource Management**: Proper cleanup and initialization
- **Visual Standards**: Consistent UI design across all modules

## Installation and Deployment

### VCV Rack Integration
The plugin loads from: `/Users/joshpanzarella/Documents/src/shapetaker/`

### Deployment Process
1. Build in development directory: `/Users/joshpanzarella/vcv-dev/shapetaker/`
2. Copy `plugin.dylib` and `plugin.json` to VCV Rack directory
3. Copy updated panel files from `res/panels/` if modified
4. Copy chord pack files to ensure Transmutation functionality

## Future Enhancements

### Transmutation Module
- **Additional Chord Packs**: More musical styles and keys
- **MIDI Integration**: External MIDI control for chord selection
- **Pattern Morphing**: Smooth interpolation between sequences
- **Advanced Voice Allocation**: Configurable voice distribution algorithms
- **Preset System**: Save/load complete sequence configurations
- **Export Functionality**: Generate MIDI or CV sequences

### General Plugin
- **Additional Modules**: More synthesis and processing modules
- **Consistent UI Theme**: Unified visual design language
- **Performance Optimization**: Enhanced processing efficiency
- **Documentation**: Comprehensive user manuals
- **Community Integration**: ModularGrid integration and user presets

## Utility Libraries & Refactoring (Updated 2025-01-09)

The codebase has undergone extensive refactoring to eliminate code duplication, improve maintainability, and provide a professional foundation for future development. The utilities are organized into functional domains with comprehensive class hierarchies.

### Project Structure (Updated)
```
shapetaker/
├── src/
│   ├── dsp/                    # DSP Utilities (NEW)
│   │   ├── polyphony.hpp      # Polyphonic voice management
│   │   ├── parameters.hpp     # Parameter configuration helpers
│   │   ├── effects.hpp        # Audio effects (sidechain, distortion)
│   │   ├── filters.hpp        # Filter utilities 
│   │   ├── envelopes.hpp      # Envelope generators
│   │   ├── oscillators.hpp    # Oscillator helpers
│   │   └── audio.hpp          # Audio processing utilities
│   ├── graphics/              # Graphics Utilities
│   │   ├── drawing.hpp        # Drawing functions
│   │   ├── lighting.hpp       # RGB lighting helpers
│   │   └── effects.hpp        # Visual effects
│   ├── ui/                    # UI Utilities
│   │   ├── widgets.hpp        # Custom widget library
│   │   └── helpers.hpp        # UI helper functions
│   ├── transmutation/         # Transmutation module components
│   │   ├── ui.hpp/.cpp        # UI classes
│   │   ├── engine.hpp/.cpp    # Sequencer engine
│   │   └── chords.hpp/.cpp    # Chord pack system
│   ├── utilities.hpp          # Unified utility access with aliases
│   └── [existing module files]
```

### DSP Utilities (src/dsp/)

#### Polyphony Management (polyphony.hpp) ✅
**PolyphonicProcessor class** - Unified polyphonic voice management:
- `MAX_VOICES = 6` constant across all Shapetaker modules
- `updateChannels()` - Automatic input analysis and output configuration
- `getChannelCount()` - Safe channel count determination with clamping

**VoiceArray<T> template** - Type-safe voice arrays:
- Automatic bounds checking with `operator[]` clamping to valid range
- `forEach()`, `forEachActive()`, `forEachWithIndex()` functional operations
- `reset()` for initializing all voices to default state
- Pre-defined aliases: `FloatVoices`, `IntVoices`, `BoolVoices`

**SampleRateManager class** - Batch sample rate updates:
- `updateVoiceArray()` - Apply sample rate changes to all voices
- Template-based for any DSP class with `setSampleRate()` method

```cpp
// Usage Example:
shapetaker::PolyphonicProcessor polyProcessor;
shapetaker::VoiceArray<shapetaker::DistortionEngine> distortion_l, distortion_r;

// In process():
int channels = polyProcessor.updateChannels(inputs[AUDIO_L_INPUT], {outputs[AUDIO_L_OUTPUT]});
float processed = distortion_l[ch].process(input, drive, type);
```

#### Parameter Configuration (parameters.hpp) ✅
**ParameterHelper class** - Standardized parameter setup:

**Common Parameter Types:**
- `configGain()` - Standard 0-100% gain parameters
- `configVCAGain()` - VCA with 0-200% range
- `configAttenuverter()` - Bipolar -100% to +100%
- `configDrive()` - Drive/distortion 0-100%
- `configMix()` - Mix/blend 0-100%
- `configResonance()` - Filter Q with standard range
- `configFrequency()` - Exponential frequency scaling with 1V/oct
- `configAudioFrequency()` - 20Hz-20kHz range
- `configLFOFrequency()` - 0.1Hz-50Hz range

**Discrete Parameters:**
- `configBPM()` - BPM with automatic snap enabled
- `configLength()` - Sequence length with snap
- `configDiscrete()` - Generic discrete with snap + no smoothing

**I/O Configuration:**
- `configAudioInput/Output()` - Standard audio I/O
- `configCVInput()` - CV inputs with ±5V/±10V documentation
- `configGateInput()`, `configClockInput()` - Specialized inputs
- `configPolyCVOutput()`, `configPolyGateOutput()` - Polyphonic outputs

**Parameter Value Utilities:**
- `getParameterValue()` - With optional CV modulation
- `getClampedParameterValue()` - With automatic range limiting
- `setParameterValue()` - Programmatic parameter setting

**StandardParams namespace** - Constants for consistent ranges:
```cpp
constexpr float GAIN_MIN = 0.0f, GAIN_MAX = 1.0f;
constexpr float CV_SCALE_1V = 0.1f;  // 10V → 1.0
constexpr float CV_SCALE_5V = 0.2f;  // 5V → 1.0
```

```cpp
// Usage Example - Replace verbose configParam calls:
// OLD: configParam(VCA_PARAM, 0.0f, 1.0f, 0.0f, "VCA Gain", "%", 0.0f, 100.0f);
shapetaker::ParameterHelper::configGain(this, VCA_PARAM, "VCA Gain");
shapetaker::ParameterHelper::configAttenuverter(this, VCA_ATT_PARAM, "VCA CV Attenuverter");
shapetaker::ParameterHelper::configDiscrete(this, TYPE_PARAM, "Distortion Type", 0, 5, 0);
```

#### Audio Effects (effects.hpp)
**SidechainDetector class** - Envelope following for sidechain compression:
- `setTiming()` - Configure attack/release in milliseconds
- `process()` - Real-time envelope detection with smoothing

**DistortionEngine class** - Multiple distortion algorithms:
- 6 distortion types: Hard Clip, Wave Fold, Bit Crush, Destroy, Ring Mod, Tube Sat
- `process()` - Apply distortion with drive control and type selection
- Sample rate adaptive processing

Both classes moved from standalone headers to organized `shapetaker::dsp` namespace.

#### Filter System (filters.hpp)
**BiquadFilter class** - Generic biquad implementation:
- Filter types: Lowpass, Highpass, Bandpass, Notch, Allpass
- Coefficient caching to avoid redundant calculations
- Stability checking with automatic reset on NaN/overflow

**MorphingFilter class** - Extends BiquadFilter:
- Smooth morphing between different filter types
- Maintains filter state during type transitions

#### Other DSP Components
- **envelopes.hpp**: `EnvelopeGenerator` (ADSR), `TriggerHelper` (button/CV triggers)  
- **oscillators.hpp**: `OscillatorHelper` (phase management, basic waveforms)
- **audio.hpp**: `AudioProcessor` (crossfade, soft clipping, DC blocking)

### Graphics Utilities (src/graphics/)
- **drawing.hpp**: Symbol rendering, voice count dots, visual effects
- **lighting.hpp**: `LightingHelper` with Chiaroscuro color progressions, VU meter colors
- **effects.hpp**: Visual effect implementations

### UI Utilities (src/ui/)
- **widgets.hpp**: Custom LED widgets, VU meters, visualizer base classes
- **helpers.hpp**: Widget positioning, screw placement, template functions

### Integration & Backward Compatibility

**Unified Access (utilities.hpp):**
```cpp
namespace shapetaker {
    // All utilities available through shapetaker:: namespace
    using PolyphonicProcessor = dsp::PolyphonicProcessor;
    using ParameterHelper = dsp::ParameterHelper;
    using BiquadFilter = dsp::BiquadFilter;
    // ... all other utilities
}
```

**Automatic Inclusion:**
- `plugin.hpp` includes `utilities.hpp` automatically
- All modules gain access to utilities without modification
- Existing code continues to work unchanged during transition

### Demonstrated Refactoring

**Chiaroscuro Module** - Successfully refactored to demonstrate both utility systems:
1. **Polyphony**: Replaced manual `MAX_POLY_VOICES` arrays with `VoiceArray<DistortionEngine>`
2. **Parameters**: Replaced verbose `configParam()` calls with semantic helpers
3. **Sample Rate**: Used `forEach()` functional approach for batch updates

### Benefits Achieved
- **~800+ lines of duplicated code** consolidated into reusable utilities
- **Consistent behavior** across all modules (polyphony limits, parameter ranges)
- **Easier maintenance**: Bug fixes and improvements propagate to all modules
- **Faster development**: New modules can leverage existing utilities immediately
- **Professional foundation**: Well-tested, documented utility library
- **Type safety**: Template-based voice arrays prevent common indexing errors
- **Semantic clarity**: `configGain()` vs generic `configParam()` makes intent clear

### Refactoring Progress (2025-01-09)
✅ **Polyphony Management Utilities** - Complete with demonstration
✅ **Parameter Configuration Helpers** - Complete with demonstration  
🟨 **Decompose Large Modules** - In Progress
⏳ **Extract Common Widgets** - Pending
⏳ **Layout & Positioning Utilities** - Pending
⏳ **Visual Theme Management** - Pending

### Usage Guidelines for Future Development
1. **New Modules**: Use `ParameterHelper` for all parameter configuration
2. **Polyphonic Processing**: Use `PolyphonicProcessor` and `VoiceArray<T>`
3. **Before Implementing**: Check if utilities already provide the functionality
4. **Common Patterns**: Add new utilities rather than duplicating code
5. **C++11 Compatibility**: All utilities maintain VCV Rack's C++11 requirement

### Future Utility Enhancements
- **Delay line utilities**: Common delay/echo implementations
- **Modulation utilities**: LFO and modulation source helpers  
- **Sequencer utilities**: Common sequencer pattern implementations
- **SIMD optimizations**: Vectorized processing for performance
- **Unit testing framework**: Comprehensive testing of utility functions

## Notes for AI Assistants

### Build & Development
- **Build System**: Always run `make clean` before major changes
- **Build Command**: `make -j4` for parallel compilation
- **C++ Standard**: Project uses C++11 (VCV Rack requirement) - avoid C++17+ features
- **UI Updates**: Modify both code and SVG files for visual changes  
- **Audio Processing**: Test all changes with various input signals
- **Documentation**: Update this file when adding major features
- **Commit Messages**: Include "🤖 Generated with Claude Code" footer

### Project Architecture  
- **File Organization**: Use `.claude/CLAUDE.md` as the primary documentation
- **Module Count**: Plugin contains 7 modules (Clairaudient, Chiaroscuro, Fatebinder, Involution, Evocation, Incantation, Transmutation)
- **Transmutation Focus**: The flagship 26HP sequencer module with advanced chord functionality
- **Color System**: Teal (#00ffb4) for A, Purple (#b400ff) for B throughout UI

### Utility System (IMPORTANT)
- **Organized by Function**: DSP utilities in `src/dsp/`, graphics in `src/graphics/`, UI in `src/ui/`
- **Automatic Inclusion**: All utilities available via `#include "utilities.hpp"` (already in plugin.hpp)
- **Namespace**: Access utilities through `shapetaker::` (e.g., `shapetaker::ParameterHelper::configGain()`)
- **Parameter Configuration**: ALWAYS use `ParameterHelper` for new parameters instead of raw `configParam()`
- **Polyphonic Processing**: Use `PolyphonicProcessor` and `VoiceArray<T>` instead of manual voice management
- **Before Implementing**: Check if utilities already provide the functionality - avoid code duplication

### Common Patterns
```cpp
// Parameter Configuration (USE THIS):
shapetaker::ParameterHelper::configGain(this, GAIN_PARAM, "Gain");
shapetaker::ParameterHelper::configAttenuverter(this, ATT_PARAM, "Attenuverter");

// Polyphonic Processing (USE THIS):
shapetaker::PolyphonicProcessor polyProcessor;
shapetaker::VoiceArray<MyDSPClass> voices;
int channels = polyProcessor.updateChannels(input, {output});

// Legacy Approach (AVOID):
configParam(GAIN_PARAM, 0.0f, 1.0f, 0.0f, "Gain", "%", 0.0f, 100.0f);
static const int MAX_VOICES = 6;
MyDSPClass voices[MAX_VOICES];
```

### Refactoring Guidelines
- **New Modules**: Leverage existing utilities from day one
- **Existing Modules**: Refactor incrementally using established patterns
- **Code Duplication**: Add new utilities rather than duplicating common patterns
- **Consistency**: All modules should use same polyphony limits (6 voices), parameter ranges, etc.
- **Backward Compatibility**: Maintained through namespace aliases during transition

### Current Refactoring Status
- ✅ **Functional Organization**: DSP/Graphics/UI separation complete  
- ✅ **Polyphony Utilities**: `PolyphonicProcessor`, `VoiceArray<T>` implemented & demonstrated
- ✅ **Parameter Helpers**: Complete standardized parameter configuration system
- ✅ **Effects Migration**: SidechainDetector, DistortionEngine moved to organized structure
- 🟨 **Module Decomposition**: In progress - breaking down large modules
- ⏳ **Widget Extraction**: Common widget base classes pending
- ⏳ **Layout Utilities**: Positioning and spacing helpers pending

### Quality Standards
- **Type Safety**: Use templates and strong typing (VoiceArray vs raw arrays)
- **Semantic Clarity**: Use descriptive function names (`configGain()` vs generic `configParam()`)
- **Error Prevention**: Automatic bounds checking, parameter validation, stability checks
- **Documentation**: Every utility class has comprehensive documentation with usage examples
- **Testing**: All utilities tested through real module refactoring