/* Host stand-in for ADI's ADBMS6830 driver: a scripted model of the isoSPI
   chain that the bms_monitor tests drive. */
#ifndef FAKE_ADBMS_H
#define FAKE_ADBMS_H

#include "adbms_main.h"

#define FAKE_MAX_IC 16U

typedef struct
{
  int16_t  cells[FAKE_MAX_IC][CELL];   /* cell result registers */
  int16_t  die_temp;                   /* ITMP after an aux conversion of TEMP */
  bool     die_temp_converted;         /* until then ITMP holds its power-on 0x8000 */
  cfa_     cfga[FAKE_MAX_IC];          /* configuration registers as last written */
  cfb_     cfgb[FAKE_MAX_IC];
  uint16_t cell_pec_fail;              /* ICs whose cell reads fail PEC ... */
  uint32_t cell_pec_fail_reads;        /* ... for this many more group reads */
  uint16_t status_pec_fail;            /* ICs whose status reads fail PEC */
  bool     absent;                     /* nothing on the chain answers */
  uint32_t forced_wakes;
} fake_chain_t;

extern fake_chain_t fake_chain;

void fake_chain_reset(void);

#endif /* FAKE_ADBMS_H */
