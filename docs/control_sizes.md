Control Sizing (mm-first)

Purpose: Keep UI visuals, panel art, and hardware layouts aligned by sizing controls in millimeters. Use `mm2px()` in code when setting widget sizes.

House Sizes (mm)
- Ports (3.5 mm): 6.5 hole, 10 washer OD, 12.7 grid spacing
- Toggle (vintage look): 7 × 14 footprint, 6.3–6.5 hole, ~10 nut OD
- Buttons (tact + cap): 8–10 cap OD, 12 spacing
- Attenuverter knobs: 10 mm cap OD (compact control)
- LEDs: 3 or 5 dome; “jewel” 10–12 lens
- Knobs (pot cap OD): 16 / 20 / 24 / 30 tiers
  - Attenuverters (small trim knobs): 10 mm target
- Sliders: 45 mm travel primary; 30 mm secondary (slot ≈ travel + 5)

Keep-Out & Spacing (mm)
- Jack center → tall control edge: ≥ 10
- Nut keep-out ring: ≥ 2.5 around washer OD
- Panel edge clearance: ≥ 2–3 from nut/washer
- Knob centers: ≥ knob OD (tight) or OD + 2–3 (comfortable)

Code Conventions
- Set widget sizes from mm: `box.size = mm2px(Vec(W_mm, H_mm))`
- Use panel markers (`id`ed shapes) for positions and convert: `mm2px(center)`
- Avoid pixel constants for sizes to keep UI and hardware consistent

Current Module Defaults
- Vintage toggle (`ShapetakerVintageToggleSwitch`): 7 × 14 mm (src/plugin.hpp)
- Chiaroscuro toggles: explicitly set to 7 × 14 mm to match global

Conversion
- Rack uses approx: px ≈ mm × 96 / 25.4 (≈ 3.78 px per mm)
