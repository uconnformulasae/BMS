# Host unit tests for the hardware-independent App/ code: make -C tests
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -I../App/Inc
# ADI's headers are vendor code: include them as system headers so their
# style does not trip -Werror in our code. ADI's wrapper also defines
# functions with empty parameter lists, e.g. adBmsCsLow().
ADI_CFLAGS = $(CFLAGS) -Wno-strict-prototypes -Ifakes -isystem ../ADBMS6830/lib/inc -isystem ../ADBMS6830/program/inc

.PHONY: test clean
test: test_can_frames test_can_tx test_mcu_wrapper config_check
	./test_can_frames
	./test_can_tx
	./test_mcu_wrapper

test_can_frames: test_can_frames.c ../App/Src/can_frames.c ../App/Inc/can_frames.h
	$(CC) $(CFLAGS) -o $@ test_can_frames.c ../App/Src/can_frames.c

# fakes/ stands in for Core/Inc/main.h so can_tx.c builds without the HAL.
test_can_tx: test_can_tx.c ../App/Src/can_tx.c ../App/Inc/can_tx.h fakes/main.h
	$(CC) $(CFLAGS) -Ifakes -o $@ test_can_tx.c ../App/Src/can_tx.c

test_mcu_wrapper: test_mcu_wrapper.c ../ADBMS6830/program/src/mcuWrapper.c ../ADBMS6830/program/inc/mcuWrapper.h fakes/main.h
	$(CC) $(ADI_CFLAGS) -o $@ test_mcu_wrapper.c ../ADBMS6830/program/src/mcuWrapper.c

# bms_config.h must compile as shipped, and must refuse 10 ICs x 16 channels
# (160 cells would run the 0x700 cell frames into the 0x720 per-IC frames).
config_check: ../App/Inc/bms_config.h ../App/Inc/can_frames.h
	echo '#include "bms_config.h"' | $(CC) $(CFLAGS) -fsyntax-only -x c -
	sed 's/^#define TOTAL_IC .*/#define TOTAL_IC 10U/' ../App/Inc/bms_config.h > overflow_config.h
	! echo '#include "overflow_config.h"' | $(CC) $(CFLAGS) -I. -fsyntax-only -x c - 2>/dev/null
	rm -f overflow_config.h

clean:
	rm -f test_can_frames test_can_tx test_mcu_wrapper overflow_config.h
