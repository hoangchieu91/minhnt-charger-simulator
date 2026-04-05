#include "led_rgw.h"
#include <stddef.h>

#define BLINK_INTERVAL_MS 500

static LedHardwareConfig_t *hw = NULL;
static LedMode_t current_modes[3] = {LED_OFF, LED_OFF, LED_OFF};
static bool blink_state = false;
static uint32_t last_blink_tick = 0;

void LED_Init(LedHardwareConfig_t *config) {
    if (config) {
        hw = config;
        for (int i=0; i<3; i++) {
            current_modes[i] = LED_OFF;
            hw->write_pin((LedColor_t)i, 0);
        }
        blink_state = false;
        last_blink_tick = hw->get_tick();
    }
}

void LED_Set(LedColor_t color, LedMode_t mode) {
    if ((int)color > 2) return;
    current_modes[color] = mode;
    
    // Áp dụng luôn nếu là OFF hoặc SOLID
    if (hw) {
        if (mode == LED_OFF) hw->write_pin(color, 0);
        else if (mode == LED_SOLID) hw->write_pin(color, 1);
        // Nếu là BLINK, hàm Process sẽ tự lo
    }
}

void LED_Process(void) {
    if (!hw) return;
    
    uint32_t current_tick = hw->get_tick();
    if (current_tick - last_blink_tick >= BLINK_INTERVAL_MS) {
        last_blink_tick = current_tick;
        blink_state = !blink_state; // Đảo trạng thái nhấp nháy
        
        for (int i=0; i<3; i++) {
            if (current_modes[i] == LED_BLINK) {
                hw->write_pin((LedColor_t)i, blink_state ? 1 : 0);
            }
        }
    }
}
