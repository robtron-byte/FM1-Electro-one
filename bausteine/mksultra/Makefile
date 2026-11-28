# mksultra: Host-Test gegen die Golden-Renders der Vorlage (FNV-1a-64 je Fall, ref/manifest.json).
#   make test                  volle Fassung (12 Stimmen) bitgenau prüfen, dann Sparfassung messen
#   make test WAV=<Ordner>     zusätzlich erste Abweichung gegen die Golden-WAVs zeigen (docs/golden der Vorlage)
#   make bench                 Rechenzeit je Stimme auf diesem PC
#   make sweep RUST=<Datei>    alle 520 Presets gegen eine Prüfsummenliste des Rust-Kerns (README, „Alle Presets“)
#   make presets PRESETS=<…/app/presets>   mksultra_presets.c neu erzeugen
SHELL := /bin/bash
CC ?= cc
# -ffp-contract=off: keine FMA, gleiche Rundung wie die Rust-Vorlage (docs/SOUND_ENGINE_PORTING.md §4)
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -Wdouble-promotion -ffp-contract=off
CFLAGS += -I.. -I. -D_DEFAULT_SOURCE
BUILD := ../build/mksultra
WAV ?= $(wildcard ref/wav)
SPAR ?= 6
SRC := mksultra.c mksultra_presets.c
DEP := $(SRC) mksultra.h mksultra_dsp.h mksultra_fx.h test/test.c ../baustein.h

.PHONY: test bench presets sweep
test: $(BUILD)/test $(BUILD)/test_spar$(SPAR)
	$(BUILD)/test ref $(BUILD) $(WAV)
	@mkdir -p $(BUILD)/spar
	$(BUILD)/test_spar$(SPAR) --vs ref $(BUILD)/spar $(BUILD)

bench: $(BUILD)/test $(BUILD)/test_spar$(SPAR)
	$(BUILD)/test --bench
	$(BUILD)/test_spar$(SPAR) --bench

$(BUILD)/test: $(DEP)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRC) test/test.c -lm

$(BUILD)/test_spar$(SPAR): $(DEP)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DMKSULTRA_VOICES=$(SPAR) -o $@ $(SRC) test/test.c -lm

sweep: $(BUILD)/test
	$(BUILD)/test --sweep > $(BUILD)/sweep.txt
	diff <(sort -k2 "$(RUST)") <(sort -k2 $(BUILD)/sweep.txt) && echo "sweep: alle $$(wc -l < $(BUILD)/sweep.txt) Presets gleich"

presets:
	python3 tools/mks2c.py "$(PRESETS)" mksultra_presets.c
