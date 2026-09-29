/**
  ******************************************************************************
  * @file    bms_monitor.c
  * @brief   ADBMS6830 chain measurement cycle: chain state, PEC tracking, CAN2
  *          telemetry and the SWO report.
  ******************************************************************************
  */
#include "bms_monitor.h"

#include <stdio.h>
#include <string.h>

#include "adbms_main.h"
#include "adBms6830CmdList.h"   /* defines the opcode arrays: include it in this file only */
#include "bms_config.h"
#include "can_frames.h"
#include "can_tx.h"
#include "main.h"

extern CAN_HandleTypeDef hcan2;

#define ALL_ICS ((uint16_t)((1UL << TOTAL_IC) - 1U))

typedef enum
{
  CHAIN_INIT = CAN_FRAMES_CHAIN_INIT,
  CHAIN_RUNNING = CAN_FRAMES_CHAIN_RUNNING,
} chain_state_t;

static cell_asic ic[TOTAL_IC];
static chain_state_t chain_state = CHAIN_INIT;
static uint32_t cycle_count;
static uint32_t last_cycle_ms;
static uint16_t invalid_ics = ALL_ICS;  /* bit i: IC i has no valid data this cycle */
static uint8_t pec_fail_run[TOTAL_IC];
static uint8_t all_invalid_run;

static const char *state_name(chain_state_t state)
{
  return (state == CHAIN_RUNNING) ? "RUNNING" : "INIT";
}

static void set_state(chain_state_t next, const char *reason)
{
  if (next == chain_state)
  {
    return;
  }
  printf("chain %s -> %s: %s\n", state_name(chain_state), state_name(next), reason);
  chain_state = next;
  all_invalid_run = 0U;
}

static uint8_t pec_error(const cell_asic *chip, TYPE type)
{
  switch (type)
  {
    case Cell:   return chip->cccrc.cell_pec;
    case Status: return chip->cccrc.stat_pec;
    case Config: return chip->cccrc.cfgr_pec;
    default:     return 1U;
  }
}

/* ADI's driver overwrites each IC's PEC flag for a register type on every read,
   so collect it straight after the read. Returns the ICs whose PEC failed. */
static uint16_t read_group(uint8_t cmd[2], TYPE type, GRP group)
{
  adBmsReadData(TOTAL_IC, ic, cmd, type, group);
  uint16_t failed = 0U;
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    if (pec_error(&ic[i], type) != 0U)
    {
      failed |= (uint16_t)(1U << i);
    }
  }
  return failed;
}

/* Snapshots and reads all 16 cell results. Returns the ICs whose PEC failed. */
static uint16_t read_cells(void)
{
  adBms6830_Snap();
  uint16_t failed = read_group(RDCVA, Cell, A);
  failed |= read_group(RDCVB, Cell, B);
  failed |= read_group(RDCVC, Cell, C);
  failed |= read_group(RDCVD, Cell, D);
  failed |= read_group(RDCVE, Cell, E);
  failed |= read_group(RDCVF, Cell, F);
  adBms6830_Unsnap();
  return failed;
}

/* Starts an aux-ADC conversion of the die temperature. ADCV only converts the
   cells, so without this ITMP never leaves its power-on value. */
static void start_die_temp_conversion(void)
{
  adBms6830_Adax(AUX_OW_OFF, PUP_DOWN, TEMP);
}

/* Die temperature (A), THSD/SPIFLT (C) and per-cell OV/UV (D), then starts the
   die-temperature conversion the next status read will see. */
static uint16_t read_status(void)
{
  uint16_t failed = read_group(RDSTATA, Status, A);
  failed |= read_group(RDSTATC, Status, C);
  failed |= read_group(RDSTATD, Status, D);
  start_die_temp_conversion();
  return failed;
}

/* Reads the config back. Returns the ICs that read back PEC-clean but different
   from what was written; *pec_failed gets the ICs whose readback failed PEC.
   Only REFON/VOV/VUV are compared: other bits (GPO, for one) can read back
   live state rather than the written value. */
static uint16_t config_mismatch(uint16_t *pec_failed)
{
  *pec_failed = read_group(RDCFGA, Config, A) | read_group(RDCFGB, Config, B);
  uint16_t mismatch = 0U;
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    bool pec_ok = ((*pec_failed >> i) & 1U) == 0U;
    if (pec_ok && ((ic[i].rx_cfga.refon != ic[i].tx_cfga.refon) ||
                   (ic[i].rx_cfgb.vov != ic[i].tx_cfgb.vov) ||
                   (ic[i].rx_cfgb.vuv != ic[i].tx_cfgb.vuv)))
    {
      mismatch |= (uint16_t)(1U << i);
    }
  }
  return mismatch;
}

/* A PEC-clean IC whose populated channels read 0x8000 has reset: its result
   registers are at their power-on value and no conversions are running. */
static bool chip_reset_seen(void)
{
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    if (((invalid_ics >> i) & 1U) != 0U)
    {
      continue;
    }
    for (uint8_t ch = 0; ch < CAN_FRAMES_CHANNELS_PER_IC; ch++)
    {
      if ((((BMS_CHANNEL_MASK >> ch) & 1U) != 0U) && (ic[i].cell.c_codes[ch] == CAN_FRAMES_RESET_CODE))
      {
        return true;
      }
    }
  }
  return false;
}

/* INIT: wake the chain, write the config, start continuous cell conversion
   and a die-temperature conversion, and confirm the config stuck. The first
   cell read is a full cycle later, which covers the conversion warm-up. */
static void start_chain(void)
{
  adBmsForceWakeupIc(TOTAL_IC);
  adBmsWriteData(TOTAL_IC, ic, WRCFGA, Config, A);
  adBmsWriteData(TOTAL_IC, ic, WRCFGB, Config, B);
  adBms6830_Adcv(RD_ON, CONTINUOUS, DCP_OFF, RSTF_OFF, OW_OFF_ALL_CH);
  start_die_temp_conversion();

  uint16_t pec_failed;
  if ((config_mismatch(&pec_failed) | pec_failed) == 0U)
  {
    set_state(CHAIN_RUNNING, "config verified");
  }
}

/* One RUNNING cycle. Returns the ICs whose status read failed PEC. */
static uint16_t measure(bool status_cycle, bool config_cycle)
{
  adBmsWakeupIc(TOTAL_IC);              /* spiSendCmd() never wakes, and SNAP goes first */
  uint16_t failed = read_cells();
  if (failed != 0U)
  {
    adBmsForceWakeupIc(TOTAL_IC);
    failed = read_cells();
  }
  invalid_ics = failed;

  if (chip_reset_seen())
  {
    set_state(CHAIN_INIT, "cells read 0x8000, a chip reset");
    invalid_ics = ALL_ICS;
    return 0U;
  }
  all_invalid_run = (failed == ALL_ICS) ? (uint8_t)(all_invalid_run + 1U) : 0U;
  if (all_invalid_run >= BMS_CHAIN_LOST_CYCLES)
  {
    set_state(CHAIN_INIT, "no valid data from any IC");
    return 0U;
  }

  uint16_t status_failed = status_cycle ? read_status() : 0U;
  uint16_t readback_pec_failed;
  if (config_cycle && (config_mismatch(&readback_pec_failed) != 0U))
  {
    set_state(CHAIN_INIT, "config changed, a chip reset or slept");
    invalid_ics = ALL_ICS;
  }
  return status_failed;
}

static void count_pec_failures(void)
{
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    if (((invalid_ics >> i) & 1U) == 0U)
    {
      pec_fail_run[i] = 0U;
    }
    else if (pec_fail_run[i] < UINT8_MAX)
    {
      pec_fail_run[i]++;
    }
  }
}

static can_frames_ic_t ic_status(uint8_t i, uint16_t status_failed)
{
  can_frames_ic_t status = {
    .index = i,
    .status = CAN_FRAMES_IC_PEC_FAIL,
    .cmd_count = 0U,
    .pec_fail_run = pec_fail_run[i],
    .die_temp_0p1c = CAN_FRAMES_TEMP_UNKNOWN,
    .balance_mask = 0U,                 /* balancing not implemented */
  };
  if ((((invalid_ics | status_failed) >> i) & 1U) != 0U)
  {
    return status;                      /* nothing trustworthy: report the PEC failure only */
  }
  status.status = 0U;
  if (can_frames_any_flag(ic[i].statd.c_ov, BMS_CHANNEL_MASK))
  {
    status.status |= CAN_FRAMES_IC_ANY_OV;
  }
  if (can_frames_any_flag(ic[i].statd.c_uv, BMS_CHANNEL_MASK))
  {
    status.status |= CAN_FRAMES_IC_ANY_UV;
  }
  if (ic[i].statc.thsd != 0U)
  {
    status.status |= CAN_FRAMES_IC_THSD;
  }
  if (ic[i].statc.spiflt != 0U)
  {
    status.status |= CAN_FRAMES_IC_SPIFLT;
  }
  status.cmd_count = ic[i].cccrc.cmd_cntr;
  status.die_temp_0p1c = can_frames_die_temp_0p1c((int16_t)ic[i].stata.itmp);
  return status;
}

static void send_frames(bool status_cycle, uint16_t status_failed)
{
  can_frame_t frame;
  can_tx_begin_cycle();

  can_frames_build_status((uint8_t)chain_state, (uint8_t)cycle_count, invalid_ics,
                          BMS_CAN_STATUS_ID, &frame);
  can_tx_send(&frame);

  int16_t codes[TOTAL_IC * CAN_FRAMES_CHANNELS_PER_IC];
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    memcpy(&codes[i * CAN_FRAMES_CHANNELS_PER_IC], ic[i].cell.c_codes, sizeof ic[i].cell.c_codes);
  }
  uint16_t cells[BMS_CELL_COUNT];
  can_frame_t cell_frames[BMS_CELL_FRAME_COUNT];
  size_t n_cells = can_frames_map_cells(codes, TOTAL_IC, BMS_CHANNEL_MASK, invalid_ics, cells);
  size_t n_frames = can_frames_build_cells(cells, n_cells, BMS_CAN_CELL_BASE_ID, cell_frames);
  for (size_t f = 0; f < n_frames; f++)
  {
    can_tx_send(&cell_frames[f]);
  }

  if (status_cycle)
  {
    for (uint8_t i = 0; i < TOTAL_IC; i++)
    {
      can_frames_ic_t status = ic_status(i, status_failed);
      can_frames_build_ic(&status, BMS_CAN_IC_BASE_ID, &frame);
      can_tx_send(&frame);
    }
  }
}

/* Prints a cell code as volts to 0.1 mV, e.g. "3.7121V", keeping the sign. */
static void print_volts(int16_t code)
{
  long tenth_mv = ((long)code + 10000L) * 3L / 2L;   /* V = (code + 10000) * 150 uV */
  unsigned long magnitude = (unsigned long)((tenth_mv < 0) ? -tenth_mv : tenth_mv);
  printf("%s%lu.%04luV", (tenth_mv < 0) ? "-" : "", magnitude / 10000UL, magnitude % 10000UL);
}

static void report_ic(uint8_t i)
{
  const cell_asic *chip = &ic[i];
  if (((invalid_ics >> i) & 1U) != 0U)
  {
    printf("IC%u  no valid data, %u failed cycles in a row\n", i, pec_fail_run[i]);
    return;
  }
  int16_t die = can_frames_die_temp_0p1c((int16_t)chip->stata.itmp);
  printf("IC%u  cmd count %u", i, chip->cccrc.cmd_cntr);
  if (die != CAN_FRAMES_TEMP_UNKNOWN)
  {
    long magnitude = (die < 0) ? -(long)die : (long)die;
    printf("  die %s%ld.%ldC", (die < 0) ? "-" : "", magnitude / 10L, magnitude % 10L);
  }
  printf("  OV %u  UV %u\n", can_frames_any_flag(chip->statd.c_ov, BMS_CHANNEL_MASK),
         can_frames_any_flag(chip->statd.c_uv, BMS_CHANNEL_MASK));
  for (uint8_t ch = 0; ch < CAN_FRAMES_CHANNELS_PER_IC; ch++)
  {
    printf("  C%-2u ", ch + 1U);
    print_volts(chip->cell.c_codes[ch]);
    if ((ch % 4U) == 3U)
    {
      printf("\n");
    }
  }
}

static void report(void)
{
  uint32_t now = HAL_GetTick();
  can_tx_counters_t tx = can_tx_counters();
  uint32_t esr = hcan2.Instance->ESR;
  printf("\n[%lu.%lus] %s  invalid ICs 0x%04X  CAN2 sent %lu dropped %lu TEC %lu REC %lu%s%s\n",
         (unsigned long)(now / 1000U), (unsigned long)((now / 100U) % 10U), state_name(chain_state),
         invalid_ics, (unsigned long)tx.sent, (unsigned long)tx.dropped,
         (unsigned long)((esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos),
         (unsigned long)((esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos),
         ((esr & CAN_ESR_BOFF) != 0U) ? "  BUS-OFF" : "",
         ((esr & CAN_ESR_EPVF) != 0U) ? "  error-passive" : "");
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    report_ic(i);
  }
}

static void report_can_timing(const CAN_TypeDef *can)
{
  uint32_t btr = can->BTR;
  uint32_t prescaler = ((btr & CAN_BTR_BRP) >> CAN_BTR_BRP_Pos) + 1U;
  uint32_t seg1 = ((btr & CAN_BTR_TS1) >> CAN_BTR_TS1_Pos) + 1U;
  uint32_t seg2 = ((btr & CAN_BTR_TS2) >> CAN_BTR_TS2_Pos) + 1U;
  uint32_t quanta = 1U + seg1 + seg2;
  uint32_t sample_permille = 1000U * (1U + seg1) / quanta;
  printf("CAN2 %lu bit/s, %lu time quanta, sample point %lu.%lu%%\n",
         (unsigned long)(HAL_RCC_GetPCLK1Freq() / (prescaler * quanta)), (unsigned long)quanta,
         (unsigned long)(sample_permille / 10U), (unsigned long)(sample_permille % 10U));
}

void bms_monitor_init(void)
{
  for (uint8_t i = 0; i < TOTAL_IC; i++)
  {
    ic[i].tx_cfga.refon = PWR_UP;
    ic[i].tx_cfga.gpo = 0x3FFU;         /* all GPIO pull-downs off */
    ic[i].tx_cfgb.vov = SetOverVoltageThreshold(BMS_OV_THRESHOLD_V);
    ic[i].tx_cfgb.vuv = SetUnderVoltageThreshold(BMS_UV_THRESHOLD_V);
  }

  printf("\nBMS controller: STM32F105, %u x ADBMS6830, %u cells, %u ms cycle\n",
         TOTAL_IC, BMS_CELL_COUNT, BMS_CYCLE_MS);
  if (can_tx_init(&hcan2))
  {
    report_can_timing(hcan2.Instance);
  }
  else
  {
    printf("CAN2 did not start: telemetry off\n");
  }
}

void bms_monitor_run(void)
{
  uint32_t now = HAL_GetTick();
  if ((now - last_cycle_ms) < BMS_CYCLE_MS)
  {
    return;
  }
  last_cycle_ms = now;

  bool status_cycle = (cycle_count % BMS_STATUS_EVERY) == 0U;
  uint16_t status_failed = 0U;
  if (chain_state == CHAIN_INIT)
  {
    start_chain();
    invalid_ics = ALL_ICS;
  }
  else
  {
    status_failed = measure(status_cycle, (cycle_count % BMS_CONFIG_CHECK_EVERY) == 0U);
  }
  count_pec_failures();
  send_frames(status_cycle, status_failed);
  if ((cycle_count % BMS_REPORT_EVERY) == 0U)
  {
    report();
  }
  cycle_count++;
}
