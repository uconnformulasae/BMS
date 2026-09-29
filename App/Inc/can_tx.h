/**
  ******************************************************************************
  * @file    can_tx.h
  * @brief   Non-blocking transmit queue for one bxCAN controller (CAN2).
  ******************************************************************************
  */
#ifndef CAN_TX_H
#define CAN_TX_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frames.h"
#include "main.h"

typedef struct
{
  uint32_t sent;      /* frames the controller reported transmitted */
  uint32_t dropped;   /* queue full, stale at a new cycle, or aborted unsent */
} can_tx_counters_t;

/* Resets the queue and starts hcan with TX-mailbox-empty interrupts and no
   reception. Returns false if the controller did not start; frames are then
   dropped (and counted) instead of sent. */
bool can_tx_init(CAN_HandleTypeDef *hcan);

/* Drops whatever the previous cycle left queued or pending. Call before each
   cycle's sends. */
void can_tx_begin_cycle(void);

/* Queues a frame. Never blocks. */
void can_tx_send(const can_frame_t *frame);

can_tx_counters_t can_tx_counters(void);

#endif /* CAN_TX_H */
