#include "diagnostics.h"
#include <stddef.h>
#include <limits.h>

static DiagHardwareConfig_t *hw = NULL;

static uint32_t uptime_sec = 0;
static uint16_t heartbeat = 0;
static uint32_t last_sec_tick = 0;

static int16_t temp_min = 32767;   /* INT16_MAX */
static int16_t temp_max = -32768;  /* INT16_MIN */

static uint16_t error_count = 0;
static uint16_t charge_count = 0;
static uint16_t dlt645_ok_count = 0;
static uint16_t dlt645_fail_count = 0;

static uint16_t alarm_flags = 0;

void Diag_Init(DiagHardwareConfig_t *config) {
    if (config) {
        hw = config;
        uptime_sec = 0;
        heartbeat = 0;
        last_sec_tick = hw->get_tick();
        temp_min = 32767;
        temp_max = -32768;
        error_count = charge_count = 0;
        dlt645_ok_count = dlt645_fail_count = 0;
        alarm_flags = 0;
    }
}

void Diag_Process(void) {
    if (!hw) return;
    uint32_t now = hw->get_tick();
    /* Mỗi 1000ms → +1 giây uptime, +1 heartbeat */
    if (now - last_sec_tick >= 1000) {
        uint32_t elapsed = (now - last_sec_tick) / 1000;
        uptime_sec += elapsed;
        heartbeat += (uint16_t)elapsed;
        last_sec_tick += elapsed * 1000;
    }
}

void Diag_UpdateTemp(int16_t temp_01c) {
    if (temp_01c < temp_min) temp_min = temp_01c;
    if (temp_01c > temp_max) temp_max = temp_01c;
}

void Diag_IncrementError(void) { if (error_count < 65535) error_count++; }
void Diag_IncrementCharge(void) { if (charge_count < 65535) charge_count++; }
void Diag_DLT645Ok(void)   { if (dlt645_ok_count < 65535) dlt645_ok_count++; }
void Diag_DLT645Fail(void) { if (dlt645_fail_count < 65535) dlt645_fail_count++; }

void Diag_SetAlarmFlag(uint16_t flag)   { alarm_flags |= flag; }
void Diag_ClearAlarmFlag(uint16_t flag) { alarm_flags &= ~flag; }
void Diag_SetAlarmFlags(uint16_t flags) { alarm_flags = flags; }

uint32_t Diag_GetUptime(void)      { return uptime_sec; }
uint16_t Diag_GetHeartbeat(void)   { return heartbeat; }
int16_t  Diag_GetTempMin(void)     { return temp_min; }
int16_t  Diag_GetTempMax(void)     { return temp_max; }
uint16_t Diag_GetErrorCount(void)  { return error_count; }
uint16_t Diag_GetChargeCount(void) { return charge_count; }
uint16_t Diag_GetDLT645Ok(void)    { return dlt645_ok_count; }
uint16_t Diag_GetDLT645Fail(void)  { return dlt645_fail_count; }
uint16_t Diag_GetAlarmFlags(void)  { return alarm_flags; }
