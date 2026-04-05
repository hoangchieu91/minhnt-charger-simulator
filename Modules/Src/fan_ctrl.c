#include "fan_ctrl.h"
#include <stddef.h>

static FanHardwareConfig_t *hw = NULL;

static int16_t threshold_high = 450;   /* Mặc định 45.0°C */
static int16_t threshold_low  = 380;   /* Mặc định 38.0°C */
static uint8_t fan_state = 0;
static uint32_t fan_on_tick = 0;       /* Thời điểm bật quạt */

static void clamp_thresholds(void) {
    /* Trần/Sàn */
    if (threshold_high < FAN_TEMP_FLOOR) threshold_high = FAN_TEMP_FLOOR;
    if (threshold_high > FAN_TEMP_CEILING) threshold_high = FAN_TEMP_CEILING;

    /* Gap tối thiểu: low phải < high - GAP_MIN */
    if (threshold_low >= threshold_high - FAN_TEMP_GAP_MIN) {
        threshold_low = threshold_high - FAN_TEMP_GAP_MIN;
    }
    /* Sàn cho low_temp */
    if (threshold_low < 0) threshold_low = 0;
}

void Fan_Init(FanHardwareConfig_t *config) {
    if (config) {
        hw = config;
        fan_state = 0;
        fan_on_tick = 0;
        if (hw->write_fan) hw->write_fan(0);
    }
}

void Fan_SetThresholds(int16_t high_temp_01c, int16_t low_temp_01c) {
    threshold_high = high_temp_01c;
    threshold_low = low_temp_01c;
    clamp_thresholds();
}

void Fan_Process(int16_t current_temp_01c) {
    if (!hw) return;

    if (fan_state == 0) {
        /* Quạt đang TẮT → kiểm tra bật */
        if (current_temp_01c >= threshold_high) {
            fan_state = 1;
            fan_on_tick = hw->get_tick();
            if (hw->write_fan) hw->write_fan(1);
        }
    } else {
        /* Quạt đang BẬT → kiểm tra tắt */
        /* Anti-chatter: phải chạy tối thiểu FAN_MIN_ON_TIME_MS */
        uint32_t elapsed = hw->get_tick() - fan_on_tick;
        if (elapsed >= FAN_MIN_ON_TIME_MS) {
            if (current_temp_01c <= threshold_low) {
                fan_state = 0;
                if (hw->write_fan) hw->write_fan(0);
            }
        }
        /* Nếu chưa đủ 30s hoặc temp vẫn trong vùng hysteresis → giữ nguyên ON */
    }
}

uint8_t Fan_GetState(void) { return fan_state; }
int16_t Fan_GetHighTemp(void) { return threshold_high; }
int16_t Fan_GetLowTemp(void) { return threshold_low; }
