# Hardware Review Notes

This file lists the hardware parts assumed by the current firmware and the
technical details that matter for code review.

## Controller Board

### STM32F407G-DISC1 / STM32F407VGT6

- MCU family: STM32F4, ARM Cortex-M4F
- Core clock used by firmware: 168 MHz
- Logic level: 3.3 V
- Project HAL target define: `STM32F407xx`
- Important peripherals currently used:
  - I2C1 for LCD
  - SPI1 and I2S3 remain configured by CubeMX
  - TIM3 is configured directly in `board_config.c` for PWM outputs

## Sensors

### DHT22 / AM2302 Temperature-Humidity Sensor

- Interface: single-wire digital data line, not I2C
- Supply: typically 3.3 V to 5 V
- Temperature range: about -40 C to +80 C
- Humidity range: about 0 %RH to 100 %RH
- Typical resolution:
  - Temperature: 0.1 C
  - Humidity: 0.1 %RH
- Current firmware connection:
  - `+` -> 3.3 V
  - `OUT` -> `PD0`
  - `-` -> GND
- Firmware driver: `Core/Src/dhtsensor.c`

## Display And Input

### 20x4 I2C Character LCD

- Assumed LCD type: HD44780-compatible 20 columns x 4 rows
- Assumed I2C backpack: PCF8574-compatible
- I2C address used by firmware: `0x27`
- Current firmware connection:
  - `SCL` -> `PB6`
  - `SDA` -> `PB9`
  - `VCC` -> module supply, usually 5 V or 3.3 V depending on module
  - `GND` -> GND
- Firmware files:
  - `Core/Src/lcd_i2c.c`
  - `Core/Src/cabinet_ui.c`

### 4x4 Matrix Keypad

- Interface: 4 row GPIO outputs, 4 column GPIO inputs with pull-up
- Current firmware connection:
  - `R1` -> `PE7`
  - `R2` -> `PE8`
  - `R3` -> `PE9`
  - `R4` -> `PE10`
  - `C1` -> `PE11`
  - `C2` -> `PE12`
  - `C3` -> `PE13`
  - `C4` -> `PE14`
- Firmware driver: `Core/Src/keypad.c`

## Actuators

### Main Peltier With BTS7960B H-Bridge

- Purpose: main cabinet temperature control
- Driver module: BTS7960B H-bridge
- Control type: bidirectional PWM
- Firmware behavior:
  - Positive thermal output drives heating direction
  - Negative thermal output drives cooling direction
  - Direction reversal includes a short dead-time
- Current firmware connection:
  - `R_EN` -> `PC4`
  - `L_EN` -> `PC5`
  - `RPWM` -> `PC6` / `TIM3_CH1`
  - `LPWM` -> `PB5` / `TIM3_CH2`
  - Driver GND -> STM32 GND, common with Peltier supply negative
- Firmware files:
  - `Core/Src/thermal.c`
  - `Core/Src/board_config.c`

### Humidifier With D4184 MOSFET Module

- Purpose: increase relative humidity
- Driver module: D4184 MOSFET module
- Control type: low-side PWM switching
- Module pin labels in this project:
  - `+`
  - `LOAD`
  - `-`
  - `PWM`
  - `GND`
- Current firmware connection:
  - D4184 `PWM` -> `PB0` / `TIM3_CH3`
  - D4184 `GND` -> STM32 GND
  - D4184 `+` -> humidifier power supply positive
  - D4184 `-` -> humidifier power supply negative
  - D4184 `LOAD` -> humidifier negative
  - Humidifier positive -> humidifier power supply positive
- Firmware file: `Core/Src/humidity.c`

### Condensation Peltier With D4184 MOSFET Module

- Purpose: reduce humidity by cooling a condensation surface
- Driver module: D4184 MOSFET module
- Control type: single-direction PWM switching
- This Peltier is not used for cabinet temperature control.
- Current firmware connection:
  - D4184 `PWM` -> `PB1` / `TIM3_CH4`
  - D4184 `GND` -> STM32 GND
  - D4184 `+` -> condensation Peltier power supply positive
  - D4184 `-` -> condensation Peltier power supply negative
  - D4184 `LOAD` -> condensation Peltier negative
  - Condensation Peltier positive -> Peltier power supply positive
- Firmware file: `Core/Src/humidity.c`

## PWM Configuration

### TIM3

- Configured directly in `Core/Src/board_config.c`
- PWM frequency: 1 kHz
- Counter clock: 1 MHz
- Period/ARR: `999`
- Duty range: `0` to `999`
- Channels:
  - `CH1 PC6`: main Peltier heat / BTS7960B `RPWM`
  - `CH2 PB5`: main Peltier cool / BTS7960B `LPWM`
  - `CH3 PB0`: humidifier D4184 `PWM`
  - `CH4 PB1`: condensation Peltier D4184 `PWM`

## Review Notes

- All external actuator power supplies must share GND with STM32 GND.
- Do not power Peltier modules or the humidifier from STM32 pins.
- D4184 modules are used as low-side switches. The load positive side stays
  connected to the external supply positive; the module switches the load
  negative side through `LOAD`.
- Part numbers for the actual Peltier elements and humidifier module are not
  yet recorded in the firmware. Add exact voltage/current ratings here when the
  physical labels or datasheets are confirmed.
