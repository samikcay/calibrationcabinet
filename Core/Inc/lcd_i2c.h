#ifndef LCD_I2C_H
#define LCD_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define LCD_I2C_DEFAULT_ADDRESS 0x27U
#define LCD_I2C_DEFAULT_COLS    16U
#define LCD_I2C_DEFAULT_ROWS    2U

typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint8_t address;
  uint8_t columns;
  uint8_t rows;
  uint8_t backlight;
} LCD_I2C_HandleTypeDef;

HAL_StatusTypeDef LCD_I2C_Init(LCD_I2C_HandleTypeDef *lcd,
                               I2C_HandleTypeDef *hi2c,
                               uint8_t address,
                               uint8_t columns,
                               uint8_t rows);
HAL_StatusTypeDef LCD_I2C_Clear(LCD_I2C_HandleTypeDef *lcd);
HAL_StatusTypeDef LCD_I2C_Home(LCD_I2C_HandleTypeDef *lcd);
HAL_StatusTypeDef LCD_I2C_SetCursor(LCD_I2C_HandleTypeDef *lcd, uint8_t column, uint8_t row);
HAL_StatusTypeDef LCD_I2C_Print(LCD_I2C_HandleTypeDef *lcd, const char *text);
HAL_StatusTypeDef LCD_I2C_PrintPadded(LCD_I2C_HandleTypeDef *lcd, const char *text, uint8_t width);
HAL_StatusTypeDef LCD_I2C_Backlight(LCD_I2C_HandleTypeDef *lcd, uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif
