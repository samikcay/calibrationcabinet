#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "app_types.h"

void           StateMachine_Init(void);
system_state_t StateMachine_Get(void);
const char    *StateMachine_StateName(system_state_t s);
void           StateMachine_PostEvent(system_event_t ev);

/* Sensor task signals the first valid DHT22 reading. Triggers INIT -> IDLE. */
void           StateMachine_NotifyFirstReading(void);

/* Control task pushes the latest PID inputs/setpoints. Used by the
 * stable-check timer to evaluate the +/- 0.5 C / +/- 3 %RH band. */
void           StateMachine_UpdatePidSnapshot(float temp_c,
                                              float humidity_rh,
                                              float target_temp_c,
                                              float target_humidity_rh);

/* Supervisor task body: blocks on the event group, applies one transition. */
void           StateMachine_RunStep(void);

/* Stable-check timer callback: evaluates band-hit count, posts EV_STABLE
 * after 30 consecutive in-band ticks (RUNNING -> STANDBY) or EV_UNSTABLE
 * on the first out-of-band tick (STANDBY -> RUNNING). */
void           StateMachine_StableCheck(void);

#endif /* STATE_MACHINE_H */
