/**
  ******************************************************************************
  * @file    can_frames.h
  * @brief   CAN2 telemetry frame encoding. Plain C with no HAL or ADI types, so
  *          tests/ builds and runs it on the host.
  *
  *          Standard 11-bit IDs, DLC 8, little-endian:
  *            status   id        [0] chain state  [1] cycle counter
  *                               [2..3] invalid-IC bitmap  [4..7] 0
  *            cells    base + k  cells 4k+1 .. 4k+4, u16 each, 0.1 mV/bit
  *            per IC   base + i  [0] index  [1] status bits  [2] command counter
  *                               [3] consecutive cycles without valid data
  *                               [4..5] die temperature s16, 0.1 C
  *                               [6..7] balancing bitmap
  ******************************************************************************
  */
#ifndef CAN_FRAMES_H
#define CAN_FRAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_FRAMES_CHANNELS_PER_IC  16U
#define CAN_FRAMES_CELLS_PER_FRAME  4U

#define CAN_FRAMES_NO_DATA          0xFFFFU    /* cell: invalid IC, no conversion, or unused slot */
#define CAN_FRAMES_TEMP_UNKNOWN     INT16_MAX  /* die temperature unknown */
#define CAN_FRAMES_RESET_CODE       INT16_MIN  /* 0x8000: ADBMS6830 result register power-on value */

#define CAN_FRAMES_CHAIN_INIT       0U
#define CAN_FRAMES_CHAIN_RUNNING    1U

/* Per-IC status bits, byte 1 of the per-IC frame. */
#define CAN_FRAMES_IC_PEC_FAIL      0x01U
#define CAN_FRAMES_IC_ANY_OV        0x02U
#define CAN_FRAMES_IC_ANY_UV        0x04U
#define CAN_FRAMES_IC_OPEN_WIRE     0x08U      /* reserved: open-wire detection not implemented */
#define CAN_FRAMES_IC_THSD          0x10U
#define CAN_FRAMES_IC_SPIFLT        0x20U

typedef struct
{
  uint16_t id;
  uint8_t  dlc;
  uint8_t  data[8];
} can_frame_t;

typedef struct
{
  uint8_t  index;
  uint8_t  status;           /* CAN_FRAMES_IC_* bits */
  uint8_t  cmd_count;
  uint8_t  invalid_run;
  int16_t  die_temp_0p1c;
  uint16_t balance_mask;
} can_frames_ic_t;

/* ADBMS6830 cell code -> volts in 0.1 mV. Negative readings clamp to 0. */
uint16_t can_frames_cell_0p1mv(int16_t code);

/* ADBMS6830 ITMP code -> die temperature in 0.1 C (ADI: V / 7.5 mV - 273 C). */
int16_t can_frames_die_temp_0p1c(int16_t code);

/* True if any channel in channel_mask has a nonzero flag. flags has one entry
   per channel, like ADI's per-cell OV/UV arrays. */
bool can_frames_any_flag(const uint8_t *flags, uint16_t channel_mask);

/* Converts the channels in channel_mask to logical cells, IC 0 first. codes
   holds n_ic rows of CAN_FRAMES_CHANNELS_PER_IC codes; an IC whose bit is set
   in invalid_ics yields CAN_FRAMES_NO_DATA. Returns the number of cells. */
size_t can_frames_map_cells(const int16_t *codes, uint8_t n_ic, uint16_t channel_mask,
                            uint16_t invalid_ics, uint16_t *cells);

/* Packs cells four per frame, IDs from base_id. Returns the number of frames. */
size_t can_frames_build_cells(const uint16_t *cells, size_t n_cells, uint16_t base_id,
                              can_frame_t *frames);

void can_frames_build_ic(const can_frames_ic_t *ic, uint16_t base_id, can_frame_t *frame);

void can_frames_build_status(uint8_t chain_state, uint8_t cycle, uint16_t invalid_ics,
                             uint16_t id, can_frame_t *frame);

#endif /* CAN_FRAMES_H */
