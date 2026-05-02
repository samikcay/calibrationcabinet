#include "lcd_i2c.h"
#include <stddef.h>

#define LCD_RS       0x01U
#define LCD_ENABLE   0x04U
#define LCD_BACKLIGHT 0x08U

#define LCD_FUNCTION_SET 0x20U
#define LCD_4BIT_MODE    0x00U
#define LCD_2LINE        0x08U
#define LCD_5X8_DOTS     0x00U
#define LCD_DISPLAY_CTRL 0x08U
#define LCD_DISPLAY_ON   0x04U
#define LCD_CURSOR_OFF   0x00U
#define LCD_BLINK_OFF    0x00U
#define LCD_CLEAR        0x01U
#define LCD_ENTRY_MODE   0x04U
#define LCD_ENTRY_LEFT   0x02U
#define LCD_DDRAM_ADDR   0x80U

static HAL_StatusTypeDef LCD_I2C_WriteExpander(LCD_I2C_HandleTypeDef *lcd, uint8_t data)
{
  uint8_t packet = data | (lcd->backlight ? LCD_BACKLIGHT : 0U);
  return HAL_I2C_Master_Transmit(lcd->hi2c, (uint16_t)(lcd->address << 1U), &packet, 1U, 10U);
}

static HAL_StatusTypeDef LCD_I2C_PulseEnable(LCD_I2C_HandleTypeDef *lcd, uint8_t data)
{
  HAL_StatusTypeDef status;

  status = LCD_I2C_WriteExpander(lcd, data | LCD_ENABLE);
  if (status != HAL_OK)
  {
    return status;
  }

  HAL_Delay(1U);
  status = LCD_I2C_WriteExpander(lcd, data & (uint8_t)~LCD_ENABLE);
  HAL_Delay(1U);
  return status;
}

static HAL_StatusTypeDef LCD_I2C_Write4Bits(LCD_I2C_HandleTypeDef *lcd, uint8_t value)
{
  HAL_StatusTypeDef status;

  status = LCD_I2C_WriteExpander(lcd, value);
  if (status != HAL_OK)
  {
    return status;
  }

  return LCD_I2C_PulseEnable(lcd, value);
}

static HAL_StatusTypeDef LCD_I2C_Send(LCD_I2C_HandleTypeDef *lcd, uint8_t value, uint8_t mode)
{
  HAL_StatusTypeDef status;
  uint8_t high = value & 0xF0U;
  uint8_t low = (uint8_t)((value << 4U) & 0xF0U);

  status = LCD_I2C_Write4Bits(lcd, high | mode);
  if (status != HAL_OK)
  {
    return status;
  }

  return LCD_I2C_Write4Bits(lcd, low | mode);
}

static HAL_StatusTypeDef LCD_I2C_Command(LCD_I2C_HandleTypeDef *lcd, uint8_t command)
{
  return LCD_I2C_Send(lcd, command, 0U);
}

static HAL_StatusTypeDef LCD_I2C_Data(LCD_I2C_HandleTypeDef *lcd, uint8_t data)
{
  return LCD_I2C_Send(lcd, data, LCD_RS);
}

HAL_StatusTypeDef LCD_I2C_Init(LCD_I2C_HandleTypeDef *lcd,
                               I2C_HandleTypeDef *hi2c,
                               uint8_t address,
                               uint8_t columns,
                               uint8_t rows)
{
  HAL_StatusTypeDef status;

  if ((lcd == NULL) || (hi2c == NULL) || (columns == 0U) || (rows == 0U))
  {
    return HAL_ERROR;
  }

  lcd->hi2c = hi2c;
  lcd->address = address;
  lcd->columns = columns;
  lcd->rows = rows;
  lcd->backlight = 1U;

  HAL_Delay(50U);

  status = LCD_I2C_Write4Bits(lcd, 0x30U);
  if (status != HAL_OK)
  {
    return status;
  }
  HAL_Delay(5U);

  status = LCD_I2C_Write4Bits(lcd, 0x30U);
  if (status != HAL_OK)
  {
    return status;
  }
  HAL_Delay(5U);

  status = LCD_I2C_Write4Bits(lcd, 0x30U);
  if (status != HAL_OK)
  {
    return status;
  }
  HAL_Delay(1U);

  status = LCD_I2C_Write4Bits(lcd, 0x20U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = LCD_I2C_Command(lcd, LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2LINE | LCD_5X8_DOTS);
  if (status != HAL_OK)
  {
    return status;
  }

  status = LCD_I2C_Command(lcd, LCD_DISPLAY_CTRL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF);
  if (status != HAL_OK)
  {
    return status;
  }

  status = LCD_I2C_Clear(lcd);
  if (status != HAL_OK)
  {
    return status;
  }

  return LCD_I2C_Command(lcd, LCD_ENTRY_MODE | LCD_ENTRY_LEFT);
}

HAL_StatusTypeDef LCD_I2C_Clear(LCD_I2C_HandleTypeDef *lcd)
{
  HAL_StatusTypeDef status = LCD_I2C_Command(lcd, LCD_CLEAR);
  HAL_Delay(2U);
  return status;
}

HAL_StatusTypeDef LCD_I2C_Home(LCD_I2C_HandleTypeDef *lcd)
{
  HAL_StatusTypeDef status = LCD_I2C_Command(lcd, 0x02U);
  HAL_Delay(2U);
  return status;
}

HAL_StatusTypeDef LCD_I2C_SetCursor(LCD_I2C_HandleTypeDef *lcd, uint8_t column, uint8_t row)
{
  static const uint8_t row_offsets[] = {0x00U, 0x40U, 0x14U, 0x54U};

  if (lcd == NULL)
  {
    return HAL_ERROR;
  }

  if (row >= lcd->rows)
  {
    row = (uint8_t)(lcd->rows - 1U);
  }

  if (column >= lcd->columns)
  {
    column = (uint8_t)(lcd->columns - 1U);
  }

  return LCD_I2C_Command(lcd, LCD_DDRAM_ADDR | (uint8_t)(column + row_offsets[row]));
}

HAL_StatusTypeDef LCD_I2C_Print(LCD_I2C_HandleTypeDef *lcd, const char *text)
{
  HAL_StatusTypeDef status = HAL_OK;

  if ((lcd == NULL) || (text == NULL))
  {
    return HAL_ERROR;
  }

  while ((*text != '\0') && (status == HAL_OK))
  {
    status = LCD_I2C_Data(lcd, (uint8_t)*text);
    text++;
  }

  return status;
}

HAL_StatusTypeDef LCD_I2C_PrintPadded(LCD_I2C_HandleTypeDef *lcd, const char *text, uint8_t width)
{
  HAL_StatusTypeDef status;
  uint8_t written = 0U;

  if ((lcd == NULL) || (text == NULL))
  {
    return HAL_ERROR;
  }

  while ((*text != '\0') && (written < width))
  {
    status = LCD_I2C_Data(lcd, (uint8_t)*text);
    if (status != HAL_OK)
    {
      return status;
    }

    text++;
    written++;
  }

  while (written < width)
  {
    status = LCD_I2C_Data(lcd, (uint8_t)' ');
    if (status != HAL_OK)
    {
      return status;
    }

    written++;
  }

  return HAL_OK;
}

HAL_StatusTypeDef LCD_I2C_Backlight(LCD_I2C_HandleTypeDef *lcd, uint8_t enabled)
{
  if (lcd == NULL)
  {
    return HAL_ERROR;
  }

  lcd->backlight = (enabled != 0U) ? 1U : 0U;
  return LCD_I2C_WriteExpander(lcd, 0U);
}
