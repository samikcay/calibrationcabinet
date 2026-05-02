#ifndef HUMIDITY_H
#define HUMIDITY_H

/*
 * Bang-bang humidity controller with +/-3 % RH hysteresis.
 *
 * Actuators (TIM3, 1 kHz PWM, ARR = 999):
 *   CH3  PB0  -- humidifier   (on when RH too low)
 *   CH4  PB1  -- dehumidifier (on when RH too high)
 *
 * Call Humidity_Init() once at startup.
 * Call Humidity_Step() every control cycle with live sensor data.
 * Call Humidity_AllOff() on any fault or non-RUNNING state.
 */

void Humidity_Init(void);
void Humidity_Step(float measured_rh, float target_rh);
void Humidity_AllOff(void);

#endif /* HUMIDITY_H */
