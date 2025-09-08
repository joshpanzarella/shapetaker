Transmutation Panel Ids (for auto-placement)
-------------------------------------------

Controls (read from `res/panels/Transmutation.svg`)

Buttons / UI
- `edit_a_btn`, `edit_b_btn`
- `main_screen` (matrix rectangle)
- `rest_btn` (or `rest_button`), `tie_btn` (or `tie_button`)
- `alchem_1` .. `alchem_12` (12 alchemical symbol button rects)

Knobs / Selectors
- `seq_a_length`
- `main_bpm`
- `clk_mult_select`
- `seq_b_length`
- `mode_switch` (vintage selector)

Transport buttons (momentary)
- `a_play_btn`, `a_stop_btn`, `a_reset_btn`
- `b_play_btn`, `b_stop_btn`, `b_reset_btn`

I/O (BNC ports)
- `a_clk_cv`, `a_reset_cv`, `a_play_cv`, `a_stop_cv`
- `seq_a_length_cv`
- `a_cv_out`, `a_gate_out`
- `b_clk_cv`, `b_reset_cv`, `b_play_cv`, `b_stop_cv`
- `seq_b_length_cv`
- `b_cv_out`, `b_gate_out`

Lights
- `seq_a_led`, `seq_b_led`

Notes
- For `<rect>`, the code uses the visual center: `(x + width/2, y + height/2)`.
- For `<circle>/<ellipse>`, the code uses `cx, cy`.
- Maintain ids and centers to keep the module placement aligned with your design.
