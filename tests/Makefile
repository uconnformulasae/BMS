# Host unit tests for the hardware-independent App/ code: make -C tests
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -I../App/Inc
# ADI's headers are vendor code: include them as system headers so their
# style does not trip -Werror in our code. ADI's wrapper also defines
# functions with empty parameter lists, e.g. adBmsCsLow().
ADI_CFLAGS = $(CFLAGS) -Wno-strict-prototypes -Ifakes -isystem ../ADBMS6830/lib/inc -isystem ../ADBMS6830/program/inc

.PHONY: test clean config_check
test: test_can_frames test_can_tx test_mcu_wrapper test_bms_monitor config_check
	./test_can_frames
	./test_can_tx
	./test_mcu_wrapper
	./test_bms_monitor

test_can_frames: test_can_frames.c ../App/Src/can_frames.c ../App/Inc/can_frames.h
	$(CC) $(CFLAGS) -o $@ test_can_frames.c ../App/Src/can_frames.c

# fakes/ stands in for Core/Inc/main.h so can_tx.c builds without the HAL.
test_can_tx: test_can_tx.c ../App/Src/can_tx.c ../App/Inc/can_tx.h fakes/main.h
	$(CC) $(CFLAGS) -Ifakes -o $@ test_can_tx.c ../App/Src/can_tx.c

test_mcu_wrapper: test_mcu_wrapper.c ../ADBMS6830/program/src/mcuWrapper.c ../ADBMS6830/program/inc/mcuWrapper.h fakes/main.h
	$(CC) $(ADI_CFLAGS) -o $@ test_mcu_wrapper.c ../ADBMS6830/program/src/mcuWrapper.c

# printf is renamed so the tests can read the monitor's SWO report.
bms_monitor_test.o: ../App/Src/bms_monitor.c ../App/Inc/bms_monitor.h ../App/Inc/bms_config.h ../App/Inc/can_frames.h ../App/Inc/can_tx.h fakes/main.h
	$(CC) $(ADI_CFLAGS) -Dprintf=monitor_printf -c -o $@ ../App/Src/bms_monitor.c

test_bms_monitor: test_bms_monitor.c fake_adbms.c fake_adbms.h bms_monitor_test.o ../App/Src/can_frames.c
	$(CC) $(ADI_CFLAGS) -o $@ test_bms_monitor.c fake_adbms.c bms_monitor_test.o ../App/Src/can_frames.c

# bms_config.h must compile as shipped, and must refuse each bad configuration
# below with its own message, e.g. 10 ICs x 16 channels (160 cells would run
# the 0x700 cell frames into the 0x720 per-IC frames).
config_check: ../App/Inc/bms_config.h ../App/Inc/can_frames.h
	echo '#include "bms_config.h"' | $(CC) $(CFLAGS) -fsyntax-only -x c -
	sed 's/^#define TOTAL_IC .*/#define TOTAL_IC 10U/' ../App/Inc/bms_config.h > bad_config.h
	echo '#include "bad_config.h"' | $(CC) $(CFLAGS) -I. -fsyntax-only -x c - 2>&1 | grep -q 'too many cells'
	sed 's/^#define BMS_CHANNEL_MASK .*/#define BMS_CHANNEL_MASK 0x0000U/' ../App/Inc/bms_config.h > bad_config.h
	echo '#include "bad_config.h"' | $(CC) $(CFLAGS) -I. -fsyntax-only -x c - 2>&1 | grep -q 'select 1 to 16 channels'
	sed 's/^#define BMS_CHANNEL_MASK .*/#define BMS_CHANNEL_MASK 0x10000U/' ../App/Inc/bms_config.h > bad_config.h
	echo '#include "bad_config.h"' | $(CC) $(CFLAGS) -I. -fsyntax-only -x c - 2>&1 | grep -q 'select 1 to 16 channels'
	sed 's/^#define BMS_REPORT_EVERY .*/#define BMS_REPORT_EVERY 7U/' ../App/Inc/bms_config.h > bad_config.h
	echo '#include "bad_config.h"' | $(CC) $(CFLAGS) -I. -fsyntax-only -x c - 2>&1 | grep -q 'fall on a status cycle'
	rm -f bad_config.h

clean:
	rm -f test_can_frames test_can_tx test_mcu_wrapper test_bms_monitor bms_monitor_test.o bad_config.h
