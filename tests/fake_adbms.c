/* Host stand-in for ADI's ADBMS6830 driver; see fake_adbms.h. */
#include "fake_adbms.h"

#include <string.h>

fake_chain_t fake_chain;

void fake_chain_reset(void)
{
  memset(&fake_chain, 0, sizeof fake_chain);
}

static uint8_t pec_error(uint16_t failing_ics, uint8_t ic)
{
  return (fake_chain.absent || (((failing_ics >> ic) & 1U) != 0U)) ? 1U : 0U;
}

void adBmsReadData(uint8_t tIC, cell_asic *ic, uint8_t cmd_arg[2], TYPE type, GRP group)
{
  (void)cmd_arg;
  uint16_t cell_failing = (fake_chain.cell_pec_fail_reads > 0U) ? fake_chain.cell_pec_fail : 0U;
  for (uint8_t i = 0; i < tIC; i++)
  {
    switch (type)
    {
      case Cell:
        memcpy(ic[i].cell.c_codes, fake_chain.cells[i], sizeof ic[i].cell.c_codes);
        ic[i].cccrc.cell_pec = pec_error(cell_failing, i);
        break;
      case Status:
        if (group == A)
        {
          ic[i].stata.itmp = (uint16_t)(fake_chain.die_temp_converted ? fake_chain.die_temp : INT16_MIN);
        }
        else if (group == C)
        {
          ic[i].statc = fake_chain.statc[i];
        }
        else if (group == D)
        {
          ic[i].statd = fake_chain.statd[i];
        }
        ic[i].cccrc.stat_pec = pec_error(fake_chain.status_pec_fail, i);
        break;
      case Config:
        if (group == A)
        {
          ic[i].rx_cfga = fake_chain.cfga[i];
        }
        else
        {
          ic[i].rx_cfgb = fake_chain.cfgb[i];
        }
        ic[i].cccrc.cfgr_pec = pec_error(0U, i);
        break;
      default:
        break;
    }
  }
  if ((type == Cell) && (fake_chain.cell_pec_fail_reads > 0U))
  {
    fake_chain.cell_pec_fail_reads--;
  }
}

void adBmsWriteData(uint8_t tIC, cell_asic *ic, uint8_t cmd_arg[2], TYPE type, GRP group)
{
  (void)cmd_arg;
  if ((type != Config) || fake_chain.absent)
  {
    return;
  }
  for (uint8_t i = 0; i < tIC; i++)
  {
    if (group == A)
    {
      fake_chain.cfga[i] = ic[i].tx_cfga;
    }
    else
    {
      fake_chain.cfgb[i] = ic[i].tx_cfgb;
    }
  }
}

void adBms6830_Adax(OW_AUX owaux, PUP pup, CH ch)
{
  (void)owaux;
  (void)pup;
  if ((ch == TEMP) && !fake_chain.absent)
  {
    fake_chain.die_temp_converted = true;
  }
}

void adBms6830_Adcv(RD rd, CONT cont, DCP dcp, RSTF rstf, OW_C_S owcs)
{
  (void)rd;
  (void)cont;
  (void)dcp;
  (void)rstf;
  (void)owcs;
}

void adBms6830_Snap(void)
{
}

void adBms6830_Unsnap(void)
{
}

void adBmsWakeupIc(uint8_t total_ic)
{
  (void)total_ic;
}

void adBmsForceWakeupIc(uint8_t total_ic)
{
  (void)total_ic;
  fake_chain.forced_wakes++;
}

uint16_t SetOverVoltageThreshold(float volt)
{
  return (uint16_t)(volt * 1000.0f);
}

uint16_t SetUnderVoltageThreshold(float voltage)
{
  return (uint16_t)(voltage * 1000.0f);
}
