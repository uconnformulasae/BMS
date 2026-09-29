/**
  ******************************************************************************
  * @file    can_frames.c
  * @brief   CAN2 telemetry frame encoding.
  ******************************************************************************
  */
#include "can_frames.h"

#include <string.h>

static void put_u16(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)(value >> 8);
}

uint16_t can_frames_cell_0p1mv(int16_t code)
{
  if (code == CAN_FRAMES_RESET_CODE)
  {
    return CAN_FRAMES_NO_DATA;
  }
  int32_t x = (int32_t)code + 10000;
  if (x <= 0)
  {
    return 0U;
  }
  return (uint16_t)((3 * x + 1) / 2);
}

int16_t can_frames_die_temp_0p1c(int16_t code)
{
  if (code == CAN_FRAMES_RESET_CODE)
  {
    return CAN_FRAMES_TEMP_UNKNOWN;
  }
  int32_t x = (int32_t)code + 10000;
  return (int16_t)((x + 2) / 5 - 2730);
}

bool can_frames_any_flag(const uint8_t *flags, uint16_t channel_mask)
{
  for (uint32_t ch = 0; ch < CAN_FRAMES_CHANNELS_PER_IC; ch++)
  {
    if ((((channel_mask >> ch) & 1U) != 0U) && (flags[ch] != 0U))
    {
      return true;
    }
  }
  return false;
}

size_t can_frames_map_cells(const int16_t *codes, uint8_t n_ic, uint16_t channel_mask,
                            uint16_t invalid_ics, uint16_t *cells)
{
  size_t n = 0;
  for (uint32_t ic = 0; ic < n_ic; ic++)
  {
    bool valid = ((invalid_ics >> ic) & 1U) == 0U;
    for (uint32_t ch = 0; ch < CAN_FRAMES_CHANNELS_PER_IC; ch++)
    {
      if (((channel_mask >> ch) & 1U) != 0U)
      {
        int16_t code = codes[ic * CAN_FRAMES_CHANNELS_PER_IC + ch];
        cells[n++] = valid ? can_frames_cell_0p1mv(code) : CAN_FRAMES_NO_DATA;
      }
    }
  }
  return n;
}

size_t can_frames_build_cells(const uint16_t *cells, size_t n_cells, uint16_t base_id,
                              can_frame_t *frames)
{
  size_t n_frames = (n_cells + CAN_FRAMES_CELLS_PER_FRAME - 1U) / CAN_FRAMES_CELLS_PER_FRAME;
  for (size_t f = 0; f < n_frames; f++)
  {
    frames[f].id = (uint16_t)(base_id + f);
    frames[f].dlc = 8U;
    for (size_t slot = 0; slot < CAN_FRAMES_CELLS_PER_FRAME; slot++)
    {
      size_t cell = f * CAN_FRAMES_CELLS_PER_FRAME + slot;
      put_u16(&frames[f].data[2U * slot], (cell < n_cells) ? cells[cell] : CAN_FRAMES_NO_DATA);
    }
  }
  return n_frames;
}

void can_frames_build_ic(const can_frames_ic_t *ic, uint16_t base_id, can_frame_t *frame)
{
  frame->id = (uint16_t)(base_id + ic->index);
  frame->dlc = 8U;
  frame->data[0] = ic->index;
  frame->data[1] = ic->status;
  frame->data[2] = ic->cmd_count;
  frame->data[3] = ic->invalid_run;
  put_u16(&frame->data[4], (uint16_t)ic->die_temp_0p1c);
  put_u16(&frame->data[6], ic->balance_mask);
}

void can_frames_build_status(uint8_t chain_state, uint8_t cycle, uint16_t invalid_ics,
                             uint16_t id, can_frame_t *frame)
{
  memset(frame->data, 0, sizeof frame->data);
  frame->id = id;
  frame->dlc = 8U;
  frame->data[0] = chain_state;
  frame->data[1] = cycle;
  put_u16(&frame->data[2], invalid_ics);
}
