#include "thermal.h"
#include "board_config.h"
#include <math.h>

#define DEADBAND     0.02f
#define DEAD_TIME_MS 100U

static pid_config_t s_cfg;
static float        s_setpoint;
static float        s_i_term;
static float        s_d_filtered;
static float        s_prev_measured;
static float        s_prev_u;
static uint8_t      s_has_prev_measured;

static int8_t   s_last_applied_sign;
static uint32_t s_last_reversal_tick;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void Thermal_Init(const pid_config_t *cfg)
{
    if (cfg == NULL) {
        s_cfg = k_default_pid;
    } else {
        s_cfg = *cfg;
    }

    s_setpoint           = 0.0f;
    s_i_term             = 0.0f;
    s_d_filtered         = 0.0f;
    s_prev_measured      = 0.0f;
    s_prev_u             = 0.0f;
    s_has_prev_measured  = 0U;
    s_last_applied_sign  = 0;
    s_last_reversal_tick = 0U;

    Board_TIM3_PWM_Init();
    Thermal_ApplyOutput(0.0f);
    Board_MainPeltier_Enable(0U);
}

void Thermal_SetEnabled(uint8_t enabled)
{
    if (enabled == 0U) {
        Thermal_ApplyOutput(0.0f);
        Board_MainPeltier_Enable(0U);
    } else {
        Board_MainPeltier_Enable(1U);
    }
}

void Thermal_SetSetpoint(float t_c)
{
    s_setpoint = t_c;
}

float Thermal_Step(float measured_c, float dt_s)
{
    float d_raw = 0.0f;

    if (dt_s <= 0.0f) {
        dt_s = 0.001f;
    }

    float e = s_setpoint - measured_c;
    uint8_t heating = (e > 0.0f) ? 1U : 0U;

    float kp     = (heating && s_cfg.kp_heat     > 0.0f) ? s_cfg.kp_heat     : s_cfg.kp;
    float ki     = (heating && s_cfg.ki_heat     > 0.0f) ? s_cfg.ki_heat     : s_cfg.ki;
    float kd     = (heating && s_cfg.kd_heat     > 0.0f) ? s_cfg.kd_heat     : s_cfg.kd;
    float du_max = (heating && s_cfg.du_max_heat > 0.0f) ? s_cfg.du_max_heat : s_cfg.du_max;
    float u_max  = (heating && s_cfg.u_max_heat  > 0.0f) ? s_cfg.u_max_heat  : s_cfg.u_max;

    float p = kp * e;

    s_i_term = clampf(s_i_term + ki * e * dt_s,
                      s_cfg.i_min, s_cfg.i_max);

    if (s_has_prev_measured != 0U) {
        d_raw = -kd * (measured_c - s_prev_measured) / dt_s;
    } else {
        s_has_prev_measured = 1U;
    }

    s_d_filtered = s_cfg.d_alpha * s_d_filtered
                 + (1.0f - s_cfg.d_alpha) * d_raw;

    s_prev_measured = measured_c;

    float u_unlimited = p + s_i_term + s_d_filtered;
    float du = u_unlimited - s_prev_u;
    if (du_max > 0.0f) {
        float du_lim = du_max * dt_s;
        if (du > du_lim) {
            u_unlimited = s_prev_u + du_lim;
        } else if (du < -du_lim) {
            u_unlimited = s_prev_u - du_lim;
        }
    }

    float u = clampf(u_unlimited, s_cfg.u_min, u_max);

    if (kp > 0.0f) {
        s_i_term += (1.0f / kp) * (u - u_unlimited);
        s_i_term = clampf(s_i_term, s_cfg.i_min, s_cfg.i_max);
    }

    if (fabsf(u) < DEADBAND) {
        u = 0.0f;
    }

    s_prev_u = u;
    return u;
}

void Thermal_Reset(void)
{
    s_i_term             = 0.0f;
    s_d_filtered         = 0.0f;
    s_prev_measured      = 0.0f;
    s_prev_u             = 0.0f;
    s_has_prev_measured  = 0U;
    s_last_applied_sign  = 0;
    s_last_reversal_tick = 0U;
    Thermal_ApplyOutput(0.0f);
}

void Thermal_ApplyOutput(float u)
{
    int8_t new_sign = (u > 0.0f) ? 1 : (u < 0.0f ? -1 : 0);
    uint32_t now = HAL_GetTick();
    uint32_t pwm_val;
    uint32_t arr = Board_TIM3_PWM_Period();

    if (u > 1.0f) {
        u = 1.0f;
    } else if (u < -1.0f) {
        u = -1.0f;
    }

    if ((new_sign != 0) &&
        (s_last_applied_sign != 0) &&
        (new_sign != s_last_applied_sign)) {
        if (s_last_reversal_tick == 0U) {
            s_last_reversal_tick = now;
        }

        if ((now - s_last_reversal_tick) < DEAD_TIME_MS) {
            u = 0.0f;
            new_sign = 0;
        } else {
            s_last_reversal_tick = 0U;
        }
    } else {
        s_last_reversal_tick = 0U;
    }

    if (new_sign != 0) {
        s_last_applied_sign = new_sign;
    }

    if (u > 0.0f) {
        pwm_val = (uint32_t)(u * (float)arr);
        Board_MainPeltier_SetHeat(pwm_val);
        Board_MainPeltier_SetCool(0U);
    } else if (u < 0.0f) {
        pwm_val = (uint32_t)(-u * (float)arr);
        Board_MainPeltier_SetHeat(0U);
        Board_MainPeltier_SetCool(pwm_val);
    } else {
        Board_MainPeltier_SetHeat(0U);
        Board_MainPeltier_SetCool(0U);
    }
}
