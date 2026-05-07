#include "state_machine.h"
#include "cmsis_os.h"
#include <stdint.h>

#define STABLE_TEMP_BAND_C   0.5f
#define STABLE_HUM_BAND_RH   3.0f
#define STABLE_REQUIRED_HITS 30U

#define FAULT_EVENT_MASK ((uint32_t)EV_SENSOR_TIMEOUT | \
                          (uint32_t)EV_SENSOR_FAULT   | \
                          (uint32_t)EV_OVER_TEMP      | \
                          (uint32_t)EV_ESTOP)

#define ALL_EVENT_MASK   ((uint32_t)EV_SENSOR_TIMEOUT | \
                          (uint32_t)EV_SENSOR_FAULT   | \
                          (uint32_t)EV_START          | \
                          (uint32_t)EV_STOP           | \
                          (uint32_t)EV_ESTOP          | \
                          (uint32_t)EV_OVER_TEMP      | \
                          (uint32_t)EV_ALARM_ACK      | \
                          (uint32_t)EV_IDLE_TIMEOUT   | \
                          (uint32_t)EV_STABLE         | \
                          (uint32_t)EV_FIRST_READING  | \
                          (uint32_t)EV_UNSTABLE)

extern osEventFlagsId_t sysStateEventGroupHandle;

static volatile system_state_t s_state = STATE_INIT;

/* PID snapshot, written by control task, read by stable-check timer.
 * Aligned 32-bit reads/writes are atomic on Cortex-M, so no mutex needed. */
static volatile float   s_temp_c          = 0.0f;
static volatile float   s_humidity_rh     = 0.0f;
static volatile float   s_target_temp_c   = 0.0f;
static volatile float   s_target_hum_rh   = 0.0f;
static volatile uint8_t s_snapshot_valid  = 0U;

static uint16_t s_stable_hits = 0U;

void StateMachine_Init(void)
{
    s_state          = STATE_INIT;
    s_snapshot_valid = 0U;
    s_stable_hits    = 0U;
}

system_state_t StateMachine_Get(void)
{
    return s_state;
}

const char *StateMachine_StateName(system_state_t s)
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

void StateMachine_PostEvent(system_event_t ev)
{
    if (sysStateEventGroupHandle == NULL) {
        return;
    }
    (void)osEventFlagsSet(sysStateEventGroupHandle, (uint32_t)ev);
}

void StateMachine_NotifyFirstReading(void)
{
    StateMachine_PostEvent(EV_FIRST_READING);
}

void StateMachine_UpdatePidSnapshot(float temp_c, float humidity_rh,
                                    float target_temp_c, float target_hum_rh)
{
    s_temp_c         = temp_c;
    s_humidity_rh    = humidity_rh;
    s_target_temp_c  = target_temp_c;
    s_target_hum_rh  = target_hum_rh;
    s_snapshot_valid = 1U;
}

static void enter_state(system_state_t next)
{
    if (next != s_state) {
        s_stable_hits = 0U;
    }
    s_state = next;
}

void StateMachine_RunStep(void)
{
    if (sysStateEventGroupHandle == NULL) {
        osDelay(100U);
        return;
    }

    uint32_t flags = osEventFlagsWait(sysStateEventGroupHandle,
                                      ALL_EVENT_MASK,
                                      osFlagsWaitAny,
                                      osWaitForever);

    /* osEventFlagsWait returns an error code with the high bit set if it
     * times out or the call fails. Bail without touching state. */
    if ((flags & 0x80000000U) != 0U) {
        return;
    }

    /* Fault events have highest priority — go to ALARM regardless of state.
     * EV_ALARM_ACK in the same wake-up does not exit ALARM in this step;
     * the user must re-issue ack after the fault clears. */
    if ((flags & FAULT_EVENT_MASK) != 0U) {
        if (s_state != STATE_ALARM) {
            enter_state(STATE_ALARM);
        }
        return;
    }

    switch (s_state) {
        case STATE_INIT:
            if (flags & (uint32_t)EV_FIRST_READING) {
                enter_state(STATE_IDLE);
            }
            break;

        case STATE_IDLE:
            if (flags & (uint32_t)EV_START) {
                enter_state(STATE_RUNNING);
            }
            break;

        case STATE_RUNNING:
            if (flags & (uint32_t)EV_STOP) {
                enter_state(STATE_IDLE);
            } else if (flags & (uint32_t)EV_STABLE) {
                enter_state(STATE_STANDBY);
            }
            break;

        case STATE_STANDBY:
            if (flags & (uint32_t)EV_STOP) {
                enter_state(STATE_IDLE);
            } else if (flags & (uint32_t)EV_UNSTABLE) {
                enter_state(STATE_RUNNING);
            }
            break;

        case STATE_ALARM:
            if (flags & (uint32_t)EV_ALARM_ACK) {
                enter_state(STATE_IDLE);
            }
            break;

        default:
            break;
    }
}

void StateMachine_StableCheck(void)
{
    system_state_t st = s_state;

    if ((st != STATE_RUNNING) && (st != STATE_STANDBY)) {
        s_stable_hits = 0U;
        return;
    }

    if (s_snapshot_valid == 0U) {
        s_stable_hits = 0U;
        return;
    }

    float dt = s_temp_c - s_target_temp_c;
    float dh = s_humidity_rh - s_target_hum_rh;
    if (dt < 0.0f) dt = -dt;
    if (dh < 0.0f) dh = -dh;

    uint8_t in_band = (dt < STABLE_TEMP_BAND_C) && (dh < STABLE_HUM_BAND_RH);

    if (st == STATE_RUNNING) {
        if (in_band) {
            if (s_stable_hits < UINT16_MAX) {
                s_stable_hits++;
            }
            if (s_stable_hits >= STABLE_REQUIRED_HITS) {
                s_stable_hits = 0U;
                StateMachine_PostEvent(EV_STABLE);
            }
        } else {
            s_stable_hits = 0U;
        }
    } else { /* STATE_STANDBY */
        if (!in_band) {
            s_stable_hits = 0U;
            StateMachine_PostEvent(EV_UNSTABLE);
        }
    }
}
