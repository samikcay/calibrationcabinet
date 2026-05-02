/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_types.h"
#include "thermal.h"
#include "humidity.h"
#include "state_machine.h"
#include "comms.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STABLE_TOLERANCE  0.5f   /* +/- degC band considered stable */
#define STABLE_COUNT      30U    /* consecutive 1-second checks required */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
setpoints_t           g_active_setpoints = { .temp_target = 25.0f, .rh_target = 50.0f };
static volatile float s_last_temp_c      = 0.0f;
/* USER CODE END Variables */
/* Definitions for supervisorTask */
osThreadId_t supervisorTaskHandle;
const osThreadAttr_t supervisorTask_attributes = {
  .name = "supervisorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for controlTask */
osThreadId_t controlTaskHandle;
const osThreadAttr_t controlTask_attributes = {
  .name = "controlTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for commTask */
osThreadId_t commTaskHandle;
const osThreadAttr_t commTask_attributes = {
  .name = "commTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for uiTask */
osThreadId_t uiTaskHandle;
const osThreadAttr_t uiTask_attributes = {
  .name = "uiTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for sensorDataQueue */
osMessageQueueId_t sensorDataQueueHandle;
const osMessageQueueAttr_t sensorDataQueue_attributes = {
  .name = "sensorDataQueue"
};
/* Definitions for userCmdQueue */
osMessageQueueId_t userCmdQueueHandle;
const osMessageQueueAttr_t userCmdQueue_attributes = {
  .name = "userCmdQueue"
};
/* Definitions for lcdRenderQueue */
osMessageQueueId_t lcdRenderQueueHandle;
const osMessageQueueAttr_t lcdRenderQueue_attributes = {
  .name = "lcdRenderQueue"
};
/* Definitions for stableCheckTimer */
osTimerId_t stableCheckTimerHandle;
const osTimerAttr_t stableCheckTimer_attributes = {
  .name = "stableCheckTimer"
};
/* Definitions for actuatorMutex */
osMutexId_t actuatorMutexHandle;
const osMutexAttr_t actuatorMutex_attributes = {
  .name = "actuatorMutex"
};
/* Definitions for i2cMutex */
osMutexId_t i2cMutexHandle;
const osMutexAttr_t i2cMutex_attributes = {
  .name = "i2cMutex"
};
/* Definitions for sysStateEventGroup */
osEventFlagsId_t sysStateEventGroupHandle;
const osEventFlagsAttr_t sysStateEventGroup_attributes = {
  .name = "sysStateEventGroup"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartSupervisorTask(void *argument);
void StartControlTask(void *argument);
void StartSensorTask(void *argument);
void StartCommTask(void *argument);
void StartUiTask(void *argument);
void stableCheckTimerCallback(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of actuatorMutex */
  actuatorMutexHandle = osMutexNew(&actuatorMutex_attributes);

  /* creation of i2cMutex */
  i2cMutexHandle = osMutexNew(&i2cMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of stableCheckTimer */
  stableCheckTimerHandle = osTimerNew(stableCheckTimerCallback, osTimerPeriodic, NULL, &stableCheckTimer_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of sensorDataQueue */
  sensorDataQueueHandle = osMessageQueueNew (4, 16, &sensorDataQueue_attributes);

  /* creation of userCmdQueue */
  userCmdQueueHandle = osMessageQueueNew (8, 16, &userCmdQueue_attributes);

  /* creation of lcdRenderQueue */
  lcdRenderQueueHandle = osMessageQueueNew (8, 32, &lcdRenderQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of supervisorTask */
  supervisorTaskHandle = osThreadNew(StartSupervisorTask, NULL, &supervisorTask_attributes);

  /* creation of controlTask */
  controlTaskHandle = osThreadNew(StartControlTask, NULL, &controlTask_attributes);

  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  /* creation of commTask */
  commTaskHandle = osThreadNew(StartCommTask, NULL, &commTask_attributes);

  /* creation of uiTask */
  uiTaskHandle = osThreadNew(StartUiTask, NULL, &uiTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of sysStateEventGroup */
  sysStateEventGroupHandle = osEventFlagsNew(&sysStateEventGroup_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartSupervisorTask */
/**
  * @brief  Function implementing the supervisorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSupervisorTask */
void StartSupervisorTask(void *argument)
{
  /* USER CODE BEGIN StartSupervisorTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartSupervisorTask */
}

/* USER CODE BEGIN Header_StartControlTask */
/**
* @brief Function implementing the controlTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartControlTask */
void StartControlTask(void *argument)
{
  /* USER CODE BEGIN StartControlTask */
  Thermal_Init(&k_default_pid);
  Humidity_Init();

  sensor_frame_t f;
  setpoints_t    sp;
  uint32_t       last_tick     = osKernelGetTickCount();
  uint32_t       last_pid_tick = last_tick;

  for (;;) {
    osStatus_t status = osMessageQueueGet(sensorDataQueueHandle, &f, NULL, 2000U);

    if (status != osOK) {
      /* Sensor timeout — safe shutdown */
      Thermal_ApplyOutput(0.0f);
      Humidity_AllOff();
      StateMachine_PostEvent(EV_SENSOR_TIMEOUT);
      Comms_QueueTelemetry(NULL, &sp, StateMachine_Get());
      last_tick = osKernelGetTickCount();
      osDelayUntil(last_tick + 1000U);
      continue;
    }

    if (!f.valid) {
      Thermal_ApplyOutput(0.0f);
      Humidity_AllOff();
      StateMachine_PostEvent(EV_SENSOR_FAULT);
      Comms_QueueTelemetry(&f, &sp, StateMachine_Get());
      last_tick = osKernelGetTickCount();
      osDelayUntil(last_tick + 1000U);
      continue;
    }

    /* Apply any pending setpoint command (non-blocking) */
    user_cmd_t cmd;
    if (osMessageQueueGet(userCmdQueueHandle, &cmd, NULL, 0U) == osOK) {
      osMutexAcquire(actuatorMutexHandle, osWaitForever);
      switch ((cmd_id_t)cmd.cmd) {
        case CMD_SET_TEMP:
          g_active_setpoints.temp_target = cmd.temp_target;
          break;
        case CMD_SET_RH:
          g_active_setpoints.rh_target = cmd.rh_target;
          break;
        case CMD_SET_BOTH:
          g_active_setpoints.temp_target = cmd.temp_target;
          g_active_setpoints.rh_target   = cmd.rh_target;
          break;
        case CMD_START:     StateMachine_PostEvent(EV_START);     break;
        case CMD_STOP:      StateMachine_PostEvent(EV_STOP);      break;
        case CMD_ESTOP:     StateMachine_PostEvent(EV_ESTOP);     break;
        case CMD_ALARM_ACK: StateMachine_PostEvent(EV_ALARM_ACK); break;
        default: break;
      }
      osMutexRelease(actuatorMutexHandle);
    }

    /* Read shared setpoints under mutex */
    osMutexAcquire(actuatorMutexHandle, osWaitForever);
    sp = g_active_setpoints;
    osMutexRelease(actuatorMutexHandle);

    Thermal_SetSetpoint(sp.temp_target);

    /* Over-temperature safety check (REQ-FUN-008) */
    if (f.temperature_c > 70.0f) {
      Thermal_ApplyOutput(0.0f);
      Humidity_AllOff();
      StateMachine_PostEvent(EV_OVER_TEMP);
      Comms_QueueTelemetry(&f, &sp, StateMachine_Get());
      osDelayUntil(last_tick + 1000U);
      last_tick += 1000U;
      continue;
    }

    system_state_t cur_state = StateMachine_Get();
    static system_state_t s_prev_state = STATE_INIT;

    if (cur_state == STATE_RUNNING && s_prev_state != STATE_RUNNING) {
      osTimerStart(stableCheckTimerHandle, 1000U);
    } else if (cur_state != STATE_RUNNING && s_prev_state == STATE_RUNNING) {
      osTimerStop(stableCheckTimerHandle);
    }
    s_prev_state = cur_state;

    s_last_temp_c = f.temperature_c;

    if (cur_state == STATE_RUNNING) {
      uint32_t now    = osKernelGetTickCount();
      float    dt_s   = (float)(now - last_pid_tick)
                        / (float)osKernelGetTickFreq();
      last_pid_tick   = now;

      float u = Thermal_Step(f.temperature_c, dt_s);
      Thermal_ApplyOutput(u);
      Humidity_Step(f.humidity_rh, sp.rh_target);
    } else {
      Thermal_ApplyOutput(0.0f);
      Thermal_Reset();
      Humidity_AllOff();
      last_pid_tick = osKernelGetTickCount();
    }

    Comms_QueueTelemetry(&f, &sp, cur_state);

    osDelayUntil(last_tick + 1000U);
    last_tick += 1000U;
  }
  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartCommTask */
/**
* @brief Function implementing the commTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommTask */
void StartCommTask(void *argument)
{
  /* USER CODE BEGIN StartCommTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCommTask */
}

/* USER CODE BEGIN Header_StartUiTask */
/**
* @brief Function implementing the uiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUiTask */
void StartUiTask(void *argument)
{
  /* USER CODE BEGIN StartUiTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUiTask */
}

/* stableCheckTimerCallback function */
void stableCheckTimerCallback(void *argument)
{
  /* USER CODE BEGIN stableCheckTimerCallback */
  static uint16_t s_count = 0U;

  if (StateMachine_Get() != STATE_RUNNING) {
    s_count = 0U;
    return;
  }

  float err = s_last_temp_c - g_active_setpoints.temp_target;
  if (err < 0.0f) err = -err;

  if (err <= STABLE_TOLERANCE) {
    if (++s_count >= STABLE_COUNT) {
      s_count = 0U;
      StateMachine_PostEvent(EV_STABLE);
    }
  } else {
    s_count = 0U;
  }
  /* USER CODE END stableCheckTimerCallback */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

