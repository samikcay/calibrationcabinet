#ifndef KEYPAD_H
#define KEYPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define KEYPAD_ROWS 4U
#define KEYPAD_COLS 4U
#define KEYPAD_NO_KEY '\0'

typedef struct
{
  GPIO_TypeDef *row_ports[KEYPAD_ROWS];
  uint16_t row_pins[KEYPAD_ROWS];
  GPIO_TypeDef *col_ports[KEYPAD_COLS];
  uint16_t col_pins[KEYPAD_COLS];
  char keymap[KEYPAD_ROWS][KEYPAD_COLS];
  uint32_t debounce_ms;
  char stable_key;
  char last_raw_key;
  char reported_key;
  uint32_t last_change_tick;
} Keypad_HandleTypeDef;

void Keypad_Init(Keypad_HandleTypeDef *keypad,
                 GPIO_TypeDef *row_ports[KEYPAD_ROWS],
                 const uint16_t row_pins[KEYPAD_ROWS],
                 GPIO_TypeDef *col_ports[KEYPAD_COLS],
                 const uint16_t col_pins[KEYPAD_COLS],
                 const char keymap[KEYPAD_ROWS][KEYPAD_COLS]);
char Keypad_GetKey(Keypad_HandleTypeDef *keypad);

#ifdef __cplusplus
}
#endif

#endif
