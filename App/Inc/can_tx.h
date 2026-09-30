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
  uint32_t sent;
  uint32_t dropped;
} can_tx_counters_t;

bool can_tx_init(CAN_HandleTypeDef *hcan);
void can_tx_begin_cycle(void);
void can_tx_send(const can_frame_t *frame);
can_tx_counters_t can_tx_counters(void);

#endif /* CAN_TX_H */
