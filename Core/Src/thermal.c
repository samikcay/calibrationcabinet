#include "thermal.h"
#include "board_config.h"
#include <math.h>

#define DEADBAND      0.02f   /* |u| below this → output = 0 */
#define DEAD_TIME_MS  100U    /* polarity reversal dead-time */

/* ── PID state ─────────────────────────────────────────────────────── */
static pid_config_t s_cfg;
static float        s_setpoint;
static float        s_i_term;
static float        s_d_filtered;
static float        s_prev_measured;
static float        s_prev_u;

/* ── ApplyOutput state ──────────────────────────────────────────────── */
static int8_t   s_last_applied_sign;
static uint32_t s_last_reversal_tick;

/* ── Helpers ────────────────────────────────────────────────────────── */
static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ── Public API ─────────────────────────────────────────────────────── */

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
    s_last_applied_sign  = 0;
    s_last_reversal_tick = 0;

    /* Enable BTS7960 H-bridge */
    HAL_GPIO_WritePin(BTS_R_EN_GPIO_Port, BTS_R_EN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BTS_L_EN_GPIO_Port, BTS_L_EN_Pin, GPIO_PIN_SET);

    /* Start PWM outputs at 0% duty */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

void Thermal_SetSetpoint(float t_c)
{
    s_setpoint = t_c;
}

/*
 * Discrete-time PID, parallel form, derivative on measurement.
 *
 * e[n]     = setpoint − measured[n]
 * P[n]     = Kp · e[n]
 * I[n]     = clamp(I[n-1] + Ki · e[n] · dt,  i_min, i_max)
 * D_raw[n] = −Kd · (measured[n] − measured[n-1]) / dt
 * D[n]     = d_alpha · D[n-1] + (1 − d_alpha) · D_raw[n]
 * u_raw    = P + I + D   (with slew-rate limit)
 * u        = clamp(u_raw, u_min, u_max)
 * Back-calc anti-windup: I += (1/Kp) · (u − u_raw)
 */
float Thermal_Step(float measured_c, float dt_s)
{
    if (dt_s <= 0.0f)
        dt_s = 0.001f;

    float e = s_setpoint - measured_c;

    float p = s_cfg.kp * e;

    s_i_term = clampf(s_i_term + s_cfg.ki * e * dt_s,
                      s_cfg.i_min, s_cfg.i_max);

    float d_raw = -s_cfg.kd * (measured_c - s_prev_measured) / dt_s;
    s_d_filtered = s_cfg.d_alpha * s_d_filtered
                 + (1.0f - s_cfg.d_alpha) * d_raw;

    s_prev_measured = measured_c;

    /* Slew-rate limit (scaled by dt_s) */
    float u_unlimited = p + s_i_term + s_d_filtered;
    float du = u_unlimited - s_prev_u;
    if (s_cfg.du_max > 0.0f) {
        float du_max = s_cfg.du_max * dt_s;
        if      (du >  du_max) u_unlimited = s_prev_u + du_max;
        else if (du < -du_max) u_unlimited = s_prev_u - du_max;
    }

    float u = clampf(u_unlimited, s_cfg.u_min, s_cfg.u_max);

    /* Back-calculation anti-windup */
    float kt = (s_cfg.kp > 0.0f) ? (1.0f / s_cfg.kp) : 0.0f;
    s_i_term += kt * (u - u_unlimited);
    s_i_term = clampf(s_i_term, s_cfg.i_min, s_cfg.i_max);

    /* Dead-band */
    if (fabsf(u) < DEADBAND)
        u = 0.0f;

    s_prev_u = u;
    return u;
}

void Thermal_Reset(void)
{
    s_i_term             = 0.0f;
    s_d_filtered         = 0.0f;
    s_prev_measured      = 0.0f;
    s_prev_u             = 0.0f;
    s_last_applied_sign  = 0;
    s_last_reversal_tick = 0;
}

/*
 * Maps u ∈ [-1, +1] to BTS7960 H-bridge:
 *   u > 0  → TIM3_CH1 (RPWM) active, CH2 = 0  (heating)
 *   u < 0  → TIM3_CH2 (LPWM) active, CH1 = 0  (cooling)
 *   u = 0  → both channels 0 (coast)
 *
 * Polarity reversal dead-time: if direction changes, output is forced
 * to 0 for DEAD_TIME_MS milliseconds to protect the H-bridge.
 */
void Thermal_ApplyOutput(float u)
{
    int8_t  new_sign = (u > 0.0f) ? 1 : (u < 0.0f ? -1 : 0);
    uint32_t now     = HAL_GetTick();

    /* Enforce dead-time on polarity reversal */
    if (new_sign != 0 && s_last_applied_sign != 0
        && new_sign != s_last_applied_sign)
    {
        if (s_last_reversal_tick == 0U) {
            s_last_reversal_tick = now;
        }

        if ((now - s_last_reversal_tick) < DEAD_TIME_MS) {
            u        = 0.0f;
            new_sign = 0;
        } else {
            s_last_reversal_tick = 0U;
        }
    }

    if (new_sign != 0)
        s_last_applied_sign = new_sign;

    uint32_t pwm_val;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3);

    if (u > 0.0f) {
        pwm_val = (uint32_t)(u * (float)arr);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_val);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0U);
    } else if (u < 0.0f) {
        pwm_val = (uint32_t)(-u * (float)arr);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm_val);
    } else {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0U);
    }
}
