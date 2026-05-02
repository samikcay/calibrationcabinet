#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/*
 * Hardware pin assignments — update here when schematic is finalised.
 * Add these peripherals in STM32CubeIDE (.ioc) if not already present.
 */

/* BTS7960 H-bridge enable (PC4 = R_EN, PC5 = L_EN) */
#define BTS_R_EN_Pin         GPIO_PIN_4
#define BTS_R_EN_GPIO_Port   GPIOC
#define BTS_L_EN_Pin         GPIO_PIN_5
#define BTS_L_EN_GPIO_Port   GPIOC

/* TIM3 — 1 kHz PWM, ARR = 999
 *   CH1 PA6 = Peltier RPWM (heat)
 *   CH2 PA7 = Peltier LPWM (cool)
 *   CH3 PB0 = Humidifier
 *   CH4 PB1 = Dehumidifier
 * Defined in CubeIDE generated tim.c — declare extern here. */
extern TIM_HandleTypeDef htim3;

#endif /* BOARD_CONFIG_H */
