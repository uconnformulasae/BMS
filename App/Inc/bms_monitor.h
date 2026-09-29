/**
  ******************************************************************************
  * @file    bms_monitor.h
  * @brief   ADBMS6830 chain measurement cycle with CAN2 telemetry.
  ******************************************************************************
  */
#ifndef BMS_MONITOR_H
#define BMS_MONITOR_H

/* Call once after the CubeMX peripheral init. */
void bms_monitor_init(void);

/* Call from the main loop; runs one cycle every BMS_CYCLE_MS. */
void bms_monitor_run(void);

#endif /* BMS_MONITOR_H */
