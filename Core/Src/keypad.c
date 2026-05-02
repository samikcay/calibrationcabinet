#include "keypad.h"
#include <stddef.h>

#define KEYPAD_DEFAULT_DEBOUNCE_MS 30U

static void Keypad_SetAllRows(Keypad_HandleTypeDef *keypad, GPIO_PinState state)
{
  uint8_t row;

  for (row = 0U; row < KEYPAD_ROWS; row++)
  {
    HAL_GPIO_WritePin(keypad->row_ports[row], keypad->row_pins[row], state);
  }
}

static char Keypad_ScanRaw(Keypad_HandleTypeDef *keypad)
{
  uint8_t row;
  uint8_t col;

  Keypad_SetAllRows(keypad, GPIO_PIN_SET);

  for (row = 0U; row < KEYPAD_ROWS; row++)
  {
    HAL_GPIO_WritePin(keypad->row_ports[row], keypad->row_pins[row], GPIO_PIN_RESET);

    for (col = 0U; col < KEYPAD_COLS; col++)
    {
      if (HAL_GPIO_ReadPin(keypad->col_ports[col], keypad->col_pins[col]) == GPIO_PIN_RESET)
      {
        Keypad_SetAllRows(keypad, GPIO_PIN_SET);
        return keypad->keymap[row][col];
      }
    }

    HAL_GPIO_WritePin(keypad->row_ports[row], keypad->row_pins[row], GPIO_PIN_SET);
  }

  return KEYPAD_NO_KEY;
}

void Keypad_Init(Keypad_HandleTypeDef *keypad,
                 GPIO_TypeDef *row_ports[KEYPAD_ROWS],
                 const uint16_t row_pins[KEYPAD_ROWS],
                 GPIO_TypeDef *col_ports[KEYPAD_COLS],
                 const uint16_t col_pins[KEYPAD_COLS],
                 const char keymap[KEYPAD_ROWS][KEYPAD_COLS])
{
  uint8_t row;
  uint8_t col;

  if ((keypad == NULL) || (row_ports == NULL) || (row_pins == NULL) ||
      (col_ports == NULL) || (col_pins == NULL) || (keymap == NULL))
  {
    return;
  }

  for (row = 0U; row < KEYPAD_ROWS; row++)
  {
    keypad->row_ports[row] = row_ports[row];
    keypad->row_pins[row] = row_pins[row];
  }

  for (col = 0U; col < KEYPAD_COLS; col++)
  {
    keypad->col_ports[col] = col_ports[col];
    keypad->col_pins[col] = col_pins[col];
  }

  for (row = 0U; row < KEYPAD_ROWS; row++)
  {
    for (col = 0U; col < KEYPAD_COLS; col++)
    {
      keypad->keymap[row][col] = keymap[row][col];
    }
  }

  keypad->debounce_ms = KEYPAD_DEFAULT_DEBOUNCE_MS;
  keypad->stable_key = KEYPAD_NO_KEY;
  keypad->last_raw_key = KEYPAD_NO_KEY;
  keypad->reported_key = KEYPAD_NO_KEY;
  keypad->last_change_tick = HAL_GetTick();

  Keypad_SetAllRows(keypad, GPIO_PIN_SET);
}

char Keypad_GetKey(Keypad_HandleTypeDef *keypad)
{
  char raw_key;
  uint32_t now;

  if (keypad == NULL)
  {
    return KEYPAD_NO_KEY;
  }

  raw_key = Keypad_ScanRaw(keypad);
  now = HAL_GetTick();

  if (raw_key != keypad->last_raw_key)
  {
    keypad->last_raw_key = raw_key;
    keypad->last_change_tick = now;
    return KEYPAD_NO_KEY;
  }

  if ((now - keypad->last_change_tick) < keypad->debounce_ms)
  {
    return KEYPAD_NO_KEY;
  }

  if (raw_key != keypad->stable_key)
  {
    keypad->stable_key = raw_key;

    if (raw_key == KEYPAD_NO_KEY)
    {
      keypad->reported_key = KEYPAD_NO_KEY;
      return KEYPAD_NO_KEY;
    }

    if (raw_key != keypad->reported_key)
    {
      keypad->reported_key = raw_key;
      return raw_key;
    }
  }

  return KEYPAD_NO_KEY;
}
