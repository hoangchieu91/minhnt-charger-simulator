#include <stdio.h>
#include <assert.h>
#include "../Modules/Inc/door_lock.h"

static uint32_t mock_tick = 0;
static uint8_t mock_relay = 0;

uint32_t dl_get_tick(void) { return mock_tick; }
void dl_write_relay(uint8_t s) { mock_relay = s; }

void test_door_lock_basic(void) {
    mock_tick = 0; mock_relay = 0;
    DoorLockHardwareConfig_t hw = { dl_write_relay, dl_get_tick };
    DoorLock_Init(&hw);
    
    assert(DoorLock_IsUnlocked() == 0);
    assert(mock_relay == 0); /* Mặc định đóng */

    /* Kích mở */
    mock_tick = 1000;
    DoorLock_Unlock();
    assert(DoorLock_IsUnlocked() == 1);
    assert(mock_relay == 1);

    /* Chưa hết 5s → vẫn mở */
    mock_tick = 4000;
    DoorLock_Process();
    assert(DoorLock_IsUnlocked() == 1);
    assert(mock_relay == 1);

    /* Hết 5s → tự đóng */
    mock_tick = 6001;
    DoorLock_Process();
    assert(DoorLock_IsUnlocked() == 0);
    assert(mock_relay == 0);

    printf("[PASS] test_door_lock_basic\n");
}

void test_door_lock_relock(void) {
    mock_tick = 0; mock_relay = 0;
    DoorLockHardwareConfig_t hw = { dl_write_relay, dl_get_tick };
    DoorLock_Init(&hw);

    /* Mở lần 1 */
    mock_tick = 100;
    DoorLock_Unlock();
    mock_tick = 5200;
    DoorLock_Process();
    assert(DoorLock_IsUnlocked() == 0);

    /* Mở lần 2 ngay sau khi đóng */
    mock_tick = 6000;
    DoorLock_Unlock();
    assert(DoorLock_IsUnlocked() == 1);
    mock_tick = 10000;
    DoorLock_Process();
    assert(DoorLock_IsUnlocked() == 1); /* 4s, chưa đủ 5s */
    mock_tick = 11001;
    DoorLock_Process();
    assert(DoorLock_IsUnlocked() == 0); /* Hết 5s */

    printf("[PASS] test_door_lock_relock\n");
}

int main(void) {
    printf("\n===== TDD: Door Lock 12V =====\n\n");
    test_door_lock_basic();
    test_door_lock_relock();
    printf("\n===== ALL DOOR LOCK TESTS PASSED =====\n\n");
    return 0;
}
