#include "ntc_temp.h"
#include <stddef.h>

/* Hằng số mạch cầu chia áp */
#define R_PULLUP      10000U   /* 10kΩ treo lên Vcc */
#define ADC_MAX       4095U    /* 12-bit ADC */

/* Bảng Steinhart-Hart lookup: R_NTC(Ohm) → Temp(0.1°C)
 * NTC20K: R25=20kΩ, B=3950
 * Bảng rút gọn từ -10°C đến 100°C, nội suy tuyến tính giữa các điểm */
typedef struct { uint32_t r_ohm; int16_t temp_01c; } NTC_LookupEntry_t;

static const NTC_LookupEntry_t lookup[] = {
    {97900,  -100},  /* -10.0°C */
    {67800,     0},  /*   0.0°C */
    {47600,   100},  /*  10.0°C */
    {33900,   200},  /*  20.0°C */
    {20000,   250},  /*  25.0°C (danh định) */
    {24400,   220},  /*  22.0°C */
    {17400,   300},  /*  30.0°C */
    {12700,   350},  /*  35.0°C */
    {10000,   400},  /*  40.0°C */
    { 7500,   450},  /*  45.0°C */
    { 5700,   500},  /*  50.0°C */
    { 4400,   550},  /*  55.0°C */
    { 3400,   600},  /*  60.0°C */
    { 2700,   650},  /*  65.0°C */
    { 2100,   700},  /*  70.0°C */
    { 1700,   750},  /*  75.0°C */
    { 1400,   800},  /*  80.0°C */
    { 1100,   850},  /*  85.0°C */
    {  900,   900},  /*  90.0°C */
    {  740,   950},  /*  95.0°C */
    {  620,  1000},  /* 100.0°C */
};
#define LOOKUP_SIZE (sizeof(lookup) / sizeof(lookup[0]))

static NTC_HardwareConfig_t *hw = NULL;
static uint16_t filter_buf[NTC_FILTER_SIZE];
static uint8_t filter_idx = 0;
static bool filter_full = false;

void NTC_Init(NTC_HardwareConfig_t *config) {
    if (config) {
        hw = config;
        filter_idx = 0;
        filter_full = false;
        for (int i = 0; i < NTC_FILTER_SIZE; i++) filter_buf[i] = 0;
    }
}

void NTC_Process(void) {
    if (!hw) return;
    uint16_t raw = hw->read_adc();
    if (raw > ADC_MAX) raw = ADC_MAX;
    filter_buf[filter_idx] = raw;
    filter_idx = (filter_idx + 1) % NTC_FILTER_SIZE;
    if (filter_idx == 0) filter_full = true;
}

uint16_t NTC_GetRawADC(void) {
    /* Trả giá trị mới nhất */
    uint8_t last = (filter_idx == 0) ? (NTC_FILTER_SIZE - 1) : (filter_idx - 1);
    return filter_buf[last];
}

static uint16_t get_filtered_adc(void) {
    uint8_t count = filter_full ? NTC_FILTER_SIZE : filter_idx;
    if (count == 0) return 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; i++) sum += filter_buf[i];
    return (uint16_t)(sum / count);
}

uint32_t NTC_GetResistance(void) {
    uint16_t adc = get_filtered_adc();
    if (adc == 0) return 999999;       /* Hở mạch */
    if (adc >= ADC_MAX) return 0;      /* Chập GND */
    /* R_NTC = R_PULLUP × ADC / (4095 - ADC) */
    return (uint32_t)R_PULLUP * adc / (ADC_MAX - adc);
}

int16_t NTC_GetTempC(void) {
    uint32_t r = NTC_GetResistance();

    /* Tìm khoảng trong bảng lookup (R giảm khi T tăng) */
    /* Sắp xếp bảng theo R giảm dần */
    if (r >= lookup[0].r_ohm) return lookup[0].temp_01c;   /* Dưới -10°C */
    if (r <= lookup[LOOKUP_SIZE - 1].r_ohm) return lookup[LOOKUP_SIZE - 1].temp_01c; /* Trên 100°C */

    for (uint8_t i = 0; i < LOOKUP_SIZE - 1; i++) {
        if (r <= lookup[i].r_ohm && r >= lookup[i + 1].r_ohm) {
            /* Nội suy tuyến tính giữa 2 điểm */
            int32_t r_hi = (int32_t)lookup[i].r_ohm;
            int32_t r_lo = (int32_t)lookup[i + 1].r_ohm;
            int32_t t_hi = lookup[i].temp_01c;
            int32_t t_lo = lookup[i + 1].temp_01c;
            if (r_hi == r_lo) return (int16_t)t_hi;
            return (int16_t)(t_hi + (t_lo - t_hi) * (r_hi - (int32_t)r) / (r_hi - r_lo));
        }
    }
    return 250; /* Fallback 25°C */
}
