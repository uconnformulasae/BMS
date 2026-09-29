/* Host tests for App/Src/bms_monitor.c against a scripted chain (fake_adbms.c)
   and a CAN queue that records each cycle's frames. Run: make -C tests */
#include "bms_monitor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bms_config.h"
#include "can_tx.h"
#include "fake_adbms.h"

static int failures;

#define CHECK_EQ(actual, expected)                                                   \
  do                                                                                 \
  {                                                                                  \
    long long a_ = (long long)(actual);                                              \
    long long e_ = (long long)(expected);                                            \
    if (a_ != e_)                                                                    \
    {                                                                                \
      printf("%s:%d: %s is %lld, expected %lld\n", __FILE__, __LINE__, #actual, a_, e_); \
      failures++;                                                                    \
    }                                                                                \
  } while (0)

/* The HAL pieces the monitor touches. CAN2 is at 250 kbit/s: prescaler 4,
   13 + 2 time quanta. */
static uint32_t now_ms;
static CAN_TypeDef can2_regs = {
  .BTR = (3UL << CAN_BTR_BRP_Pos) | (12UL << CAN_BTR_TS1_Pos) | (1UL << CAN_BTR_TS2_Pos),
};
CAN_HandleTypeDef hcan2 = { .Instance = &can2_regs };

uint32_t HAL_GetTick(void)
{
  return now_ms;
}

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
  return 16000000U;
}

/* A CAN queue that keeps the frames of the latest cycle. */
static can_frame_t frames[64];
static size_t n_frames;

bool can_tx_init(CAN_HandleTypeDef *hcan)
{
  (void)hcan;
  return true;
}

void can_tx_begin_cycle(void)
{
  n_frames = 0U;
}

void can_tx_send(const can_frame_t *frame)
{
  frames[n_frames++] = *frame;
}

can_tx_counters_t can_tx_counters(void)
{
  return (can_tx_counters_t){0};
}

static const can_frame_t *sent_frame(uint16_t id)
{
  for (size_t f = 0; f < n_frames; f++)
  {
    if (frames[f].id == id)
    {
      return &frames[f];
    }
  }
  return NULL;
}

static unsigned frame_u16(uint16_t id, unsigned byte)
{
  const can_frame_t *frame = sent_frame(id);
  return (frame == NULL) ? 0xDEADU : (unsigned)(frame->data[byte] | (frame->data[byte + 1U] << 8));
}

/* The monitor's SWO output: bms_monitor.c is built with printf renamed to this. */
static char output[32768];
static size_t output_len;

int monitor_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int n = vsnprintf(&output[output_len], sizeof output - output_len, format, args);
  va_end(args);
  if (n > 0)
  {
    output_len += (size_t)n;
    if (output_len >= sizeof output)
    {
      output_len = sizeof output - 1U;
    }
  }
  return n;
}

static void boot(void)
{
  fake_chain_reset();
  now_ms = 0U;
  output_len = 0U;
  output[0] = '\0';
  bms_monitor_init();
}

static void run_cycles(unsigned count)
{
  for (unsigned c = 0; c < count; c++)
  {
    now_ms += BMS_CYCLE_MS;
    bms_monitor_run();
  }
}

static void test_init_starts_the_monitor_from_scratch(void)
{
  boot();
  run_cycles(3);
  boot();
  run_cycles(1);                                         /* a fresh INIT cycle, counted from zero */
  CHECK_EQ(frame_u16(BMS_CAN_STATUS_ID, 0) >> 8, 0);     /* cycle counter */
  CHECK_EQ(frame_u16(BMS_CAN_STATUS_ID, 2), 0x0003);     /* no IC has data yet */
}

static void test_die_temperature_reaches_the_per_ic_frame(void)
{
  boot();
  fake_chain.die_temp = 4900;                            /* 2.235 V is 25.0 C */
  run_cycles(1U + BMS_STATUS_EVERY);                     /* INIT, then to the first RUNNING status cycle */
  CHECK_EQ(frame_u16(BMS_CAN_IC_BASE_ID, 4), 250);
}

static void test_report_prints_cells_as_the_frames_carry_them(void)
{
  boot();
  fake_chain.cells[0][0] = 14747;                        /* 3.712050 V */
  run_cycles(1U + BMS_REPORT_EVERY);                     /* INIT, then to the first RUNNING report */
  CHECK_EQ(frame_u16(BMS_CAN_CELL_BASE_ID, 0), 37121);
  CHECK_EQ(strstr(output, "C1  3.7121V") != NULL, 1);
}

static void test_report_marks_a_failed_status_read(void)
{
  boot();
  run_cycles(BMS_REPORT_EVERY);                          /* up to the cycle before the report */
  fake_chain.status_pec_fail = 0x0002U;                  /* IC1's status reads fail PEC */
  output_len = 0U;
  run_cycles(1);
  CHECK_EQ(frame_u16(BMS_CAN_IC_BASE_ID + 1U, 0) >> 8, CAN_FRAMES_IC_PEC_FAIL);
  CHECK_EQ(strstr(output, "IC1  status read failed PEC") != NULL, 1);
  CHECK_EQ(strstr(output, "IC1  cmd count") == NULL, 1);
}

int main(void)
{
  test_init_starts_the_monitor_from_scratch();
  test_die_temperature_reaches_the_per_ic_frame();
  test_report_prints_cells_as_the_frames_carry_them();
  test_report_marks_a_failed_status_read();
  if (failures != 0)
  {
    printf("bms_monitor: %d check(s) failed\n", failures);
    return 1;
  }
  printf("bms_monitor: all tests passed\n");
  return 0;
}
