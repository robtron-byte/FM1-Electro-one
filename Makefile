# Host build: engine tests and WAV demos (the device build uses build.bat).
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -Iengine -Iui -Ifm1 -Ithird_party/daisy808 -Isdk_include

ENGINE = engine/transport.c engine/drums.c engine/drum_factory.c engine/electro.c \
         engine/tr808.c engine/mks.c engine/mixbus.c engine/mks_presets.c engine/synth_track.c engine/fx.c

build/render_demo: host/render_demo.c $(ENGINE) $(wildcard engine/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/render_demo.c $(ENGINE) -lm

UI = ui/gfx.c ui/el_ui.c fm1/el_store.c

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

demo: build/render_demo
	@mkdir -p build/wav
	./build/render_demo build/wav

build/synth_test: host/synth_test.c engine/mks.c engine/mks_presets.c engine/mks.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ host/synth_test.c engine/mks.c engine/mks_presets.c -lm

test: build/render_demo build/ui_sim build/synth_test
	@mkdir -p build/wav build/screens
	./build/synth_test build/wav > /dev/null
	./build/render_demo build/wav | grep -E "^(timing|midi clock)"
	./build/ui_sim build/screens | grep "^store"
	@echo "host tests ok"

clean:
	rm -rf build

.PHONY: demo test screens presets soundcheck clean
