/* Host unit tests for App/Src/can_frames.c. Run: make -C tests */
#include "can_frames.h"

#include <stdio.h>
#include <string.h>

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

static void test_cell_voltage_conversion(void)
{
  CHECK_EQ(can_frames_cell_0p1mv(0), 15000);                   /* code 0 is 1.5000 V */
  CHECK_EQ(can_frames_cell_0p1mv(14746), 37119);               /* 3.71190 V exactly */
  CHECK_EQ(can_frames_cell_0p1mv(14747), 37121);               /* 3.712050 V rounds half up */
  CHECK_EQ(can_frames_cell_0p1mv(32767), 64151);               /* top of range stays below NO_DATA */
  CHECK_EQ(can_frames_cell_0p1mv(-10000), 0);                  /* exactly 0 V */
  CHECK_EQ(can_frames_cell_0p1mv(-20000), 0);                  /* negative clamps to 0 V */
  CHECK_EQ(can_frames_cell_0p1mv(-32767), 0);                  /* most negative real reading */
  CHECK_EQ(can_frames_cell_0p1mv(INT16_MIN), CAN_FRAMES_NO_DATA); /* 0x8000: no conversion ran */
}

static void test_die_temperature(void)
{
  CHECK_EQ(can_frames_die_temp_0p1c(4900), 250);               /* 2.235 V is 25.0 C */
  CHECK_EQ(can_frames_die_temp_0p1c(0), -730);                 /* 1.5 V is -73.0 C */
  CHECK_EQ(can_frames_die_temp_0p1c(INT16_MIN), CAN_FRAMES_TEMP_UNKNOWN);
}

static void test_any_flag_only_counts_masked_channels(void)
{
  uint8_t flags[CAN_FRAMES_CHANNELS_PER_IC] = {0};
  CHECK_EQ(can_frames_any_flag(flags, 0xFFFF), 0);
  flags[12] = 1;                                               /* channel 13: unpopulated on a 9-cell IC */
  CHECK_EQ(can_frames_any_flag(flags, 0x01FF), 0);
  CHECK_EQ(can_frames_any_flag(flags, 0xFFFF), 1);
  flags[3] = 1;
  CHECK_EQ(can_frames_any_flag(flags, 0x01FF), 1);
}

static void test_map_cells_orders_by_ic_then_channel(void)
{
  int16_t codes[2][CAN_FRAMES_CHANNELS_PER_IC];
  for (int ic = 0; ic < 2; ic++)
  {
    for (int ch = 0; ch < (int)CAN_FRAMES_CHANNELS_PER_IC; ch++)
    {
      codes[ic][ch] = (int16_t)(ic * 100 + ch);
    }
  }
  uint16_t cells[32];

  CHECK_EQ(can_frames_map_cells(&codes[0][0], 2, 0xFFFF, 0x0000, cells), 32);
  CHECK_EQ(cells[0], can_frames_cell_0p1mv(0));
  CHECK_EQ(cells[15], can_frames_cell_0p1mv(15));
  CHECK_EQ(cells[16], can_frames_cell_0p1mv(100));
  CHECK_EQ(cells[31], can_frames_cell_0p1mv(115));

  /* 9 populated channels per IC: channels 10..16 are skipped. */
  CHECK_EQ(can_frames_map_cells(&codes[0][0], 2, 0x01FF, 0x0000, cells), 18);
  CHECK_EQ(cells[8], can_frames_cell_0p1mv(8));
  CHECK_EQ(cells[9], can_frames_cell_0p1mv(100));
}

static void test_map_cells_marks_invalid_ic(void)
{
  int16_t codes[2][CAN_FRAMES_CHANNELS_PER_IC] = {{0}};
  uint16_t cells[32];
  CHECK_EQ(can_frames_map_cells(&codes[0][0], 2, 0xFFFF, 0x0002, cells), 32);
  CHECK_EQ(cells[15], 15000);                                  /* IC 0 still valid */
  CHECK_EQ(cells[16], CAN_FRAMES_NO_DATA);                     /* IC 1 invalid */
  CHECK_EQ(cells[31], CAN_FRAMES_NO_DATA);
}

static void test_cell_frame_layout(void)
{
  uint16_t cells[18];
  for (int i = 0; i < 18; i++)
  {
    cells[i] = (uint16_t)(0x1100 + i);
  }
  can_frame_t frames[5];
  CHECK_EQ(can_frames_build_cells(cells, 18, 0x700, frames), 5);
  CHECK_EQ(frames[0].id, 0x700);
  CHECK_EQ(frames[4].id, 0x704);
  CHECK_EQ(frames[0].dlc, 8);
  CHECK_EQ(frames[0].data[0], 0x00);                           /* cell 1 = 0x1100, little-endian */
  CHECK_EQ(frames[0].data[1], 0x11);
  CHECK_EQ(frames[0].data[6], 0x03);                           /* cell 4 = 0x1103 */
  CHECK_EQ(frames[4].data[2], 0x11);                           /* cell 18 = 0x1111 */
  CHECK_EQ(frames[4].data[3], 0x11);
  CHECK_EQ(frames[4].data[4], 0xFF);                           /* unused slots carry NO_DATA */
  CHECK_EQ(frames[4].data[7], 0xFF);
  CHECK_EQ(can_frames_build_cells(cells, 0, 0x700, frames), 0);
}

static void test_ic_frame_layout(void)
{
  can_frames_ic_t ic = {
    .index = 1,
    .status = CAN_FRAMES_IC_ANY_UV | CAN_FRAMES_IC_THSD,
    .cmd_count = 37,
    .pec_fail_run = 255,
    .die_temp_0p1c = -123,
    .balance_mask = 0x1234,
  };
  can_frame_t frame;
  can_frames_build_ic(&ic, 0x720, &frame);
  CHECK_EQ(frame.id, 0x721);
  CHECK_EQ(frame.dlc, 8);
  CHECK_EQ(frame.data[0], 1);
  CHECK_EQ(frame.data[1], 0x14);
  CHECK_EQ(frame.data[2], 37);
  CHECK_EQ(frame.data[3], 255);
  CHECK_EQ(frame.data[4], 0x85);                               /* -123 = 0xFF85, little-endian */
  CHECK_EQ(frame.data[5], 0xFF);
  CHECK_EQ(frame.data[6], 0x34);
  CHECK_EQ(frame.data[7], 0x12);
}

static void test_status_frame_layout(void)
{
  can_frame_t frame;
  memset(&frame, 0xAA, sizeof frame);                          /* reserved bytes must be cleared */
  can_frames_build_status(CAN_FRAMES_CHAIN_RUNNING, 255, 0x0102, 0x6FF, &frame);
  CHECK_EQ(frame.id, 0x6FF);
  CHECK_EQ(frame.dlc, 8);
  CHECK_EQ(frame.data[0], 1);
  CHECK_EQ(frame.data[1], 255);
  CHECK_EQ(frame.data[2], 0x02);
  CHECK_EQ(frame.data[3], 0x01);
  for (int i = 4; i < 8; i++)
  {
    CHECK_EQ(frame.data[i], 0);
  }
}

int main(void)
{
  test_cell_voltage_conversion();
  test_die_temperature();
  test_any_flag_only_counts_masked_channels();
  test_map_cells_orders_by_ic_then_channel();
  test_map_cells_marks_invalid_ic();
  test_cell_frame_layout();
  test_ic_frame_layout();
  test_status_frame_layout();
  if (failures != 0)
  {
    printf("can_frames: %d check(s) failed\n", failures);
    return 1;
  }
  printf("can_frames: all tests passed\n");
  return 0;
}
