#include "digital_input.h"
#include "door_lock.h"
#include <stddef.h>

static DI_HardwareConfig_t *hw = NULL;

static bool door_open = false;          /* Trạng thái debounced */
static uint8_t door_raw_last = 1;       /* 1=đóng */
static uint32_t door_change_tick = 0;
static bool door_debouncing = false;

static uint8_t modbus_addr = 0;

void DI_Init(DI_HardwareConfig_t *config) {
    if (config) {
        hw = config;
        door_open = false;
        door_raw_last = 1;
        door_debouncing = false;
        /* Đọc DIP Switch 1 lần khi khởi động */
        modbus_addr = hw->read_dip() & 0x07;
    }
}

void DI_Process(void) {
    if (!hw) return;
    uint32_t now = hw->get_tick();
    uint8_t raw = hw->read_door();

    if (raw != door_raw_last) {
        /* Phát hiện thay đổi → bắt đầu debounce */
        door_raw_last = raw;
        door_change_tick = now;
        door_debouncing = true;
    }

    if (door_debouncing && (now - door_change_tick >= DI_DEBOUNCE_MS)) {
        /* Debounce xong → cập nhật trạng thái */
        door_open = (door_raw_last == 0); /* LOW = cửa mở (pullup) */
        door_debouncing = false;
    }
}

bool DI_IsDoorOpen(void) { return door_open; }
uint8_t DI_GetModbusAddr(void) { return modbus_addr; }

bool DI_IsTamper(void) {
    /* Phá hoại = Khóa ĐANG CÀI (relay OFF) + Cửa MỞ */
    return (DoorLock_IsUnlocked() == 0) && door_open;
}

