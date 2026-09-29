# Host unit tests for the hardware-independent App/ code: make -C tests
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -I../App/Inc

.PHONY: test clean
test: test_can_frames test_can_tx
	./test_can_frames
	./test_can_tx

test_can_frames: test_can_frames.c ../App/Src/can_frames.c ../App/Inc/can_frames.h
	$(CC) $(CFLAGS) -o $@ test_can_frames.c ../App/Src/can_frames.c

# fakes/ stands in for Core/Inc/main.h so can_tx.c builds without the HAL.
test_can_tx: test_can_tx.c ../App/Src/can_tx.c ../App/Inc/can_tx.h fakes/main.h
	$(CC) $(CFLAGS) -Ifakes -o $@ test_can_tx.c ../App/Src/can_tx.c

clean:
	rm -f test_can_frames test_can_tx
