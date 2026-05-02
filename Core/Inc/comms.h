/* TODO: implemented by comms-task team member */
#ifndef COMMS_H
#define COMMS_H

#include "app_types.h"

/* Pass NULL for frame when sensor data is unavailable */
void Comms_QueueTelemetry(const sensor_frame_t *frame,
                           const setpoints_t    *sp,
                           system_state_t        state);

#endif /* COMMS_H */
