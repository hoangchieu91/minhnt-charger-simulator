#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

/**
 * @brief Diagnostics Module — Runtime, Heartbeat, Counters, Alarm Flags
 * 
 * Gọi Diag_Process() mỗi vòng loop (cần biết tick hiện tại).
 * Heartbeat tăng +1 mỗi giây. Master đọc 2 lần → nếu bằng nhau = board treo.
 */

/* Alarm flags bitmask */
#define ALARM_FLAG_OVERTEMP      (1 << 0)
#define ALARM_FLAG_DOOR_OPEN     (1 << 1)
#define ALARM_FLAG_TAMPER        (1 << 2)
#define ALARM_FLAG_OVERPOWER     (1 << 3)
#define ALARM_FLAG_VOLTAGE_FAULT (1 << 4)
#define ALARM_FLAG_ENERGY_LIMIT  (1 << 5)
#define ALARM_FLAG_LOW_CURRENT   (1 << 6)
#define ALARM_FLAG_COMM_FAIL     (1 << 7)
#define ALARM_FLAG_GROUND_FAULT  (1 << 8)
#define ALARM_FLAG_CONNECTOR     (1 << 9)
#define ALARM_FLAG_OVERCURRENT   (1 << 10)

typedef struct {
    uint32_t (*get_tick)(void);
} DiagHardwareConfig_t;

void Diag_Init(DiagHardwareConfig_t *config);

/** @brief Gọi mỗi vòng loop — đếm uptime, heartbeat */
void Diag_Process(void);

/** @brief Tracking nhiệt độ min/max */
void Diag_UpdateTemp(int16_t temp_01c);

/** @brief Increment counters */
void Diag_IncrementError(void);
void Diag_IncrementCharge(void);
void Diag_DLT645Ok(void);
void Diag_DLT645Fail(void);

/** @brief Set/clear alarm flags */
void Diag_SetAlarmFlag(uint16_t flag);
void Diag_ClearAlarmFlag(uint16_t flag);
void Diag_SetAlarmFlags(uint16_t flags);

/** @brief Getters */
uint32_t Diag_GetUptime(void);
uint16_t Diag_GetHeartbeat(void);
int16_t  Diag_GetTempMin(void);
int16_t  Diag_GetTempMax(void);
uint16_t Diag_GetErrorCount(void);
uint16_t Diag_GetChargeCount(void);
uint16_t Diag_GetDLT645Ok(void);
uint16_t Diag_GetDLT645Fail(void);
uint16_t Diag_GetAlarmFlags(void);

#endif // DIAGNOSTICS_H
