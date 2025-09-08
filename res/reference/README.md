Shapetaker Panel Placement Reference
-----------------------------------

This folder contains lightweight, Inkscape‑friendly references to help line up control centers and keep spacing consistent across modules. Import these into your panel SVGs as a temporary layer and align your art to the center guides, then remove the guides before exporting.

Contents
- Clairaudient_placements.svg — full module outline with center markers for every control used by the Clairaudient panel (matching the ids used by the code).
- Transmutation_ids.md — list of element ids that the Transmutation module reads from the panel SVG. Ensure these ids exist and are centered on your intended control centers.

Tips
- Keep your “placement circle” (the red dot/circle) centered over the intended control center. The code reads `cx,cy` for `<circle>/<ellipse>` and the center of `<rect>` (`x + width/2`, `y + height/2`).
- Use consistent sizes for your center guides (e.g., 1.0–1.5 mm radius, no fill, contrasting stroke).
- Once aligned, you can hide or remove the placement layer.
