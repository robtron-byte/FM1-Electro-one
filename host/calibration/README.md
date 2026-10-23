# 808 calibration against recordings

Tools used to fit the 808 voices in `engine/tr808.c` to recordings of a
real TR-808 (single hits with digital silence in between).

1. `make build/hit_render` renders one hit with given knob and calibration values.
2. `python3 host/calibration/segment_kit.py kit.wav kit_seg.npy` finds the hits.
3. `KIT_WAV=kit.wav KIT_SEG=kit_seg.npy python3 host/calibration/fit_bd.py 400`
   fits the bass drum (indices of the bass drum hits are set in `IDX`);
   `fit_misc.py cb cp rs ch oh` fits cowbell, clap, rim shot and hats.
   `... eval` prints the current match without fitting.

The hit indices in the scripts belong to the "808 Basics Kit A" recording
used for the current calibration. The cymbal and snare have their own
tools (`host/cymbal_check.c` + `host/analyze_cymbal.py`,
`host/snare_check.c` + `host/analyze_snare.py`).
