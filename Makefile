# Host build: engine tests and WAV demos (the device build uses build.bat).
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -Iengine -Iui -Ifm1 -Ithird_party/daisy808 -Isdk_include

ENGINE = engine/transport.c engine/drums.c engine/drum_factory.c engine/electro.c \
         engine/tr808.c engine/mks.c engine/mixbus.c engine/mks_presets.c engine/synth_track.c engine/fx.c \
         engine/synth.c engine/jp6.c engine/jp6_presets.c engine/dist.c engine/po12.c \
         engine/arp.c

build/render_demo: host/render_demo.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/render_demo.c $(ENGINE) -lm

UI = ui/gfx.c ui/el_ui.c ui/el_editor.c fm1/el_store.c

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
	clang $(WASM_FLAGS) -o $@ host/web/sim.c host/web/libc.c $(ENGINE) ui/gfx.c ui/el_ui.c ui/el_editor.c fm1/el_store.c

build/web/fm1-electro-simulator.html: build/web/fm1sim.wasm host/web/index.html tools/build_web.py
	python3 tools/build_web.py build/web/fm1sim.wasm host/web/index.html $@

test: build/render_demo build/ui_sim build/synth_test build/jp6_test
	@mkdir -p build/wav build/wav_jp6 build/screens
	./build/synth_test build/wav > /dev/null
	./build/jp6_test build/wav_jp6 | grep "^strings hpf\|FAILED"
	./build/render_demo build/wav | grep -E "^(timing|midi clock|punch|dist send|tape|punch together|punch colour)"
	./build/ui_sim build/screens | grep "^store\|^engine\|^dist\|^comp\|^options\|^slots\|^punch keys\|^steprec\|^rec modes\|^arp\|^layout\|^editor\|^sounds"
	@echo "host tests ok"

clean:
	rm -rf build

.PHONY: demo test screens presets soundcheck clean web
