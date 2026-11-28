# jp6: der Jupiter-6 von FM1-Electro als Baustein (docs/adr/0048). Bindet engine/jp6.c und Co. ein (keine Kopie), mit
# denselben Schaltern wie der Host-Build im Hauptordner (wie jp6_test), und prüft ihn über baustein.h.
#   make test         alle Presets bitgleich mit dem Kern und mit ref/sums.txt, WAV der eigenen Presets nach ../build/jp6
#   make ref          ref/sums.txt und ref/sums4.txt neu schreiben (nur nach einer gewollten Klangänderung im Kern)
#   make last         Rechenzeit je Stimme auf diesem Rechner
#   make eigene       jp6_eigene.c aus den iPhone-Presets neu erzeugen (Dropbox, tools/eigene2c.py)
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
TOP := ../..
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -DEL_SYNTH_JX=0 -I. -I.. -I$(TOP)/engine -I$(TOP)/sdk_include \
            -I$(TOP)/third_party/daisy808 -D_DEFAULT_SOURCE
KERN := $(TOP)/engine/jp6.c $(TOP)/engine/jp6_presets.c $(TOP)/engine/dist.c $(TOP)/engine/chords.c
SRC := jp6_bs.c jp6_eigene.c $(KERN)
HDR := jp6_bs.h jp6_eigene.h ../baustein.h $(TOP)/engine/jp6.h $(TOP)/engine/dist.h $(TOP)/engine/el_config.h $(TOP)/engine/chords.h
BUILD := ../build/jp6

.PHONY: test ref last eigene pool
test: $(BUILD)/test $(BUILD)/test_4 pool
	@mkdir -p $(BUILD)/wav $(BUILD)/wav4
	$(BUILD)/test ref $(BUILD)/wav
	$(BUILD)/test_4 ref $(BUILD)/wav4

ref: $(BUILD)/test $(BUILD)/test_4
	@mkdir -p $(BUILD)/wav $(BUILD)/wav4
	$(BUILD)/test ref $(BUILD)/wav write
	$(BUILD)/test_4 ref $(BUILD)/wav4 write

last: $(BUILD)/test
	$(BUILD)/test last

eigene:
	python3 tools/eigene2c.py

$(BUILD)/test: $(SRC) $(HDR) test/test.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) test/test.c -lm

# Sparfassung mit 4 Stimmen (wie viele das Gerät schafft, misst die Solo-Firmware im Emulator)
$(BUILD)/test_4: $(SRC) $(HDR) test/test.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DJP6_BS_VOICES=4 -o $@ $(SRC) test/test.c -lm

# JP-6 solo rechnet mit dem Pool-Kern varianten/jp6/engine/jpp.c (bindet engine/jp6.c ein, ADR 0048): mit einem Part
# bitgleich mit jp6_render
pool: $(BUILD)/pool
	$(BUILD)/pool

$(BUILD)/pool: test/pool.c ../varianten/jp6/engine/jpp.c ../varianten/jp6/engine/jpp.h jp6_eigene.c $(HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) -I../varianten/jp6/engine $(CFLAGS) -o $@ test/pool.c ../varianten/jp6/engine/jpp.c jp6_eigene.c \
	  $(TOP)/engine/jp6_presets.c $(TOP)/engine/dist.c $(TOP)/engine/chords.c -lm
