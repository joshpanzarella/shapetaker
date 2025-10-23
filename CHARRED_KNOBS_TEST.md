# Charred Knobs Test - Reversion Guide

This document explains how to revert the Clairaudient module back to the original oscilloscope knobs.

## Changes Made

### Files Modified:
1. `src/plugin.hpp` - Added new charred knob widgets (lines 350-440)
2. `src/clairaudient.cpp` - Replaced oscilloscope knobs with charred versions
3. **SVG Files Updated** (improved metallic appearance based on reference photos):
   - `res/knobs/backgrounds/shapetaker_knob_BASE_charred_M.svg`
   - `res/knobs/backgrounds/shapetaker_knob_BASE_charred_S.svg`
   - `res/knobs/indicators/shapetaker_knob_ROTATE_charred_M.svg`
   - `res/knobs/indicators/shapetaker_knob_ROTATE_charred_S.svg`
   - **Backups saved** as `*_backup.svg` in same directories

## Improvements Made to Charred Knobs:

### Visual Enhancements:
- ✅ Hex-shaped outer rim (matches reference photos)
- ✅ Multi-layer beveled edges with depth
- ✅ Metallic chrome-like gradients (#5a5a5a → #3a3a3a → #6a6a6a → #2a2a2a)
- ✅ Stepped circular layers (dark recess → bevel ring → inner ring → dome)
- ✅ Radial highlights on center dome for 3D appearance
- ✅ Crisp white indicator line (cleaner, more defined)
- ✅ Proper shadows and inner bevels

### Size Matching:
- Small knobs: 16mm (matches ShapetakerKnobOscilloscopeSmall)
- Medium knobs: 18mm (matches ShapetakerKnobOscilloscopeMedium)

## To Revert to Original Oscilloscope Knobs:

### In `src/clairaudient.cpp`:

Replace all instances of:
- `ShapetakerKnobCharredMedium` → `ShapetakerKnobOscilloscopeMedium`
- `ShapetakerKnobCharredSmall` → `ShapetakerKnobOscilloscopeSmall`

Specifically, change these lines:

**Line 493-494** (Frequency knobs):
```cpp
// CHANGE FROM:
addParam(createParamCentered<ShapetakerKnobCharredMedium>(...));
// BACK TO:
addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(...));
```

**Line 508-509** (Fine tune knobs):
```cpp
// CHANGE FROM:
addParam(createParamCentered<ShapetakerKnobCharredSmall>(...));
// BACK TO:
addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(...));
```

**Line 519** (Crossfade knob):
```cpp
// CHANGE FROM:
addParam(createParamCentered<ShapetakerKnobCharredMedium>(...));
// BACK TO:
addParam(createParamCentered<ShapetakerKnobOscilloscopeMedium>(...));
```

**Line 528-529** (Shape knobs):
```cpp
// CHANGE FROM:
addParam(createParamCentered<ShapetakerKnobCharredSmall>(...));
// BACK TO:
addParam(createParamCentered<ShapetakerKnobOscilloscopeSmall>(...));
```

### To Restore Original SVG Files:

If you want to restore the original charred knob SVGs (before improvements):
```bash
cd /Users/joshpanzarella/vcv-dev/shapetaker/res/knobs
mv backgrounds/shapetaker_knob_BASE_charred_M_backup.svg backgrounds/shapetaker_knob_BASE_charred_M.svg
mv backgrounds/shapetaker_knob_BASE_charred_S_backup.svg backgrounds/shapetaker_knob_BASE_charred_S.svg
mv indicators/shapetaker_knob_ROTATE_charred_M_backup.svg indicators/shapetaker_knob_ROTATE_charred_M.svg
mv indicators/shapetaker_knob_ROTATE_charred_S_backup.svg indicators/shapetaker_knob_ROTATE_charred_S.svg
```

### Optional: Remove Charred Knob Widgets from plugin.hpp

If you don't want the charred knob widgets in the codebase, remove lines 350-440 in `src/plugin.hpp`:
- Remove the comment block "// CHARRED KNOBS (for testing alternative aesthetics)"
- Remove `struct ShapetakerKnobCharredSmall { ... };`
- Remove `struct ShapetakerKnobCharredMedium { ... };`

Then run `make clean && make -j4` to rebuild.

## Current Knob Mapping:

| Control | Original | Test (Charred) |
|---------|----------|----------------|
| Freq V/Z | ShapetakerKnobOscilloscopeMedium | ShapetakerKnobCharredMedium |
| Fine V/Z | ShapetakerKnobOscilloscopeSmall | ShapetakerKnobCharredSmall |
| Crossfade | ShapetakerKnobOscilloscopeMedium | ShapetakerKnobCharredMedium |
| Shape V/Z | ShapetakerKnobOscilloscopeSmall | ShapetakerKnobCharredSmall |

All comments marked with "(TESTING CHARRED)" can also be removed when reverting.

---

## Reference Photos Used:
The improved SVGs were designed based on the reference photos showing:
- Hex-nut shaped outer rim with beveled edges
- Metallic chrome/gunmetal finish with highlights and shadows
- Stepped circular layers creating depth
- Crisp white indicator line on top surface
