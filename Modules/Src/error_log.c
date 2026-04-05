#include "error_log.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════
 * Flash EEPROM Emulation — STM32F103C8T6
 *
 * Page 62 (0x0800_F000): Header — persistent counters (32 bytes)
 * Page 63 (0x0800_F400): Error event log (80 × 12 = 960 bytes)
 * ═══════════════════════════════════════════════════════════ */

#define FLASH_HEADER_ADDR  0x0800F000U
#define FLASH_LOG_ADDR     0x0800F400U
#define MAGIC_VALUE        0x54525531U  /* "TRU1" */

static ErrLogFlashConfig_t *flash = (void*)0;

/* RAM copies */
static PersistentHeader_t header;
static ErrorEvent_t event_log[ERRLOG_MAX_EVENTS];
static uint8_t dirty = 0;  /* 1 = header changed, needs flush */

/* ═══════════════════════════════════════════════════════════
 * INIT — Load from Flash or create fresh
 * ═══════════════════════════════════════════════════════════ */

void ErrLog_Init(ErrLogFlashConfig_t *flash_cfg) {
    flash = flash_cfg;
    if (!flash || !flash->flash_read) {
        /* No flash — init blank */
        memset(&header, 0, sizeof(header));
        header.magic = MAGIC_VALUE;
        header.next_event_id = 1;
        memset(event_log, 0, sizeof(event_log));
        return;
    }

    /* Read header from Flash */
    flash->flash_read(FLASH_HEADER_ADDR, (uint8_t*)&header, sizeof(header));

    if (header.magic != MAGIC_VALUE) {
        /* First time or corrupted — init fresh */
        memset(&header, 0, sizeof(header));
        header.magic = MAGIC_VALUE;
        header.next_event_id = 1;
        memset(event_log, 0, sizeof(event_log));
        /* Write fresh header to Flash */
        if (flash->flash_erase_page) flash->flash_erase_page(FLASH_HEADER_ADDR);
        if (flash->flash_erase_page) flash->flash_erase_page(FLASH_LOG_ADDR);
        if (flash->flash_write) flash->flash_write(FLASH_HEADER_ADDR,
            (const uint8_t*)&header, sizeof(header));
    } else {
        /* Load event log from Flash */
        flash->flash_read(FLASH_LOG_ADDR, (uint8_t*)event_log, sizeof(event_log));
    }

    /* Increment boot count */
    header.boot_count++;
    dirty = 1;
    ErrLog_Save();  /* Flush boot_count immediately */
}

/* ═══════════════════════════════════════════════════════════
 * RECORD ERROR — Ghi event + tăng counter
 * ═══════════════════════════════════════════════════════════ */

void ErrLog_RecordError(uint8_t error_type, uint8_t fsm_state,
                        int16_t temp, uint16_t power, uint32_t uptime) {
    /* Build event */
    uint16_t idx = header.log_index % ERRLOG_MAX_EVENTS;
    event_log[idx].event_id   = header.next_event_id;
    event_log[idx].error_type = error_type;
    event_log[idx].state_when = fsm_state;
    event_log[idx].uptime     = uptime;
    event_log[idx].temp       = temp;
    event_log[idx].power      = power;

    header.next_event_id++;
    header.log_index++;
    if (header.log_index >= ERRLOG_MAX_EVENTS) {
        header.log_index = 0;  /* Circular wrap */
    }
    header.total_error_count++;
    dirty = 1;

    /* Write event to Flash immediately (critical data) */
    if (flash && flash->flash_write) {
        uint32_t event_addr = FLASH_LOG_ADDR + (uint32_t)idx * sizeof(ErrorEvent_t);
        flash->flash_write(event_addr, (const uint8_t*)&event_log[idx],
                           sizeof(ErrorEvent_t));
    }

    /* Also flush header */
    ErrLog_Save();
}

/* ═══════════════════════════════════════════════════════════
 * COUNTER UPDATES
 * ═══════════════════════════════════════════════════════════ */

void ErrLog_IncrementCharge(void) {
    header.total_charge_count++;
    dirty = 1;
}

void ErrLog_AddEnergy(uint32_t wh) {
    header.total_energy_wh += wh;
    dirty = 1;
}

/* ═══════════════════════════════════════════════════════════
 * SAVE — Flush header to Flash (gọi mỗi 60s hoặc khi error)
 * ═══════════════════════════════════════════════════════════ */

void ErrLog_Save(void) {
    if (!dirty || !flash) return;

    if (flash->flash_erase_page) {
        flash->flash_erase_page(FLASH_HEADER_ADDR);
    }
    if (flash->flash_write) {
        flash->flash_write(FLASH_HEADER_ADDR,
                           (const uint8_t*)&header, sizeof(header));
    }
    dirty = 0;
}

/* ═══════════════════════════════════════════════════════════
 * GETTERS
 * ═══════════════════════════════════════════════════════════ */

uint32_t ErrLog_GetBootCount(void)    { return header.boot_count; }
uint32_t ErrLog_GetTotalErrors(void)  { return header.total_error_count; }
uint32_t ErrLog_GetTotalCharges(void) { return header.total_charge_count; }
uint32_t ErrLog_GetTotalEnergy(void)  { return header.total_energy_wh; }
uint16_t ErrLog_GetNextEventId(void)  { return header.next_event_id; }

ErrorEvent_t* ErrLog_GetLastEvent(void) {
    if (header.next_event_id <= 1) return (void*)0; /* No events yet */
    uint16_t last_idx;
    if (header.log_index == 0) {
        last_idx = ERRLOG_MAX_EVENTS - 1;
    } else {
        last_idx = header.log_index - 1;
    }
    return &event_log[last_idx];
}

ErrorEvent_t* ErrLog_GetEvent(uint16_t index) {
    if (index >= ERRLOG_MAX_EVENTS) return (void*)0;
    return &event_log[index];
}
