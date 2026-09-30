/* Host fake for ADBMS6830 driver */
#ifndef FAKE_ADBMS_H
#define FAKE_ADBMS_H

#include "adbms_main.h"

#define FAKE_MAX_IC 16U

typedef struct
{
  int16_t  cells[FAKE_MAX_IC][CELL];
  int16_t  die_temp;
  bool     die_temp_converted;
  stc_     statc[FAKE_MAX_IC];
  std_     statd[FAKE_MAX_IC];
  cfa_     cfga[FAKE_MAX_IC];
  cfb_     cfgb[FAKE_MAX_IC];
  uint16_t cell_pec_fail;
  uint32_t cell_pec_fail_reads;
  uint16_t status_pec_fail;
  bool     absent;
  uint32_t forced_wakes;
} fake_chain_t;

extern fake_chain_t fake_chain;

void fake_chain_reset(void);

#endif /* FAKE_ADBMS_H */
