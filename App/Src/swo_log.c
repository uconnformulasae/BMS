/**
  ******************************************************************************
  * @file    swo_log.c
  * @brief   ITM / SWO printf output.
  ******************************************************************************
  */
#include "main.h"

int __io_putchar(int ch)
{
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U)
  {
    ITM_SendChar((uint32_t)ch);
  }
  return ch;
}
