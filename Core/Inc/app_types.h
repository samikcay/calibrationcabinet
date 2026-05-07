#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

/* Matches sensorDataQueue msgSize = 16 bytes */
typedef struct {
    float    temperature_c;     /* calibration offset already applied */
    float    humidity_rh;
    uint8_t  valid;             /* 1 if last read OK */
    uint8_t  _pad[3];
    uint32_t consecutive_fails; /* sensor read failures up to and including this frame */
} sensor_frame_t;

/* Matches userCmdQueue msgSize = 16 bytes */
typedef enum {
    CMD_SET_TEMP   = 0x01,
    CMD_SET_RH     = 0x02,
    CMD_SET_BOTH   = 0x03,
    CMD_START      = 0x10,
    CMD_STOP       = 0x11,
    CMD_ESTOP      = 0x12,
    CMD_ALARM_ACK  = 0x13,
} cmd_id_t;

typedef struct {
    uint8_t  cmd;         /* cmd_id_t stored as 1 byte */
    uint8_t  _pad[7];     /* padding to align floats   */
    float    temp_target;
    float    rh_target;
} user_cmd_t;             /* = 16 bytes, matches userCmdQueue */

typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_RUNNING,
    STATE_ALARM,
    STATE_STANDBY,
} system_state_t;

typedef enum {
    EV_SENSOR_TIMEOUT  = (1u << 0),
    EV_SENSOR_FAULT    = (1u << 1),
    EV_START           = (1u << 2),
    EV_STOP            = (1u << 3),
    EV_ESTOP           = (1u << 4),
    EV_OVER_TEMP       = (1u << 5),
    EV_ALARM_ACK       = (1u << 6),
    EV_IDLE_TIMEOUT    = (1u << 7),
    EV_STABLE          = (1u << 8),
    EV_FIRST_READING   = (1u << 9),
    EV_UNSTABLE        = (1u << 10),
} system_event_t;

#endif /* APP_TYPES_H */
