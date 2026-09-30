/**
  ******************************************************************************
  * @file    can_frames.h
  * @brief   CAN2 telemetry frame encoding.
  ******************************************************************************
  */
#ifndef CAN_FRAMES_H
#define CAN_FRAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_FRAMES_CHANNELS_PER_IC  16U
#define CAN_FRAMES_CELLS_PER_FRAME  4U

#define CAN_FRAMES_NO_DATA          0xFFFFU
#define CAN_FRAMES_TEMP_UNKNOWN     INT16_MAX
#define CAN_FRAMES_RESET_CODE       INT16_MIN

#define CAN_FRAMES_CHAIN_INIT       0U
#define CAN_FRAMES_CHAIN_RUNNING    1U

#define CAN_FRAMES_IC_PEC_FAIL      0x01U
#define CAN_FRAMES_IC_ANY_OV        0x02U
#define CAN_FRAMES_IC_ANY_UV        0x04U
#define CAN_FRAMES_IC_OPEN_WIRE     0x08U
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
  uint8_t  status;
  uint8_t  cmd_count;
  uint8_t  invalid_run;
  int16_t  die_temp_0p1c;
  uint16_t balance_mask;
} can_frames_ic_t;

uint16_t can_frames_cell_0p1mv(int16_t code);
int16_t can_frames_die_temp_0p1c(int16_t code);
bool can_frames_any_flag(const uint8_t *flags, uint16_t channel_mask);
size_t can_frames_map_cells(const int16_t *codes, uint8_t n_ic, uint16_t channel_mask,
                            uint16_t invalid_ics, uint16_t *cells);
size_t can_frames_build_cells(const uint16_t *cells, size_t n_cells, uint16_t base_id,
                              can_frame_t *frames);
void can_frames_build_ic(const can_frames_ic_t *ic, uint16_t base_id, can_frame_t *frame);
void can_frames_build_status(uint8_t chain_state, uint8_t cycle, uint16_t invalid_ics,
                             uint16_t id, can_frame_t *frame);

#endif /* CAN_FRAMES_H */
