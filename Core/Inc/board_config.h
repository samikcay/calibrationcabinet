#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/*
 * Hardware pin assignments.
 * TIM3 is configured directly here because this project does not include
 * the STM32 HAL TIM module.
 *
 * TIM3 - 1 kHz PWM, ARR = 999 (only main Peltier uses PWM):
 *   CH1 PC6 = Main Peltier BTS7960 RPWM (heat)
 *   CH2 PB5 = Main Peltier BTS7960 LPWM (cool)
 *
 * GPIO ON/OFF (driven full-on or full-off, no PWM):
 *   PB0 = Humidifier D4184 gate
 *   PB1 = Condensation Peltier D4184 gate
 */

#define BTS_R_EN_Pin         GPIO_PIN_4
#define BTS_R_EN_GPIO_Port   GPIOC
#define BTS_L_EN_Pin         GPIO_PIN_5
#define BTS_L_EN_GPIO_Port   GPIOC

#define BOARD_TIM3_PWM_PERIOD 999U
#define BOARD_TIM3_CH1        1U
#define BOARD_TIM3_CH2        2U
#define BOARD_TIM3_CH3        3U
#define BOARD_TIM3_CH4        4U

void Board_TIM3_PWM_Init(void);
uint32_t Board_TIM3_PWM_Period(void);
void Board_TIM3_PWM_Set(uint8_t channel, uint32_t compare);
void Board_MainPeltier_Enable(uint8_t enabled);
void Board_MainPeltier_SetHeat(uint32_t compare);
void Board_MainPeltier_SetCool(uint32_t compare);
void Board_Humidifier_Set(uint32_t compare);
void Board_CondensationPeltier_Set(uint32_t compare);

#endif /* BOARD_CONFIG_H */
