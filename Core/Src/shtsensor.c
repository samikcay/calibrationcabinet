/*
 * temperature.c
 *
 *  Created on: 1 May 2026
 *      Author: sami
 */

#include <shtsensor.h>

static I2C_HandleTypeDef *sht31_i2c;

void SHT31_Init(I2C_HandleTypeDef *hi2c_ptr)
{
	sht31_i2c = hi2c_ptr;
}
/*
 * sıcaklık için SHT31_GetValues(&temp, NULL)
 * nem için SHT31_GetValues(NULL, &humidity)
 * */
void SHT31_GetValues(float* temp, float* humidity)
{
	uint8_t cmd[2] = {0x24, 0x00};

	uint8_t rx_data[6];

	HAL_I2C_Master_Transmit(sht31_i2c, SHT31_I2C_ADDR, cmd, 2, HAL_MAX_DELAY);
	HAL_Delay(20);
	HAL_I2C_Master_Receive(sht31_i2c, SHT31_I2C_ADDR, rx_data, 6, HAL_MAX_DELAY);

	uint16_t raw_temp = (rx_data[0] << 8) | rx_data[1];
	uint16_t raw_hum = (rx_data[3] << 8) | rx_data[4];
	if (temp != NULL)
		*temp = -45.0f + (175.0f * ((float)raw_temp / 65535.0f));
	if (humidity != NULL)
	*humidity    = 100.0f * ((float)raw_hum / 65535.0f);
}
