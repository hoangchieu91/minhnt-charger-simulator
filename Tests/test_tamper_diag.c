#include <stdio.h>
#include <assert.h>
#include "../Modules/Inc/digital_input.h"
#include "../Modules/Inc/door_lock.h"
#include "../Modules/Inc/diagnostics.h"

/* === MOCKS === */
static uint32_t mock_tick = 0;
static uint8_t mock_door = 1; /* 1=đóng */
static uint8_t mock_dip = 3;
static uint8_t mock_lock_relay = 0;

uint32_t tm_get_tick(void) { return mock_tick; }
uint8_t tm_read_door(void) { return mock_door; }
uint8_t tm_read_dip(void) { return mock_dip; }
void tm_write_lock(uint8_t s) { mock_lock_relay = s; }

/* === TEST TAMPER === */
void test_tamper_locked_door_open(void) {
    /* Setup */
    mock_tick = 0; mock_door = 1;
    DoorLockHardwareConfig_t lock_hw = { tm_write_lock, tm_get_tick };
    DoorLock_Init(&lock_hw);
    DI_HardwareConfig_t di_hw = { tm_read_door, tm_read_dip, tm_get_tick };
    DI_Init(&di_hw);

    /* Khóa cài, cửa đóng → không tamper */
    mock_door = 1; mock_tick = 100; DI_Process(); mock_tick = 200; DI_Process();
    assert(DI_IsTamper() == 0);

    /* Khóa cài, cửa MỞ → TAMPER! */
    mock_door = 0; mock_tick = 300; DI_Process(); mock_tick = 400; DI_Process();
    assert(DI_IsDoorOpen() == 1);
    assert(DoorLock_IsUnlocked() == 0); /* Khóa vẫn cài */
    assert(DI_IsTamper() == 1);         /* ⚠️ PHÁ HOẠI! */

    printf("[PASS] test_tamper_locked_door_open\n");
}

void test_tamper_unlocked_door_open(void) {
    mock_tick = 0; mock_door = 1;
    DoorLockHardwareConfig_t lock_hw = { tm_write_lock, tm_get_tick };
    DoorLock_Init(&lock_hw);
    DI_HardwareConfig_t di_hw = { tm_read_door, tm_read_dip, tm_get_tick };
    DI_Init(&di_hw);

    /* Mở khóa trước */
    mock_tick = 100;
    DoorLock_Unlock();
    assert(DoorLock_IsUnlocked() == 1);

    /* Cửa mở khi khóa đã mở → KHÔNG phải tamper */
    mock_door = 0; mock_tick = 200; DI_Process(); mock_tick = 300; DI_Process();
    assert(DI_IsDoorOpen() == 1);
    assert(DI_IsTamper() == 0); /* Khóa đang mở → bình thường */

    printf("[PASS] test_tamper_unlocked_door_open\n");
}

/* === TEST DIAGNOSTICS === */
void test_diag_heartbeat(void) {
    mock_tick = 0;
    DiagHardwareConfig_t hw = { tm_get_tick };
    Diag_Init(&hw);

    assert(Diag_GetHeartbeat() == 0);
    assert(Diag_GetUptime() == 0);

    mock_tick = 1000; Diag_Process();
    assert(Diag_GetHeartbeat() == 1);
    assert(Diag_GetUptime() == 1);

    mock_tick = 5500; Diag_Process();
    assert(Diag_GetHeartbeat() == 5);
    assert(Diag_GetUptime() == 5);

    printf("[PASS] test_diag_heartbeat\n");
}

void test_diag_temp_tracking(void) {
    mock_tick = 0;
    DiagHardwareConfig_t hw = { tm_get_tick };
    Diag_Init(&hw);

    Diag_UpdateTemp(300); /* 30°C */
    Diag_UpdateTemp(450); /* 45°C */
    Diag_UpdateTemp(200); /* 20°C */
    assert(Diag_GetTempMin() == 200);
    assert(Diag_GetTempMax() == 450);

    printf("[PASS] test_diag_temp_tracking\n");
}

void test_diag_counters(void) {
    mock_tick = 0;
    DiagHardwareConfig_t hw = { tm_get_tick };
    Diag_Init(&hw);

    Diag_IncrementError();
    Diag_IncrementError();
    Diag_IncrementCharge();
    Diag_DLT645Ok();
    Diag_DLT645Ok();
    Diag_DLT645Ok();
    Diag_DLT645Fail();

    assert(Diag_GetErrorCount() == 2);
    assert(Diag_GetChargeCount() == 1);
    assert(Diag_GetDLT645Ok() == 3);
    assert(Diag_GetDLT645Fail() == 1);

    printf("[PASS] test_diag_counters\n");
}

void test_diag_alarm_flags(void) {
    mock_tick = 0;
    DiagHardwareConfig_t hw = { tm_get_tick };
    Diag_Init(&hw);

    assert(Diag_GetAlarmFlags() == 0);

    Diag_SetAlarmFlag(ALARM_FLAG_OVERTEMP);
    Diag_SetAlarmFlag(ALARM_FLAG_TAMPER);
    assert(Diag_GetAlarmFlags() == (ALARM_FLAG_OVERTEMP | ALARM_FLAG_TAMPER));
    assert(Diag_GetAlarmFlags() == 0x05); /* bit0 + bit2 */

    Diag_ClearAlarmFlag(ALARM_FLAG_OVERTEMP);
    assert(Diag_GetAlarmFlags() == ALARM_FLAG_TAMPER);

    printf("[PASS] test_diag_alarm_flags\n");
}

int main(void) {
    printf("\n===== TDD: Tamper + Diagnostics =====\n\n");
    test_tamper_locked_door_open();
    test_tamper_unlocked_door_open();
    test_diag_heartbeat();
    test_diag_temp_tracking();
    test_diag_counters();
    test_diag_alarm_flags();
    printf("\n===== ALL 6 TESTS PASSED =====\n\n");
    return 0;
}
