#include "humidity.h"
#include "board_config.h"

#define TIM3_ARR   999U    /* matches htim3.Init.Period */
#define RH_HYST    3.0f    /* +/- % RH hysteresis band  */

typedef enum {
    HUM_IDLE = 0,
    HUM_HUMIDIFYING,
    HUM_DEHUMIDIFYING,
} hum_state_t;

static hum_state_t s_state = HUM_IDLE;

static void set_humidifier(uint8_t on)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, on ? TIM3_ARR : 0U);
}

static void set_dehumidifier(uint8_t on)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, on ? TIM3_ARR : 0U);
}

void Humidity_Init(void)
{
    s_state = HUM_IDLE;
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
    set_humidifier(0);
    set_dehumidifier(0);
}

/*
 * Bang-bang controller with hysteresis.
 *
 * Transitions:
 *   measured < target - HYST  -->  humidify
 *   measured > target + HYST  -->  dehumidify
 *   inside band               -->  keep current state (hysteresis)
 */
void Humidity_Step(float measured_rh, float target_rh)
{
    if (measured_rh < (target_rh - RH_HYST)) {
        s_state = HUM_HUMIDIFYING;
    } else if (measured_rh > (target_rh + RH_HYST)) {
        s_state = HUM_DEHUMIDIFYING;
    }
    /* else: stay in current state — hysteresis prevents chattering */

    switch (s_state) {
        case HUM_HUMIDIFYING:
            set_humidifier(1);
            set_dehumidifier(0);
            break;
        case HUM_DEHUMIDIFYING:
            set_humidifier(0);
            set_dehumidifier(1);
            break;
        default:
            set_humidifier(0);
            set_dehumidifier(0);
            break;
    }
}

void Humidity_AllOff(void)
{
    s_state = HUM_IDLE;
    set_humidifier(0);
    set_dehumidifier(0);
}
