#include "cabinet_ui.h"
#include "comms.h"
#include "keypad.h"
#include "lcd_i2c.h"
#include "main.h"
#include "state_machine.h"
#include <stdint.h>
#include <stdio.h>

#define UI_REFRESH_MS 500U
#define UI_MIN_DRAW_INTERVAL_MS 80U
#define UI_LCD_ADDRESS LCD_I2C_DEFAULT_ADDRESS
#define UI_LCD_COLS 20U
#define UI_LCD_ROWS 4U
#define UI_NUMERIC_INPUT_MAX 4U

#define UI_FW_VERSION "v0.3.0"

typedef enum
{
  UI_SCREEN_HOME = 0,

  /* Menu levels */
  UI_SCREEN_MENU_ROOT,
  UI_SCREEN_MENU_SETPOINTS,
  UI_SCREEN_MENU_ALARMS,
  UI_SCREEN_MENU_LOGS,
  UI_SCREEN_MENU_COMM,

  /* Setpoint leaves */
  UI_SCREEN_EDIT_TEMP,
  UI_SCREEN_EDIT_HUMIDITY,
  UI_SCREEN_EDIT_OFFSET,

  /* Info / placeholder leaves */
  UI_SCREEN_ALARMS_OVER_TEMP,
  UI_SCREEN_ALARMS_SENSOR_RETRY,
  UI_SCREEN_LOGS_VIEW,
  UI_SCREEN_LOGS_ERASE,
  UI_SCREEN_COMM_WIFI,
  UI_SCREEN_COMM_HTTP,
  UI_SCREEN_SYSTEM_INFO
} UI_ScreenTypeDef;

typedef struct
{
  const char       *title;
  UI_ScreenTypeDef  next;
} UI_MenuItemTypeDef;

typedef struct
{
  const UI_MenuItemTypeDef *items;
  uint8_t                   count;
  const char               *title;
} UI_MenuTableTypeDef;

static LCD_I2C_HandleTypeDef lcd;
static Keypad_HandleTypeDef keypad;
static I2C_HandleTypeDef *s_lcd_hi2c = NULL;
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
static int16_t current_temperature_x10 = 0;
static int16_t current_humidity_x10 = 0;

static const UI_MenuItemTypeDef k_menu_root_items[] = {
  {"Setpoints",     UI_SCREEN_MENU_SETPOINTS},
  {"Alarms",        UI_SCREEN_MENU_ALARMS},
  {"Logs",          UI_SCREEN_MENU_LOGS},
  {"Communication", UI_SCREEN_MENU_COMM},
  {"System Info",   UI_SCREEN_SYSTEM_INFO}
};

static const UI_MenuItemTypeDef k_menu_setpoints_items[] = {
  {"Temperature", UI_SCREEN_EDIT_TEMP},
  {"Humidity",    UI_SCREEN_EDIT_HUMIDITY},
  {"Cal Offset",  UI_SCREEN_EDIT_OFFSET}
};

static const UI_MenuItemTypeDef k_menu_alarms_items[] = {
  {"Over-temp",      UI_SCREEN_ALARMS_OVER_TEMP},
  {"Sensor retries", UI_SCREEN_ALARMS_SENSOR_RETRY}
};

static const UI_MenuItemTypeDef k_menu_logs_items[] = {
  {"View entries", UI_SCREEN_LOGS_VIEW},
  {"Erase logs",   UI_SCREEN_LOGS_ERASE}
};

static const UI_MenuItemTypeDef k_menu_comm_items[] = {
  {"Wi-Fi status",  UI_SCREEN_COMM_WIFI},
  {"HTTP endpoint", UI_SCREEN_COMM_HTTP}
};

static const UI_MenuTableTypeDef k_menu_root      = {k_menu_root_items,      5U, "Menu"};
static const UI_MenuTableTypeDef k_menu_setpoints = {k_menu_setpoints_items, 3U, "Setpoints"};
static const UI_MenuTableTypeDef k_menu_alarms    = {k_menu_alarms_items,    2U, "Alarms"};
static const UI_MenuTableTypeDef k_menu_logs      = {k_menu_logs_items,      2U, "Logs"};
static const UI_MenuTableTypeDef k_menu_comm      = {k_menu_comm_items,      2U, "Communication"};

static const UI_MenuTableTypeDef *UI_TableForScreen(UI_ScreenTypeDef s)
{
  switch (s)
  {
    case UI_SCREEN_MENU_ROOT:      return &k_menu_root;
    case UI_SCREEN_MENU_SETPOINTS: return &k_menu_setpoints;
    case UI_SCREEN_MENU_ALARMS:    return &k_menu_alarms;
    case UI_SCREEN_MENU_LOGS:      return &k_menu_logs;
    case UI_SCREEN_MENU_COMM:      return &k_menu_comm;
    default:                       return NULL;
  }
}

static UI_ScreenTypeDef UI_ParentScreen(UI_ScreenTypeDef s)
{
  switch (s)
  {
    case UI_SCREEN_MENU_ROOT:
      return UI_SCREEN_HOME;

    case UI_SCREEN_MENU_SETPOINTS:
    case UI_SCREEN_MENU_ALARMS:
    case UI_SCREEN_MENU_LOGS:
    case UI_SCREEN_MENU_COMM:
    case UI_SCREEN_SYSTEM_INFO:
      return UI_SCREEN_MENU_ROOT;

    case UI_SCREEN_EDIT_TEMP:
    case UI_SCREEN_EDIT_HUMIDITY:
    case UI_SCREEN_EDIT_OFFSET:
      return UI_SCREEN_MENU_SETPOINTS;

    case UI_SCREEN_ALARMS_OVER_TEMP:
    case UI_SCREEN_ALARMS_SENSOR_RETRY:
      return UI_SCREEN_MENU_ALARMS;

    case UI_SCREEN_LOGS_VIEW:
    case UI_SCREEN_LOGS_ERASE:
      return UI_SCREEN_MENU_LOGS;

    case UI_SCREEN_COMM_WIFI:
    case UI_SCREEN_COMM_HTTP:
      return UI_SCREEN_MENU_COMM;

    default:
      return UI_SCREEN_HOME;
  }
}

static void UI_GoToScreen(UI_ScreenTypeDef next)
{
  if (next != screen)
  {
    menu_index = 0U;
  }
  screen = next;
  redraw_requested = 1U;
}

static int16_t UI_FloatToX10(float value)
{
  if (value >= 0.0f)
  {
    return (int16_t)((value * 10.0f) + 0.5f);
  }

  return (int16_t)((value * 10.0f) - 0.5f);
}

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
  int16_t value_x10;

  UI_ResetNumericInput();
  screen = next_screen;

  if (next_screen == UI_SCREEN_EDIT_TEMP)
  {
    value_x10 = settings.set_temperature_x10;
    if (value_x10 < 0)
    {
      value_x10 = 0;
    }
    else if (value_x10 > 800)
    {
      value_x10 = 800;
    }

    (void)snprintf(numeric_input, sizeof(numeric_input), "%u.%u",
                   (uint16_t)(value_x10 / 10),
                   (uint16_t)(value_x10 % 10));
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

  if (value_x10 < 0)
  {
    value_x10 = 0;
  }
  else if (value_x10 > 800)
  {
    value_x10 = 800;
  }

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

static void UI_FormatX10(char *buffer, uint32_t buffer_size, int16_t value_x10)
{
  int16_t integer;
  int16_t fraction;

  if (value_x10 < 0)
  {
    value_x10 = (int16_t)-value_x10;
    integer = (int16_t)(value_x10 / 10);
    fraction = (int16_t)(value_x10 % 10);
    (void)snprintf(buffer, buffer_size, "-%d.%d", integer, fraction);
  }
  else
  {
    integer = (int16_t)(value_x10 / 10);
    fraction = (int16_t)(value_x10 % 10);
    (void)snprintf(buffer, buffer_size, "%d.%d", integer, fraction);
  }
}

static void UI_ClearRows(uint8_t first_row)
{
  uint8_t row;

  for (row = first_row; row < UI_LCD_ROWS; row++)
  {
    (void)LCD_I2C_SetCursor(&lcd, 0U, row);
    (void)LCD_I2C_PrintPadded(&lcd, "", UI_LCD_COLS);
  }
}

static void UI_DrawHome(void)
{
  char current[8];
  char target[8];
  char line[32];

  UI_FormatX10(current, sizeof(current), current_temperature_x10);
  UI_FormatX10(target, sizeof(target), settings.set_temperature_x10);
  (void)snprintf(line, sizeof(line), "T %sC  Set %sC", current, target);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  UI_FormatX10(current, sizeof(current), current_humidity_x10);
  (void)snprintf(line, sizeof(line), "RH %s%%  Set %u%%",
                 current, (unsigned)settings.set_humidity);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)snprintf(line, sizeof(line), "State: %s",
                 StateMachine_StateName(StateMachine_Get()));
  (void)LCD_I2C_SetCursor(&lcd, 0U, 2U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 3U);
  (void)LCD_I2C_PrintPadded(&lcd, "# Menu  D Run/Stop", UI_LCD_COLS);
}

static void UI_DrawMenuList(const UI_MenuTableTypeDef *table)
{
  char line[32];

  if (menu_index >= table->count)
  {
    menu_index = 0U;
  }

  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)snprintf(line, sizeof(line), "[%s]", table->title);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)snprintf(line, sizeof(line), ">%s",
                 table->items[menu_index].title);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 2U);
  (void)snprintf(line, sizeof(line), "%u/%u",
                 (unsigned)(menu_index + 1U), (unsigned)table->count);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 3U);
  (void)LCD_I2C_PrintPadded(&lcd, "A/B # OK  C Back", UI_LCD_COLS);
}

static void UI_DrawSystemInfo(void)
{
  char line[32];
  uint32_t uptime_s = HAL_GetTick() / 1000U;
  uint32_t hh = uptime_s / 3600U;
  uint32_t mm = (uptime_s / 60U) % 60U;
  uint32_t ss = uptime_s % 60U;

  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)snprintf(line, sizeof(line), "FW: %s", UI_FW_VERSION);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)snprintf(line, sizeof(line), "Build: %s", __DATE__);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 2U);
  (void)snprintf(line, sizeof(line), "Up %02lu:%02lu:%02lu",
                 (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 3U);
  (void)LCD_I2C_PrintPadded(&lcd, "C Back", UI_LCD_COLS);
}

static void UI_DrawInfoLeaf(const char *title, const char *line2,
                            const char *line3)
{
  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)LCD_I2C_PrintPadded(&lcd, title, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)LCD_I2C_PrintPadded(&lcd, (line2 != NULL) ? line2 : "", UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 2U);
  (void)LCD_I2C_PrintPadded(&lcd, (line3 != NULL) ? line3 : "", UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 3U);
  (void)LCD_I2C_PrintPadded(&lcd, "C Back", UI_LCD_COLS);
}

static void UI_DrawAlarmsOverTemp(void)
{
  char line[32];
  (void)snprintf(line, sizeof(line), "Limit: %u C",
                 (unsigned)COMMS_OVER_TEMP_LIMIT_C);
  UI_DrawInfoLeaf("[Over-temp]", line, "(read-only)");
}

static void UI_DrawAlarmsSensorRetry(void)
{
  char line[32];
  (void)snprintf(line, sizeof(line), "Retries: %u",
                 (unsigned)COMMS_SENSOR_FAIL_THRESHOLD);
  UI_DrawInfoLeaf("[Sensor retries]", line, "(read-only)");
}

static void UI_DrawEditValue(const char *title, int16_t value_x10, const char *unit)
{
  char value[8];
  char line[32];

  UI_FormatSignedX10(value, sizeof(value), value_x10);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)LCD_I2C_PrintPadded(&lcd, title, UI_LCD_COLS);

  (void)snprintf(line, sizeof(line), "%s%s A/B +/-", value, unit);
  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);
  UI_ClearRows(2U);
}

static void UI_DrawNumericEdit(const char *title, const char *unit)
{
  char line[32];
  const char *value_text = (numeric_input_len == 0U) ? "_" : numeric_input;

  (void)LCD_I2C_SetCursor(&lcd, 0U, 0U);
  (void)snprintf(line, sizeof(line), "%s:%s%s", title, value_text, unit);
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);

  (void)LCD_I2C_SetCursor(&lcd, 0U, 1U);
  (void)snprintf(line, sizeof(line), "0-9 *Dot DDel");
  (void)LCD_I2C_PrintPadded(&lcd, line, UI_LCD_COLS);
  UI_ClearRows(2U);
}

static void UI_Draw(void)
{
  switch (screen)
  {
    case UI_SCREEN_HOME:
      UI_DrawHome();
      break;

    case UI_SCREEN_MENU_ROOT:
    case UI_SCREEN_MENU_SETPOINTS:
    case UI_SCREEN_MENU_ALARMS:
    case UI_SCREEN_MENU_LOGS:
    case UI_SCREEN_MENU_COMM:
      UI_DrawMenuList(UI_TableForScreen(screen));
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

    case UI_SCREEN_SYSTEM_INFO:
      UI_DrawSystemInfo();
      break;

    case UI_SCREEN_ALARMS_OVER_TEMP:
      UI_DrawAlarmsOverTemp();
      break;

    case UI_SCREEN_ALARMS_SENSOR_RETRY:
      UI_DrawAlarmsSensorRetry();
      break;

    case UI_SCREEN_LOGS_VIEW:
      UI_DrawInfoLeaf("[Logs]", "No entries yet.", "Use web UI for now.");
      break;

    case UI_SCREEN_LOGS_ERASE:
      UI_DrawInfoLeaf("[Erase logs]", "Not available.", "Use web UI to clear.");
      break;

    case UI_SCREEN_COMM_WIFI:
      UI_DrawInfoLeaf("[Wi-Fi]", "Status not pushed", "by ESP32 yet.");
      break;

    case UI_SCREEN_COMM_HTTP:
      UI_DrawInfoLeaf("[HTTP endpoint]", "Status not pushed", "by ESP32 yet.");
      break;

    default:
      screen = UI_SCREEN_HOME;
      UI_DrawHome();
      break;
  }

  /* HAL accumulates per-transfer errors in hi2c->ErrorCode. If any LCD write
   * during this draw came back with a non-zero error (NACK, ARLO, AF, etc.),
   * count it. After 3 consecutive bad draws, force recovery. */
  static uint8_t s_lcd_error_count = 0U;
  if ((s_lcd_hi2c != NULL) && (s_lcd_hi2c->ErrorCode != HAL_I2C_ERROR_NONE))
  {
    s_lcd_hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
    HAL_GPIO_TogglePin(GPIOD, LD5_Pin);   /* visual: LCD transfer error */
    if (s_lcd_error_count < 255U) {
      s_lcd_error_count++;
    }
    if (s_lcd_error_count >= 3U) {
      lcd_ready = 0U;
      s_lcd_error_count = 0U;
    }
  }
  else
  {
    s_lcd_error_count = 0U;
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
    UI_GoToScreen(UI_SCREEN_MENU_ROOT);
  }
  else if (key == 'D')
  {
    settings.running = settings.running ? 0U : 1U;
    StateMachine_PostEvent(settings.running ? EV_START : EV_STOP);
    redraw_requested = 1U;
  }
}

static void UI_HandleMenuListKey(char key, const UI_MenuTableTypeDef *table)
{
  if (table == NULL || table->count == 0U)
  {
    return;
  }

  if (key == 'A')
  {
    menu_index = (menu_index == 0U)
                   ? (uint8_t)(table->count - 1U)
                   : (uint8_t)(menu_index - 1U);
    redraw_requested = 1U;
  }
  else if (key == 'B')
  {
    menu_index = (uint8_t)((menu_index + 1U) % table->count);
    redraw_requested = 1U;
  }
  else if (key == '#')
  {
    UI_ScreenTypeDef next = table->items[menu_index].next;

    /* Numeric-edit screens need their input buffer primed. */
    if ((next == UI_SCREEN_EDIT_TEMP) || (next == UI_SCREEN_EDIT_HUMIDITY))
    {
      UI_BeginNumericEdit(next);
    }
    else
    {
      UI_GoToScreen(next);
    }
  }
  else if (key == 'C')
  {
    UI_GoToScreen(UI_ParentScreen(screen));
  }
}

static void UI_HandleInfoLeafKey(char key)
{
  if (key == 'C')
  {
    UI_GoToScreen(UI_ParentScreen(screen));
  }
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

      UI_GoToScreen(UI_SCREEN_MENU_SETPOINTS);
    }
    else if (key == 'C')
    {
      UI_GoToScreen(UI_SCREEN_MENU_SETPOINTS);
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
    UI_GoToScreen(UI_SCREEN_MENU_SETPOINTS);
  }
}

static void UI_HandleKey(char key)
{
  if (key == KEYPAD_NO_KEY)
  {
    return;
  }

  switch (screen)
  {
    case UI_SCREEN_HOME:
      UI_HandleHomeKey(key);
      break;

    case UI_SCREEN_MENU_ROOT:
    case UI_SCREEN_MENU_SETPOINTS:
    case UI_SCREEN_MENU_ALARMS:
    case UI_SCREEN_MENU_LOGS:
    case UI_SCREEN_MENU_COMM:
      UI_HandleMenuListKey(key, UI_TableForScreen(screen));
      break;

    case UI_SCREEN_EDIT_TEMP:
    case UI_SCREEN_EDIT_HUMIDITY:
    case UI_SCREEN_EDIT_OFFSET:
      UI_HandleEditKey(key);
      break;

    case UI_SCREEN_SYSTEM_INFO:
    case UI_SCREEN_ALARMS_OVER_TEMP:
    case UI_SCREEN_ALARMS_SENSOR_RETRY:
    case UI_SCREEN_LOGS_VIEW:
    case UI_SCREEN_LOGS_ERASE:
    case UI_SCREEN_COMM_WIFI:
    case UI_SCREEN_COMM_HTTP:
      UI_HandleInfoLeafKey(key);
      break;

    default:
      break;
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

  s_lcd_hi2c = lcd_i2c;

  if (LCD_I2C_Init(&lcd, lcd_i2c, UI_LCD_ADDRESS, UI_LCD_COLS, UI_LCD_ROWS) == HAL_OK)
  {
    lcd_ready = 1U;
    (void)LCD_I2C_Clear(&lcd);
    redraw_requested = 1U;
    UI_Draw();
  }
}

/* GPIO bit-bang bus clear: 9 SCL pulses with SDA released. Recovers a stuck
 * slave that's holding SDA low mid-transfer (HW-level, not just peripheral). */
static void UI_I2cBusClear(void)
{
  GPIO_InitTypeDef io = {0};

  /* SCL = PB6, SDA = PB9 — both as open-drain outputs, idle high. */
  io.Pin   = GPIO_PIN_6 | GPIO_PIN_9;
  io.Mode  = GPIO_MODE_OUTPUT_OD;
  io.Pull  = GPIO_PULLUP;
  io.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &io);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_9, GPIO_PIN_SET);
  for (volatile uint32_t i = 0U; i < 1000U; i++) { __NOP(); }

  /* 9 SCL pulses to flush any stuck slave bit. */
  for (uint8_t i = 0U; i < 9U; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    for (volatile uint32_t j = 0U; j < 800U; j++) { __NOP(); }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    for (volatile uint32_t j = 0U; j < 800U; j++) { __NOP(); }
  }

  /* Manual STOP condition: SDA low → high while SCL high. */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
  for (volatile uint32_t i = 0U; i < 800U; i++) { __NOP(); }
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
  for (volatile uint32_t i = 0U; i < 800U; i++) { __NOP(); }

  /* MspInit will reconfigure as I2C alternate function on HAL_I2C_Init. */
}

static void UI_TryRecoverLCD(void)
{
  static uint32_t s_next_attempt_ms = 0U;

  if (s_lcd_hi2c == NULL)
  {
    return;
  }
  /* Two triggers:
   *   - peripheral wedged (state != READY)
   *   - draw layer flagged repeated transfer errors (lcd_ready == 0) */
  uint8_t periph_stuck = (HAL_I2C_GetState(s_lcd_hi2c) != HAL_I2C_STATE_READY);
  uint8_t draw_failing = (lcd_ready == 0U);
  if (!periph_stuck && !draw_failing)
  {
    return;
  }

  uint32_t now = HAL_GetTick();
  if (now < s_next_attempt_ms)
  {
    return;
  }
  s_next_attempt_ms = now + 5000U;

  /* LD5 (red) toggle = visual confirmation that recovery fired. */
  HAL_GPIO_TogglePin(GPIOD, LD5_Pin);

  (void)HAL_I2C_DeInit(s_lcd_hi2c);
  UI_I2cBusClear();
  if (HAL_I2C_Init(s_lcd_hi2c) != HAL_OK)
  {
    lcd_ready = 0U;
    return;
  }

  if (LCD_I2C_Init(&lcd, s_lcd_hi2c, UI_LCD_ADDRESS, UI_LCD_COLS, UI_LCD_ROWS) == HAL_OK)
  {
    lcd_ready = 1U;
    (void)LCD_I2C_Clear(&lcd);
    redraw_requested = 1U;
  }
  else
  {
    lcd_ready = 0U;
  }
}

void CabinetUI_Process(void)
{
  char key = Keypad_GetKey(&keypad);
  uint32_t now = HAL_GetTick();

  UI_HandleKey(key);

  UI_TryRecoverLCD();

  if (lcd_ready == 0U)
  {
    return;
  }

  /* Two redraw triggers with different rate-limits:
   *   - redraw_requested: data changed, but enforce min draw interval to avoid
   *     flooding the LCD I2C bus with rapid keypresses
   *   - periodic refresh: catch missed redraws (live values, uptime, etc.) */
  uint32_t since_last_draw = now - last_refresh_tick;
  uint8_t  due_request     = (redraw_requested != 0U) &&
                             (since_last_draw >= UI_MIN_DRAW_INTERVAL_MS);
  uint8_t  due_periodic    = (since_last_draw >= UI_REFRESH_MS);

  if (due_request || due_periodic)
  {
    UI_Draw();
  }
}

void CabinetUI_SetCurrentValues(float temperature, float humidity)
{
  current_temperature_x10 = UI_FloatToX10(temperature);
  current_humidity_x10 = UI_FloatToX10(humidity);
  redraw_requested = 1U;
}

const CabinetUI_SettingsTypeDef *CabinetUI_GetSettings(void)
{
  return &settings;
}

void CabinetUI_ApplyCommand_SetRunning(uint8_t running)
{
  settings.running = (running != 0U) ? 1U : 0U;
  StateMachine_PostEvent(settings.running ? EV_START : EV_STOP);
  redraw_requested = 1U;
}

void CabinetUI_ApplyCommand_SetSetpoints(const float *setpoint_temp_c,
                                         const float *setpoint_humidity_rh)
{
  if (setpoint_temp_c != NULL)
  {
    float v = (*setpoint_temp_c * 10.0f);
    int32_t x10 = (int32_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
    if (x10 < INT16_MIN) x10 = INT16_MIN;
    if (x10 > INT16_MAX) x10 = INT16_MAX;
    settings.set_temperature_x10 = (int16_t)x10;
  }

  if (setpoint_humidity_rh != NULL)
  {
    float h = *setpoint_humidity_rh;
    if (h < 0.0f)   h = 0.0f;
    if (h > 100.0f) h = 100.0f;
    settings.set_humidity = (uint8_t)(h + 0.5f);
  }

  UI_ClampSettings();
  redraw_requested = 1U;
}
