#include "meter_monitor.h"
#include <stddef.h>

#define OVERLOAD_DURATION_MS   30000U  /* 30s quá tải liên tục */
#define LOW_CURRENT_DURATION_MS 60000U /* 60s dòng thấp = xe đầy */
#define LOW_CURRENT_THRESH_001A 50     /* 0.5A */
#define VOLTAGE_MIN_01V       1800     /* 180.0V */
#define VOLTAGE_MAX_01V       2600     /* 260.0V */

static MeterHardwareConfig_t *hw = NULL;
static MeterConfig_t cfg = { .max_power_W = 7000, .rated_power_W = 3500, .energy_limit_Wh = 20000 };

static uint16_t voltage = 0, current = 0, power = 0;
static uint32_t energy = 0, energy_start = 0;
static MeterAlarm_t alarm = METER_OK;

/* Data validity tracking */
static uint32_t last_update_tick = 0;
static uint8_t  meter_valid = 0;       /* 1=data tươi, 0=stale/chưa có */
#define METER_STALE_TIMEOUT_MS 10000    /* 10s không update = stale */

/* Timers cho alarm có thời gian */
static uint32_t overload_start = 0;
static bool overload_timing = false;
static uint32_t lowcurr_start = 0;
static bool lowcurr_timing = false;

void Meter_Init(MeterHardwareConfig_t *hw_config) {
    if (hw_config) {
        hw = hw_config;
        voltage = current = power = 0;
        energy = energy_start = 0;
        alarm = METER_OK;
        overload_timing = false;
        lowcurr_timing = false;
    }
}

void Meter_SetConfig(MeterConfig_t *c) {
    if (c) cfg = *c;
}

void Meter_Update(uint16_t voltage_01V, uint16_t current_001A, uint16_t power_W, uint32_t energy_Wh) {
    voltage = voltage_01V;
    current = current_001A;
    power = power_W;
    energy = energy_Wh;
    if (hw) last_update_tick = hw->get_tick();
    meter_valid = 1;
}

void Meter_StartSession(void) {
    energy_start = energy;
    alarm = METER_OK;
    overload_timing = false;
    lowcurr_timing = false;
}

void Meter_Process(void) {
    if (!hw) return;
    uint32_t now = hw->get_tick();

    /* Check data freshness — nếu >10s không có Update → data stale */
    if (meter_valid && (now - last_update_tick >= METER_STALE_TIMEOUT_MS)) {
        meter_valid = 0;
        voltage = 0;
        current = 0;
        power = 0;
        /* energy giữ nguyên — giá trị tổng vẫn đúng */
    }

    /* 1. Kiểm tra quá áp / mất pha */
    if (voltage > 0 && (voltage < VOLTAGE_MIN_01V || voltage > VOLTAGE_MAX_01V)) {
        alarm = METER_VOLTAGE_FAULT;
        return;
    }

    /* 2. Kiểm tra quá công suất tức thì */
    if (power > cfg.max_power_W) {
        alarm = METER_OVERPOWER;
        return;
    }

    /* 3. Kiểm tra quá tải có thời gian (> 110% rated liên tục 30s) */
    uint32_t overload_thresh = (uint32_t)cfg.rated_power_W * 110 / 100;
    if (power > overload_thresh) {
        if (!overload_timing) {
            overload_timing = true;
            overload_start = now;
        } else if (now - overload_start >= OVERLOAD_DURATION_MS) {
            alarm = METER_OVERLOAD;
            return;
        }
    } else {
        overload_timing = false;
    }

    /* 4. Kiểm tra limit năng lượng phiên */
    if (energy >= energy_start && (energy - energy_start) >= cfg.energy_limit_Wh) {
        alarm = METER_ENERGY_LIMIT;
        return;
    }

    /* 5. Kiểm tra dòng thấp (xe đầy pin) */
    if (current < LOW_CURRENT_THRESH_001A) {
        if (!lowcurr_timing) {
            lowcurr_timing = true;
            lowcurr_start = now;
        } else if (now - lowcurr_start >= LOW_CURRENT_DURATION_MS) {
            alarm = METER_LOW_CURRENT;
            return;
        }
    } else {
        lowcurr_timing = false;
    }

    alarm = METER_OK;
}

MeterAlarm_t Meter_GetAlarm(void) { return alarm; }

uint32_t Meter_GetSessionEnergy(void) {
    if (energy >= energy_start) return energy - energy_start;
    return 0;
}

uint16_t Meter_GetVoltage(void) { return voltage; }
uint16_t Meter_GetCurrent(void) { return current; }
uint16_t Meter_GetPower(void) { return power; }
uint32_t Meter_GetEnergy(void) { return energy; }
uint8_t  Meter_IsValid(void) { return meter_valid; }
