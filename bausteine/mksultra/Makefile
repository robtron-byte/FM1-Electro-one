# mksultra: Host-Test gegen die Golden-Renders der Vorlage (FNV-1a-64 je Fall, ref/manifest.json).
#   make test                  volle Fassung (12 Stimmen) bitgenau prüfen, dann Sparfassung messen
#   make test WAV=<Ordner>     zusätzlich erste Abweichung gegen die Golden-WAVs zeigen (docs/golden der Vorlage)
#   make bench                 Rechenzeit je Stimme auf diesem PC
#   make presets PRESETS=<…/app/presets>   mksultra_presets.c neu erzeugen
CC ?= cc
# -ffp-contract=off: keine FMA, gleiche Rundung wie die Rust-Vorlage (docs/SOUND_ENGINE_PORTING.md §4)
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -Wdouble-promotion -ffp-contract=off
CFLAGS += -I.. -I. -D_DEFAULT_SOURCE
BUILD := ../build/mksultra
WAV ?= $(wildcard ref/wav)
SPAR ?= 6
SRC := mksultra.c mksultra_presets.c
DEP := $(SRC) mksultra.h mksultra_dsp.h mksultra_fx.h test/test.c ../baustein.h

.PHONY: test bench presets
test: $(BUILD)/test $(BUILD)/test_spar
	$(BUILD)/test ref $(BUILD) $(WAV)
	@mkdir -p $(BUILD)/spar
	$(BUILD)/test_spar --vs ref $(BUILD)/spar $(BUILD)

bench: $(BUILD)/test $(BUILD)/test_spar
	$(BUILD)/test --bench
	$(BUILD)/test_spar --bench

$(BUILD)/test: $(DEP)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRC) test/test.c -lm

$(BUILD)/test_spar: $(DEP)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DMKSULTRA_VOICES=$(SPAR) -o $@ $(SRC) test/test.c -lm

presets:
	python3 tools/mks2c.py "$(PRESETS)" mksultra_presets.c
