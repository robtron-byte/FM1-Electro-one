# Prüfungen der Firmware-Varianten auf dem PC (make -C bausteine test findet dieses Makefile):
#   Live-Aufnahme nach RK-008 (gemeinsam/shared/seq/syn_live.h, docs/adr/0053)
#   Sequencer-Zusätze nach den iOS-Apps (gemeinsam/shared/seq/syn_pat.h, docs/adr/0052)
#   808 solo: die Drum-Machine wie BEAT (dk808/app/dk_beat.c, docs/adr/0050)
CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function
BUILD := ../build/varianten
.PHONY: test
test: $(BUILD)/test_live $(BUILD)/test_pat $(BUILD)/test_beat
	$(BUILD)/test_live
	$(BUILD)/test_pat
	$(BUILD)/test_beat

$(BUILD)/test_live: gemeinsam/shared/seq/test_live.c gemeinsam/shared/seq/syn_live.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Igemeinsam/shared/seq -o $@ gemeinsam/shared/seq/test_live.c

$(BUILD)/test_pat: gemeinsam/shared/seq/test_pat.c gemeinsam/shared/seq/syn_pat.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Igemeinsam/shared/seq -o $@ gemeinsam/shared/seq/test_pat.c

$(BUILD)/test_beat: dk808/test/beat.c dk808/app/dk_beat.c dk808/app/dk_beat.h ../dk808/dk808.c ../dk808/dk808_bs.c ../dk808/dk808.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DDK808_FLOAT=1 -I.. -I../dk808 -Idk808/app -o $@ dk808/test/beat.c dk808/app/dk_beat.c ../dk808/dk808.c ../dk808/dk808_bs.c -lm
