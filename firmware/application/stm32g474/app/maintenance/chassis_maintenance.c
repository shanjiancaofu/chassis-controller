#include "app/maintenance/chassis_maintenance.h"

#include <stddef.h>

#include "subsys/communication/ota/ota_can.h"
#include "subsys/communication/ota/ota_session.h"
#include "subsys/communication/ota/ota_uart_arm_guard.h"
#include "subsys/communication/ota/ota_uart.h"
#include "app/diagnostics/telemetry.h"
#include "subsys/communication/uart/uart_messages.h"
#include "app/chassis/command_manager.h"

#define OTA_UART_ARM_TIMEOUT_MS 30000U

static ChassisMaintenancePort maintenance_port;
static OtaUartArmGuard ota_uart_arm_guard;
static bool ota_terminal_cleaned;
static bool ota_response_waiting;
static OtaResponse ota_response;

bool ChassisMaintenance_Init(const ChassisMaintenancePort *port)
{
  if (port == NULL || port->acquire_ota_lock == NULL ||
      port->release_ota_lock == NULL) {
    return false;
  }
  maintenance_port = *port;
  OtaUartArmGuard_Init(&ota_uart_arm_guard);
  ota_terminal_cleaned = true;
  ota_response_waiting = false;
  ota_response = (OtaResponse){0};
  return true;
}

bool ChassisMaintenance_ArmUartOta(uint32_t now_ms)
{
  if (!maintenance_port.acquire_ota_lock()) {
    return false;
  }
  Telemetry_SetMode(TELEMETRY_MODE_OFF);
  OtaUart_Enable();
  OtaUartArmGuard_Arm(&ota_uart_arm_guard, now_ms);
  ota_terminal_cleaned = false;
  return true;
}

void ChassisMaintenance_Run(uint32_t now_ms)
{
  OtaMessage message;
  bool begin_allowed;
  bool prepared_here;
  bool response_submitted;

  if (!ota_response_waiting &&
      (OtaCan_TakeMessage(&message) ||
       OtaUart_TakeMessage(&message))) {
    prepared_here = false;
    if (message.type == OTA_MESSAGE_BEGIN && !OtaSession_IsActive() &&
        CommandManager_GetOwner() != COMMAND_SOURCE_OTA) {
      prepared_here = maintenance_port.acquire_ota_lock();
    }
    begin_allowed = CommandManager_GetOwner() == COMMAND_SOURCE_OTA;
    if (message.type == OTA_MESSAGE_BEGIN && OtaUart_IsEnabled() &&
        message.source != OTA_SOURCE_UART) {
      begin_allowed = false;
    }
    (void)OtaSession_Submit(&message, now_ms, begin_allowed);
    if (message.type == OTA_MESSAGE_BEGIN &&
        message.source == OTA_SOURCE_UART && OtaSession_IsActive() &&
        OtaSession_GetSource() == OTA_SOURCE_UART) {
      OtaUartArmGuard_EndWait(&ota_uart_arm_guard);
    }
    if (prepared_here && !OtaSession_IsActive()) {
      maintenance_port.release_ota_lock();
    }
  }

  OtaSession_Run(now_ms);
  if (!ota_response_waiting) {
    ota_response_waiting = OtaSession_TakeResponse(&ota_response);
  }
  if (ota_response_waiting) {
    response_submitted =
        ota_response.source == OTA_SOURCE_CAN_FD
            ? OtaCan_SendResponse(&ota_response)
            : ota_response.source == OTA_SOURCE_UART
                  ? OtaUart_SendResponse(&ota_response)
                  : false;
    if (response_submitted) {
      ota_response_waiting = false;
      OtaSession_ResponseSubmitted();
      if (ota_response.source == OTA_SOURCE_CAN_FD) {
        OtaCan_ResponseAccepted();
      }
    }
  }

  if (!ota_terminal_cleaned && !ota_response_waiting &&
      (OtaSession_GetState() == OTA_TRANSFER_ABORTED ||
       OtaSession_GetState() == OTA_TRANSFER_FAILED) &&
      !OtaSession_IsActive()) {
    if (OtaSession_GetSource() == OTA_SOURCE_UART) {
      OtaUart_Disable();
      OtaUartArmGuard_EndWait(&ota_uart_arm_guard);
    }
    maintenance_port.release_ota_lock();
    ota_terminal_cleaned = true;
  }

  if (OtaUart_IsEnabled() &&
      OtaUartArmGuard_ShouldTimeout(
          &ota_uart_arm_guard, now_ms, OTA_UART_ARM_TIMEOUT_MS,
          OtaSession_IsActive(), ota_response_waiting)) {
    OtaUart_Disable();
    OtaUartArmGuard_EndWait(&ota_uart_arm_guard);
    maintenance_port.release_ota_lock();
    ota_terminal_cleaned = true;
    (void)UartMessages_SendLog(now_ms, UART_MESSAGES_LOG_WARN, "ota",
                               "UART_ARM_TIMEOUT", "code=TIMEOUT");
  }
}
