Shapetaker — Context for Agents and Contributors

This repository contains a VCV Rack 2 plugin bundle named "shapetaker" — a collection of musical modules with custom UI and DSP. This document orients AI agents and human contributors to the stack, layout, and conventions so you can make precise, minimal changes.

Sections
- Stack & Build
- Repository Layout
- Modules Overview
- Shared UI Components
- Data & Assets
- Conventions & Notes
- Ideas / TODOs

Stack & Build
- Framework: VCV Rack 2 SDK (`rack.hpp`, NanoVG UI, DSP utils)
- Language: C++ (C++17 via Rack toolchain)
- Build: Makefile includes `plugin.mk`. Set `RACK_DIR` to your Rack-SDK path.
  - Example: `export RACK_DIR=../Rack-SDK && make`
  - Distributables include `res/` and `LICENSE*`.
- Plugin metadata: `plugin.json`

Repository Layout
- `src/` — module implementations and shared widgets
  - `plugin.cpp` — registers all module models
  - `plugin.hpp` — shared custom widgets (knobs, switches, ports)
  - `clairaudient.cpp` — dual sigmoid oscillator (stereo)
  - `chiaroscuro.cpp` — stereo VCA with sidechain-driven distortion
  - `fatebinder.cpp` — chaotic LFO (strange attractor)
  - `involution.cpp` — dual morphing filters with feedback/shimmer
  - `evocation.cpp` — gesture-based envelope with multi-speed outputs
  - `incantation.cpp` — utility module
  - `transmutation.cpp` — dual chord sequencer with 8×8 matrix & chord packs
- `res/` — SVG panels, knobs, switches, LEDs, meters
- `chord_packs/` — JSON chord packs grouped by musical key
- `plugin.json` — plugin/module metadata
- `Makefile` — Rack SDK build integration

Modules Overview
- Clairaudient: Dual sigmoid oscillator, morphing waveforms, stereo crossfade, polyphony up to 6 voices.
- Chiaroscuro: Stereo VCA + sidechain-controlled distortion engine; custom large VU.
- Fatebinder: Chaotic LFO with chaos/order morphing.
- Involution: Dual 6th‑order morphing filters with cross-feedback, shimmer, stereo effects.
- Evocation: Gesture/touch-strip envelope recorder, four speeds, looping, invert, gate output.
- Incantation: Utility functionality (see source for details).
- Transmutation: Alchemical chord “Transmutation” — dual sequencer with 8×8 matrix, symbol-to-chord mapping, chord packs, internal/external clocking, A/B modes (independent/harmony/lock), poly CV/gate outputs.

Shared UI Components
- Defined in `src/plugin.hpp` and reused across modules:
  - `ShapetakerKnob*` (Large/Medium/Small + Oscilloscope variants)
  - `ShapetakerOscilloscopeSwitch` (two-state toggle)
  - `ShapetakerBNCPort` (custom port SVG)
  - `ShapetakerAttenuverterOscilloscope` (attenuverter-style knob)
- Many modules also include local, purpose-built widgets (e.g., VU meters, touch strip, matrix, jewel LEDs) drawn with NanoVG.

Data & Assets
- Chord packs: `chord_packs/<KEY>/<pack>.json` with fields like `name`, `key`, `description`, `chords[]` (each with `name`, `intervals[]` in semitones, optional `preferredVoices`, `category`).
- Transmutation loads packs (via Jansson) and randomizes symbol→chord assignment; falls back to a built-in basic pack when needed.
- Panels/controls: SVGs in `res/` are used by custom Rack widgets for consistent branding (teal/purple accents, jewel LEDs, vintage switches).

Conventions & Notes
- Polyphony: Commonly up to 6 voices; set output channel counts explicitly per process() frame.
- Voltages: CV is V/Oct with C4 = 0 V; gates use 10 V high by default.
- Clocking: Sequencers support external clock/reset inputs; internal BPM with multipliers is available where implemented.
- Triggers: Uses Rack `dsp::SchmittTrigger`/`PulseGenerator` patterns for robust edge detection and gates.
- UI: NanoVG drawing; larger/high-res custom canvases where readability matters (e.g., Transmutation matrix).
- Style: Favor small, focused changes; match existing patterns and naming. Keep new widgets consistent with existing look/feel.

Ideas / TODOs (lightweight)
- Documentation: Fill `README.md` with a user-facing overview and link module manuals; add per-module docs (e.g., in `docs/`).
- Metadata polish: Update `plugin.json` `sourceUrl` and any placeholder manual URLs.
- Transmutation: Centralize jewel LED styling (avoid duplicate widget variants) and verify chord pack discovery paths.
- Testing: Add simple Rack patches for smoke-tests and example usage per module.
- Build notes: Optionally include notes for Rack’s plugin build system versions you target.

Ownership & Contact
- Author: Josh Panzarella (`plugin.json` contains author metadata).

