# sh101: Host-Test gegen die Testvektoren der Vorlage (MonoBass, ref/vectors.json), einmal voll, einmal schlank
# (ohne Poly und Arp/Seq) und wie im FM-1 (float, Polynome).
#   make test                 64 Samples + RMS je 100 ms aus ref/vectors.json
#   make test WAV=<Ordner>    zusätzlich jedes Sample gegen die Referenz-WAVs (Dropbox REA/SH101/app/docs/test-vectors)
CC ?= cc
# -ffp-contract=off: keine zusammengezogenen Multiplikationen/Additionen, gleiche Rundung wie der Swift-Kern
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror -ffp-contract=off
CFLAGS += -I.. -I. -D_DEFAULT_SOURCE
BUILD := ../build/sh101
WAV ?=

.PHONY: test
test: $(BUILD)/test $(BUILD)/test_schlank $(BUILD)/test_fm1
	$(BUILD)/test ref $(BUILD) "$(WAV)"
	@mkdir -p $(BUILD)/schlank
	$(BUILD)/test_schlank ref $(BUILD)/schlank "$(WAV)"
	@mkdir -p $(BUILD)/fm1
	$(BUILD)/test_fm1 ref $(BUILD)/fm1 "$(WAV)"

$(BUILD)/test: sh101.c sh101.h test/test.c ref/vectors.h ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ sh101.c test/test.c -lm

$(BUILD)/test_schlank: sh101.c sh101.h test/test.c ref/vectors.h ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSH101_WITH_POLY=0 -DSH101_WITH_ARPSEQ=0 -o $@ sh101.c test/test.c -lm

# wie im FM-1 (bausteine/varianten/sh101): float statt double, Polynome statt Bibliothek, eine Stimme, ohne Arp/Seq
$(BUILD)/test_fm1: sh101.c sh101.h test/test.c ref/vectors.h ../baustein.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSH101_FLOAT=1 -DSH101_FASTMATH=1 -DSH101_WITH_POLY=0 -DSH101_WITH_ARPSEQ=0 -o $@ sh101.c test/test.c -lm
