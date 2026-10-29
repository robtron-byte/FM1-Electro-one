# Host build: engine tests and WAV demos (the device build uses build.bat).
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -Iengine -Iui -Ifm1 -Ithird_party/daisy808 -Isdk_include

ENGINE = engine/transport.c engine/drums.c engine/drum_factory.c engine/electro.c \
         engine/tr808.c engine/mks.c engine/mixbus.c engine/mks_presets.c engine/synth_track.c engine/fx.c \
         engine/synth.c engine/jp6.c engine/jp6_presets.c engine/dist.c engine/po12.c \
         engine/arp.c

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
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/hit_render.c engine/tr808.c engine/po12.c -lm

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
	  grep -E "^(timing|midi clock|punch|dist send|tape|punch together|punch colour|808 tune|808 key)" build/render_demo.log; \
	  [ $$s -eq 0 ] && ! grep -q "FAIL" build/render_demo.log
	./build/ui_sim build/screens > build/ui_sim.log; s=$$?; \
	  grep "^store\|^engine\|^dist\|^comp\|^options\|^slots\|^punch keys\|^steprec\|^rec modes\|^arp\|^layout\|^editor\|^sounds" build/ui_sim.log; \
	  [ $$s -eq 0 ] && ! grep -q "FAIL" build/ui_sim.log
	@echo "host tests ok"

clean:
	rm -rf build

.PHONY: demo test screens presets soundcheck clean web kick-ab metal-ab clap-ab gate-ab

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
