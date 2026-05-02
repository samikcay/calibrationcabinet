#include "cabinet_ui.h"
#include "keypad.h"
#include "lcd_i2c.h"
#include "main.h"
#include <stdio.h>

#define UI_REFRESH_MS 250U
#define UI_LCD_ADDRESS LCD_I2C_DEFAULT_ADDRESS
#define UI_LCD_COLS LCD_I2C_DEFAULT_COLS
#define UI_LCD_ROWS LCD_I2C_DEFAULT_ROWS
#define UI_NUMERIC_INPUT_MAX 4U

typedef enum
{
  UI_SCREEN_HOME = 0,
  UI_SCREEN_MENU,
  UI_SCREEN_EDIT_TEMP,
  UI_SCREEN_EDIT_HUMIDITY,
  UI_SCREEN_EDIT_OFFSET
} UI_ScreenTypeDef;

static LCD_I2C_HandleTypeDef lcd;
static Keypad_HandleTypeDef keypad;
static CabinetUI_SettingsTypeDef settings = {
  .set_temperature_x10 = 250,
  .set_humidity = 50,
  .calibration_offset_x10 = 0,
  .running = 0U
};
static UI_ScreenTypeDef screen = UI_SCREEN_HOME;
static uint8_t menu_index = 0U;
static uint32_t last_refresh_tick = 0U;
static uint8_t redraw_requested = 1U;
static uint8_t lcd_ready = 0U;
static char numeric_input[UI_NUMERIC_INPUT_MAX + 1U] = {0};
static uint8_t numeric_input_len = 0U;
static uint8_t numeric_input_prefilled = 0U;

static uint8_t UI_IsDigit(char key)
{
  return ((key >= '0') && (key <= '9')) ? 1U : 0U;
}

static uint8_t UI_InputHasDecimalPoint(void)
{
  uint8_t index;

  for (index = 0U; index < numeric_input_len; index++)
  {
    if (numeric_input[index] == '.')
    {
      return 1U;
    }
  }

  return 0U;
}

static uint8_t UI_InputFractionDigits(void)
{
  uint8_t index;
  uint8_t fraction_digits = 0U;
  uint8_t decimal_found = 0U;

  for (index = 0U; index < numeric_input_len; index++)
  {
    if (numeric_input[index] == '.')
    {
      decimal_found = 1U;
    }
    else if (decimal_found != 0U)
    {
      fraction_digits++;
    }
  }

  return fraction_digits;
}

static void UI_ResetNumericInput(void)
{
  uint8_t index;

  for (index = 0U; index < sizeof(numeric_input); index++)
  {
    numeric_input[index] = '\0';
  }

  numeric_input_len = 0U;
  numeric_input_prefilled = 0U;
}

static void UI_BeginNumericEdit(UI_ScreenTypeDef next_screen)
{
  UI_ResetNumericInput();
  screen = next_screen;

  if (next_screen == UI_SCREEN_EDIT_TEMP)
  {
    (void)snprintf(numeric_input, sizeof(numeric_input), "%u.%u",
                   (uint16_t)(settings.set_temperature_x10 / 10),
                   (uint16_t)(settings.set_temperature_x10 % 10));
  }
  else if (next_screen == UI_SCREEN_EDIT_HUMIDITY)
  {
    (void)snprintf(numeric_input, sizeof(numeric_input), "%u", settings.set_humidity);
  }

  numeric_input_len = 0U;
  while ((numeric_input[numeric_input_len] != '\0') &&
         (numeric_input_len < UI_NUMERIC_INPUT_MAX))
  {
    numeric_input_len++;
  }
  numeric_input_prefilled = 1U;
  redraw_requested = 1U;
}

static void UI_AppendNumericInput(char key)
{
  if (UI_IsDigit(key) == 0U)
  {
    return;
  }

  if (numeric_input_prefilled != 0U)
  {
    UI_ResetNumericInput();
  }

  if (numeric_input_len >= UI_NUMERIC_INPUT_MAX)
  {
    return;
  }

  if ((numeric_input_len == 0U) && (key == '0'))
  {
    numeric_input[numeric_input_len++] = key;
  }
  else if ((screen == UI_SCREEN_EDIT_TEMP) &&
           (UI_InputHasDecimalPoint() != 0U) &&
           (UI_InputFractionDigits() >= 1U))
  {
    return;
  }
  else if (!((numeric_input_len == 1U) && (numeric_input[0] == '0')))
  {
    numeric_input[numeric_input_len++] = key;
  }

  numeric_input[numeric_input_len] = '\0';
  redraw_requested = 1U;
}

static void UI_AppendDecimalPoint(void)
{
  if ((screen != UI_SCREEN_EDIT_TEMP) ||
      (UI_InputHasDecimalPoint() != 0U))
  {
    return;
  }

  if (numeric_input_prefilled != 0U)
  {
    UI_ResetNumericInput();
  }

  if (numeric_input_len >= UI_NUMERIC_INPUT_MAX)
  {
    return;
  }

  if (numeric_input_len == 0U)
  {
    numeric_input[numeric_input_len++] = '0';
  }

  if (numeric_input_len < UI_NUMERIC_INPUT_MAX)
  {
    numeric_input[numeric_input_len++] = '.';
    numeric_input[numeric_input_len] = '\0';
    redraw_requested = 1U;
  }
}

static void UI_BackspaceNumericInput(void)
{
  numeric_input_prefilled = 0U;

  if (numeric_input_len == 0U)
  {
    return;
  }

  numeric_input_len--;
  numeric_input[numeric_input_len] = '\0';
  redraw_requested = 1U;
}

static uint16_t UI_GetNumericInputValue(void)
{
  uint8_t index;
  uint16_t value = 0U;

  for (index = 0U; index < numeric_input_len; index++)
  {
    value = (uint16_t)((value * 10U) + (uint16_t)(numeric_input[index] - '0'));
  }

  return value;
}

static int16_t UI_GetTemperatureInputX10(void)
{
  uint8_t index;
  uint16_t integer = 0U;
  uint8_t fraction = 0U;
  uint8_t decimal_found = 0U;
  uint8_t fraction_found = 0U;

  for (index = 0U; index < numeric_input_len; index++)
  {
    if (numeric_input[index] == '.')
    {
      decimal_found = 1U;
    }
    else if (UI_IsDigit(numeric_input[index]) != 0U)
    {
      if (decimal_found == 0U)
      {
        integer = (uint16_t)((integer * 10U) + (uint16_t)(numeric_input[index] - '0'));
      }
      else if (fraction_found == 0U)
      {
        fraction = (uint8_t)(numeric_input[index] - '0');
        fraction_found = 1U;
      }
    }
  }

  return (int16_t)((integer * 10U) + fraction);
}

static void UI_SetNumericInputFromTempX10(int16_t value_x10)
{
  UI_ResetNumericInput();
  (void)snprintf(numeric_input, sizeof(numeric_input), "%u.%u",
                 (uint16_t)(value_x10 / 10),
                 (uint16_t)(value_x10 % 10));

  while ((numeric_input[numeric_input_len] != '\0') &&
         (numeric_input_len < UI_NUMERIC_INPUT_MAX))
  {
    numeric_input_len++;
  }
}

static void UI_SetNumericInputFromHumidity(uint8_t value)
{
  UI_ResetNumericInput();
  (void)snprintf(numeric_input, sizeof(numeric_input), "%u", value);

  while ((numeric_input[numeric_input_len] != '\0') &&
         (numeric_input_len < UI_NUMERIC_INPUT_MAX))
  {
    numeric_input_len++;
  }
}

static void UI_AdjustNumericEdit(char key)
{
  if (screen == UI_SCREEN_EDIT_TEMP)
  {
    int16_t value_x10 = UI_GetTemperatureInputX10();
    value_x10 += (key == 'A') ? 1 : -1;

    if (value_x10 < 0)
    {
      value_x10 = 0;
    }
    else if (value_x10 > 800)
    {
      value_x10 = 800;
    }

    UI_SetNumericInputFromTempX10(value_x10);
  }
  else if (screen == UI_SCREEN_EDIT_HUMIDITY)
  {
    uint16_t value = UI_GetNumericInputValue();

    if ((key == 'A') && (value < 100U))
    {
      value++;
    }
    else if ((key == 'B') && (value > 0U))
    {
      value--;
    }

    UI_SetNumericInputFromHumidity((uint8_t)value);
  }

  numeric_input_prefilled = 0U;
  redraw_requested = 1U;
}

static void UI_FormatSignedX10(char *buffer, uint32_t buffer_size, int16_t value_x10)
{
  int16_t integer;
  int16_t fraction;
  char sign = ' ';

  if (value_x10 < 0)
  {
    sign = '-';
    value_x10 = (int16_t)-value_x10;
  }

  integer = (int16_t)(value_x10 / 10);
  fraction = (int16_t)(value_x10 % 10);
  (void)snprintf(buffer, buffer_size, "%c%d.%d", sign, integer, fraction);
}

static void UI_DrawHome(void)
{
  char temp[8];
  char line[17];

  UI_FormatSignedX10(temp, sizeof(temp), settings.set_temperature_x10);
  (void)snprintf(line, sizeof(line), "T:%sC H:%3u%%", temp, settings.set_humidity);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  UI_FormatSignedX10(temp, sizeof(temp), settings.calibration_offset_x10);
  (void)snprintf(line, sizeof(line), "%s Off:%s", settings.running ? "RUN " : "STOP", temp);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);
}

static void UI_DrawMenu(void)
{
  static const char *items[] = {
    "Set Temp",
    "Set Humidity",
    "Cal Offset",
    "Start/Stop"
  };
  char line[17];

  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)snprintf(line, sizeof(line), ">%s", items[menu_index]);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)snprintf(line, sizeof(line), "A/B Nav # OK");
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);
}

static void UI_DrawEditValue(const char *title, int16_t value_x10, const char *unit)
{
  char value[8];
  char line[17];

  UI_FormatSignedX10(value, sizeof(value), value_x10);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)LCD_I2C_PrintPadded(&lcd, title, UI_LCD_COLS);

  (void)snprintf(line, sizeof(line), "%s%s A/B +/-", value, unit);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);
}

static void UI_DrawNumericEdit(const char *title, const char *unit)
{
  char line[17];
  const char *value_text = (numeric_input_len == 0U) ? "_" : numeric_input;

  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)snprintf(line, sizeof(line), "%s:%s%s", title, value_text, unit);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)snprintf(line, sizeof(line), "0-9 *Dot DDel");
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);
}

static void UI_Draw(void)
{
  switch (screen)
  {
    case UI_SCREEN_HOME:
      UI_DrawHome();
      break;

    case UI_SCREEN_MENU:
      UI_DrawMenu();
      break;

    case UI_SCREEN_EDIT_TEMP:
      UI_DrawNumericEdit("Temp", "C");
      break;

    case UI_SCREEN_EDIT_HUMIDITY:
      UI_DrawNumericEdit("Humidity", "%");
      break;

    case UI_SCREEN_EDIT_OFFSET:
      UI_DrawEditValue("Cal Offset", settings.calibration_offset_x10, "C");
      break;

    default:
      screen = UI_SCREEN_HOME;
      UI_DrawHome();
      break;
  }

  redraw_requested = 0U;
  last_refresh_tick = HAL_GetTick();
}

static void UI_ClampSettings(void)
{
  if (settings.set_temperature_x10 < 0)
  {
    settings.set_temperature_x10 = 0;
  }
  else if (settings.set_temperature_x10 > 800)
  {
    settings.set_temperature_x10 = 800;
  }

  if (settings.set_humidity > 100U)
  {
    settings.set_humidity = 100U;
  }

  if (settings.calibration_offset_x10 < -100)
  {
    settings.calibration_offset_x10 = -100;
  }
  else if (settings.calibration_offset_x10 > 100)
  {
    settings.calibration_offset_x10 = 100;
  }
}

static void UI_HandleHomeKey(char key)
{
  if (key == '#')
  {
    screen = UI_SCREEN_MENU;
  }
  else if (key == 'D')
  {
    settings.running = settings.running ? 0U : 1U;
  }
  else
  {
    return;
  }

  redraw_requested = 1U;
}

static void UI_HandleMenuKey(char key)
{
  if (key == 'A')
  {
    menu_index = (menu_index == 0U) ? 3U : (uint8_t)(menu_index - 1U);
  }
  else if (key == 'B')
  {
    menu_index = (uint8_t)((menu_index + 1U) % 4U);
  }
  else if (key == '#')
  {
    if (menu_index == 0U)
    {
      UI_BeginNumericEdit(UI_SCREEN_EDIT_TEMP);
    }
    else if (menu_index == 1U)
    {
      UI_BeginNumericEdit(UI_SCREEN_EDIT_HUMIDITY);
    }
    else if (menu_index == 2U)
    {
      screen = UI_SCREEN_EDIT_OFFSET;
    }
    else
    {
      settings.running = settings.running ? 0U : 1U;
      screen = UI_SCREEN_HOME;
    }
  }
  else if (key == 'C')
  {
    screen = UI_SCREEN_HOME;
  }
  else
  {
    return;
  }

  redraw_requested = 1U;
}

static void UI_HandleEditKey(char key)
{
  if ((screen == UI_SCREEN_EDIT_TEMP) || (screen == UI_SCREEN_EDIT_HUMIDITY))
  {
    if (UI_IsDigit(key) != 0U)
    {
      UI_AppendNumericInput(key);
    }
    else if (key == '*')
    {
      UI_AppendDecimalPoint();
    }
    else if (key == 'D')
    {
      UI_BackspaceNumericInput();
    }
    else if ((key == 'A') || (key == 'B'))
    {
      UI_AdjustNumericEdit(key);
    }
    else if (key == '#')
    {
      uint16_t value = UI_GetNumericInputValue();

      if (numeric_input_len != 0U)
      {
        if (screen == UI_SCREEN_EDIT_TEMP)
        {
          settings.set_temperature_x10 = UI_GetTemperatureInputX10();
        }
        else
        {
          settings.set_humidity = (value > 100U) ? 100U : (uint8_t)value;
        }

        UI_ClampSettings();
      }

      screen = UI_SCREEN_MENU;
      redraw_requested = 1U;
    }
    else if (key == 'C')
    {
      screen = UI_SCREEN_MENU;
      redraw_requested = 1U;
    }
  }
  else if ((key == 'A') || (key == 'B'))
  {
    if (screen == UI_SCREEN_EDIT_OFFSET)
    {
      settings.calibration_offset_x10 += (key == 'A') ? 1 : -1;
    }

    UI_ClampSettings();
    redraw_requested = 1U;
  }
  else if ((key == '#') || (key == 'C'))
  {
    screen = UI_SCREEN_MENU;
    redraw_requested = 1U;
  }
}

static void UI_HandleKey(char key)
{
  if (key == KEYPAD_NO_KEY)
  {
    return;
  }

  if (screen == UI_SCREEN_HOME)
  {
    UI_HandleHomeKey(key);
  }
  else if (screen == UI_SCREEN_MENU)
  {
    UI_HandleMenuKey(key);
  }
  else
  {
    UI_HandleEditKey(key);
  }
}

void CabinetUI_Init(I2C_HandleTypeDef *lcd_i2c)
{
  GPIO_TypeDef *row_ports[KEYPAD_ROWS] = {
    KEYPAD_R1_GPIO_Port,
    KEYPAD_R2_GPIO_Port,
    KEYPAD_R3_GPIO_Port,
    KEYPAD_R4_GPIO_Port
  };
  const uint16_t row_pins[KEYPAD_ROWS] = {
    KEYPAD_R1_Pin,
    KEYPAD_R2_Pin,
    KEYPAD_R3_Pin,
    KEYPAD_R4_Pin
  };
  GPIO_TypeDef *col_ports[KEYPAD_COLS] = {
    KEYPAD_C1_GPIO_Port,
    KEYPAD_C2_GPIO_Port,
    KEYPAD_C3_GPIO_Port,
    KEYPAD_C4_GPIO_Port
  };
  const uint16_t col_pins[KEYPAD_COLS] = {
    KEYPAD_C1_Pin,
    KEYPAD_C2_Pin,
    KEYPAD_C3_Pin,
    KEYPAD_C4_Pin
  };
  const char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
  };

  Keypad_Init(&keypad, row_ports, row_pins, col_ports, col_pins, keymap);

  if (LCD_I2C_Init(&lcd, lcd_i2c, UI_LCD_ADDRESS, UI_LCD_COLS, UI_LCD_ROWS) == HAL_OK)
  {
    lcd_ready = 1U;
    (void)LCD_I2C_Clear(&lcd);
    redraw_requested = 1U;
    UI_Draw();
  }
}

void CabinetUI_Process(void)
{
  char key = Keypad_GetKey(&keypad);
  uint32_t now = HAL_GetTick();

  UI_HandleKey(key);

  if ((lcd_ready != 0U) &&
      ((redraw_requested != 0U) || ((now - last_refresh_tick) >= UI_REFRESH_MS)))
  {
    UI_Draw();
  }
}

const CabinetUI_SettingsTypeDef *CabinetUI_GetSettings(void)
{
  return &settings;
}
