/**
  ******************************************************************************
  * @file    bms_config.h
  * @brief   Build-time configuration of the BMS controller firmware.
  ******************************************************************************
  */
#ifndef BMS_CONFIG_H
#define BMS_CONFIG_H

#include "can_frames.h"

/* ADBMS6830s on the isoSPI chain: 1 on the test board, 2 per segment board,
   10 for the car. */
#define TOTAL_IC                  2U

/* Channels wired to cells, the same on every IC: bit n = channel n+1.
   0xFFFF is all 16; 0x01FF would be channels 1-9. */
#define BMS_CHANNEL_MASK          0xFFFFU
#define BMS_CELL_COUNT            (TOTAL_IC * (unsigned)__builtin_popcount(BMS_CHANNEL_MASK))
#define BMS_CELL_FRAME_COUNT      ((BMS_CELL_COUNT + CAN_FRAMES_CELLS_PER_FRAME - 1U) / CAN_FRAMES_CELLS_PER_FRAME)

/* Thresholds written to every chip. 0.5 V UV is a bench value: set it from the
   chosen cell's datasheet before this goes on the car. */
#define BMS_OV_THRESHOLD_V        4.2f
#define BMS_UV_THRESHOLD_V        0.5f

#define BMS_CYCLE_MS              100U   /* measurement + CAN cycle: 10 Hz */
#define BMS_STATUS_EVERY          5U     /* status read + per-IC frames: 2 Hz */
#define BMS_CONFIG_CHECK_EVERY    10U    /* config readback: 1 Hz */
#define BMS_REPORT_EVERY          10U    /* SWO report: 1 Hz */
#define BMS_CHAIN_LOST_CYCLES     5U     /* every IC invalid this many cycles -> INIT */

/* CAN2 standard IDs. 0x730 and up are reserved for temperatures. */
#define BMS_CAN_STATUS_ID         0x6FFU
#define BMS_CAN_CELL_BASE_ID      0x700U
#define BMS_CAN_IC_BASE_ID        0x720U
#define BMS_CAN_TEMP_BASE_ID      0x730U

_Static_assert((TOTAL_IC >= 1U) && (BMS_CAN_IC_BASE_ID + TOTAL_IC <= BMS_CAN_TEMP_BASE_ID),
               "TOTAL_IC must be 1..16: per-IC frames must stay below 0x730 and fit the invalid-IC bitmap");
_Static_assert(BMS_CAN_CELL_BASE_ID + BMS_CELL_FRAME_COUNT <= BMS_CAN_IC_BASE_ID,
               "too many cells: the 0x700 cell frames would run into the 0x720 per-IC frames");
_Static_assert((BMS_CHANNEL_MASK != 0U) && (BMS_CHANNEL_MASK <= 0xFFFFU),
               "BMS_CHANNEL_MASK must select 1 to 16 channels");
_Static_assert((BMS_CYCLE_MS >= 1U) && (BMS_STATUS_EVERY >= 1U) && (BMS_CONFIG_CHECK_EVERY >= 1U) &&
               (BMS_REPORT_EVERY >= 1U) && (BMS_CHAIN_LOST_CYCLES >= 1U),
               "BMS_CYCLE_MS, the *_EVERY periods and BMS_CHAIN_LOST_CYCLES must be at least 1");
_Static_assert((BMS_REPORT_EVERY % BMS_STATUS_EVERY) == 0U,
               "the SWO report must fall on a status cycle, or it shows stale status");

#endif /* BMS_CONFIG_H */
