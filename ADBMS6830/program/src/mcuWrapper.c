/*******************************************************************************
Copyright (c) 2020 - Analog Devices Inc. All Rights Reserved.
This software is proprietary & confidential to Analog Devices, Inc.
and its licensor.
******************************************************************************
* @file:    mcuWrapper.c
* @brief:   BMS SPI driver functions
* @version: $Revision$
* @date:    $Date$
* Developed by: ADIBMS Software team, Bangalore, India
*****************************************************************************/
/*! \addtogroup MCU DRIVER
*  @{
*/

/*! @addtogroup Mcu Driver
*  @{
*/
#include "common.h"
#include "mcuWrapper.h"

#define SPI_TIME_OUT 10U                        /* SPI time out (ms); a failed transfer then fails PEC */
#define WAKEUP_PULSE_US 500U                    /* CS low and high time per IC: tWAKE max, datasheet Table 7 */
#define WAKEUP_QUIET_US 2000U                   /* chain counts as awake this long after a CS rising edge */

SPI_HandleTypeDef *hspi         = &hspi1;       /* MUC SPI Handler      */

static uint32_t last_cs_high;                   /* DWT cycle count at the last CS rising edge */
static bool chain_touched;                      /* false until the first transaction */

static uint32_t usToCycles(uint32_t us)
{
  return us * (SystemCoreClock / 1000000U);
}

/* The DWT cycle count, re-enabling the counter first: a debugger attaching,
   detaching or setting up SWV can clear its enable bits, and the wake-up
   timing must never wait on a stopped counter. */
static uint32_t cycleCount(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  return DWT->CYCCNT;
}

static void Delay_us(uint32_t us)
{  
  uint32_t start = cycleCount();
  while ((cycleCount() - start) < usToCycles(us))
  {
  }
}

/**
 *******************************************************************************
 * Function: Delay_ms
 * @brief Delay mili second
 *
 * @details This function insert delay in ms.
 *     
 * Parameters:
 * @param [in]  delay   Delay_ms
 *
 * @return None
 *
 *******************************************************************************
*/
void Delay_ms(uint32_t delay)
{
  HAL_Delay(delay);
}

/**
 *******************************************************************************
 * Function: adBmsCsLow
 * @brief Select chip select low
 *
 * @details This function does spi chip select low.
 *
 * @return None
 *
 *******************************************************************************
*/
void adBmsCsLow()
{
  HAL_GPIO_WritePin(GPIO_PORT, CS_PIN, GPIO_PIN_RESET);
}

/**
 *******************************************************************************
 * Function: adBmsCsHigh
 * @brief Select chip select High
 *
 * @details This function does spi chip select high and notes when the chain last saw traffic.
 *
 * @return None
 *
 *******************************************************************************
*/
void adBmsCsHigh()
{
  HAL_GPIO_WritePin(GPIO_PORT, CS_PIN, GPIO_PIN_SET);
  last_cs_high = cycleCount();
  chain_touched = true;
}

/**
 *******************************************************************************
 * Function: spiWriteBytes
 * @brief Writes an array of bytes out of the SPI port.
 *
 * @details This function wakeup bms ic in IsoSpi mode send dumy byte data in spi line..
 *
 * @param [in]  size            Numberof bytes to be send on the SPI line
 *
 * @param [in]  *tx_Data    Tx data pointer 
 *
 * @return None
 *
 *******************************************************************************
*/
void spiWriteBytes
( 
uint16_t size,                     /*Option: Number of bytes to be written on the SPI port*/
uint8_t *tx_Data                       /*Array of bytes to be written on the SPI port*/
)
{
  HAL_SPI_Transmit(hspi, tx_Data, size, SPI_TIME_OUT); /* SPI1 , data, size, timeout */ 
}

/**
 *******************************************************************************
 * Function: spiWriteReadBytes
 * @brief Writes and read a set number of bytes using the SPI port.
 *
 * @details This function writes and read a set number of bytes using the SPI port.
 *
 * @param [in]  *tx_data    Tx data pointer
 *
 * @param [in]  *rx_data    Rx data pointer 
 *
 * @param [in]  size            Data size 
 *
 * @return None
 *
 *******************************************************************************
*/
void spiWriteReadBytes
(
uint8_t *tx_data,                   /*array of data to be written on SPI port*/
uint8_t *rx_data,                   /*Input: array that will store the data read by the SPI port*/
uint16_t size                           /*Option: number of bytes*/
)
{
  HAL_SPI_Transmit(hspi, tx_data, 4, SPI_TIME_OUT);
  HAL_SPI_Receive(hspi, rx_data, size, SPI_TIME_OUT);
}

/**
 *******************************************************************************
 * Function: spiReadBytes
 * @brief Read number of bytes using the SPI port.
 *
 * @details This function Read a set number of bytes using the SPI port.
 *
 * @param [in]  size            Data size 
 *
 * @param [in]  *rx_data    Rx data pointer
 * 
 * @return None
 *
 *******************************************************************************
*/
void spiReadBytes(uint16_t size, uint8_t *rx_data)
{   
  HAL_SPI_Receive(hspi, rx_data, size, SPI_TIME_OUT);
}

/**
 *******************************************************************************
 * Function: adBmsForceWakeupIc
 * @brief Wakeup bms ic using chip select, unconditionally
 *
 * @details This function sends one chip select pulse per ic, each pulse long enough to
 *          wake one device, so the whole daisy chain wakes in sequence.
 *
 * @param [in]  total_ic    Total_ic
 *
 * @return None
 *
 *******************************************************************************
*/
void adBmsForceWakeupIc(uint8_t total_ic)
{   
  for (uint8_t ic = 0; ic < total_ic; ic++)
  {
    adBmsCsLow();
    Delay_us(WAKEUP_PULSE_US);
    adBmsCsHigh();
    Delay_us(WAKEUP_PULSE_US);
  }
}

/**
 *******************************************************************************
 * Function: adBmsWakeupIc
 * @brief Wakeup bms ic using chip select
 *
 * @details This function wakes the chain unless it saw traffic within WAKEUP_QUIET_US,
 *          so back-to-back transactions in one measurement cycle pay for one wake-up.
 *
 * @param [in]  total_ic    Total_ic
 *
 * @return None
 *
 *******************************************************************************
*/
void adBmsWakeupIc(uint8_t total_ic)
{
  bool awake = chain_touched && ((cycleCount() - last_cs_high) < usToCycles(WAKEUP_QUIET_US));
  if (!awake)
  {
    adBmsForceWakeupIc(total_ic);
  }
}

/** @}*/
/** @}*/
