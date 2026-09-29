/* Host stand-in for Core/Inc/main.h: only the HAL and CMSIS pieces that the
   host-tested code uses. Values match the STM32F1 HAL/CMSIS headers where
   they matter to the tests. */
#ifndef FAKE_MAIN_H
#define FAKE_MAIN_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

#define DISABLE 0U

/* System ------------------------------------------------------------------ */
extern uint32_t SystemCoreClock;
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t Delay);
uint32_t HAL_RCC_GetPCLK1Freq(void);

/* GPIO and SPI (the isoSPI port) ------------------------------------------ */
typedef struct
{
  uint32_t ODR;
} GPIO_TypeDef;

typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;

typedef struct
{
  uint32_t ErrorCode;
} SPI_HandleTypeDef;

extern GPIO_TypeDef fake_gpioa;
#define SPI1_NSS_Pin       0x0010U
#define SPI1_NSS_GPIO_Port (&fake_gpioa)

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout);

/* DWT cycle counter. DWT goes through fake_dwt() so a test can model the
   counter running only while both enable bits are set. */
typedef struct
{
  uint32_t CTRL;
  uint32_t CYCCNT;
} DWT_Type;

typedef struct
{
  uint32_t DHCSR;
  uint32_t DEMCR;
} CoreDebug_Type;

#define DWT_CTRL_CYCCNTENA_Msk        (1UL << 0)
#define CoreDebug_DHCSR_C_DEBUGEN_Msk (1UL << 0)
#define CoreDebug_DEMCR_TRCENA_Msk    (1UL << 24)

DWT_Type *fake_dwt(void);
extern CoreDebug_Type fake_core_debug;
#define DWT       (fake_dwt())
#define CoreDebug (&fake_core_debug)

/* ITM stimulus port 0, the SWO output --------------------------------------- */
typedef struct
{
  union
  {
    volatile uint8_t  u8;
    volatile uint32_t u32;              /* reads 0 while the port's FIFO is full */
  } PORT[1];
  volatile uint32_t TER;
  volatile uint32_t TCR;
} ITM_Type;

#define ITM_TCR_ITMENA_Msk (1UL << 0)

extern ITM_Type fake_itm;
#define ITM (&fake_itm)

/* CMSIS's ITM_SendChar(): with ITM and port 0 enabled it waits, with no limit,
   for the port to take the character. */
static inline uint32_t ITM_SendChar(uint32_t ch)
{
  if (((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0UL) && ((ITM->TER & 1UL) != 0UL))
  {
    while (ITM->PORT[0U].u32 == 0UL)
    {
    }
    ITM->PORT[0U].u8 = (uint8_t)ch;
  }
  return ch;
}

/* bxCAN ------------------------------------------------------------------- */
typedef struct
{
  uint32_t TSR;
  uint32_t ESR;
  uint32_t BTR;
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

#define CAN_TSR_RQCP0           (1U << 0)
#define CAN_TSR_RQCP1           (1U << 8)
#define CAN_TSR_RQCP2           (1U << 16)
#define CAN_TSR_ABRQ0           (1U << 7)
#define CAN_TSR_ABRQ1           (1U << 15)
#define CAN_TSR_ABRQ2           (1U << 23)

#define CAN_ESR_EPVF_Pos        1U
#define CAN_ESR_EPVF            (0x1UL << CAN_ESR_EPVF_Pos)
#define CAN_ESR_BOFF_Pos        2U
#define CAN_ESR_BOFF            (0x1UL << CAN_ESR_BOFF_Pos)
#define CAN_ESR_TEC_Pos         16U
#define CAN_ESR_TEC             (0xFFUL << CAN_ESR_TEC_Pos)
#define CAN_ESR_REC_Pos         24U
#define CAN_ESR_REC             (0xFFUL << CAN_ESR_REC_Pos)

#define CAN_BTR_BRP_Pos         0U
#define CAN_BTR_BRP             (0x3FFUL << CAN_BTR_BRP_Pos)
#define CAN_BTR_TS1_Pos         16U
#define CAN_BTR_TS1             (0xFUL << CAN_BTR_TS1_Pos)
#define CAN_BTR_TS2_Pos         20U
#define CAN_BTR_TS2             (0x7UL << CAN_BTR_TS2_Pos)

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

/* Interrupt masking --------------------------------------------------------- */
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t primask) { (void)primask; }

#endif /* FAKE_MAIN_H */
