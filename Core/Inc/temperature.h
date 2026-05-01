/*
 * temperature.h
 *
 *  Created on: 1 May 2026
 *      Author: sami
 */

#ifndef INC_TEMPERATURE_H_
#define INC_TEMPERATURE_H_

#include "stm32f4xx_hal.h"

#define SHT31_I2C_ADDR (0x45 << 1)

void SHT31_Init(I2C_HandleTypeDef *hi2c_ptr);
void SHT31_GetValues(float *temperature, float *humidity);

#endif /* INC_TEMPERATURE_H_ */
