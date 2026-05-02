#ifndef CABINET_UI_H
#define CABINET_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct
{
  int16_t set_temperature_x10;
  uint8_t set_humidity;
  int16_t calibration_offset_x10;
  uint8_t running;
} CabinetUI_SettingsTypeDef;

void CabinetUI_Init(I2C_HandleTypeDef *lcd_i2c);
void CabinetUI_Process(void);
void CabinetUI_SetCurrentValues(float temperature, float humidity);
const CabinetUI_SettingsTypeDef *CabinetUI_GetSettings(void);

#ifdef __cplusplus
}
#endif

#endif
