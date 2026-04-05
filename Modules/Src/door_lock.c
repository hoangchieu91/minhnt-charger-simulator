#include "door_lock.h"
#include <stddef.h>

static DoorLockHardwareConfig_t *hw = NULL;
static uint8_t unlocked = 0;
static uint32_t unlock_tick = 0;

void DoorLock_Init(DoorLockHardwareConfig_t *config) {
    if (config) {
        hw = config;
        unlocked = 0;
        hw->write_relay(0); /* Đóng khóa */
    }
}

void DoorLock_Unlock(void) {
    if (!hw) return;
    unlocked = 1;
    unlock_tick = hw->get_tick();
    hw->write_relay(1); /* Mở khóa */
}

void DoorLock_Process(void) {
    if (!hw || !unlocked) return;
    
    if (hw->get_tick() - unlock_tick >= DOOR_LOCK_PULSE_MS) {
        unlocked = 0;
        hw->write_relay(0); /* Tự đóng khóa sau 5s */
    }
}

uint8_t DoorLock_IsUnlocked(void) {
    return unlocked;
}
