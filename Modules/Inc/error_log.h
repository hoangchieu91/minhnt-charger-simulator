#ifndef ERROR_LOG_H
#define ERROR_LOG_H

#include <stdint.h>

/**
 * @brief Error Log — Persistent Error Tracking (Flash EEPROM Emulation)
 *
 * Lưu vào 2 page Flash cuối STM32F103 (0x0800_F000 - 0x0800_FFFF)
 * Survive power loss. Error ID tuần tự không reset.
 */

/* Error type codes (cố định, không đổi) */
#define ERR_OVERTEMP       1  /* Quá nhiệt >70°C */
#define ERR_TAMPER         2  /* Phá hoại: khóa cài + cửa mở */
#define ERR_OVERPOWER      3  /* Quá công suất tức thì */
#define ERR_OVERLOAD       4  /* Quá tải liên tục 30s */
#define ERR_VOLTAGE        5  /* Điện áp ngoài 180-260V */
#define ERR_COMM_FAIL      6  /* Mất giao tiếp DLT645 >30s */
#define ERR_DOOR_CHARGING  7  /* Cửa mở khi đang sạc */
#define ERR_GROUND_FAULT   8  /* Rò dòng ra vỏ (RCD/GFCI) */
#define ERR_CONNECTOR_FAULT 9 /* Cảm biến đầu cắm bất thường */
#define ERR_OVERCURRENT   10  /* Dòng vượt current_limit +10% */

/* Error event — 12 bytes */
typedef struct {
    uint16_t event_id;     /* ID tuần tự (1,2,3...) không reset */
    uint8_t  error_type;   /* ERR_OVERTEMP, ERR_TAMPER, ... */
    uint8_t  state_when;   /* FSM state khi xảy ra */
    uint32_t uptime;       /* Uptime (giây) */
    int16_t  temp;         /* Nhiệt độ 0.1°C */
    uint16_t power;        /* Công suất W */
} ErrorEvent_t;            /* = 12 bytes */

/* Persistent counters header — 32 bytes */
typedef struct {
    uint32_t magic;            /* 0x54525531 = "TRU1" */
    uint32_t boot_count;
    uint32_t total_error_count;
    uint32_t total_charge_count;
    uint32_t total_energy_wh;
    uint16_t next_event_id;
    uint16_t log_index;        /* Vị trí ghi tiếp (circular) */
    uint32_t reserved;
} PersistentHeader_t;          /* = 32 bytes */

#define ERRLOG_MAX_EVENTS  80

/* Hardware abstraction for Flash */
typedef struct {
    int  (*flash_erase_page)(uint32_t page_addr);
    int  (*flash_write)(uint32_t addr, const uint8_t *data, uint16_t len);
    void (*flash_read)(uint32_t addr, uint8_t *data, uint16_t len);
} ErrLogFlashConfig_t;

/* ─── API ─── */

void     ErrLog_Init(ErrLogFlashConfig_t *flash_cfg);
void     ErrLog_RecordError(uint8_t error_type, uint8_t fsm_state,
                            int16_t temp, uint16_t power, uint32_t uptime);
void     ErrLog_IncrementCharge(void);
void     ErrLog_AddEnergy(uint32_t wh);
void     ErrLog_Save(void);   /* Flush counters to Flash (gọi mỗi 60s) */

/* Getters */
uint32_t       ErrLog_GetBootCount(void);
uint32_t       ErrLog_GetTotalErrors(void);
uint32_t       ErrLog_GetTotalCharges(void);
uint32_t       ErrLog_GetTotalEnergy(void);
uint16_t       ErrLog_GetNextEventId(void);
ErrorEvent_t*  ErrLog_GetLastEvent(void);
ErrorEvent_t*  ErrLog_GetEvent(uint16_t index);

#endif // ERROR_LOG_H
