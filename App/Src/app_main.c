#include "app_main.h"
#include "relay_ctrl.h"
#include "led_rgw.h"
#include "ntc_temp.h"
#include "fan_ctrl.h"
#include "meter_monitor.h"
#include "digital_input.h"
#include "door_lock.h"
#include "diagnostics.h"
#include "error_log.h"

static SystemState_t current_state = STATE_INIT;
static uint32_t state_enter_tick = 0;
static uint32_t (*app_get_tick)(void) = (void*)0;
static uint32_t charge_start_tick = 0;   /* Session duration timer */

/* ─── Rev 2.0: Session Management ─── */
static uint16_t session_id = 0;          /* Auto-increment, Flash-persistent */
static uint16_t last_stop_reason = 0;    /* REASON_UNKNOWN */

/* ─── Rev 2.0: Current Limit (Dynamic Load Balancing) ─── */
static uint16_t current_limit_001A = 3200;  /* Default 32.00A */

/* ─── Rev 2.0: Session Energy Limit ─── */
static uint16_t session_energy_limit_wh = 0; /* 0 = disabled */

/* ─── Rev 2.0: Time Sync ─── */
static uint32_t unix_timestamp = 0;

/* ─── Rev 2.0: Force Fan ─── */
static uint8_t fan_forced = 0;
static uint32_t fan_force_tick = 0;
#define FAN_FORCE_TIMEOUT_MS 300000  /* 5 minutes auto-off */
static uint32_t last_dlt645_ok_tick = 0; /* DLT645 comm timeout tracking */

/* Lưu tick vào state khi chuyển trạng thái */
static void enter_state(SystemState_t new_state) {
    current_state = new_state;
    if (app_get_tick) state_enter_tick = app_get_tick();
}

/**
 * @brief Build alarm_flags tổng hợp từ tất cả sources
 * Gọi mỗi vòng loop sau khi quét tất cả module
 */
static void build_alarm_flags(int16_t temp) {
    uint16_t flags = 0;

    /* bit0: OVERTEMP */
    if (temp > OVERTEMP_LIMIT) flags |= ALARM_FLAG_OVERTEMP;

    /* bit1: DOOR_OPEN */
    if (DI_IsDoorOpen()) flags |= ALARM_FLAG_DOOR_OPEN;

    /* bit2: TAMPER — khóa cài + cửa mở = phá hoại! */
    if (DI_IsTamper()) flags |= ALARM_FLAG_TAMPER;

    /* bit3-6: từ Meter Monitor */
    MeterAlarm_t alarm = Meter_GetAlarm();
    if (alarm == METER_OVERPOWER || alarm == METER_OVERLOAD)
        flags |= ALARM_FLAG_OVERPOWER;
    if (alarm == METER_VOLTAGE_FAULT)
        flags |= ALARM_FLAG_VOLTAGE_FAULT;
    if (alarm == METER_ENERGY_LIMIT)
        flags |= ALARM_FLAG_ENERGY_LIMIT;
    if (alarm == METER_LOW_CURRENT)
        flags |= ALARM_FLAG_LOW_CURRENT;

    /* bit7: COMM_FAIL — DLT645 không phản hồi >30s */
    if (app_get_tick && (app_get_tick() - last_dlt645_ok_tick > 30000)) {
        flags |= ALARM_FLAG_COMM_FAIL;
    }

    Diag_SetAlarmFlags(flags);
}

void App_Init(void) {
    enter_state(STATE_IDLE);
    LED_Set(LED_WHITE, LED_SOLID);
    LED_Set(LED_GREEN, LED_OFF);
    LED_Set(LED_RED, LED_OFF);
    Relay_SetTarget(RL_CHARGER, RELAY_OFF);
    Relay_SetTarget(RL_SOCKET, RELAY_OFF);
}

void App_Process(void) {
    /* 1. Quét Module LED (lo nhấp nháy nếu có) */
    LED_Process();

    /* 2. Quét Module Relay (lo khóa chéo + delay dập hồ quang) */
    Relay_Process();

    /* 3. Quét NTC nhiệt độ */
    NTC_Process();
    int16_t temp = NTC_GetTempC();

    /* 4. Quét Fan Controller (Hysteresis tự quản) */
    Fan_Process(temp);

    /* 5. Quét Meter Monitor (alarm timing) */
    Meter_Process();

    /* 6. Quét Door Sensor (debounce) */
    DI_Process();

    /* 7. Quét Khóa cửa (tự tắt sau 5s) */
    DoorLock_Process();

    /* 8. Quét Diagnostics (uptime, heartbeat) */
    Diag_Process();
    Diag_UpdateTemp(temp);

    /* 9. Build alarm_flags tổng hợp */
    build_alarm_flags(temp);

    /* 10. Logic FSM theo trạng thái */
    switch (current_state) {
        case STATE_IDLE:
        case STATE_STANDBY:
            /* TAMPER → ERROR (ưu tiên cao nhất) */
            if (DI_IsTamper()) {
                App_TriggerError();
                break;
            }
            /* Cửa mở → ERROR */
            if (DI_IsDoorOpen()) {
                App_TriggerError();
                break;
            }
            /* Quá nhiệt → ERROR */
            if (temp > OVERTEMP_LIMIT) {
                App_TriggerError();
            }
            break;

        case STATE_CHARGING:
            /* === LOGIC NGẮT NGUỒN SẠC === */

            /* a. TAMPER → ERROR (cắt ngay) */
            if (DI_IsTamper()) {
                App_TriggerError();
                break;
            }

            /* b. Quá nhiệt → ERROR (cắt ngay) */
            if (temp > OVERTEMP_LIMIT) {
                App_TriggerError();
                break;
            }

            /* c. Cửa mở → ERROR (cắt ngay) */
            if (DI_IsDoorOpen()) {
                App_TriggerError();
                break;
            }

            /* d. Kiểm tra alarm từ Meter Monitor */
            {
                MeterAlarm_t alarm = Meter_GetAlarm();
                if (alarm == METER_OVERPOWER || alarm == METER_OVERLOAD || alarm == METER_VOLTAGE_FAULT) {
                    last_stop_reason = SAFETY_ALARM_STOP;
                    App_TriggerError();
                } else if (alarm == METER_ENERGY_LIMIT || alarm == METER_LOW_CURRENT) {
                    last_stop_reason = FINISHED_AUTO;
                    App_TriggerStopCharge();
                }
            }

            /* e. Rev 2.0: Overcurrent detection */
            {
                uint16_t cur = Meter_GetCurrent();
                uint32_t limit_110 = (uint32_t)current_limit_001A * 110 / 100;
                if (cur > limit_110 && current_limit_001A > 0) {
                    last_stop_reason = OVERCURRENT_STOP;
                    Diag_SetAlarmFlag(ALARM_FLAG_OVERCURRENT);
                    App_TriggerError();
                }
            }

            /* f. Rev 2.0: Session energy limit */
            if (session_energy_limit_wh > 0) {
                uint32_t sess_e = Meter_GetSessionEnergy();
                if (sess_e >= session_energy_limit_wh) {
                    last_stop_reason = SESSION_ENERGY_EXCEEDED;
                    App_TriggerStopCharge();
                }
            }
            break;

        case STATE_FINISH:
            /* After-cooling: Fan_Process tự quản bằng hysteresis */
            /* TAMPER vẫn kiểm tra */
            if (DI_IsTamper()) {
                App_TriggerError();
                break;
            }
            if (DI_IsDoorOpen()) {
                App_TriggerError();
            }
            break;

        case STATE_ERROR:
            /* Cương quyết ngắt mọi relay nguy hiểm */
            Relay_SetTarget(RL_CHARGER, RELAY_OFF);
            Relay_SetTarget(RL_SOCKET, RELAY_OFF);
            /* Fan vẫn chạy (Fan_Process tự quản bằng hysteresis) */
            break;

        default:
            break;
    }
}

void App_TriggerStandby(void) {
    if (current_state == STATE_IDLE || current_state == STATE_FINISH) {
        enter_state(STATE_STANDBY);
        LED_Set(LED_WHITE, LED_OFF);
        LED_Set(LED_GREEN, LED_BLINK);
        LED_Set(LED_RED, LED_OFF);
    }
}

void App_TriggerStartCharge(void) {
    if (current_state != STATE_ERROR && current_state != STATE_CHARGING) {
        enter_state(STATE_CHARGING);
        LED_Set(LED_WHITE, LED_OFF);
        LED_Set(LED_GREEN, LED_OFF);
        LED_Set(LED_RED, LED_BLINK);
        Relay_SetTarget(RL_CHARGER, RELAY_ON);
        Meter_StartSession();
        if (app_get_tick) charge_start_tick = app_get_tick();
        /* Rev 2.0: Increment session_id */
        session_id++;
        if (session_id == 0) session_id = 1; /* wrap 65535→1, skip 0 */
        last_stop_reason = REASON_UNKNOWN;
    }
}

void App_TriggerStopCharge(void) {
    if (current_state == STATE_CHARGING) {
        enter_state(STATE_FINISH);
        LED_Set(LED_RED, LED_OFF);
        LED_Set(LED_GREEN, LED_SOLID);
        Relay_SetTarget(RL_CHARGER, RELAY_OFF);
        Diag_IncrementCharge(); /* +1 phiên sạc thành công */
        /* Rev 2.0: If no reason set yet, default to remote user stop */
        if (last_stop_reason == REASON_UNKNOWN) {
            last_stop_reason = REMOTE_STOP_USER;
        }
    }
}

void App_TriggerError(void) {
    enter_state(STATE_ERROR);
    LED_Set(LED_WHITE, LED_OFF);
    LED_Set(LED_GREEN, LED_OFF);
    LED_Set(LED_RED, LED_SOLID);
    Relay_SetTarget(RL_CHARGER, RELAY_OFF);
    Relay_SetTarget(RL_SOCKET, RELAY_OFF);
    Diag_IncrementError(); /* +1 lần error */
    /* Rev 2.0: Default safety reason if none set */
    if (last_stop_reason == REASON_UNKNOWN) {
        last_stop_reason = SAFETY_ALARM_STOP;
    }
}

void App_TriggerClearError(void) {
    if (current_state == STATE_ERROR) {
        /* Clear chỉ khi: cửa đóng + nhiệt hạ + không tamper */
        if (!DI_IsDoorOpen() && !DI_IsTamper() && NTC_GetTempC() < OVERTEMP_LIMIT) {
            App_Init();
        }
    }
}

void App_TriggerUnlockDoor(void) {
    if (current_state != STATE_ERROR) {
        DoorLock_Unlock();
    }
}

/** @brief Gọi khi DLT645 đọc thành công → reset comm timeout */
void App_DLT645Success(void) {
    Diag_DLT645Ok();
    if (app_get_tick) last_dlt645_ok_tick = app_get_tick();
}

/** @brief Gọi khi DLT645 đọc thất bại */
void App_DLT645Fail(void) {
    Diag_DLT645Fail();
}

/** @brief Lấy thời gian sạc phiên hiện tại (giây) */
uint32_t App_GetSessionDuration(void) {
    if (current_state == STATE_CHARGING && app_get_tick) {
        return (app_get_tick() - charge_start_tick) / 1000;
    }
    return 0;
}

SystemState_t App_GetState(void) {
    return current_state;
}

/* ═══════════════════════════════════════════════════════════
 * Rev 2.0 API IMPLEMENTATIONS
 * ═══════════════════════════════════════════════════════════ */

uint16_t App_GetSessionId(void) { return session_id; }
uint16_t App_GetLastStopReason(void) { return last_stop_reason; }
void App_SetStopReason(StopReason_t reason) { last_stop_reason = (uint16_t)reason; }

void App_SetCurrentLimit(uint16_t limit_001A) { current_limit_001A = limit_001A; }
uint16_t App_GetCurrentLimit(void) { return current_limit_001A; }

void App_SetSessionEnergyLimit(uint16_t limit_wh) { session_energy_limit_wh = limit_wh; }
uint16_t App_GetSessionEnergyLimit(void) { return session_energy_limit_wh; }

void App_SetUnixTimestamp(uint32_t ts) { unix_timestamp = ts; }
uint32_t App_GetUnixTimestamp(void) { return unix_timestamp; }

void App_ForceFanOn(void) {
    fan_forced = 1;
    if (app_get_tick) fan_force_tick = app_get_tick();
}
void App_ForceFanOff(void) { fan_forced = 0; }
uint8_t App_IsFanForced(void) {
    /* Auto-off after 5 minutes */
    if (fan_forced && app_get_tick) {
        if (app_get_tick() - fan_force_tick >= FAN_FORCE_TIMEOUT_MS) {
            fan_forced = 0;
        }
    }
    return fan_forced;
}

/* Placeholder: chưa có phần cứng connector / RCD */
uint16_t App_GetConnectorStatus(void) { return (uint16_t)CONNECTOR_UNKNOWN; /* 0xFFFF */ }
uint16_t App_GetGroundFault(void) { return 0; /* OK — chưa có mạch RCD */ }
