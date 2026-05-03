#include "comms.h"
#include "cabinet_ui.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMS_BAUD              115200U
#define COMMS_RX_LINE_MAX       128U
#define COMMS_TX_TIMEOUT_MS     50U

/* Single 1-byte ring slot is enough — RX is byte-at-a-time, consumed in
 * Comms_Process() at loop rate. Volatile because the IRQ writes them. */
static volatile char     s_rx_line[COMMS_RX_LINE_MAX];
static volatile uint16_t s_rx_len      = 0U;
static volatile uint8_t  s_rx_overflow = 0U;
static volatile uint8_t  s_line_ready  = 0U;

static void usart2_gpio_init(void)
{
    GPIO_InitTypeDef io = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    io.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    io.Mode      = GPIO_MODE_AF_PP;
    io.Pull      = GPIO_PULLUP;
    io.Speed     = GPIO_SPEED_FREQ_HIGH;
    io.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &io);
}

static void usart2_periph_init(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    __HAL_RCC_USART2_CLK_ENABLE();

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;

    /* OVER8=0 (oversampling 16) → BRR = fck / baud */
    USART2->BRR = (pclk1 + (COMMS_BAUD / 2U)) / COMMS_BAUD;

    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void Comms_Init(void)
{
    usart2_gpio_init();
    usart2_periph_init();
}

void Comms_OnUsart2IRQ(void)
{
    uint32_t sr = USART2->SR;

    /* Overrun: read DR after SR to clear ORE. The byte still in DR predates
     * the loss event, so the line is already corrupted — discarding it just
     * means the whole line gets dropped at the next '\n'. The byte that
     * actually overran is gone (hardware never queued it).
     *
     * Note: the main source of overruns is the DHT22 driver disabling
     * interrupts for ~5 ms during a sensor read. At 115200 baud that can
     * lose ~55 byte-times if a command happens to arrive in that window.
     * The proper fix is UART RX via DMA — TODO if reliability becomes a
     * problem. For a 1 Hz dashboard with human-driven commands, the user
     * just retries. */
    if (sr & USART_SR_ORE) {
        (void)USART2->DR;
        s_rx_overflow = 1U;
        return;
    }

    if (sr & USART_SR_RXNE) {
        char c = (char)(USART2->DR & 0xFFU);

        if (c == '\r') {
            return;
        }

        if (c == '\n') {
            if (s_rx_overflow) {
                s_rx_len      = 0U;
                s_rx_overflow = 0U;
                return;
            }
            s_rx_line[s_rx_len] = '\0';
            s_line_ready = 1U;
            return;
        }

        if (s_line_ready) {
            /* Previous line not yet consumed — drop incoming bytes until
             * it is, to avoid tearing the next line into the old buffer. */
            return;
        }

        if (s_rx_len < (COMMS_RX_LINE_MAX - 1U)) {
            s_rx_line[s_rx_len++] = c;
        } else {
            s_rx_overflow = 1U;
            s_rx_len      = 0U;
        }
    }
}

static void uart_send_blocking(const char *s, uint16_t len)
{
    uint32_t start = HAL_GetTick();

    for (uint16_t i = 0U; i < len; i++) {
        while ((USART2->SR & USART_SR_TXE) == 0U) {
            if ((HAL_GetTick() - start) > COMMS_TX_TIMEOUT_MS) {
                return;
            }
        }
        USART2->DR = (uint8_t)s[i];
    }

    while ((USART2->SR & USART_SR_TC) == 0U) {
        if ((HAL_GetTick() - start) > COMMS_TX_TIMEOUT_MS) {
            return;
        }
    }
}

static const char *state_name(system_state_t s)
{
    switch (s) {
        case STATE_INIT:    return "INIT";
        case STATE_IDLE:    return "IDLE";
        case STATE_RUNNING: return "RUNNING";
        case STATE_ALARM:   return "ALARM";
        case STATE_STANDBY: return "STANDBY";
        default:            return "UNKNOWN";
    }
}

uint8_t Comms_ComputeAlarms(uint32_t consecutive_sensor_fails, float temperature_c)
{
    uint8_t mask = COMMS_ALARM_NONE;

    if (consecutive_sensor_fails >= COMMS_SENSOR_FAIL_THRESHOLD) {
        mask |= COMMS_ALARM_SENSOR_FAULT;
    }

    /* Only flag over-temp if the sensor reading is trustworthy. */
    if (((mask & COMMS_ALARM_SENSOR_FAULT) == 0U) &&
        (temperature_c > COMMS_OVER_TEMP_LIMIT_C)) {
        mask |= COMMS_ALARM_OVER_TEMP;
    }

    return mask;
}

system_state_t Comms_DeriveState(uint8_t alarm_mask,
                                 uint8_t has_first_reading,
                                 uint8_t running,
                                 float   temperature_c,
                                 float   humidity_rh,
                                 float   setpoint_temp_c,
                                 float   setpoint_humidity_rh)
{
    if (alarm_mask != 0U) {
        return STATE_ALARM;
    }
    if (has_first_reading == 0U) {
        return STATE_INIT;
    }
    if (running == 0U) {
        return STATE_IDLE;
    }
    if ((fabsf(temperature_c - setpoint_temp_c)   < COMMS_STANDBY_TEMP_BAND_C) &&
        (fabsf(humidity_rh   - setpoint_humidity_rh) < COMMS_STANDBY_RH_BAND)) {
        return STATE_STANDBY;
    }
    return STATE_RUNNING;
}

void Comms_SendTelemetry(float          temperature_c,
                         float          humidity_rh,
                         float          setpoint_temp_c,
                         float          setpoint_humidity_rh,
                         system_state_t state,
                         uint8_t        alarm_mask)
{
    char buf[160];
    int  n = snprintf(buf, sizeof(buf),
        "{\"temperature\":%.2f,\"humidity\":%.2f,"
        "\"setpoint_temp\":%.2f,\"setpoint_humidity\":%.2f,"
        "\"state\":\"%s\",\"alarm\":%u}\n",
        (double)temperature_c, (double)humidity_rh,
        (double)setpoint_temp_c, (double)setpoint_humidity_rh,
        state_name(state), (unsigned)alarm_mask);

    if (n > 0 && n < (int)sizeof(buf)) {
        uart_send_blocking(buf, (uint16_t)n);
    }
}

static int extract_float_after(const char *line, const char *key, float *out)
{
    const char *p = strstr(line, key);
    if (p == NULL) return 0;

    p = strchr(p + strlen(key), ':');
    if (p == NULL) return 0;
    p++;

    char *start = (char *)p;
    char *end   = start;
    float v     = strtof(start, &end);
    if (end == start) return 0;

    *out = v;
    return 1;
}

static void handle_command_line(const char *line)
{
    if (strstr(line, "\"START\"") != NULL) {
        CabinetUI_ApplyCommand_SetRunning(1U);
        return;
    }
    if (strstr(line, "\"STOP\"") != NULL) {
        CabinetUI_ApplyCommand_SetRunning(0U);
        return;
    }
    if (strstr(line, "\"SET\"") != NULL) {
        float st = NAN, sh = NAN;
        int got_t = extract_float_after(line, "\"setpoint_temp\"",     &st);
        int got_h = extract_float_after(line, "\"setpoint_humidity\"", &sh);
        if (got_t || got_h) {
            CabinetUI_ApplyCommand_SetSetpoints(got_t ? &st : NULL,
                                                got_h ? &sh : NULL);
        }
        return;
    }
}

void Comms_Process(void)
{
    if (s_line_ready == 0U) {
        return;
    }

    /* Snapshot under brief IRQ-disable to avoid the IRQ touching the line
     * while we copy it out. The line buffer is short so this is microseconds. */
    char     local[COMMS_RX_LINE_MAX];
    uint16_t len;

    __disable_irq();
    len = s_rx_len;
    if (len >= sizeof(local)) len = sizeof(local) - 1U;
    memcpy(local, (const void *)s_rx_line, len);
    local[len] = '\0';
    s_rx_len     = 0U;
    s_line_ready = 0U;
    __enable_irq();

    if (len >= 2U) {
        handle_command_line(local);
    }
}
