/*
 * dhtsensor.h
 *
 *  Created on: 2 May 2026
 *      Author: sami
 */

#ifndef INC_DHTSENSOR_H_
#define INC_DHTSENSOR_H_

#include "stm32f4xx_hal.h"

typedef enum
{
	DHT22_OK = 0,
	DHT22_ERROR_TIMEOUT,
	DHT22_ERROR_CHECKSUM,
	DHT22_ERROR_NOT_INITIALIZED
} DHT22_StatusTypeDef;

void DHT22_Init(GPIO_TypeDef *gpio_port, uint16_t gpio_pin);

/*
 * Sadece sicaklik icin: DHT22_GetValues(&temperature, NULL)
 * Sadece nem icin:      DHT22_GetValues(NULL, &humidity)
 */
DHT22_StatusTypeDef DHT22_GetValues(float *temperature, float *humidity);

#endif /* INC_DHTSENSOR_H_ */
