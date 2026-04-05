#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "app_main.h"
#include "../../Modules/Inc/relay_ctrl.h"
#include "../../Modules/Inc/led_rgw.h"

// --- Hardware Mocking ---
static uint8_t mock_relay_pins[4] = {0};
static uint8_t mock_led_pins[3] = {0};
static uint32_t mock_tick = 0;

void hw_write_relay(uint8_t r_id, uint8_t s) { mock_relay_pins[r_id] = s; }
void hw_write_led(LedColor_t c, uint8_t s) { mock_led_pins[c] = s; }
uint32_t hw_get_tick(void) { return mock_tick; }

RelayHardwareConfig_t relay_hw = { .write_pin = hw_write_relay, .get_tick = hw_get_tick };
LedHardwareConfig_t led_hw     = { .write_pin = hw_write_led, .get_tick = hw_get_tick };

void advance_time(uint32_t ms) {
    for(uint32_t i=0; i<ms; i++) {
        mock_tick++;
        App_Process(); // Gọi State Machine Loop
    }
}

int main() {
    printf("==========================================\n");
    printf("= Starting State Machine TDD Native Test =\n");
    printf("==========================================\n\n");

    // Khởi tạo Module phần cứng
    Relay_Init(&relay_hw);
    LED_Init(&led_hw);
    
    // Khởi tạo State Machine FSM
    App_Init();
    
    // TEST 1: IDLE State
    printf("TEST 1: Init to IDLE. White LED should be ON, Relays OFF.\n");
    advance_time(10);
    assert(App_GetState() == STATE_IDLE);
    assert(mock_led_pins[LED_WHITE] == 1);
    assert(mock_led_pins[LED_GREEN] == 0);
    assert(mock_led_pins[LED_RED] == 0);
    assert(mock_relay_pins[RL_CHARGER] == 0);
    printf("--> [PASS] IDLE State OK.\n\n");

    // TEST 2: Trigger CHARGING
    printf("TEST 2: Trigger Charging. Red Blink 500ms, Relay ON delayed 100ms.\n");
    App_TriggerStartCharge();
    assert(App_GetState() == STATE_CHARGING);
    
    // Sau 10ms Charger vẫn phải Tắt vì Module Relay có khóa an toàn dead-time 100ms!
    advance_time(10);
    assert(mock_relay_pins[RL_CHARGER] == 0); 
    assert(mock_led_pins[LED_WHITE] == 0); // Led trắng tắt
    
    // Cấp thêm 100ms (tổng > 100ms)
    advance_time(100);
    assert(mock_relay_pins[RL_CHARGER] == 1); // Đã khóa relay sạc thành công
    
    // Tiến tới 500ms để test Red LED chớp tắt
    mock_tick = 500;
    App_Process();
    assert(mock_led_pins[LED_RED] == 1); // Nhấp nháy ON
    mock_tick = 1000;
    App_Process();
    assert(mock_led_pins[LED_RED] == 0); // Nhấp nháy OFF
    printf("--> [PASS] CHARGING State OK. (Relay Interlock + Blinking LED integrated)\n\n");

    // TEST 3: Ngắt mạc, chuyển dời FINISH
    printf("TEST 3: Stop Charging -> Switch to FINISH mode.\n");
    App_TriggerStopCharge();
    advance_time(50);
    assert(App_GetState() == STATE_FINISH);
    assert(mock_led_pins[LED_RED] == 0);
    assert(mock_led_pins[LED_GREEN] == 1); // Xanh liên tục báo hoàn thành
    assert(mock_relay_pins[RL_CHARGER] == 0); // Ngắt điện xe
    printf("--> [PASS] FINISH State OK.\n\n");

    // TEST 4: Sự cố giả định (Cửa Mở / Quá Nhiệt)
    printf("TEST 4: Critical Error Injection!\n");
    App_TriggerStartCharge(); // Cố tình bật sạc lại
    advance_time(110);
    assert(mock_relay_pins[RL_CHARGER] == 1); // Sạc đang lên
    
    // OOPS!
    App_TriggerError();
    advance_time(10);
    assert(App_GetState() == STATE_ERROR);
    assert(mock_relay_pins[RL_CHARGER] == 0); // PHẢI TẮT LẬP TỨC
    assert(mock_led_pins[LED_RED] == 1); // Đỏ khè báo lỗi
    printf("--> [PASS] ERROR Guard correctly shut down hardware.\n\n");


    printf("==========================================\n");
    printf("=> ALL FSM TESTS PASSED! \n");
    printf("==========================================\n");
    return 0;
}
