#ifndef COMMS_H
#define COMMS_H

#include <stdint.h>
#include "app_types.h"

#define COMMS_ALARM_NONE           0u
#define COMMS_ALARM_SENSOR_FAULT   (1u << 0)
#define COMMS_ALARM_OVER_TEMP      (1u << 1)

#define COMMS_OVER_TEMP_LIMIT_C       80.0f
#define COMMS_SENSOR_FAIL_THRESHOLD   3u
#define COMMS_STANDBY_TEMP_BAND_C     0.5f
#define COMMS_STANDBY_RH_BAND         3.0f

void Comms_Init(void);
void Comms_Process(void);

uint8_t        Comms_ComputeAlarms(uint32_t consecutive_sensor_fails,
                                   float    temperature_c);

system_state_t Comms_DeriveState(uint8_t  alarm_mask,
                                 uint8_t  has_first_reading,
                                 uint8_t  running,
                                 float    temperature_c,
                                 float    humidity_rh,
                                 float    setpoint_temp_c,
                                 float    setpoint_humidity_rh);

void Comms_SendTelemetry(float          temperature_c,
                         float          humidity_rh,
                         float          setpoint_temp_c,
                         float          setpoint_humidity_rh,
                         system_state_t state,
                         uint8_t        alarm_mask);

void Comms_OnUsart2IRQ(void);

#endif /* COMMS_H */
