#ifndef THERMAL_H
#define THERMAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* cooling / base gains */
    float kp;
    float ki;
    float kd;
    float d_alpha;        /* derivative IIR coefficient [0,1] */
    float du_max;         /* slew-rate limit per second */
    float i_min, i_max;   /* anti-windup integral clamp */
    float u_min, u_max;   /* output saturation */
    /* heating overrides — 0 = inherit base value */
    float kp_heat;
    float ki_heat;
    float kd_heat;
    float du_max_heat;
    float u_max_heat;
} pid_config_t;

/* Default gains from Ziegler-Nichols open-loop tuning (TDR §3.3) */
static const pid_config_t k_default_pid = {
    /* cooling */
    .kp      = 0.40f,
    .ki      = 0.02f,
    .kd      = 1.00f,
    .d_alpha = 0.80f,
    .du_max  = 0.10f,
    .i_min   = -0.5f,
    .i_max   = +0.5f,
    .u_min   = -1.0f,
    .u_max   = +1.0f,
    /* heating — conservative for powerful Peltier with slow internal block */
    .kp_heat     = 0.25f,
    .ki_heat     = 0.01f,
    .kd_heat     = 2.50f,
    .du_max_heat = 0.05f,
    .u_max_heat  = 0.25f,
};

void  Thermal_Init(const pid_config_t *cfg);
void  Thermal_SetSetpoint(float t_c);
float Thermal_Step(float measured_c, float dt_s); /* returns u in [-1, +1] */
void  Thermal_Reset(void);
void  Thermal_ApplyOutput(float u); /* drives TIM3 CH1/CH2 via BTS7960 */
void  Thermal_SetEnabled(uint8_t enabled); /* gates BTS7960 R_EN/L_EN */

#ifdef __cplusplus
}
#endif

#endif /* THERMAL_H */
