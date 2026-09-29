/**
  ******************************************************************************
  * @file    swo_log.c
  * @brief   Sends printf output to the SWO pin through ITM stimulus port 0 while
  *          a debugger is attached. The stock syscalls.c _write() calls
  *          __io_putchar() once per character.
  ******************************************************************************
  */
#include "main.h"

int __io_putchar(int ch)
{
  /* Nothing reads SWO without a debugger, and a probe that disconnects can
     leave ITM enabled with nothing draining it, where ITM_SendChar() would
     wait forever. */
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U)
  {
    ITM_SendChar((uint32_t)ch);
  }
  return ch;
}
