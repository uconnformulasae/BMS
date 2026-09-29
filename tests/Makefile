# Host unit tests for the hardware-independent App/ code: make -C tests
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -I../App/Inc

.PHONY: test clean
test: test_can_frames
	./test_can_frames

test_can_frames: test_can_frames.c ../App/Src/can_frames.c ../App/Inc/can_frames.h
	$(CC) $(CFLAGS) -o $@ test_can_frames.c ../App/Src/can_frames.c

clean:
	rm -f test_can_frames
