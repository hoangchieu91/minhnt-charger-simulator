#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../Modules/Inc/ntc_temp.h"
#include "../Modules/Inc/fan_ctrl.h"
#include "../Modules/Inc/meter_monitor.h"
#include "../Modules/Inc/digital_input.h"

/* ============ MOCK HARDWARE ============ */
static uint16_t mock_adc_val = 2048;
static uint32_t mock_tick = 0;
static uint8_t  mock_fan_state = 0;
static uint8_t  mock_door_state = 1; /* 1=đóng */
static uint8_t  mock_dip_val = 3;

uint16_t mock_read_adc(void) { return mock_adc_val; }
uint32_t mock_get_tick(void) { return mock_tick; }
void mock_write_fan(uint8_t s) { mock_fan_state = s; }
uint8_t mock_read_door(void) { return mock_door_state; }
uint8_t mock_read_dip(void)  { return mock_dip_val; }

/* ============ TEST NTC ============ */
void test_ntc_basic(void) {
    NTC_HardwareConfig_t hw = { mock_read_adc };
    NTC_Init(&hw);

    /* ADC=2048 → R_NTC = 10000 × 2048 / (4095-2048) = ~10000Ω → ~40°C */
    mock_adc_val = 2048;
    for (int i = 0; i < 8; i++) NTC_Process();
    uint32_t r = NTC_GetResistance();
    int16_t t = NTC_GetTempC();
    printf("  NTC ADC=2048: R=%u Ohm, T=%d.%d°C\n", r, t/10, t%10);
    assert(r > 9000 && r < 11000);  /* ~10kΩ */
    assert(t > 350 && t < 450);     /* ~40°C */

    /* ADC=3000 → R_NTC = 10000*3000/1095 = ~27397Ω → ~20-22°C */
    mock_adc_val = 3000;
    for (int i = 0; i < 8; i++) NTC_Process();
    r = NTC_GetResistance();
    t = NTC_GetTempC();
    printf("  NTC ADC=3000: R=%u Ohm, T=%d.%d°C\n", r, t/10, t%10);
    assert(r > 25000 && r < 30000);
    assert(t > 180 && t < 260);

    printf("[PASS] test_ntc_basic\n");
}

void test_ntc_filter(void) {
    NTC_HardwareConfig_t hw = { mock_read_adc };
    NTC_Init(&hw);

    /* Bơm 4 mẫu ADC=1000, rồi 4 mẫu ADC=2000 → trung bình = 1500 */
    mock_adc_val = 1000;
    for (int i = 0; i < 4; i++) NTC_Process();
    mock_adc_val = 2000;
    for (int i = 0; i < 4; i++) NTC_Process();

    uint16_t raw = NTC_GetRawADC(); /* Mẫu cuối = 2000 */
    assert(raw == 2000);

    /* R tính trên giá trị đã lọc (trung bình) */
    uint32_t r = NTC_GetResistance();
    printf("  NTC Filter: R=%u Ohm (avg ADC~1500)\n", r);
    /* ADC=1500: R = 10000×1500/(4095-1500) = ~5780Ω ≈ 50°C */
    assert(r > 5000 && r < 6500);

    printf("[PASS] test_ntc_filter\n");
}

/* ============ TEST FAN ============ */
void test_fan_hysteresis(void) {
    FanHardwareConfig_t hw = { mock_write_fan, mock_get_tick };
    Fan_Init(&hw);
    Fan_SetThresholds(450, 380); /* 45°C bật, 38°C tắt */

    /* Nhiệt 40°C → chưa bật */
    Fan_Process(400);
    assert(Fan_GetState() == 0);

    /* Nhiệt 45°C → BẬT */
    Fan_Process(450);
    assert(Fan_GetState() == 1);
    assert(mock_fan_state == 1);

    /* Nhiệt 42°C (vùng hysteresis) → VẪN BẬT (vì chưa đủ 30s) */
    mock_tick = 10000;
    Fan_Process(420);
    assert(Fan_GetState() == 1);

    /* Nhiệt 37°C nhưng chưa đủ 30s → VẪN BẬT (anti-chatter) */
    mock_tick = 20000;
    Fan_Process(370);
    assert(Fan_GetState() == 1); /* min ON chưa hết */

    /* Tick qua 30s, nhiệt 37°C → TẮT */
    mock_tick = 35000;
    Fan_Process(370);
    assert(Fan_GetState() == 0);
    assert(mock_fan_state == 0);

    printf("[PASS] test_fan_hysteresis\n");
}

void test_fan_invalid_thresholds(void) {
    FanHardwareConfig_t hw = { mock_write_fan, mock_get_tick };
    Fan_Init(&hw);

    /* Set high=40 low=42 (sai! low > high) → auto clamp */
    Fan_SetThresholds(400, 420);
    assert(Fan_GetHighTemp() == 400);
    assert(Fan_GetLowTemp() == 350); /* 400 - 50 = 350 */

    /* Set high=20°C (dưới sàn 30°C) → clamp lên 30°C */
    Fan_SetThresholds(200, 100);
    assert(Fan_GetHighTemp() == 300);
    assert(Fan_GetLowTemp() == 100); /* low=100 hợp lệ (300-100=200 > gap 50) */

    /* Set high=90°C (vượt trần 80°C) → clamp xuống 80°C */
    Fan_SetThresholds(900, 700);
    assert(Fan_GetHighTemp() == 800);
    assert(Fan_GetLowTemp() == 700);

    printf("[PASS] test_fan_invalid_thresholds\n");
}

/* ============ TEST METER ============ */
void test_meter_overpower(void) {
    MeterHardwareConfig_t hw = { mock_get_tick };
    Meter_Init(&hw);
    MeterConfig_t cfg = { .max_power_W = 7000, .rated_power_W = 3500, .energy_limit_Wh = 20000 };
    Meter_SetConfig(&cfg);
    Meter_StartSession();

    /* Công suất bình thường */
    Meter_Update(2200, 1000, 3000, 100);
    Meter_Process();
    assert(Meter_GetAlarm() == METER_OK);

    /* Quá công suất tức thì → OVERPOWER */
    Meter_Update(2200, 3500, 8000, 200);
    Meter_Process();
    assert(Meter_GetAlarm() == METER_OVERPOWER);

    printf("[PASS] test_meter_overpower\n");
}

void test_meter_energy_limit(void) {
    mock_tick = 0;
    MeterHardwareConfig_t hw = { mock_get_tick };
    Meter_Init(&hw);
    MeterConfig_t cfg = { .max_power_W = 7000, .rated_power_W = 3500, .energy_limit_Wh = 100 };
    Meter_SetConfig(&cfg);

    Meter_Update(2200, 1000, 3000, 5000);
    Meter_StartSession(); /* energy_start = 5000 */

    /* Chưa đạt limit */
    Meter_Update(2200, 1000, 3000, 5050);
    Meter_Process();
    assert(Meter_GetAlarm() == METER_OK);
    assert(Meter_GetSessionEnergy() == 50);

    /* Đạt limit 100Wh */
    Meter_Update(2200, 1000, 3000, 5100);
    Meter_Process();
    assert(Meter_GetAlarm() == METER_ENERGY_LIMIT);
    assert(Meter_GetSessionEnergy() == 100);

    printf("[PASS] test_meter_energy_limit\n");
}

void test_meter_low_current(void) {
    mock_tick = 0;
    MeterHardwareConfig_t hw = { mock_get_tick };
    Meter_Init(&hw);
    MeterConfig_t cfg = { .max_power_W = 7000, .rated_power_W = 3500, .energy_limit_Wh = 99999 };
    Meter_SetConfig(&cfg);
    Meter_StartSession();

    /* Dòng thấp < 0.5A */
    Meter_Update(2200, 30, 10, 100);
    mock_tick = 0; Meter_Process();
    assert(Meter_GetAlarm() == METER_OK); /* Chưa đủ 60s */

    mock_tick = 30000; Meter_Process();
    assert(Meter_GetAlarm() == METER_OK); /* Chưa đủ 60s */

    mock_tick = 61000; Meter_Process();
    assert(Meter_GetAlarm() == METER_LOW_CURRENT); /* Đủ 60s → xe đầy! */

    printf("[PASS] test_meter_low_current\n");
}

void test_meter_voltage_fault(void) {
    mock_tick = 0;
    MeterHardwareConfig_t hw = { mock_get_tick };
    Meter_Init(&hw);
    Meter_StartSession();

    /* Mất pha (V < 180V) */
    Meter_Update(1500, 1000, 3000, 100); /* 150V */
    Meter_Process();
    assert(Meter_GetAlarm() == METER_VOLTAGE_FAULT);

    printf("[PASS] test_meter_voltage_fault\n");
}

/* ============ TEST DIGITAL INPUT ============ */
void test_door_debounce(void) {
    mock_tick = 0;
    mock_door_state = 1; /* đóng */
    mock_dip_val = 5;
    DI_HardwareConfig_t hw = { mock_read_door, mock_read_dip, mock_get_tick };
    DI_Init(&hw);

    assert(DI_IsDoorOpen() == false);
    assert(DI_GetModbusAddr() == 5);

    /* Mở cửa */
    mock_door_state = 0;
    mock_tick = 100;
    DI_Process(); /* Phát hiện thay đổi, bắt đầu debounce */
    assert(DI_IsDoorOpen() == false); /* Chưa debounce xong */

    mock_tick = 140;
    DI_Process();
    assert(DI_IsDoorOpen() == false); /* Chưa 50ms */

    mock_tick = 155;
    DI_Process();
    assert(DI_IsDoorOpen() == true); /* Debounce xong! */

    /* Đóng cửa lại */
    mock_door_state = 1;
    mock_tick = 200;
    DI_Process();
    mock_tick = 260;
    DI_Process();
    assert(DI_IsDoorOpen() == false);

    printf("[PASS] test_door_debounce\n");
}

/* ============ MAIN ============ */
int main(void) {
    printf("\n===== TDD Phase 3: NTC + Fan + Meter + DI =====\n\n");

    printf("[NTC Temperature]\n");
    test_ntc_basic();
    test_ntc_filter();

    printf("\n[Fan Controller]\n");
    mock_tick = 0;
    test_fan_hysteresis();
    test_fan_invalid_thresholds();

    printf("\n[Meter Monitor]\n");
    test_meter_overpower();
    test_meter_energy_limit();
    test_meter_low_current();
    test_meter_voltage_fault();

    printf("\n[Digital Input]\n");
    test_door_debounce();

    printf("\n===== ALL 9 TESTS PASSED =====\n\n");
    return 0;
}
