#ifndef METER_MONITOR_H
#define METER_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Meter Monitor — Giám sát Công suất & Năng lượng từ DLT645
 * 
 * Alarm types:
 * - OVERPOWER: power > max_power (cắt ngay)
 * - OVERLOAD: power > rated × 1.1 liên tục 30s
 * - ENERGY_LIMIT: consumed >= limit_Wh (sạc xong)
 * - LOW_CURRENT: current < 0.5A liên tục 60s (xe đầy)
 * - VOLTAGE_FAULT: V < 180V hoặc V > 260V
 */

typedef enum {
    METER_OK = 0,
    METER_OVERPOWER,
    METER_OVERLOAD,
    METER_ENERGY_LIMIT,
    METER_LOW_CURRENT,
    METER_VOLTAGE_FAULT
} MeterAlarm_t;

typedef struct {
    uint32_t (*get_tick)(void);
} MeterHardwareConfig_t;

typedef struct {
    uint16_t max_power_W;      /* Ngưỡng cầu dao cứng (VD: 7000W) */
    uint16_t rated_power_W;    /* Công suất danh định (VD: 3500W) */
    uint32_t energy_limit_Wh;  /* Giới hạn năng lượng 1 phiên (VD: 20000Wh = 20kWh) */
} MeterConfig_t;

void Meter_Init(MeterHardwareConfig_t *hw_config);
void Meter_SetConfig(MeterConfig_t *cfg);

/**
 * @brief Cập nhật giá trị đo từ DLT645 (gọi sau mỗi lần đọc đồng hồ thành công)
 * @param voltage_01V Điện áp (0.1V, VD: 2205 = 220.5V)
 * @param current_001A Dòng điện (0.01A, VD: 1050 = 10.5A)
 * @param power_W Công suất (W)
 * @param energy_Wh Năng lượng tích lũy (Wh)
 */
void Meter_Update(uint16_t voltage_01V, uint16_t current_001A, uint16_t power_W, uint32_t energy_Wh);

/** @brief Gọi liên tục trong main loop để kiểm tra alarm thời gian */
void Meter_Process(void);

/** @brief Bắt đầu phiên sạc mới (ghi nhận energy_start) */
void Meter_StartSession(void);

/** @brief Lấy alarm hiện tại */
MeterAlarm_t Meter_GetAlarm(void);

/** @brief Lấy năng lượng tiêu thụ trong phiên hiện tại (Wh) */
uint32_t Meter_GetSessionEnergy(void);

/** @brief Lấy giá trị đo */
uint16_t Meter_GetVoltage(void);
uint16_t Meter_GetCurrent(void);
uint16_t Meter_GetPower(void);
uint32_t Meter_GetEnergy(void);

/** @brief 1=data tươi (DLT645 OK), 0=stale (mất kết nối >10s) */
uint8_t  Meter_IsValid(void);

#endif // METER_MONITOR_H
