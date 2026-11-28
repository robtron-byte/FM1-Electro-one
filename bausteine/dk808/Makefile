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

.PHONY: test
test: $(BUILD)/test $(BUILD)/test_fm1
	$(BUILD)/test ref $(BUILD) $(WAV)
	$(BUILD)/test_fm1 ref $(BUILD)/fm1 $(WAV)

$(BUILD)/test: dk808.c dk808.h test/test.c ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ dk808.c test/test.c -lm

$(BUILD)/test_fm1: dk808.c dk808.h test/test.c ../baustein.h
	@mkdir -p $(BUILD)/fm1
	$(CC) $(CFLAGS) -DDK808_INSTRUMENTS=DK808_MASK_FM1_ELECTRO -o $@ dk808.c test/test.c -lm
