# dk808: Host-Test gegen die Referenz-Renders des Swift-Kerns, einmal mit allen 17 Instrumenten, einmal mit der Auswahl
# der FM1-Electro-Firmware (weggelassene Instrumente müssen stumm sein).
#   make test                 Kennwerte aus ref/manifest.json
#   make test WAV=<Ordner>    zusätzlich jedes Sample gegen die Referenz-WAVs (Dropbox app/reference-renders)
CC ?= cc
# -ffp-contract=off: keine zusammengezogenen Multiplikationen/Additionen, gleiche Rundung wie der Swift-Kern
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -ffp-contract=off
CFLAGS += -I.. -I. -D_DEFAULT_SOURCE
BUILD := ../build/dk808
WAV ?= $(wildcard ref/wav)
# float-Fassung wie im Gerät: Konstanten einfach genau (tools/build.py übersetzt Bausteine mit -cl-single-precision-constant)
SP := $(shell $(CC) -cl-single-precision-constant -x c -c /dev/null -o /dev/null 2>/dev/null && echo -cl-single-precision-constant || echo -fsingle-precision-constant)

.PHONY: test float
test: $(BUILD)/test $(BUILD)/test_fm1 $(BUILD)/test_bs $(BUILD)/test_bs_f float
	$(BUILD)/test ref $(BUILD) $(WAV)
	$(BUILD)/test_fm1 ref $(BUILD)/fm1 $(WAV)
	$(BUILD)/test_bs
	$(BUILD)/test_bs_f

$(BUILD)/test: dk808.c dk808.h test/test.c ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ dk808.c test/test.c -lm

$(BUILD)/test_fm1: dk808.c dk808.h test/test.c ../baustein.h
	@mkdir -p $(BUILD)/fm1
	$(CC) $(CFLAGS) -DDK808_INSTRUMENTS=DK808_MASK_FM1_ELECTRO -o $@ dk808.c test/test.c -lm

# float gegen double (docs/adr/0050): zwei Programme (DK808_FLOAT 0 und 1) rendern alle Instrumente, ein Skript vergleicht
float: $(BUILD)/render_d $(BUILD)/render_f
	$(BUILD)/render_d > $(BUILD)/all_d.raw
	$(BUILD)/render_f > $(BUILD)/all_f.raw
	python3 test/float_cmp.py $(BUILD)/all_d.raw $(BUILD)/all_f.raw -60

$(BUILD)/render_d: dk808.c dk808.h test/render_all.c ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test/render_all.c dk808.c -lm

$(BUILD)/render_f: dk808.c dk808.h test/render_all.c ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DDK808_FLOAT=1 $(SP) -o $@ test/render_all.c dk808.c -lm

# Gerüst-Adapter (dk808_bs.h), double und float
$(BUILD)/test_bs: dk808.c dk808.h dk808_bs.c dk808_bs.h test/bs.c ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test/bs.c dk808_bs.c dk808.c -lm

$(BUILD)/test_bs_f: dk808.c dk808.h dk808_bs.c dk808_bs.h test/bs.c ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DDK808_FLOAT=1 $(SP) -o $@ test/bs.c dk808_bs.c dk808.c -lm
