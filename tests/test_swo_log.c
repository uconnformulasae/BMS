/* Host tests for App/Src/swo_log.c. Run: make -C tests */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "main.h"

int __io_putchar(int ch);

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

ITM_Type fake_itm;
CoreDebug_Type fake_core_debug;

static void waited_forever(int signal_number)
{
  static const char message[] = "swo_log: printf waited forever on an ITM port nothing drains\n";
  (void)signal_number;
  (void)!write(STDOUT_FILENO, message, sizeof message - 1U);
  _exit(1);
}

static void test_output_never_waits_without_a_debugger(void)
{
  fake_core_debug.DHCSR = 0U;                  /* the probe has disconnected ... */
  fake_itm.TCR = ITM_TCR_ITMENA_Msk;           /* ... leaving ITM and port 0 enabled */
  fake_itm.TER = 1U;
  fake_itm.PORT[0].u32 = 0U;                   /* and the port full, with nothing draining it */
  signal(SIGALRM, waited_forever);
  alarm(2);
  CHECK_EQ(__io_putchar('A'), 'A');
  alarm(0);
}

static void test_output_goes_to_itm_port_0_while_a_debugger_is_attached(void)
{
  fake_core_debug.DHCSR = CoreDebug_DHCSR_C_DEBUGEN_Msk;
  fake_itm.TCR = ITM_TCR_ITMENA_Msk;
  fake_itm.TER = 1U;
  fake_itm.PORT[0].u32 = 1U;                   /* port 0 ready */
  CHECK_EQ(__io_putchar('B'), 'B');
  CHECK_EQ(fake_itm.PORT[0].u8, 'B');
}

int main(void)
{
  test_output_never_waits_without_a_debugger();
  test_output_goes_to_itm_port_0_while_a_debugger_is_attached();
  if (failures != 0)
  {
    printf("swo_log: %d check(s) failed\n", failures);
    return 1;
  }
  printf("swo_log: all tests passed\n");
  return 0;
}
