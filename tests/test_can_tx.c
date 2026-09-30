/* Host unit tests for App/Src/can_tx.c against a fake HAL. Run: make -C tests */
#include "can_tx.h"

#include <stdio.h>

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

static CAN_TypeDef regs;
static CAN_HandleTypeDef hcan2 = { .Instance = &regs };
static CAN_HandleTypeDef other_can = { .Instance = &regs };
static uint32_t free_mailboxes;
static HAL_StatusTypeDef start_result;
static uint32_t mailbox_ids[128];
static uint32_t n_mailboxed;

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, const CAN_FilterTypeDef *sFilterConfig)
{
  (void)hcan;
  (void)sFilterConfig;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan, uint32_t ActiveITs)
{
  (void)hcan;
  (void)ActiveITs;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
  (void)hcan;
  return start_result;
}

uint32_t HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *hcan)
{
  (void)hcan;
  return free_mailboxes;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, const CAN_TxHeaderTypeDef *pHeader,
                                       const uint8_t aData[], uint32_t *pTxMailbox)
{
  (void)hcan;
  (void)aData;
  if (free_mailboxes == 0U)
  {
    return HAL_ERROR;
  }
  for (uint32_t rqcp = CAN_TSR_RQCP0; rqcp <= CAN_TSR_RQCP2; rqcp <<= 8)
  {
    if ((regs.TSR & rqcp) != 0U)
    {
      regs.TSR &= ~rqcp;
      break;
    }
  }
  free_mailboxes--;
  mailbox_ids[n_mailboxed++] = pHeader->StdId;
  *pTxMailbox = 0U;
  return HAL_OK;
}

uint32_t HAL_CAN_GetError(const CAN_HandleTypeDef *hcan)
{
  return hcan->ErrorCode;
}

HAL_StatusTypeDef HAL_CAN_ResetError(CAN_HandleTypeDef *hcan)
{
  hcan->ErrorCode = 0U;
  return HAL_OK;
}

static void run_tx_interrupt(void)
{
  static void (*const complete[3])(CAN_HandleTypeDef *) = {
    HAL_CAN_TxMailbox0CompleteCallback,
    HAL_CAN_TxMailbox1CompleteCallback,
    HAL_CAN_TxMailbox2CompleteCallback,
  };
  uint32_t tsr = regs.TSR;
  for (uint32_t m = 0; m < 3U; m++)
  {
    uint32_t rqcp = CAN_TSR_RQCP0 << (8U * m);
    if ((tsr & rqcp) != 0U)
    {
      regs.TSR &= ~rqcp;
      complete[m](&hcan2);
    }
  }
}

static void reset(uint32_t mailboxes)
{
  free_mailboxes = mailboxes;
  start_result = HAL_OK;
  n_mailboxed = 0U;
  regs.TSR = 0U;
  hcan2.ErrorCode = 0U;
  CHECK_EQ(can_tx_init(&hcan2), 1);
}

static void send_ids(uint16_t first_id, uint32_t count)
{
  for (uint32_t i = 0; i < count; i++)
  {
    can_frame_t frame = { .id = (uint16_t)(first_id + i), .dlc = 8U };
    can_tx_send(&frame);
  }
}

static void test_frames_go_straight_to_free_mailboxes(void)
{
  reset(3);
  send_ids(0x700, 2);
  CHECK_EQ(n_mailboxed, 2);
  CHECK_EQ(mailbox_ids[0], 0x700);
  CHECK_EQ(mailbox_ids[1], 0x701);
  HAL_CAN_TxMailbox0CompleteCallback(&hcan2);
  CHECK_EQ(can_tx_counters().sent, 1);
  CHECK_EQ(can_tx_counters().dropped, 0);
}

static void test_queued_frames_follow_in_order_as_mailboxes_free(void)
{
  reset(0);
  send_ids(0x700, 5);
  CHECK_EQ(n_mailboxed, 0);
  free_mailboxes = 1;
  HAL_CAN_TxMailbox1CompleteCallback(&hcan2);
  CHECK_EQ(n_mailboxed, 1);
  CHECK_EQ(mailbox_ids[0], 0x700);
  free_mailboxes = 1;
  HAL_CAN_TxMailbox2CompleteCallback(&hcan2);
  CHECK_EQ(mailbox_ids[1], 0x701);
}

static void test_full_queue_drops_instead_of_blocking(void)
{
  reset(0);
  send_ids(0x700, 70);
  CHECK_EQ(can_tx_counters().dropped, 6);
}

static void test_new_cycle_drops_stale_frames_and_aborts_mailboxes(void)
{
  reset(0);
  send_ids(0x700, 10);
  regs.TSR = CAN_TSR_RQCP0;
  can_tx_begin_cycle();
  CHECK_EQ(can_tx_counters().dropped, 10);
  CHECK_EQ(regs.TSR, CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2);
  free_mailboxes = 3;
  send_ids(0x6FF, 1);
  CHECK_EQ(n_mailboxed, 1);
  CHECK_EQ(mailbox_ids[0], 0x6FF);
}

static void test_aborted_mailboxes_count_as_dropped(void)
{
  reset(0);
  HAL_CAN_TxMailbox0AbortCallback(&hcan2);
  hcan2.ErrorCode = HAL_CAN_ERROR_TX_TERR0 | HAL_CAN_ERROR_TX_ALST2;
  HAL_CAN_ErrorCallback(&hcan2);
  CHECK_EQ(can_tx_counters().dropped, 3);
  CHECK_EQ(hcan2.ErrorCode, 0);
}

static void test_controller_that_did_not_start_drops_everything(void)
{
  free_mailboxes = 3;
  start_result = HAL_ERROR;
  n_mailboxed = 0U;
  CHECK_EQ(can_tx_init(&hcan2), 0);
  send_ids(0x700, 4);
  can_tx_begin_cycle();
  CHECK_EQ(n_mailboxed, 0);
  CHECK_EQ(can_tx_counters().dropped, 4);
}

static void test_completion_pending_during_a_send_is_still_counted(void)
{
  reset(1);
  regs.TSR = CAN_TSR_RQCP0;
  send_ids(0x700, 1);
  run_tx_interrupt();
  CHECK_EQ(can_tx_counters().sent, 1);
  CHECK_EQ(n_mailboxed, 1);
  CHECK_EQ(mailbox_ids[0], 0x700);
}

static void test_other_controllers_callbacks_are_ignored(void)
{
  reset(0);
  HAL_CAN_TxMailbox0CompleteCallback(&other_can);
  other_can.ErrorCode = HAL_CAN_ERROR_TX_TERR0;
  HAL_CAN_ErrorCallback(&other_can);
  CHECK_EQ(can_tx_counters().sent, 0);
  CHECK_EQ(can_tx_counters().dropped, 0);
}

int main(void)
{
  test_frames_go_straight_to_free_mailboxes();
  test_queued_frames_follow_in_order_as_mailboxes_free();
  test_full_queue_drops_instead_of_blocking();
  test_new_cycle_drops_stale_frames_and_aborts_mailboxes();
  test_aborted_mailboxes_count_as_dropped();
  test_controller_that_did_not_start_drops_everything();
  test_completion_pending_during_a_send_is_still_counted();
  test_other_controllers_callbacks_are_ignored();
  if (failures != 0)
  {
    printf("can_tx: %d check(s) failed\n", failures);
    return 1;
  }
  printf("can_tx: all tests passed\n");
  return 0;
}
