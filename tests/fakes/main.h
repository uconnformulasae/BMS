/* Host stand-in for Core/Inc/main.h: only the HAL and CMSIS pieces can_tx.c
   uses. Values match stm32f1xx_hal_can.h where they matter to the tests. */
#ifndef FAKE_MAIN_H
#define FAKE_MAIN_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

#define DISABLE 0U

typedef struct
{
  uint32_t TSR;
} CAN_TypeDef;

typedef struct
{
  CAN_TypeDef *Instance;
  uint32_t ErrorCode;
} CAN_HandleTypeDef;

typedef struct
{
  uint32_t StdId;
  uint32_t ExtId;
  uint32_t IDE;
  uint32_t RTR;
  uint32_t DLC;
  uint32_t TransmitGlobalTime;
} CAN_TxHeaderTypeDef;

typedef struct
{
  uint32_t FilterIdHigh;
  uint32_t FilterIdLow;
  uint32_t FilterMaskIdHigh;
  uint32_t FilterMaskIdLow;
  uint32_t FilterFIFOAssignment;
  uint32_t FilterBank;
  uint32_t FilterMode;
  uint32_t FilterScale;
  uint32_t FilterActivation;
  uint32_t SlaveStartFilterBank;
} CAN_FilterTypeDef;

#define CAN_ID_STD              0x00000000U
#define CAN_RTR_DATA            0x00000000U
#define CAN_FILTERMODE_IDMASK   0x00000000U
#define CAN_FILTERSCALE_32BIT   0x00000001U
#define CAN_RX_FIFO0            0x00000000U
#define CAN_FILTER_DISABLE      0x00000000U
#define CAN_IT_TX_MAILBOX_EMPTY 0x00000001U

#define CAN_TSR_ABRQ0           (1U << 7)
#define CAN_TSR_ABRQ1           (1U << 15)
#define CAN_TSR_ABRQ2           (1U << 23)

#define HAL_CAN_ERROR_TX_ALST0  0x00000800U
#define HAL_CAN_ERROR_TX_TERR0  0x00001000U
#define HAL_CAN_ERROR_TX_ALST1  0x00002000U
#define HAL_CAN_ERROR_TX_TERR1  0x00004000U
#define HAL_CAN_ERROR_TX_ALST2  0x00008000U
#define HAL_CAN_ERROR_TX_TERR2  0x00010000U

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, const CAN_FilterTypeDef *sFilterConfig);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan, uint32_t ActiveITs);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, const CAN_TxHeaderTypeDef *pHeader,
                                       const uint8_t aData[], uint32_t *pTxMailbox);
uint32_t HAL_CAN_GetError(const CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_ResetError(CAN_HandleTypeDef *hcan);

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan);

static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t primask) { (void)primask; }

#endif /* FAKE_MAIN_H */
