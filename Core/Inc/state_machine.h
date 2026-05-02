/* TODO: implemented by supervisor-task team member */
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "app_types.h"

system_state_t StateMachine_Get(void);
void           StateMachine_PostEvent(system_event_t ev);

#endif /* STATE_MACHINE_H */
