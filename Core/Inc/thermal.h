#ifndef THERMAL_H
#define THERMAL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;
    float ki;
    float kd;
    float d_alpha;       /* derivative IIR coefficient [0,1] */
    float du_max;        /* slew-rate limit per second */
    float i_min, i_max;  /* anti-windup integral clamp */
    float u_min, u_max;  /* output saturation */
} pid_config_t;

/* Default gains from Ziegler-Nichols open-loop tuning (TDR §3.3) */
static const pid_config_t k_default_pid = {
    .kp      = 0.40f,
    .ki      = 0.02f,
    .kd      = 1.00f,
    .d_alpha = 0.80f,
    .du_max  = 0.10f,
    .i_min   = -0.5f,
    .i_max   = +0.5f,
    .u_min   = -1.0f,
    .u_max   = +1.0f,
};

void  Thermal_Init(const pid_config_t *cfg);
void  Thermal_SetSetpoint(float t_c);
float Thermal_Step(float measured_c, float dt_s); /* returns u in [-1, +1] */
void  Thermal_Reset(void);
void  Thermal_ApplyOutput(float u); /* drives TIM3 CH1/CH2 via BTS7960 */

#ifdef __cplusplus
}
#endif

#endif /* THERMAL_H */
