# jx8p: der JX-8P von FM1-Electro als Baustein (docs/adr/0046). Bindet engine/jx8p.c und Co. ein (keine Kopie), mit
# denselben Schaltern wie der Host-Build im Hauptordner (make test-jx), und prüft ihn über baustein.h.
#   make test         alle Presets bitgleich mit dem Kern und mit ref/sums.txt, WAV der eigenen Presets nach ../build/jx8p
#   make ref          ref/sums.txt und ref/sums4.txt neu schreiben (nur nach einer gewollten Klangänderung im Kern)
#   make last         Rechenzeit je Stimme auf diesem Rechner
#   make pool         Pool-Kern der Firmware JX-8P solo (varianten/jx8p/engine/jx.c) gegen engine/jx8p.c, bitgleich (Teil von test)
#   make eigene       jx8p_eigene.c aus den iPhone-Presets neu erzeugen (Dropbox, tools/eigene2c.py)
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
TOP := ../..
CPPFLAGS += -DFM1_HOST_TEST -DFM1_OPEN_WEB -DEL_SYNTH_JX=1 -I. -I.. -I$(TOP)/engine -I$(TOP)/sdk_include \
            -I$(TOP)/third_party/daisy808 -D_DEFAULT_SOURCE
KERN := $(TOP)/engine/jx8p.c $(TOP)/engine/jx8p_names.c $(TOP)/engine/jx8p_presets.c $(TOP)/engine/chords.c
SRC := jx8p_bs.c jx8p_eigene.c $(KERN)
HDR := jx8p_bs.h jx8p_eigene.h ../baustein.h $(wildcard $(TOP)/engine/jx8p*.h) $(TOP)/engine/chords.h
BUILD := ../build/jx8p

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
	$(CC) $(CPPFLAGS) $(CFLAGS) -DJX8P_BS_VOICES=4 -o $@ $(SRC) test/test.c -lm

# JX-8P solo rechnet mit dem Pool-Kern aus fm1-synths (ADR 0046): mit einem Part muss er bitgleich mit engine/jx8p.c sein
V := ../varianten/jx8p
GS := ../varianten/gemeinsam/shared
POOLINC := -I$(V)/engine -I$(GS) -I$(GS)/dsp -I$(GS)/fx -I$(GS)/third_party/fm1_open
pool: $(BUILD)/pool
	$(BUILD)/pool

$(BUILD)/pool: test/pool.c test/pool_fm1.c $(V)/engine/jx.c $(V)/engine/jx_presets.c $(V)/engine/jx_eigene.c jx8p_eigene.c $(KERN) $(HDR)
	@mkdir -p $(BUILD)/pool_o
	$(CC) $(CFLAGS) -w -DFM1_HOST_TEST $(POOLINC) -c test/pool_fm1.c -o $(BUILD)/pool_o/a.o
	$(CC) $(CFLAGS) -w -DFM1_HOST_TEST $(POOLINC) -c $(V)/engine/jx.c -o $(BUILD)/pool_o/jx.o
	$(CC) $(CFLAGS) -w -DFM1_HOST_TEST $(POOLINC) -c $(V)/engine/jx_presets.c -o $(BUILD)/pool_o/jxp.o
	$(CC) $(CFLAGS) -w -DFM1_HOST_TEST $(POOLINC) -c $(V)/engine/jx_eigene.c -o $(BUILD)/pool_o/jxe.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test/pool.c jx8p_eigene.c $(KERN) $(BUILD)/pool_o/a.o $(BUILD)/pool_o/jx.o $(BUILD)/pool_o/jxp.o $(BUILD)/pool_o/jxe.o -lm
