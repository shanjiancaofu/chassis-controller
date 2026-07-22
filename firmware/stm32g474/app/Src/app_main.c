#include "app_main.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_power_sample.h"
#include "board_self_test.h"
#include "chassis_control.h"
#include "fdcan_driver.h"
#include "iwdg.h"
#include "main.h"
#include "project_config.h"
#include "status_display.h"
#include "tim.h"
#include "usart.h"

static volatile uint8_t control_ticks_pending;
static volatile bool control_tick_overflow;

#if ENABLE_BAREMETAL_MOTOR_DEMO
static uint8_t demo_stage;
static uint32_t demo_stage_started_ms;
#endif

void App_Init(void)
{
  static const uint8_t startup_message[] = "chassis-controller started\r\n";

  HAL_UART_Transmit(&huart1, startup_message, sizeof(startup_message) - 1U,
                    HAL_MAX_DELAY);
  if (FdcanDriver_Init() != HAL_OK) {
    Error_Handler();
  }

  BspMotor_Init();
  if (!BspMotor_Start()) {
    Error_Handler();
  }
  BspEncoder_Init();
  if (!BspEncoder_Start() || !BspPowerSample_Init()) {
    Error_Handler();
  }
  (void)StatusDisplay_Init();

  ChassisControl_Init();
  if (!BoardSelfTest_Init()) {
    Error_Handler();
  }
  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
    Error_Handler();
  }

#if ENABLE_BAREMETAL_MOTOR_DEMO
  demo_stage = 0U;
  demo_stage_started_ms = HAL_GetTick();
  ChassisControl_SetTargetSpeed(0, 0);
  ChassisControl_NotifyCommandReceived(demo_stage_started_ms);
  (void)ChassisControl_Start();
#endif
}

void App_Run(void)
{
  static uint32_t last_heartbeat_ms;
  static uint32_t last_control_run_ms;
  static uint32_t last_telemetry_ms;
  static char telemetry[192];
  ChassisControlStatus status;
  uint8_t pending_ticks;
  bool tick_overflow;
  bool telemetry_enabled;
  uint32_t primask;
  const uint32_t now_ms = HAL_GetTick();
  const FdcanLoopbackStatus loopback_status =
      FdcanDriver_GetLoopbackStatus();

#if ENABLE_BAREMETAL_MOTOR_DEMO
  if (demo_stage < 6U) {
    ChassisControl_NotifyCommandReceived(now_ms);
  }
  switch (demo_stage) {
    case 0U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        ChassisControl_SetTargetSpeed(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                      MOTOR_DEMO_TARGET_COUNTS_PER_TICK);
        demo_stage = 1U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 1U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        ChassisControl_SetTargetSpeed(0, 0);
        demo_stage = 2U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 2U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        ChassisControl_SetTargetSpeed(-MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                     -MOTOR_DEMO_TARGET_COUNTS_PER_TICK);
        demo_stage = 3U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 3U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        ChassisControl_SetTargetSpeed(0, 0);
        demo_stage = 4U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 4U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_STOP_TIME_MS) {
        ChassisControl_SetTargetSpeed(MOTOR_DEMO_TARGET_COUNTS_PER_TICK,
                                     -MOTOR_DEMO_TARGET_COUNTS_PER_TICK);
        demo_stage = 5U;
        demo_stage_started_ms = now_ms;
      }
      break;
    case 5U:
      if (now_ms - demo_stage_started_ms >= MOTOR_DEMO_RUN_TIME_MS) {
        ChassisControl_Stop();
        demo_stage = 6U;
      }
      break;
    default:
      break;
  }
#endif

  primask = __get_PRIMASK();
  __disable_irq();
  pending_ticks = control_ticks_pending;
  control_ticks_pending = 0U;
  tick_overflow = control_tick_overflow;
  control_tick_overflow = false;
  __set_PRIMASK(primask);

  if (tick_overflow ||
      pending_ticks > MOTOR_CONTROL_MAX_PENDING_TICKS) {
    ChassisControl_LatchInternalFault(CHASSIS_FAULT_CONTROL_OVERRUN);
  } else {
    while (pending_ticks > 0U) {
      ChassisControl_Tick10ms(now_ms);
      --pending_ticks;
      last_control_run_ms = now_ms;
    }
  }

  StatusDisplay_Run(now_ms);
  telemetry_enabled = BoardSelfTest_Run(now_ms);

  if (BoardSelfTest_IsIwdgResetRequested()) {
    ChassisControl_Stop();
    BspMotor_EmergencyStop();
  }

  if (telemetry_enabled &&
      now_ms - last_telemetry_ms >= MOTOR_TELEMETRY_PERIOD_MS) {
    uint32_t supply_mv;
    const bool supply_valid =
        BspPowerSample_ReadMillivolts(&supply_mv);
    int length;
    const int32_t vin_mv = supply_valid ? (int32_t)supply_mv : -1;

    last_telemetry_ms = now_ms;
    ChassisControl_GetStatus(&status);
    length = snprintf(
        telemetry, sizeof(telemetry),
        "vin_mv=%ld lt=%ld ld=%ld lo=%d rt=%ld rd=%ld ro=%d state=%u fault=0x%08lx\r\n",
        (long)vin_mv,
        (long)status.left_target, (long)status.left_delta,
        (int)status.left_output, (long)status.right_target,
        (long)status.right_delta, (int)status.right_output,
        (unsigned int)status.state, (unsigned long)status.fault_flags);
    if (length > 0 && (size_t)length < sizeof(telemetry) &&
        huart1.gState == HAL_UART_STATE_READY) {
      (void)HAL_UART_Transmit_DMA(&huart1, (uint8_t *)telemetry,
                                 (uint16_t)length);
    }
  }

  if (now_ms - last_heartbeat_ms >= 500U) {
    last_heartbeat_ms = now_ms;
    HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
  }
  if (loopback_status == FDCAN_LOOPBACK_PASSED) {
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  } else if (loopback_status == FDCAN_LOOPBACK_FAILED) {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
  }

  if (last_control_run_ms != 0U &&
      now_ms - last_control_run_ms <= 50U &&
      !ChassisControl_HasInternalFault() &&
      !BoardSelfTest_IsIwdgResetRequested()) {
    HAL_IWDG_Refresh(&hiwdg);
  }
}

void App_FatalError(void)
{
  ChassisControl_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    if (control_ticks_pending < UINT8_MAX) {
      ++control_ticks_pending;
    } else {
      control_tick_overflow = true;
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == KEY_Pin) {
    StatusDisplay_OnKeyInterrupt();
  }
  if (gpio_pin == E_STOP_Pin &&
      HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    ChassisControl_EmergencyStopFromIsr();
  }
}
