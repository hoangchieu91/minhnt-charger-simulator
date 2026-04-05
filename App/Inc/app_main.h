#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_INIT,
    STATE_IDLE,        // Đèn Trắng liên tục
    STATE_STANDBY,     // Đèn Xanh lá chớp tắt
    STATE_CHARGING,    // Đèn Đỏ chớp tắt, Relay Sạc Đóng
    STATE_FINISH,      // Đèn Xanh lá liên tục, Relay cắt
    STATE_ERROR        // Đèn Đỏ liên tục, Cắt toàn bộ điện
} SystemState_t;

#define OVERTEMP_LIMIT     700   /* 70.0°C → ERROR */
#define AFTER_COOL_TIME_MS 60000 /* Quạt chạy thêm 60s sau FINISH/ERROR */

void App_Init(void);
void App_Process(void);

// Các trigger bên ngoài (từ RS485 Tủ, NTC, Cửa)
void App_TriggerStandby(void);
void App_TriggerStartCharge(void);
void App_TriggerStopCharge(void);
void App_TriggerError(void);
void App_TriggerClearError(void);
void App_TriggerUnlockDoor(void);

void App_DLT645Success(void);
void App_DLT645Fail(void);
uint32_t App_GetSessionDuration(void);

SystemState_t App_GetState(void);

/* ─── Rev 2.0: Session Management ─── */
typedef enum {
    REASON_UNKNOWN = 0,
    FINISHED_AUTO = 1,
    REMOTE_STOP_USER = 2,
    REMOTE_STOP_OUT_OF_COIN = 3,
    SAFETY_ALARM_STOP = 4,
    SESSION_ENERGY_EXCEEDED = 5,
    OVERCURRENT_STOP = 6
} StopReason_t;

typedef enum {
    CONNECTOR_UNPLUGGED = 0,
    CONNECTOR_PLUGGED = 1,
    CONNECTOR_LOCKED = 2,
    CONNECTOR_UNKNOWN = 0xFFFF
} ConnectorStatus_t;

uint16_t App_GetSessionId(void);
uint16_t App_GetLastStopReason(void);
void     App_SetStopReason(StopReason_t reason);

/* ─── Rev 2.0: Current Limit (Dynamic Load Balancing) ─── */
void     App_SetCurrentLimit(uint16_t limit_001A);
uint16_t App_GetCurrentLimit(void);

/* ─── Rev 2.0: Session Energy Limit ─── */
void     App_SetSessionEnergyLimit(uint16_t limit_wh);
uint16_t App_GetSessionEnergyLimit(void);

/* ─── Rev 2.0: Time Sync ─── */
void     App_SetUnixTimestamp(uint32_t ts);
uint32_t App_GetUnixTimestamp(void);

/* ─── Rev 2.0: Force Fan ─── */
void     App_ForceFanOn(void);
void     App_ForceFanOff(void);
uint8_t  App_IsFanForced(void);

/* ─── Rev 2.0: Connector & Ground Fault (placeholder for future HW) ─── */
uint16_t App_GetConnectorStatus(void);
uint16_t App_GetGroundFault(void);

#endif // APP_MAIN_H
