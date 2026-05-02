#ifndef HUMIDITY_H
#define HUMIDITY_H

/*
 * Bang-bang humidity controller with +/-3 % RH hysteresis.
 *
 * Actuators (TIM3, 1 kHz PWM, ARR = 999):
 *   CH3  PB0  -- humidifier D4184 PWM
 *   CH4  PB1  -- condensation Peltier D4184 PWM
 *
 * Call Humidity_Init() once at startup.
 * Call Humidity_Step() every control cycle with live sensor data.
 * Call Humidity_AllOff() on any fault or non-RUNNING state.
 * RH too high is handled by cooling the condensation Peltier.
 */

void Humidity_Init(void);
void Humidity_Step(float measured_rh, float target_rh);
void Humidity_AllOff(void);

#endif /* HUMIDITY_H */
