#ifndef DOOR_LOCK_H
#define DOOR_LOCK_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Khóa cửa solenoid 12V qua RL4 (PB11)
 * 
 * - Mặc định: ĐÓNG (relay OFF = khóa cài)
 * - Mở: kích relay ON → tự động TẮT sau 5 giây
 * - Lệnh mở từ Modbus RTU (Coil 0x0002)
 */

#define DOOR_LOCK_PULSE_MS  5000U  /* Kích 5 giây */

typedef struct {
    void     (*write_relay)(uint8_t state);  /* 1=mở khóa, 0=đóng khóa */
    uint32_t (*get_tick)(void);
} DoorLockHardwareConfig_t;

void DoorLock_Init(DoorLockHardwareConfig_t *config);

/** @brief Kích mở khóa (bắt đầu đếm 5s) */
void DoorLock_Unlock(void);

/** @brief Gọi liên tục — tự tắt relay khi hết 5s */
void DoorLock_Process(void);

/** @brief Trạng thái: 1=đang mở, 0=đã khóa */
uint8_t DoorLock_IsUnlocked(void);

#endif // DOOR_LOCK_H
