/**
  ******************************************************************************
  * @file    can_tx.c
  * @brief   Non-blocking CAN transmit queue. bxCAN has three TX mailboxes, so
  *          frames wait in a ring and the TX-mailbox-empty interrupt moves them
  *          into mailboxes as they free up.
  ******************************************************************************
  */
#include "can_tx.h"

#define QUEUE_LEN 64U   /* power of two, so free-running indices survive wrap-around */

#define TX_UNSENT_ERRORS (HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_TERR0 | \
                          HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_TERR1 | \
                          HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR2)

static CAN_HandleTypeDef *can;          /* NULL until can_tx_init() succeeds */
static can_frame_t queue[QUEUE_LEN];
static uint32_t head;                   /* next slot to fill, free-running */
static uint32_t tail;                   /* next slot to send, free-running */
static can_tx_counters_t counters;

/* Moves queued frames into free mailboxes. Call with interrupts masked or from
   the CAN TX interrupt. */
static void pump(void)
{
  while ((tail != head) && (HAL_CAN_GetTxMailboxesFreeLevel(can) > 0U))
  {
    const can_frame_t *frame = &queue[tail % QUEUE_LEN];
    const CAN_TxHeaderTypeDef header = {
      .StdId = frame->id,
      .IDE = CAN_ID_STD,
      .RTR = CAN_RTR_DATA,
      .DLC = frame->dlc,
      .TransmitGlobalTime = DISABLE,
    };
    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(can, &header, frame->data, &mailbox) != HAL_OK)
    {
      return;
    }
    tail++;
  }
}

/* True while a mailbox has finished but the TX interrupt has not yet counted
   it: loading that mailbox now would clear its completion flags unseen. */
static bool completion_pending(void)
{
  return (can->Instance->TSR & (CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2)) != 0U;
}

static void mailbox_done(CAN_HandleTypeDef *hcan, bool transmitted)
{
  if (hcan != can)
  {
    return;
  }
  if (transmitted)
  {
    counters.sent++;
  }
  else
  {
    counters.dropped++;
  }
  pump();
}

bool can_tx_init(CAN_HandleTypeDef *hcan)
{
  can = NULL;
  head = 0U;
  tail = 0U;
  counters = (can_tx_counters_t){0};

  /* Nothing is received yet, but one bank must be written so the shared filter
     block leaves init mode. Banks 14..27 belong to CAN2. */
  const CAN_FilterTypeDef filter = {
    .FilterBank = 14U,
    .FilterMode = CAN_FILTERMODE_IDMASK,
    .FilterScale = CAN_FILTERSCALE_32BIT,
    .FilterFIFOAssignment = CAN_RX_FIFO0,
    .FilterActivation = CAN_FILTER_DISABLE,
    .SlaveStartFilterBank = 14U,
  };
  if ((HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK) ||
      (HAL_CAN_ActivateNotification(hcan, CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK) ||
      (HAL_CAN_Start(hcan) != HAL_OK))
  {
    return false;
  }
  can = hcan;
  return true;
}

void can_tx_begin_cycle(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  counters.dropped += head - tail;
  tail = head;
  if (can != NULL)
  {
    /* Pending mailboxes hold last cycle's frames. A plain write, because HAL's
       read-modify-write of TSR would also clear completion flags that the TX
       interrupt has not handled yet. */
    can->Instance->TSR = CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;
  }
  __set_PRIMASK(primask);
}

void can_tx_send(const can_frame_t *frame)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if ((can == NULL) || ((head - tail) == QUEUE_LEN))
  {
    counters.dropped++;
  }
  else
  {
    queue[head % QUEUE_LEN] = *frame;
    head++;
    if (!completion_pending())
    {
      pump();                           /* otherwise the pending TX interrupt counts, then refills */
    }
  }
  __set_PRIMASK(primask);
}

can_tx_counters_t can_tx_counters(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  can_tx_counters_t snapshot = counters;
  __set_PRIMASK(primask);
  return snapshot;
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) { mailbox_done(hcan, true); }
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) { mailbox_done(hcan, true); }
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) { mailbox_done(hcan, true); }
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan) { mailbox_done(hcan, false); }
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan) { mailbox_done(hcan, false); }
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan) { mailbox_done(hcan, false); }

/* With auto-retransmission on, a mailbox only ends unsent when an abort lands
   after a failed attempt, and HAL reports that as a TX error, not an abort. */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan != can)
  {
    return;
  }
  counters.dropped += (uint32_t)__builtin_popcount(HAL_CAN_GetError(hcan) & TX_UNSENT_ERRORS);
  HAL_CAN_ResetError(hcan);
  pump();
}
