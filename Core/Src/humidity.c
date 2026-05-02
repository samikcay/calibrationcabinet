#include "humidity.h"
#include "board_config.h"

#define RH_HYST 3.0f

typedef enum {
    HUM_IDLE = 0,
    HUM_HUMIDIFYING,
    HUM_CONDENSING,
} hum_state_t;

static hum_state_t s_state = HUM_IDLE;

static void set_humidifier(uint8_t on)
{
    Board_Humidifier_Set(on ? Board_TIM3_PWM_Period() : 0U);
}

static void set_condensation_peltier(uint8_t on)
{
    Board_CondensationPeltier_Set(on ? Board_TIM3_PWM_Period() : 0U);
}

void Humidity_Init(void)
{
    Board_TIM3_PWM_Init();
    s_state = HUM_IDLE;
    set_humidifier(0);
    set_condensation_peltier(0);
}

void Humidity_Step(float measured_rh, float target_rh)
{
    switch (s_state) {
        case HUM_HUMIDIFYING:
            if (measured_rh >= target_rh) {
                s_state = HUM_IDLE;
            }
            break;

        case HUM_CONDENSING:
            if (measured_rh <= target_rh) {
                s_state = HUM_IDLE;
            }
            break;

        default:
            if (measured_rh < (target_rh - RH_HYST)) {
                s_state = HUM_HUMIDIFYING;
            } else if (measured_rh > (target_rh + RH_HYST)) {
                s_state = HUM_CONDENSING;
            }
            break;
    }

    switch (s_state) {
        case HUM_HUMIDIFYING:
            set_humidifier(1);
            set_condensation_peltier(0);
            break;

        case HUM_CONDENSING:
            set_humidifier(0);
            set_condensation_peltier(1);
            break;

        default:
            set_humidifier(0);
            set_condensation_peltier(0);
            break;
    }
}

void Humidity_AllOff(void)
{
    s_state = HUM_IDLE;
    set_humidifier(0);
    set_condensation_peltier(0);
}
