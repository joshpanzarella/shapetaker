# Chimera — Shapetaker Morph Mixer (Draft)

This document captures the first-pass architecture for the Chimera morph mixer so we can align DSP, UI, and panel design before deep implementation.

## Concept

Chimera is a four-channel stereo mixer with per-channel morph sends that continuously crossfade between two modulation effect slots. The summed mix feeds a “glue” bus compressor inspired by the SSL G-Series, providing the cohesive polish common across the Shapetaker lineup. Gesture inputs and jewel metering keep the interface in-family with the rest of the plugin bundle. All audio I/O now follows the Rack polyphony spec (up to 8 voices); per-strip loops capture the summed voice mix so multichannel patches can be overdubbed just like mono rigs.

## HP, Layout, and Sections

| Section | Controls | Notes |
| --- | --- | --- |
| 4 stereo strips | Level (fader), Width/Pan, Morph Send, Loop Threshold, Tilt EQ, Loop Arm, Bus select (A/B/Both) | Each strip accepts stereo or dual-mono. When armed, the loop auto-records on amplitude threshold and replaces the live feed. |
| Morph Section | Slot A mode, Slot B mode, per-slot mini controls (Rate, Depth, Texture), Global Morph Mix (A⇄B), CV inputs | Slots default to chorus/ensemble and flanger/trem palettes; each slot can be reconfigured later. |
| Glue Section | Threshold, Ratio switch (2:1 / 4:1 / Crush), Attack, Release, HPF sidechain (Off/60/120), Dry/Wet, Makeup | Classic stereo bus compressor placed post mix. |
| Loop & Clock | Loop length switch (1/2/4 bars), BPM knob, run/source toggles, click enable + mix route, ext clock in + click out | Internal metronome or external clock define loop quantization; click can route to mix or its own output. |
| Routing & Aux | Ping-pong toggle for morph bus, Cross-feedback knob (Morph → Mix sidechain), Aux send output pair | Keeps space for future expander ideas. |

Module width target: **34 HP** (expanded from 30 HP to give the morph/glue/clock cluster an extra column). This yields enough horizontal room for four channel strips, loop controls, morph slots, and the shared clock/compressor blocks without overcrowding.

## Signal Flow Summary

```
Stereo Inputs (x4) → Channel Conditioning (tilt EQ + width) → Bus assign (A/B)
  ↘ Morph Send knob → Crossfade between Slot A/B → Slot processing (mod effects) → Morph Return mix

Bus A + Bus B + Morph Return → Mix bus → Glue Compressor → OUT L/R
Glue sidechain optionally fed by Morph Return (selectable).
```

- Channel morph send is *post-fader*, pre-bus assign, so Morph ambience follows the channel’s overall level.
- Slots run in parallel and feed a central morph return that can be blended globally between A and B.
- Gesture CV input can target: global Morph crossfade, Glue dry/wet, or a chosen channel Morph Send (selectable via menu).
- Loop engine auto-arms per channel and captures once amplitude exceeds the threshold; the loop length is quantized to the selected bar count and driven by the clock.
- Internal click/clock can be disabled, routed to mix, or driven from an external clock input; a dedicated click output jack mirrors the metronome pulse.

## Controls Per Strip

1. **Level fader** (VintageSliderLED)
2. **Pan/Width** knob (center detent)
3. **Morph Send** knob (0→Slot A, 1→Slot B)
4. **Loop Threshold** knob (sets amplitude trip point for auto-record)
5. **Tilt EQ** knob (amber/teal duo LED to show direction)
6. **Loop Arm** switch (Off / Arm)
7. **Bus assign** three-way toggle (A / A+B / B)

## Morph Slots

Each slot hosts a lightweight multimode processor:

| Slot | Modes | Controls |
| --- | --- | --- |
| A (“Argent”) | Chorus, Ensemble, Phasewash, Tape Mod Delay | Rate, Depth, Texture |
| B (“Aurum”) | Flanger, Trem-Pan, Short Diffusion, Shimmer Lite | Rate, Depth, Texture |

- Texture morphs internal algorithm detail (e.g., feedback amount, diffusion).
- Slots expose stereo CV inputs for Rate and Depth plus a shared Texture CV.

## Glue Compressor

- Threshold, Makeup, Mix (Dry/Wet)
- Ratio toggle: 2:1, 4:1, “Crush” (~10:1 soft knee)
- Attack (0.1–30 ms) and Release (0.05–1.2 s), Auto switch optional.
- HPF SC toggle (Off / 60 Hz / 120 Hz)
- Sidechain source selector: Mix only, Mix + Morph, Morph only (for special pumping effects).

## CV + I/O

- Stereo Inputs per channel (8 jacks)
- Morph Slot sends/returns exposed (optional) for external processing.
- Gesture CV input (assign menu)
- Glue SC input (external trigger)
- External clock input + dedicated click output
- Stereo Mix outputs + Morph return outputs.

## Implementation Phases

1. **Skeleton:** Channel data structures, morph bus routing, glue compressor stub, UI placeholders.
2. **DSP Core:** Implement modulation slots (start with chorus and flanger), compressor behaviour.
3. **UI Polish:** Panel SVG, jewel metering, menu items for assignments.
4. **Extended Features:** Additional slot modes, expander hooks.

This doc will expand as the module matures—feel free to edit once we start prototyping DSP details.
