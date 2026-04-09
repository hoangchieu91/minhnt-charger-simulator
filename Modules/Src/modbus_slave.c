#include "modbus_slave.h"
#include "app_main.h"
#include "ntc_temp.h"
#include "fan_ctrl.h"
#include "meter_monitor.h"
#include "digital_input.h"
#include "door_lock.h"
#include "diagnostics.h"
#include "led_rgw.h"
#include "relay_ctrl.h"
#include "error_log.h"
#include "meter_polling.h"

/* ═══════════════════════════════════════════════════════════
 * INTERNAL STATE
 * ═══════════════════════════════════════════════════════════ */

static ModbusHardwareConfig_t *hw = (void*)0;
static uint8_t slave_addr = 1;

/* RX buffer */
static uint8_t rx_buf[MODBUS_MAX_FRAME];
static uint16_t rx_len = 0;
static uint32_t rx_last_tick = 0;
static uint8_t rx_active = 0;    /* Có byte mới chờ xử lý */

/* TX buffer */
static uint8_t tx_buf[MODBUS_MAX_FRAME];

/* Holding register storage (RAM, mất khi reset) */
static uint16_t holding_regs[14] = {
    450,    /* 0x0100: fan_high_temp */
    380,    /* 0x0101: fan_low_temp */
    7000,   /* 0x0102: max_power */
    3500,   /* 0x0103: rated_power */
    0,      /* 0x0104: energy_limit_hi */
    20000,  /* 0x0105: energy_limit_lo */
    700,    /* 0x0106: overtemp_limit */
    5000,   /* 0x0107: lock_pulse_ms */
    30,     /* 0x0108: comm_timeout_s */
    0,      /* 0x0109: master_heartbeat (master ghi vào) */
    3200,   /* 0x010A: current_limit (0.01A, default 32.00A) */
    0,      /* 0x010B: session_energy_limit (Wh, 0=disabled) */
    0,      /* 0x010C: time_sync_hi */
    0,      /* 0x010D: time_sync_lo */
};

/* Master Heartbeat Watchdog */
static uint32_t master_last_hb_tick = 0;
static uint8_t  master_alive = 0;       /* 0=chưa nhận HB, 1=alive, 2=timeout */
#define MASTER_HB_TIMEOUT_MS  10000     /* 10s */

/* ═══════════════════════════════════════════════════════════
 * CRC16 MODBUS (polynomial 0xA001)
 * ═══════════════════════════════════════════════════════════ */

uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════
 * INPUT REGISTER READ (FC=04) — 0x0000..0x001A
 * ═══════════════════════════════════════════════════════════ */

static int read_input_register(uint16_t addr, uint16_t *value) {
    switch (addr) {
        case 0x0000: *value = Meter_GetVoltage();   return 0;
        case 0x0001: *value = Meter_GetCurrent();   return 0;
        case 0x0002: *value = Meter_GetPower();     return 0;
        case 0x0003: *value = (uint16_t)(Meter_GetEnergy() >> 16);    return 0;
        case 0x0004: *value = (uint16_t)(Meter_GetEnergy() & 0xFFFF); return 0;
        case 0x0005: *value = (uint16_t)NTC_GetTempC(); return 0;
        case 0x0006: *value = (uint16_t)App_GetState(); return 0;
        case 0x0007: {
            /* relay_status bitmask */
            uint16_t r = 0;
            /* Relay states tracked internally by relay module */
            r |= (Relay_GetActual(RL_CHARGER) << 0);
            r |= (Relay_GetActual(RL_SOCKET)  << 1);
            r |= (Fan_GetState()              << 2);
            r |= (DoorLock_IsUnlocked()       << 3);
            *value = r;
            return 0;
        }
        case 0x0008: {
            /* led_status bitmask */
            uint16_t l = 0;
            /* LED state: read from module internal (simplified) */
            l = 0; /* TODO: add LED_GetState to led_rgw.h */
            *value = l;
            return 0;
        }
        case 0x0009: *value = (uint16_t)Meter_GetAlarm(); return 0;
        case 0x000A: *value = DI_IsDoorOpen() ? 1 : 0;   return 0;
        case 0x000B: *value = Fan_GetState();              return 0;
        case 0x000C: *value = Meter_GetSessionEnergy();    return 0;
        case 0x000D: *value = 0x0100; /* fw v1.0 */        return 0;
        case 0x000E: *value = slave_addr;                  return 0;
        case 0x000F: *value = (uint16_t)(Diag_GetUptime() >> 16); return 0;
        case 0x0010: *value = (uint16_t)(Diag_GetUptime() & 0xFFFF); return 0;
        case 0x0011: *value = Diag_GetHeartbeat();         return 0;
        case 0x0012: *value = Diag_GetTempMin();           return 0;
        case 0x0013: *value = Diag_GetTempMax();           return 0;
        case 0x0014: *value = Diag_GetErrorCount();        return 0;
        case 0x0015: *value = Diag_GetChargeCount();       return 0;
        case 0x0016: *value = Diag_GetDLT645Ok();          return 0;
        case 0x0017: *value = Diag_GetDLT645Fail();        return 0;
        case 0x0018: *value = Diag_GetAlarmFlags();        return 0;
        case 0x0019: *value = (uint16_t)App_GetSessionDuration(); return 0;
        case 0x001A: *value = Meter_IsValid();                return 0;
        case 0x001B: *value = (uint16_t)ErrLog_GetBootCount();        return 0;
        case 0x001C: *value = (uint16_t)ErrLog_GetTotalErrors();      return 0;
        case 0x001D: *value = (uint16_t)ErrLog_GetTotalCharges();     return 0;
        case 0x001E: *value = (uint16_t)(ErrLog_GetTotalEnergy() >> 16); return 0;
        case 0x001F: *value = (uint16_t)(ErrLog_GetTotalEnergy() & 0xFFFF); return 0;
        case 0x0020: {
            ErrorEvent_t *ev = ErrLog_GetLastEvent();
            *value = ev ? ev->event_id : 0;
            return 0;
        }
        case 0x0021: {
            ErrorEvent_t *ev = ErrLog_GetLastEvent();
            *value = ev ? ev->error_type : 0;
            return 0;
        }
        case 0x0022: *value = master_alive;  return 0; /* 0=no HB yet, 1=alive, 2=timeout */
        /* Rev 2.0: Meter serial (BCD from DLT645) */
        case 0x0023: {
            uint8_t bcd[6];
            Meter_GetSerial(bcd);
            *value = ((uint16_t)bcd[5] << 8) | bcd[4];
            return 0;
        }
        case 0x0024: {
            uint8_t bcd[6];
            Meter_GetSerial(bcd);
            *value = ((uint16_t)bcd[3] << 8) | bcd[2];
            return 0;
        }
        case 0x0025: {
            uint8_t bcd[6];
            Meter_GetSerial(bcd);
            *value = ((uint16_t)bcd[1] << 8) | bcd[0];
            return 0;
        }
        /* Rev 2.0: New Input Registers */
        case 0x0026: *value = Meter_GetFrequency(); return 0;   /* 0.1Hz */
        case 0x0027: *value = Meter_GetPowerFactor(); return 0; /* 0.001 */
        case 0x0028: *value = Meter_GetCurrent(); return 0; /* current_rms_raw */
        case 0x0029: *value = App_GetSessionId(); return 0;
        case 0x002A: *value = App_GetLastStopReason(); return 0;
        case 0x002B: *value = App_GetConnectorStatus(); return 0;
        case 0x002C: *value = App_GetGroundFault(); return 0;
        
        /* DLT645 Diagnostics (New) */
        case 0x0030: case 0x0031: case 0x0032: case 0x0033:
        case 0x0034: case 0x0035: case 0x0036: case 0x0037: {
            uint8_t tx[16];
            MeterPolling_GetLastTX(tx);
            uint16_t offset = (addr - 0x0030) * 2;
            *value = ((uint16_t)tx[offset] << 8) | tx[offset + 1];
            return 0;
        }
        case 0x0040: case 0x0041: case 0x0042: case 0x0043:
        case 0x0044: case 0x0045: case 0x0046: case 0x0047: {
            uint8_t rx[16];
            MeterPolling_GetLastRX(rx);
            uint16_t offset = (addr - 0x0040) * 2;
            *value = ((uint16_t)rx[offset] << 8) | rx[offset + 1];
            return 0;
        }
        case 0x0050: *value = (uint16_t)MeterPolling_GetTxCnt(); return 0;
        case 0x0051: *value = (uint16_t)MeterPolling_GetRxCnt(); return 0;
        case 0x0052: *value = (uint16_t)MeterPolling_GetCrcErrCnt(); return 0;
        case 0x0053: *value = (uint16_t)MeterPolling_GetTimeoutCnt(); return 0;
        
        default: return -1;
    }
}

/* ═══════════════════════════════════════════════════════════
 * DISCRETE INPUT READ (FC=02) — 0x0000..0x0005
 * ═══════════════════════════════════════════════════════════ */

static int read_discrete_input(uint16_t addr, uint8_t *value) {
    switch (addr) {
        case 0x0000: *value = DI_IsDoorOpen() ? 1 : 0;          return 0;
        case 0x0001: *value = (App_GetState() == 3) ? 1 : 0;    return 0; /* CHARGING */
        case 0x0002: *value = (App_GetState() == 5) ? 1 : 0;    return 0; /* ERROR */
        case 0x0003: *value = Fan_GetState();                    return 0;
        case 0x0004: *value = DoorLock_IsUnlocked();             return 0;
        case 0x0005: *value = DI_IsTamper() ? 1 : 0;            return 0;
        /* Rev 2.0: New Discrete Inputs */
        case 0x0006: *value = (App_GetConnectorStatus() >= 1 && App_GetConnectorStatus() != 0xFFFF) ? 1 : 0; return 0;
        case 0x0007: *value = App_GetGroundFault() ? 1 : 0;      return 0;
        default: return -1;
    }
}

/* ═══════════════════════════════════════════════════════════
 * COIL WRITE (FC=05) — 0x0000..0x0004
 * ═══════════════════════════════════════════════════════════ */

static int write_coil(uint16_t addr, uint16_t value) {
    if (value != 0xFF00 && value != 0x0000) return -1;
    if (value != 0xFF00) return 0; /* Chỉ xử lý khi ghi FF00 (ON) */

    switch (addr) {
        case 0x0000: App_TriggerStartCharge(); return 0;
        case 0x0001: App_TriggerStopCharge();  return 0;
        case 0x0002: App_TriggerUnlockDoor();  return 0;
        case 0x0003: App_TriggerClearError();  return 0;
        case 0x0004: App_TriggerStandby();     return 0;
        /* Rev 2.0: New Coils */
        case 0x0005: /* force_fan_on */
            if (value == 0xFF00) App_ForceFanOn();
            else App_ForceFanOff();
            return 0;
        case 0x0006: /* enter_fw_update — NOT IMPLEMENTED */
            return -1; /* Returns Modbus Exception 0x01 */
        default: return -1;
    }
}

/* ═══════════════════════════════════════════════════════════
 * HOLDING REGISTER READ/WRITE (FC=03/06/16) — 0x0100..0x0108
 * ═══════════════════════════════════════════════════════════ */

static int read_holding_register(uint16_t addr, uint16_t *value) {
    if (addr < 0x0100 || addr > 0x010D) return -1;
    *value = holding_regs[addr - 0x0100];
    return 0;
}

static int write_holding_register(uint16_t addr, uint16_t value) {
    if (addr < 0x0100 || addr > 0x010D) return -1;
    holding_regs[addr - 0x0100] = value;

    /* Apply to modules */
    switch (addr) {
        case 0x0100: /* fan_high_temp */
        case 0x0101: /* fan_low_temp */
            Fan_SetThresholds(holding_regs[0], holding_regs[1]);
            break;
        case 0x0102: /* max_power */
        case 0x0103: /* rated_power */
        {
            MeterConfig_t cfg;
            cfg.max_power_W = holding_regs[2];
            cfg.rated_power_W = holding_regs[3];
            cfg.energy_limit_Wh = ((uint32_t)holding_regs[4] << 16) | holding_regs[5];
            Meter_SetConfig(&cfg);
            break;
        }
        case 0x0109: /* master_heartbeat — Master ghi bất kỳ value */
            if (hw && hw->get_tick) master_last_hb_tick = hw->get_tick();
            master_alive = 1;
            break;
        /* Rev 2.0: Dynamic Load Balancing */
        case 0x010A: /* current_limit */
            App_SetCurrentLimit(value);
            break;
        case 0x010B: /* session_energy_limit */
            App_SetSessionEnergyLimit(value);
            break;
        case 0x010C: /* time_sync_hi */
        case 0x010D: /* time_sync_lo */
        {
            uint32_t ts = ((uint32_t)holding_regs[0x0C] << 16) | holding_regs[0x0D];
            App_SetUnixTimestamp(ts);
            break;
        }
        /* 0x0104-0x0108: chỉ lưu, chưa có API apply */
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 * SEND RESPONSE
 * ═══════════════════════════════════════════════════════════ */

static void send_response(uint16_t len) {
    uint16_t crc = Modbus_CRC16(tx_buf, len);
    tx_buf[len++] = crc & 0xFF;        /* CRC low */
    tx_buf[len++] = (crc >> 8) & 0xFF;  /* CRC high */
    if (hw && hw->send_bytes) {
        hw->send_bytes(tx_buf, len);
    }
}

static void send_exception(uint8_t fc, uint8_t ex_code) {
    tx_buf[0] = slave_addr;
    tx_buf[1] = fc | 0x80;  /* FC + error bit */
    tx_buf[2] = ex_code;
    send_response(3);
}

/* ═══════════════════════════════════════════════════════════
 * FRAME HANDLERS
 * ═══════════════════════════════════════════════════════════ */

/* FC04: Read Input Registers */
static void handle_fc04(uint16_t start_addr, uint16_t quantity) {
    if (quantity == 0 || quantity > 125) {
        send_exception(0x04, MODBUS_EX_ILLEGAL_VALUE); return;
    }
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x04;
    tx_buf[2] = (uint8_t)(quantity * 2); /* byte count */
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t val = 0;
        if (read_input_register(start_addr + i, &val) != 0) {
            send_exception(0x04, MODBUS_EX_ILLEGAL_ADDRESS); return;
        }
        tx_buf[3 + i*2]     = (val >> 8) & 0xFF;
        tx_buf[3 + i*2 + 1] = val & 0xFF;
    }
    send_response(3 + quantity * 2);
}

/* FC03: Read Holding Registers */
static void handle_fc03(uint16_t start_addr, uint16_t quantity) {
    if (quantity == 0 || quantity > 125) {
        send_exception(0x03, MODBUS_EX_ILLEGAL_VALUE); return;
    }
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x03;
    tx_buf[2] = (uint8_t)(quantity * 2);
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t val = 0;
        if (read_holding_register(start_addr + i, &val) != 0) {
            send_exception(0x03, MODBUS_EX_ILLEGAL_ADDRESS); return;
        }
        tx_buf[3 + i*2]     = (val >> 8) & 0xFF;
        tx_buf[3 + i*2 + 1] = val & 0xFF;
    }
    send_response(3 + quantity * 2);
}

/* FC02: Read Discrete Inputs */
static void handle_fc02(uint16_t start_addr, uint16_t quantity) {
    if (quantity == 0 || quantity > 2000) {
        send_exception(0x02, MODBUS_EX_ILLEGAL_VALUE); return;
    }
    uint8_t byte_count = (quantity + 7) / 8;
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x02;
    tx_buf[2] = byte_count;
    for (uint8_t i = 0; i < byte_count; i++) tx_buf[3+i] = 0;

    for (uint16_t i = 0; i < quantity; i++) {
        uint8_t val = 0;
        if (read_discrete_input(start_addr + i, &val) != 0) {
            send_exception(0x02, MODBUS_EX_ILLEGAL_ADDRESS); return;
        }
        if (val) tx_buf[3 + i/8] |= (1 << (i%8));
    }
    send_response(3 + byte_count);
}

/* FC01: Read Coils (luôn trả 0 vì tự reset) */
static void handle_fc01(uint16_t start_addr, uint16_t quantity) {
    if (quantity == 0 || quantity > 2000 || start_addr + quantity > 7) {
        send_exception(0x01, MODBUS_EX_ILLEGAL_ADDRESS); return;
    }
    uint8_t byte_count = (quantity + 7) / 8;
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x01;
    tx_buf[2] = byte_count;
    for (uint8_t i = 0; i < byte_count; i++) tx_buf[3+i] = 0; /* Coils tự reset = 0 */
    send_response(3 + byte_count);
}

/* FC05: Write Single Coil */
static void handle_fc05(uint16_t addr, uint16_t value) {
    if (write_coil(addr, value) != 0) {
        send_exception(0x05, MODBUS_EX_ILLEGAL_ADDRESS); return;
    }
    /* Echo request as response */
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x05;
    tx_buf[2] = (addr >> 8) & 0xFF;
    tx_buf[3] = addr & 0xFF;
    tx_buf[4] = (value >> 8) & 0xFF;
    tx_buf[5] = value & 0xFF;
    send_response(6);
}

/* FC06: Write Single Register */
static void handle_fc06(uint16_t addr, uint16_t value) {
    if (write_holding_register(addr, value) != 0) {
        send_exception(0x06, MODBUS_EX_ILLEGAL_ADDRESS); return;
    }
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x06;
    tx_buf[2] = (addr >> 8) & 0xFF;
    tx_buf[3] = addr & 0xFF;
    tx_buf[4] = (value >> 8) & 0xFF;
    tx_buf[5] = value & 0xFF;
    send_response(6);
}

/* FC16: Write Multiple Registers */
static void handle_fc16(const uint8_t *frame, uint16_t frame_len) {
    uint16_t start_addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t quantity   = ((uint16_t)frame[4] << 8) | frame[5];
    uint8_t  byte_count = frame[6];

    if (quantity == 0 || quantity > 123 || byte_count != quantity * 2) {
        send_exception(0x10, MODBUS_EX_ILLEGAL_VALUE); return;
    }
    if (7 + byte_count + 2 > frame_len) {
        send_exception(0x10, MODBUS_EX_ILLEGAL_VALUE); return;
    }
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t val = ((uint16_t)frame[7 + i*2] << 8) | frame[7 + i*2 + 1];
        if (write_holding_register(start_addr + i, val) != 0) {
            send_exception(0x10, MODBUS_EX_ILLEGAL_ADDRESS); return;
        }
    }
    tx_buf[0] = slave_addr;
    tx_buf[1] = 0x10;
    tx_buf[2] = (start_addr >> 8) & 0xFF;
    tx_buf[3] = start_addr & 0xFF;
    tx_buf[4] = (quantity >> 8) & 0xFF;
    tx_buf[5] = quantity & 0xFF;
    send_response(6);
}

/* ═══════════════════════════════════════════════════════════
 * PARSE FRAME
 * ═══════════════════════════════════════════════════════════ */

static void parse_frame(void) {
    if (rx_len < 4) return; /* Minimum: addr + FC + CRC(2) */

    /* Check slave address */
    if (rx_buf[0] != slave_addr && rx_buf[0] != 0) return; /* 0=broadcast */

    /* Verify CRC */
    uint16_t crc_calc = Modbus_CRC16(rx_buf, rx_len - 2);
    uint16_t crc_recv = ((uint16_t)rx_buf[rx_len-1] << 8) | rx_buf[rx_len-2];
    if (crc_calc != crc_recv) return; /* CRC sai → bỏ qua */

    /* Broadcast (addr=0) → xử lý nhưng không respond */
    uint8_t is_broadcast = (rx_buf[0] == 0);

    uint8_t fc = rx_buf[1];
    uint16_t reg_addr = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    uint16_t reg_val  = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

    if (is_broadcast) {
        /* Broadcast: chỉ xử lý write, không respond */
        if (fc == 0x05) write_coil(reg_addr, reg_val);
        else if (fc == 0x06) write_holding_register(reg_addr, reg_val);
        return;
    }

    switch (fc) {
        case 0x01: handle_fc01(reg_addr, reg_val); break;
        case 0x02: handle_fc02(reg_addr, reg_val); break;
        case 0x03: handle_fc03(reg_addr, reg_val); break;
        case 0x04: handle_fc04(reg_addr, reg_val); break;
        case 0x05: handle_fc05(reg_addr, reg_val); break;
        case 0x06: handle_fc06(reg_addr, reg_val); break;
        case 0x10: handle_fc16(rx_buf, rx_len);    break;
        default:
            send_exception(fc, MODBUS_EX_ILLEGAL_FUNCTION);
            break;
    }
}

/* ═══════════════════════════════════════════════════════════
 * PUBLIC API
 * ═══════════════════════════════════════════════════════════ */

void Modbus_Init(ModbusHardwareConfig_t *config, uint8_t addr) {
    hw = config;
    slave_addr = addr;
    rx_len = 0;
    rx_active = 0;
}

void Modbus_ReceiveByte(uint8_t byte) {
    if (rx_len < MODBUS_MAX_FRAME) {
        rx_buf[rx_len++] = byte;
    }
    if (hw && hw->get_tick) {
        rx_last_tick = hw->get_tick();
    }
    rx_active = 1;
}

void Modbus_Process(void) {
    if (!hw || !hw->get_tick) return;

    uint32_t now = hw->get_tick();

    /* Frame silence detector */
    if (rx_active && now - rx_last_tick >= MODBUS_SILENCE_MS && rx_len > 0) {
        parse_frame();
        rx_len = 0;
        rx_active = 0;
    }

    /* Master Heartbeat Watchdog */
    if (master_alive == 1) {
        if (now - master_last_hb_tick >= MASTER_HB_TIMEOUT_MS) {
            master_alive = 2;  /* TIMEOUT */
            App_TriggerError();
            ErrLog_RecordError(ERR_COMM_FAIL, (uint8_t)App_GetState(),
                              NTC_GetTempC(), Meter_GetPower(),
                              Diag_GetUptime());
            Diag_SetAlarmFlag(ALARM_FLAG_COMM_FAIL);
        }
    }
}

void Modbus_SetAddress(uint8_t addr) {
    slave_addr = addr;
}
