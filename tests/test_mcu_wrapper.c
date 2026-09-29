/* Host tests for ADBMS6830/program/src/mcuWrapper.c. Run: make -C tests */
#define _POSIX_C_SOURCE 200809L

#include "mcuWrapper.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

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

SPI_HandleTypeDef hspi1;
GPIO_TypeDef fake_gpioa;
CoreDebug_Type fake_core_debug;
uint32_t SystemCoreClock = 16000000U;

static DWT_Type dwt_regs;
static unsigned cs_edges;

/* Every access to DWT advances the counter while both enable bits are set, as
   the real counter keeps running between reads; otherwise it holds still. */
DWT_Type *fake_dwt(void)
{
  if (((fake_core_debug.DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
      ((dwt_regs.CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U))
  {
    dwt_regs.CYCCNT += 100U;
  }
  return &dwt_regs;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
  (void)GPIOx;
  (void)GPIO_Pin;
  (void)PinState;
  cs_edges++;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  (void)hspi;
  (void)pData;
  (void)Size;
  (void)Timeout;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  (void)hspi;
  (void)pData;
  (void)Size;
  (void)Timeout;
  return HAL_OK;
}

void HAL_Delay(uint32_t Delay)
{
  (void)Delay;
}

static void spun_forever(int signal_number)
{
  static const char message[] = "mcu_wrapper: wake-up spun forever on a stopped cycle counter\n";
  (void)signal_number;
  (void)!write(STDOUT_FILENO, message, sizeof message - 1U);
  _exit(1);
}

/* Starts the fake cycle counter, as cycleCount() leaves it on hardware. */
static void count_cycles(void)
{
  fake_core_debug.DEMCR = CoreDebug_DEMCR_TRCENA_Msk;
  dwt_regs.CTRL = DWT_CTRL_CYCCNTENA_Msk;
}

/* Must run first: nothing has touched the chain yet. */
static void test_first_wakeup_wakes_the_chain(void)
{
  count_cycles();
  dwt_regs.CYCCNT = 0U;                        /* just after reset: within 2 ms of the last CS rising edge's initial 0 */
  cs_edges = 0U;
  adBmsWakeupIc(2);
  CHECK_EQ(cs_edges, 4);
}

static void test_wakeup_is_skipped_while_the_chain_is_awake(void)
{
  count_cycles();
  adBmsForceWakeupIc(2);                       /* the chain has just seen chip select */
  cs_edges = 0U;
  adBmsWakeupIc(2);
  CHECK_EQ(cs_edges, 0);
}

static void test_wakeup_wakes_a_chain_quiet_for_over_2_ms(void)
{
  count_cycles();
  adBmsForceWakeupIc(2);
  dwt_regs.CYCCNT += 2000U * 16U;              /* 2 ms at 16 MHz */
  cs_edges = 0U;
  adBmsWakeupIc(2);
  CHECK_EQ(cs_edges, 4);
}

static void test_wakeup_restarts_a_cycle_counter_a_debugger_stopped(void)
{
  fake_core_debug.DEMCR = 0U;                  /* as a debugger detaching or setting up SWV can leave it */
  dwt_regs.CTRL = 0U;
  cs_edges = 0U;
  signal(SIGALRM, spun_forever);
  alarm(2);
  adBmsForceWakeupIc(2);
  alarm(0);
  CHECK_EQ(cs_edges, 4);                       /* chip select low then high, once per IC */
}

int main(void)
{
  test_first_wakeup_wakes_the_chain();
  test_wakeup_is_skipped_while_the_chain_is_awake();
  test_wakeup_wakes_a_chain_quiet_for_over_2_ms();
  test_wakeup_restarts_a_cycle_counter_a_debugger_stopped();
  if (failures != 0)
  {
    printf("mcu_wrapper: %d check(s) failed\n", failures);
    return 1;
  }
  printf("mcu_wrapper: all tests passed\n");
  return 0;
}
