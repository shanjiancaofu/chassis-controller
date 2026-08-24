#ifndef CHASSIS_MAINTENANCE_H
#define CHASSIS_MAINTENANCE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool (*acquire_ota_lock)(void);
  void (*release_ota_lock)(void);
} ChassisMaintenancePort;

bool ChassisMaintenance_Init(const ChassisMaintenancePort *port);
bool ChassisMaintenance_ArmUartOta(uint32_t now_ms);
void ChassisMaintenance_Run(uint32_t now_ms);

#endif
