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
- **Alchemical Symbols**: 12 symbols representing different chords, with real-time RGB lighting
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
- **Compact 26HP Design**: Professional layout with proper control spacing

#### Technical Implementation
- **Sample Rate Adaptive**: All timing and filters adjust to current sample rate
- **JSON Chord Packs**: Extensible chord library system
- **Voice Allocation**: Intelligent polyphonic voice distribution with octave cycling
- **Clock System**: Internal clock with external override (separate for A & B)
- **External Trigger Inputs**: Start/Stop/Reset triggers for both sequences with CV control
- **CV Integration**: Full CV control integration throughout
- **Visual Feedback**: Real-time LED matrix and symbol lighting updates
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
- **12 Unique Symbols**: Hand-drawn geometric representations
- **RGB Lighting**: Real-time feedback based on sequence playback
- **Symbol-to-Chord Mapping**: Each symbol represents a chord from current pack
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
10. **Final Polish**: Color-coded lighting system and professional panel design

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
- **New Symbols**: Add to `drawAlchemicalSymbol()` method
- **Lighting Changes**: Update RGB lighting logic in `process()`
- **Count Changes**: Modify symbol widget creation loops and enum values

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

## Notes for AI Assistants

- **Build System**: Always run `make clean` before major changes
- **UI Updates**: Modify both code and SVG files for visual changes  
- **Audio Processing**: Test all changes with various input signals
- **Documentation**: Update this file when adding major features
- **Commit Messages**: Include "🤖 Generated with Claude Code" footer
- **File Organization**: Use `.claude/CLAUDE.md` as the primary documentation
- **Module Count**: Plugin now contains 7 modules (not 6)
- **Transmutation Focus**: The flagship sequencer module with advanced chord functionality
- **Panel Sizes**: Transmutation is 26HP, other modules vary
- **Color System**: Teal (#00ffb4) for A, Purple (#b400ff) for B throughout UI