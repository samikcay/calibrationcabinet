/*
 * dhtsensor.c
 *
 *  Created on: 2 May 2026
 *      Author: sami
 */

#include "dhtsensor.h"

#define DHT22_START_LOW_MS       18U
#define DHT22_START_HIGH_US      30U
#define DHT22_RESPONSE_TIMEOUT   120U
#define DHT22_BIT_TIMEOUT        100U
#define DHT22_ONE_THRESHOLD_US   45U
#define DHT22_BITS_COUNT         40U

static GPIO_TypeDef *dht22_port;
static uint16_t dht22_pin;
static uint8_t dht22_is_initialized;

static void DHT22_DWT_Init(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void DHT22_DelayUs(uint32_t us)
{
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000U);

	while ((DWT->CYCCNT - start) < ticks)
	{
	}
}

static void DHT22_SetPinOutput(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = dht22_pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(dht22_port, &GPIO_InitStruct);
}

static void DHT22_SetPinInput(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = dht22_pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(dht22_port, &GPIO_InitStruct);
}

static DHT22_StatusTypeDef DHT22_WaitForPinState(GPIO_PinState state, uint32_t timeout_us, uint32_t *elapsed_us)
{
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
	uint32_t timeout_ticks = timeout_us * ticks_per_us;

	while (HAL_GPIO_ReadPin(dht22_port, dht22_pin) != state)
	{
		if ((DWT->CYCCNT - start) > timeout_ticks)
		{
			return DHT22_ERROR_TIMEOUT;
		}
	}

	if (elapsed_us != NULL)
	{
		*elapsed_us = (DWT->CYCCNT - start) / ticks_per_us;
	}

	return DHT22_OK;
}

void DHT22_Init(GPIO_TypeDef *gpio_port, uint16_t gpio_pin)
{
	dht22_port = gpio_port;
	dht22_pin = gpio_pin;
	dht22_is_initialized = 1U;

	DHT22_DWT_Init();
	DHT22_SetPinInput();
}

DHT22_StatusTypeDef DHT22_GetValues(float *temperature, float *humidity)
{
	uint8_t data[5] = {0};
	uint32_t high_time_us = 0;
	DHT22_StatusTypeDef status = DHT22_OK;
	uint32_t cycles_per_ms;
	uint32_t start_cycles;
	uint32_t elapsed_ms;

	if (dht22_is_initialized == 0U)
	{
		return DHT22_ERROR_NOT_INITIALIZED;
	}

	DHT22_SetPinOutput();
	HAL_GPIO_WritePin(dht22_port, dht22_pin, GPIO_PIN_RESET);
	HAL_Delay(DHT22_START_LOW_MS);
	HAL_GPIO_WritePin(dht22_port, dht22_pin, GPIO_PIN_SET);
	DHT22_DelayUs(DHT22_START_HIGH_US);
	DHT22_SetPinInput();

	cycles_per_ms = HAL_RCC_GetHCLKFreq() / 1000U;
	start_cycles = DWT->CYCCNT;

	__disable_irq();

	if (DHT22_WaitForPinState(GPIO_PIN_RESET, DHT22_RESPONSE_TIMEOUT, NULL) != DHT22_OK ||
		DHT22_WaitForPinState(GPIO_PIN_SET, DHT22_RESPONSE_TIMEOUT, NULL) != DHT22_OK ||
		DHT22_WaitForPinState(GPIO_PIN_RESET, DHT22_RESPONSE_TIMEOUT, NULL) != DHT22_OK)
	{
		status = DHT22_ERROR_TIMEOUT;
	}
	else
	{
		for (uint8_t bit = 0; bit < DHT22_BITS_COUNT; bit++)
		{
			if (DHT22_WaitForPinState(GPIO_PIN_SET, DHT22_BIT_TIMEOUT, NULL) != DHT22_OK)
			{
				status = DHT22_ERROR_TIMEOUT;
				break;
			}

			if (DHT22_WaitForPinState(GPIO_PIN_RESET, DHT22_BIT_TIMEOUT, &high_time_us) != DHT22_OK)
			{
				status = DHT22_ERROR_TIMEOUT;
				break;
			}

			data[bit / 8U] <<= 1;
			if (high_time_us > DHT22_ONE_THRESHOLD_US)
			{
				data[bit / 8U] |= 1U;
			}
		}
	}

	__enable_irq();

	/* SysTick was suppressed during the critical section, so uwTick fell
	 * behind real time. Use DWT (which kept counting) to advance the tick
	 * by however many milliseconds elapsed. The pending SysTick exception
	 * may fire once on re-enable; the resulting <=1 ms over-count is
	 * acceptable for this application. */
	elapsed_ms = (DWT->CYCCNT - start_cycles) / cycles_per_ms;
	while (elapsed_ms-- > 0U)
	{
		HAL_IncTick();
	}

	if (status != DHT22_OK)
	{
		return status;
	}

	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
	{
		return DHT22_ERROR_CHECKSUM;
	}

	if (humidity != NULL)
	{
		uint16_t raw_humidity = ((uint16_t)data[0] << 8) | data[1];
		*humidity = (float)raw_humidity / 10.0f;
	}

	if (temperature != NULL)
	{
		uint16_t raw_temperature = (((uint16_t)data[2] & 0x7FU) << 8) | data[3];
		*temperature = (float)raw_temperature / 10.0f;

		if ((data[2] & 0x80U) != 0U)
		{
			*temperature *= -1.0f;
		}
	}

	return DHT22_OK;
}
