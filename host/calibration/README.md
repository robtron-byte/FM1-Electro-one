# 808 calibration against recordings

Tools used to fit the 808 voices in `engine/tr808.c` to recordings of a
real TR-808 (single hits with digital silence in between).

1. `make build/hit_render` renders one hit with given knob and calibration values.
2. `python3 host/calibration/segment_kit.py kit.wav kit_seg.npy` finds the hits.
3. `KIT_WAV=kit.wav KIT_SEG=kit_seg.npy python3 host/calibration/fit_bd.py 400`
   fits the bass drum (indices of the bass drum hits are set in `IDX`);
   `fit_misc.py cb cp rs ch oh` fits cowbell, clap, rim shot and hats.
   `... eval` prints the current match without fitting.

4. Clap (model 3): build `build/hit_render_cp3` (hit_render with
   `-DTR808_CP_MODEL=3`), then `KIT_WAV=... KIT_SEG=... python3
   host/calibration/fit_clap.py 800` (or `eval`). It fits `tr808_cp_cal`
   to hits 10 and 12: fine envelope, the long tail and the spectrum over time.

5. Cowbell (model 1): build `build/hit_render_cb1` (`-DTR808_CB_MODEL=1`),
   then `fit_cowbell.py 800` (or `eval`), against hit 20. Hit 9, which
   `fit_misc.py cb` uses for model 0, is not a cowbell.

The hit indices in the scripts belong to the "808 Basics Kit A" recording
used for the current calibration. The cymbal and snare have their own
tools (`host/cymbal_check.c` + `host/analyze_cymbal.py`,
`host/snare_check.c` + `host/analyze_snare.py`).

PO-12 kit: `make build/po_render`, then `PO_WAV=po12.wav PO_SEG=po_seg.npy python3 host/calibration/fit_po12.py init.json 900 [sound indices]` (one hit per sound, sounds 1-16 in order, default knobs).
