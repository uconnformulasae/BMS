/**
  ******************************************************************************
  * @file    swo_log.c
  * @brief   Sends printf output to the SWO pin through ITM stimulus port 0. The
  *          stock syscalls.c _write() calls __io_putchar() once per character.
  ******************************************************************************
  */
#include "main.h"

int __io_putchar(int ch)
{
  ITM_SendChar((uint32_t)ch);   /* returns at once when no debugger has enabled ITM */
  return ch;
}
