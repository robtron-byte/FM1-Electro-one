# Host build: engine tests and WAV demos (the device build uses build.bat).
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -Iengine -Iui -Ifm1 -Ithird_party/daisy808 -Isdk_include

ENGINE = engine/transport.c engine/drums.c engine/drum_factory.c engine/electro.c \
         engine/tr808.c engine/mks.c engine/mixbus.c engine/mks_presets.c engine/synth_track.c engine/fx.c \
         engine/synth.c engine/jp6.c engine/jp6_presets.c engine/extra_presets.c engine/dist.c engine/po12.c \
         engine/arp.c engine/dr_fixed.c

build/fmt_test: host/fmt_test.c ui/el_fmt.c ui/el_fmt.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-format-security -o $@ host/fmt_test.c ui/el_fmt.c

build/render_demo: host/render_demo.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/render_demo.c $(ENGINE) -lm

UI = ui/gfx.c ui/el_ui.c ui/el_editor.c ui/el_fmt.c fm1/el_store.c

build/ui_sim: host/ui_sim.c $(ENGINE) $(UI) $(wildcard engine/*.h ui/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/ui_sim.c $(ENGINE) $(UI) -lm

screens: build/ui_sim
	@mkdir -p build/screens
	./build/ui_sim build/screens
	python3 host/panel_png.py build/screens

build/soundcheck: host/soundcheck.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/soundcheck.c $(ENGINE) -lm

soundcheck: build/soundcheck
	@mkdir -p build/sc
	./build/soundcheck build/sc
	python3 host/analyze_808.py build/sc

presets:
	python3 tools/convert_presets.py ../Mks50-iOS-/Shared/Model/FactoryPresets.swift engine/mks_presets.c
	python3 tools/convert_jp6_presets.py ../jp6-ios/Shared/Model/FactoryPresets.swift engine/jp6_presets.c

demo: build/render_demo
	@mkdir -p build/wav
	./build/render_demo build/wav

build/synth_test: host/synth_test.c engine/mks.c engine/mks_presets.c engine/mks.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/synth_test.c engine/mks.c engine/mks_presets.c -lm

build/jp6_test: host/jp6_test.c engine/jp6.c engine/jp6_presets.c engine/jp6.h engine/dist.c engine/dist.h engine/mks.c engine/mks_presets.c engine/mks.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/jp6_test.c engine/jp6.c engine/jp6_presets.c engine/dist.c engine/mks.c engine/mks_presets.c -lm

build/hit_render: host/hit_render.c engine/tr808.c engine/tr808.h engine/po12.c engine/po12.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/hit_render.c engine/tr808.c engine/dr_fixed.c engine/po12.c -lm

# Own sounds (docs/adr/0027): tools/extra_presets.py writes engine/extra_presets.c.
build/preset_probe: host/preset_probe.c engine/synth.c engine/mks.c engine/mks_presets.c engine/jp6.c engine/jp6_presets.c engine/extra_presets.c engine/dist.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/preset_probe.c engine/synth.c engine/mks.c engine/mks_presets.c engine/jp6.c engine/jp6_presets.c engine/extra_presets.c engine/dist.c -lm

# Which bass sounds sit on the 808 kick (40-90 Hz), at the bass root C2.
bass-check: build/preset_probe
	@mkdir -p build/probe && rm -f build/probe/*.f32
	./build/preset_probe build/probe 36 2.5 --area=3
	python3 tools/preset_check.py build/probe

build/po_render: host/po_render.c engine/po12.c engine/po12.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/po_render.c engine/po12.c -lm

WASM_FLAGS = --target=wasm32 -O2 -ffreestanding -nostdlib -fvisibility=hidden -isystem host/web/include \
             -DFM1_HOST_TEST -DFM1_OPEN_WEB -Iengine -Iui -Ifm1 -Isdk_include -Ithird_party/daisy808 -std=c99 \
             -Wall -Wno-unused-parameter -Wno-misleading-indentation -Wno-unknown-pragmas \
             -Wl,--no-entry -Wl,--export-dynamic -Wl,--initial-memory=4194304 -Wl,-z,stack-size=262144

# Browser panel simulator (needs clang with the wasm32 target and wasm-ld).
web: build/web/fm1-electro-simulator.html

build/web/fm1sim.wasm: host/web/sim.c host/web/libc.c $(ENGINE) $(UI) $(wildcard engine/*.h ui/*.h)
	@mkdir -p build/web
	clang $(WASM_FLAGS) -o $@ host/web/sim.c host/web/libc.c $(ENGINE) ui/gfx.c ui/el_ui.c ui/el_editor.c ui/el_fmt.c fm1/el_store.c

build/web/fm1-electro-simulator.html: build/web/fm1sim.wasm host/web/index.html tools/build_web.py
	python3 tools/build_web.py build/web/fm1sim.wasm host/web/index.html $@

test: build/render_demo build/ui_sim build/synth_test build/jp6_test build/fmt_test
	@mkdir -p build/wav build/wav_jp6 build/screens
	./build/fmt_test
	./build/synth_test build/wav > /dev/null
	@# Each tool's output goes to a log first: a FAIL line or a failing exit
	@# code stops make (a pipe into grep would hide both).
	./build/jp6_test build/wav_jp6 > build/jp6_test.log; s=$$?; grep "^strings hpf\|FAILED" build/jp6_test.log; \
	  [ $$s -eq 0 ] && ! grep -q "FAIL" build/jp6_test.log
	./build/render_demo build/wav > build/render_demo.log; s=$$?; \
	  grep -E "^(two cores|timing|midi clock|punch|dist send|comp mix|dist comb|tape|punch together|punch colour|808 tune|808 key)" build/render_demo.log; \
	  [ $$s -eq 0 ] && ! grep -q "FAIL" build/render_demo.log
	./build/ui_sim build/screens > build/ui_sim.log; s=$$?; \
	  grep "^store\|^extras\|^strings\|^locks\|^comp mix\|^engine\|^dist\|^comp\|^options\|^slots\|^punch keys\|^steps\|^steprec\|^rec modes\|^arp\|^layout\|^editor\|^sounds" build/ui_sim.log; \
	  [ $$s -eq 0 ] && ! grep -q "FAIL" build/ui_sim.log
	@echo "host tests ok"

clean:
	rm -rf build

.PHONY: demo test screens presets soundcheck clean web bass-check kick-ab metal-ab clap-ab gate-ab metal-fixed-check ch-ab dist-fixed-check mb-fixed-check

# The fixed-point mix bus (engine/mixbus.c, MB_FIXED, own platform) against
# the float one: MASTER FX on, off, and hot into the limiter; within -60 dB.
mb-fixed-check: host/mb_fixed_check.c host/fixed_cmp.py engine/mixbus.c engine/mixbus.h
	@mkdir -p build
	@for fx in 0 1; do $(CC) $(CPPFLAGS) $(CFLAGS) -w -DMB_FIXED=$$fx -o build/mbc_$$fx host/mb_fixed_check.c engine/mixbus.c -lm && ./build/mbc_$$fx build/mbc_$$fx.raw || exit 1; done
	@python3 host/fixed_cmp.py build/mbc_0.raw build/mbc_1.raw 3 -60 "FX on" "FX off" "hot, limiter"

# The MKS-50 oscillators' grid fast path (engine/mks.c, MKS_CHOP_GRID) against
# chop_general: every MKS-50 sound, a chord low and high; within -90 dB.
chop-grid-check: host/chop_grid_check.c host/fixed_cmp.py engine/mks.c engine/mks.h engine/mks_presets.c
	@mkdir -p build
	@for g in 0 1; do $(CC) $(CPPFLAGS) $(CFLAGS) -w -DMKS_CHOP_GRID=$$g -o build/cgc_$$g host/chop_grid_check.c engine/mks.c engine/mks_presets.c -lm && ./build/cgc_$$g > build/cgc_$$g.raw || exit 1; done
	@python3 host/fixed_cmp.py build/cgc_0.raw build/cgc_1.raw 56 -90

# The fixed-point distortion (engine/dist.c, DIST_FIXED, own platform) against
# the float one: every type, five settings, each within -60 dB.
dist-fixed-check: host/dist_fixed_check.c host/dist_fixed_cmp.py engine/dist.c engine/dist.h
	@mkdir -p build
	@for fx in 0 1; do $(CC) $(CPPFLAGS) $(CFLAGS) -w -DDIST_FIXED=$$fx -o build/dfc_$$fx host/dist_fixed_check.c engine/dist.c -lm && ./build/dfc_$$fx build/dfc_$$fx.raw || exit 1; done
	@python3 host/dist_fixed_cmp.py build/dfc_0.raw build/dfc_1.raw

# The fixed-point cymbal and hats (engine/tr808_metal_fixed.h, own platform)
# against the float 808: single hits with several knob settings, each must
# stay within -40 dB of the float model (docs/adr/0022).
METAL_CASES = cy cy_acc cy_dark cy_bright oh oh_acc oh_long oh_tune ch ch_acc ch_tune oh_choke bd bd_acc bd_short bd_long bd_tune bd_tone bd_punch bd_drive bd_retrig sd sd_acc sd_tone sd_early lt ht_acc lt_conga ht_tune rs rs_clave cp cp_acc cp_maracas cb cb_tune ch_drive cp_drive zp zp_tone zp_sweep zp_retrig pu pu_ring bd_p7 bd_m12 sd_p5 rs_m3 cb_p4 cb_m5 zp_p6 pu_m7
metal-fixed-check: host/metal_fixed_check.c host/metal_fixed_cmp.py $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/mfc_0 build/mfc_1
	@for fx in 0 1; do $(CC) $(CPPFLAGS) $(CFLAGS) -w -DTR808_FIXED=$$fx -o build/mfc_bin_$$fx host/metal_fixed_check.c $(ENGINE) -lm && ./build/mfc_bin_$$fx build/mfc_$$fx || exit 1; done
	@for c in $(METAL_CASES); do echo "$$c"; python3 host/metal_fixed_cmp.py build/mfc_0/$$c.wav build/mfc_1/$$c.wav 0:1 || exit 1; done

# Bass drum tail variants for listening: A through the master (old), B dry
# tail, C with the octave body (current), D plus a sinking pitch.
KICK_VARIANTS = A:-DMB_KICK_THROUGH_MASTER:-DTR808_BD_BODY=0.0f B::-DTR808_BD_BODY=0.0f C:: D::-DTR808_BD_SINK=0.06f
kick-ab: host/kick_ab.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@for v in $(KICK_VARIANTS); do n=$${v%%:*}; f=$${v#*:}; f=$$(echo $$f | tr ':' ' '); \
	  $(CC) $(CPPFLAGS) $(CFLAGS) $$f -o build/kick_ab_$$n host/kick_ab.c $(ENGINE) -lm && \
	  ./build/kick_ab_$$n build/wav/kick_$$n.wav $${DECAY:-100} && echo "build/wav/kick_$$n.wav"; done

# Metal voices (hats, cymbal, cowbell) for listening: A steady oscillators,
# exponential open hat (before), B wandering oscillators, C open hat with a
# linear discharge, D both (the default, docs/adr/0009). E: D with the
# cowbell fitted to the recording (TR808_CB_MODEL 1, docs/adr/0012).
METAL_VARIANTS = A:-DTR808_METAL_JITTER=0:-DTR808_OH_LINEAR=0 B:-DTR808_OH_LINEAR=0 C:-DTR808_METAL_JITTER=0 D: E:-DTR808_CB_MODEL=1
metal-ab: host/metal_ab.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@for v in $(METAL_VARIANTS); do n=$${v%%:*}; f=$${v#*:}; f=$$(echo $$f | tr ':' ' '); \
	  $(CC) $(CPPFLAGS) $(CFLAGS) $$f -o build/metal_ab_$$n host/metal_ab.c $(ENGINE) -lm && \
	  ./build/metal_ab_$$n build/wav/metal_$$n.wav && echo "build/wav/metal_$$n.wav"; done

# Closed hat (docs/adr/0014): A as 8W8 (before), B fitted to four closed hats
# of a real TR-808 (TR808_CH_FIT 1, host/calibration/fit_ch.py). Through
# host/metal_ab.c (bars 1-4 the hats).
ch-ab: host/metal_ab.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@for v in A:-DTR808_CH_FIT=0 B:-DTR808_CH_FIT=1; do n=$${v%%:*}; f=$${v#*:}; \
	  $(CC) $(CPPFLAGS) $(CFLAGS) $$f -o build/ch_ab_$$n host/metal_ab.c $(ENGINE) -lm && \
	  ./build/ch_ab_$$n build/wav/ch_$$n.wav && echo "build/wav/ch_$$n.wav"; done

# Hand clap for listening (docs/adr/0008): A as before, B the circuit's
# envelope, C also the circuit's filter and rectified noise, D fitted to
# the recordings (host/calibration/fit_clap.py; the default).
CLAP_VARIANTS = A:-DTR808_CP_MODEL=0 B:-DTR808_CP_MODEL=1 C:-DTR808_CP_MODEL=2 D:-DTR808_CP_MODEL=3
clap-ab: host/clap_ab.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@for v in $(CLAP_VARIANTS); do n=$${v%%:*}; f=$${v#*:}; f=$$(echo $$f | tr ':' ' '); \
	  $(CC) $(CPPFLAGS) $(CFLAGS) $$f -o build/clap_ab_$$n host/clap_ab.c $(ENGINE) -lm && \
	  ./build/clap_ab_$$n build/wav/clap_$$n.wav && echo "build/wav/clap_$$n.wav"; done

# Gated reverb for listening (docs/adr/0013): clap D with a reverb send of
# 56 (the old factory value), the reverb as before (A), dense taps (B), a
# gated feedback network (C, the default), plus dry.
gate-ab: host/clap_ab.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@for v in A:-DFX_GATE_DENSE=0 B:-DFX_GATE_DENSE=1 C:-DFX_GATE_DENSE=2; do n=$${v%%:*}; f=$${v#*:}; \
	  $(CC) $(CPPFLAGS) $(CFLAGS) $$f -o build/gate_ab_$$n host/clap_ab.c $(ENGINE) -lm && \
	  ./build/gate_ab_$$n build/wav/gate_$$n.wav 56 && echo "build/wav/gate_$$n.wav"; done
	@./build/gate_ab_A build/wav/gate_dry.wav 0 && echo "build/wav/gate_dry.wav"

# The hall's modulation every frame (A, as before) or every 4th / 8th of its
# frames (B, C; FX_REVERB_LFO_EVERY, docs/adr/0019): the default groove with
# a big hall, then its tail; B and C compared with A.
hall-ab: host/hall_ab.c host/fixed_cmp.py $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@for v in A:1 B:4 C:8; do n=$${v%%:*}; f=$${v#*:}; \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -DFX_REVERB_LFO_EVERY=$$f -o build/hall_ab_$$n host/hall_ab.c $(ENGINE) -lm && \
	  ./build/hall_ab_$$n build/wav/hall_$$n.wav build/hall_$$n.raw && echo "build/wav/hall_$$n.wav"; done
	@python3 host/fixed_cmp.py build/hall_A.raw build/hall_B.raw 1 0 "B (every 4th)"; true
	@python3 host/fixed_cmp.py build/hall_A.raw build/hall_C.raw 1 0 "C (every 8th)"; true

# The master compressor as set from the factory (A) and as CLUB (B, COMP
# RATIO CLUB, docs/adr/0029): the default groove, 16 s.
comp-ab: host/comp_ab.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build/wav
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o build/comp_ab host/comp_ab.c $(ENGINE) -lm
	@./build/comp_ab build/wav/comp_A.wav 2 && ./build/comp_ab build/wav/comp_B.wav 8
