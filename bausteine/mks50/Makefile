# mks50: der MKS-50 von FM1-Electro als Baustein (docs/adr/0047). Bindet engine/mks.c und Co. ein (keine Kopie), mit
# denselben Schaltern wie der Host-Build im Hauptordner (EL_SYNTH_JX=0 wie synth_test), und prüft ihn über baustein.h.
#   make test         alle Presets bitgleich mit dem Kern und mit ref/sums.txt, WAV der eigenen Presets nach ../build/mks50
#   make ref          ref/sums.txt und ref/sums4.txt neu schreiben (nur nach einer gewollten Klangänderung im Kern)
#   make last         Rechenzeit je Stimme auf diesem Rechner
#   make eigene       mks50_eigene.c aus den iPhone-Presets neu erzeugen (Dropbox, tools/eigene2c.py)
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
TOP := ../..
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -DEL_SYNTH_JX=0 -I. -I.. -I$(TOP)/engine -I$(TOP)/sdk_include \
            -I$(TOP)/third_party/daisy808 -D_DEFAULT_SOURCE
KERN := $(TOP)/engine/mks.c $(TOP)/engine/mks_presets.c $(TOP)/engine/chords.c
SRC := mks50_bs.c mks50_eigene.c $(KERN)
HDR := mks50_bs.h mks50_eigene.h ../baustein.h $(TOP)/engine/mks.h $(TOP)/engine/el_config.h $(TOP)/engine/chords.h
BUILD := ../build/mks50

.PHONY: test ref last eigene
test: $(BUILD)/test $(BUILD)/test_4
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
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMKS50_BS_VOICES=4 -o $@ $(SRC) test/test.c -lm
