#ifndef DIGITAL_INPUT_H
#define DIGITAL_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Digital Input Module: Door Sensor + DIP Switch + Tamper Detection
 * Door: PB12, Input Pullup, LOW = cửa mở, debounce 50ms
 * DIP: PB5(ADD0), PB6(ADD1), PB7(ADD2) → Modbus Address 0-7
 * TAMPER: Khóa cài + Cửa mở = Phá hoại!
 */

#define DI_DEBOUNCE_MS 50

typedef struct {
    uint8_t  (*read_door)(void);   /* Trả 0=LOW(mở), 1=HIGH(đóng) */
    uint8_t  (*read_dip)(void);    /* Trả 3-bit value (0-7) */
    uint32_t (*get_tick)(void);
} DI_HardwareConfig_t;

void DI_Init(DI_HardwareConfig_t *config);

/** @brief Gọi liên tục trong main loop (xử lý debounce) */
void DI_Process(void);

/** @brief Cửa đang mở? (đã debounce) */
bool DI_IsDoorOpen(void);

/** @brief Lấy địa chỉ Modbus từ DIP Switch (0-7) */
uint8_t DI_GetModbusAddr(void);

/** @brief Phát hiện phá hoại: khóa cài + cửa mở */
bool DI_IsTamper(void);

#endif // DIGITAL_INPUT_H
