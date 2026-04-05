# 📋 ĐẶC TẢ TRÌNH GIẢ LẬP TRỤ SẠC — TRU V1.0

> **Phiên bản:** 1.0  
> **Ngày tạo:** 05/04/2026  
> **Mục đích:** Tài liệu đầy đủ nghiệp vụ, logic, giao thức, và kịch bản test để một developer ở máy khác có thể code hoàn chỉnh trình giả lập hệ thống trụ sạc TRU V1.0.

---

## MỤC LỤC

1. [Tổng quan hệ thống](#1-tổng-quan-hệ-thống)
2. [Kiến trúc giả lập](#2-kiến-trúc-giả-lập)
3. [Mô hình phần cứng ảo](#3-mô-hình-phần-cứng-ảo)
4. [Máy trạng thái FSM](#4-máy-trạng-thái-fsm)
5. [Logic nghiệp vụ chi tiết](#5-logic-nghiệp-vụ-chi-tiết)
6. [Giao thức Modbus RTU Slave](#6-giao-thức-modbus-rtu-slave)
7. [Giao thức MQTT (ESP32 Gateway)](#7-giao-thức-mqtt-esp32-gateway)
8. [Dashboard API](#8-dashboard-api)
9. [Kịch bản test tự động](#9-kịch-bản-test-tự-động)
10. [Hướng dẫn triển khai](#10-hướng-dẫn-triển-khai)

---

## 1. TỔNG QUAN HỆ THỐNG

### 1.1. Hệ thống thật (Production)

```
┌─────────────┐    RS485/Modbus RTU    ┌──────────────┐    MQTT/TLS     ┌────────────┐
│  Trụ Sạc    │ ◄───────────────────► │  ESP32        │ ◄────────────► │  Server    │
│  (STM32)    │    9600-8-N-1         │  Gateway      │   Wi-Fi/LAN    │  Cloud     │
│             │                        │  (Master)     │                │            │
│ • NTC Temp  │                        │ • Poll Modbus │                │ • Billing  │
│ • DLT645    │                        │ • Pub MQTT    │                │ • Alarm    │
│ • 4 Relay   │                        │ • Web UI      │                │ • Control  │
│ • 3 LED     │                        │ • OTA         │                │            │
│ • Door Lock │                        │               │                │            │
└─────────────┘                        └──────────────┘                └────────────┘
```

### 1.2. Hệ thống giả lập (Simulator)

Trình giả lập thay thế phần cứng STM32 bằng phần mềm Python/C, vẫn giao tiếp Modbus RTU thật qua TCP hoặc Virtual Serial Port.

```
┌─────────────────────────────────────────────────────────┐
│                    SIMULATOR (Python)                    │
│                                                          │
│  ┌──────────────┐   ┌────────────┐   ┌───────────────┐ │
│  │ Hardware     │   │ Firmware   │   │ Modbus Slave  │ │
│  │ Model        │──►│ FSM Logic  │──►│ TCP Server    │ │
│  │              │   │            │   │ Port 4321     │ │
│  │ • ADC/NTC    │   │ • States   │   │               │ │
│  │ • GPIO/Door  │   │ • Alarms   │   │ • FC01-FC16   │ │
│  │ • DLT645 Sim │   │ • Fan/Lock │   │ • CRC16       │ │
│  │ • Relay      │   │ • Error    │   │               │ │
│  └──────────────┘   └────────────┘   └───────────────┘ │
│                           │                              │
│                    ┌──────▼──────┐                       │
│                    │ HTTP API    │                        │
│                    │ Port 4323   │                        │
│                    │ /api/state  │                        │
│                    │ /api/set    │                        │
│                    └─────────────┘                        │
└─────────────────────────────────────────────────────────┘
         ▲                                    ▲
         │ Modbus RTU over TCP                │ HTTP
         ▼                                    ▼
┌──────────────┐                    ┌──────────────────┐
│ Gateway Sim  │                    │ Dashboard HTML   │
│ (hoặc Master │                    │ (Browser)        │
│  thật ESP32) │                    │                  │
└──────────────┘                    └──────────────────┘
```

---

## 2. KIẾN TRÚC GIẢ LẬP

### 2.1. Các thành phần cần code

| # | Module | Ngôn ngữ | Chức năng |
|---|--------|-----------|-----------|
| 1 | `HardwareModel` | Python | Mô phỏng GPIO, ADC, Relay, LED, NTC, DLT645 Meter |
| 2 | `FirmwareSimulator` | Python | FSM chính, logic nghiệp vụ (port từ app_main.c) |
| 3 | `ModbusSlaveServer` | Python | TCP server Modbus RTU, xử lý FC01-FC16, CRC16 |
| 4 | `GatewaySimulator` | Python | Giả lập ESP32: poll Modbus → publish MQTT |
| 5 | `DashboardAPI` | Python | HTTP API JSON cho Dashboard HTML |
| 6 | `DashboardUI` | HTML/JS | Giao diện giám sát realtime |
| 7 | `InteractiveCLI` | Python | CLI debug tương tác |

### 2.2. Cổng mạng

| Port | Protocol | Mô tả |
|------|----------|-------|
| 4321 | TCP (Modbus RTU over TCP) | Slave Modbus chính |
| 4323 | HTTP | Dashboard API |
| 1883 | MQTT | Broker (Mosquitto) |
| 9001 | WebSocket | MQTT over WS (optional) |

### 2.3. Dependencies

```
pip install pymodbus paho-mqtt
# Optional: Docker + Mosquitto cho MQTT broker
```

---

## 3. MÔ HÌNH PHẦN CỨNG ẢO

### 3.1. Class `HardwareModel`

```python
class HardwareModel:
    def __init__(self):
        self.tick = 0  # System tick (1 tick = 1ms)
        
        # ─── GPIO Inputs ───
        self.door_sensor = True       # True=đóng (pullup active), False=mở
        self.dip_switch = 1           # Địa chỉ Modbus (0-7), đọc PB5/PB6/PB7
        
        # ─── GPIO Outputs (Relay) ───
        self.relays = {
            "CHARGER": False,   # PB0 - RL1: cấp nguồn bộ sạc  
            "SOCKET":  False,   # PB1 - RL2: cấp nguồn ổ cắm
            "FAN":     False,   # PB10 - RL3: quạt tản nhiệt
            "DOORLOCK": False   # PB11 - RL4: solenoid khóa cửa 12V
        }
        
        # ─── GPIO Outputs (LED) ───
        self.leds = {
            "RED":   False,    # PA8
            "GREEN": False,    # PA11
            "WHITE": False     # PA12
        }
        self.led_blink = {
            "RED":   False,    # True = nhấp nháy 500ms
            "GREEN": False,
            "WHITE": False
        }
        
        # ─── ADC / NTC Temperature ───
        self.ntc_adc = 2048           # Raw ADC 12-bit (0-4095)
        self.ntc_temp = 250           # Đơn vị 0.1°C → 25.0°C
        
        # ─── DLT645 Meter Data (RS485_2) ───
        self.voltage = 2200           # 0.1V → 220.0V
        self.current = 0              # 0.01A → 0.00A
        self.power = 0                # W
        self.energy = 0               # Wh tổng (từ đồng hồ)
        self.session_energy = 0       # Wh phiên sạc hiện tại
        self.meter_valid = True       # DLT645 communication OK?
        
        # ─── Door Lock Timing ───
        self.lock_start_tick = 0
        self.lock_active = False      # True = đang mở khóa (5s)
```

### 3.2. Thông số quan trọng

| Thông số | Giá trị mặc định | Đơn vị thật | Ghi chú |
|----------|-------------------|-------------|---------|
| `ntc_temp` | 250 | 25.0°C | Nhân 0.1 khi hiển thị |
| `voltage` | 2200 | 220.0V | Nhân 0.1 khi hiển thị |
| `current` | 0 | 0.00A | Nhân 0.01 khi hiển thị |
| `power` | 0 | 0W | Giá trị nguyên |
| `energy` | 0 | 0Wh | Giá trị nguyên |

### 3.3. Quy tắc Khóa chéo Relay (Safety Interlock)

> ⚠️ **BẮT BUỘC — SAFETY CRITICAL**

```
Rule 1: RL1 (CHARGER) và RL2 (SOCKET) KHÔNG BAO GIỜ được ON đồng thời
Rule 2: Khi chuyển giữa RL1 ↔ RL2, phải có dead-time tối thiểu 100ms
Rule 3: Khi ERROR → TẮT CẢ RL1 và RL2 ngay lập tức
```

### 3.4. Công thức NTC (nếu cần mô phỏng từ ADC)

```
R_NTC = 10000 × ADC_val / (4095 - ADC_val)    // Ohm
Temp = Steinhart-Hart lookup table
// Trong simulator đơn giản: cho phép set trực tiếp ntc_temp
```

---

## 4. MÁY TRẠNG THÁI FSM

### 4.1. Bảng trạng thái (CHUẨN DUY NHẤT)

| Value | State | Mô tả | LED | Relay |
|-------|-------|--------|-----|-------|
| 0 | `INIT` | Khởi tạo, chưa sẵn sàng | Tất OFF | Tất OFF |
| 1 | `IDLE` | Rảnh, sẵn sàng | ⚪ White ON | Tất OFF |
| 2 | `STANDBY` | Chờ cắm súng sạc | 🟢 Green BLINK | Tất OFF |
| 3 | `CHARGING` | Đang cấp nguồn | 🔴 Red BLINK | ⚡ CHARGER ON |
| 4 | `FINISH` | Hoàn tất phiên sạc | 🟢 Green ON | Tất OFF |
| 5 | `ERROR` | Lỗi khẩn cấp | 🔴 Red ON | Tất OFF |

### 4.2. Sơ đồ chuyển trạng thái

```
                    ┌──── Boot ────┐
                    ▼              │
              ┌───────────┐       │
              │   INIT    │───────┘
              │  (0)      │
              └─────┬─────┘
                    │ auto (after HW init)
                    ▼
              ┌───────────┐
         ┌───►│   IDLE    │◄───── clear_error (nếu điều kiện OK)
         │    │  (1)      │
         │    └──┬────┬───┘
         │       │    │
         │       │    │ Coil 0x0004 (standby)
         │       │    ▼
         │       │  ┌───────────┐
         │       │  │ STANDBY   │
         │       │  │  (2)      │
         │       │  └─────┬─────┘
         │       │        │
         │       │        │ Coil 0x0000 (start_charge) 
         │       │        │ [cũng có thể từ IDLE trực tiếp]
         │       │        ▼
         │       │  ┌───────────┐
         │       │  │ CHARGING  │───── Quá nhiệt / Cửa mở / Tamper
         │       │  │  (3)      │      Quá công suất → ERROR
         │       │  └─────┬─────┘
         │       │        │
         │       │        │ Coil 0x0001 (stop_charge)
         │       │        │ OR xe đầy (low_current 60s)
         │       │        │ OR đủ kWh (energy_limit)
         │       │        ▼
         │       │  ┌───────────┐
         │       └─►│  FINISH   │
         │          │  (4)      │
         │          └─────┬─────┘
         │                │
         │ ┌──────────────┘  Bất kỳ alarm
         │ │
         │ ▼
         │ ┌───────────┐
         └─┤  ERROR    │
           │  (5)      │
           └───────────┘
              ▲
              │ Tamper / Overtemp / Door open khi sạc
              │ Master Heartbeat timeout
              │ Overvoltage / Overpower
```

### 4.3. Điều kiện chuyển trạng thái CHI TIẾT

#### IDLE (1) → STANDBY (2)
```
Trigger: Nhận Coil 0x0004 (standby) = 0xFF00
Điều kiện: state phải là IDLE(1) hoặc FINISH(4)
Hành vi:
  - LED: Green BLINK 500ms
  - Relay: giữ nguyên (OFF)
```

#### IDLE/STANDBY (1,2) → CHARGING (3)
```
Trigger: Nhận Coil 0x0000 (start_charge) = 0xFF00
Điều kiện: state KHÔNG phải ERROR(5) hoặc CHARGING(3)
Hành vi:
  - Relay CHARGER = ON
  - LED: Red BLINK 500ms  
  - Reset session_energy = 0
  - Ghi nhận charge_start_tick
```

#### CHARGING (3) → FINISH (4)
```
Trigger: Một trong các điều kiện sau:
  1. Nhận Coil 0x0001 (stop_charge) = 0xFF00
  2. Low current < 0.5A liên tục 60s (xe đầy)
  3. Session energy >= energy_limit_Wh (đủ kWh)
Hành vi:
  - Relay CHARGER = OFF
  - LED: Green ON solid
  - charge_count += 1
```

#### BẤT KỲ → ERROR (5)
```
Trigger: Bất kỳ alarm nào sau:
  1. ntc_temp > overtemp_limit (default 700 = 70.0°C)
  2. door_sensor = MỞ (khi đang IDLE/STANDBY/CHARGING/FINISH)
  3. Tamper detection (cửa mở + khóa KHÔNG active)
  4. power > max_power (default 7000W)
  5. voltage < 1800 hoặc > 2600 (ngoài 180-260V)
  6. Master heartbeat timeout (>10s không ghi HR 0x0109)
Hành vi:
  - Relay CHARGER = OFF (ngay lập tức)
  - Relay SOCKET = OFF
  - LED: Red ON solid
  - error_count += 1
  - Ghi Error Log (Flash persistent)
```

#### ERROR (5) → IDLE (1)
```
Trigger: Nhận Coil 0x0003 (clear_error) = 0xFF00
Điều kiện BẮT BUỘC tất cả:
  1. Cửa phải ĐÓNG (door_sensor = True)
  2. Không có Tamper
  3. Nhiệt độ < overtemp_limit (ntc_temp < 700)
Nếu không đủ điều kiện → KHÔNG clear, giữ ERROR
Hành vi khi clear thành công:
  - LED: White ON solid
  - Relay: giữ OFF
```

---

## 5. LOGIC NGHIỆP VỤ CHI TIẾT

### 5.1. Debounce cảm biến cửa

```python
DEBOUNCE_COUNT = 50  # 50 ticks = 50ms

def process_door():
    raw_open = not hw.door_sensor  # sensor LOW = cửa mở
    if raw_open != debounced_door_state:
        door_debounce_count += 1
        if door_debounce_count >= DEBOUNCE_COUNT:
            debounced_door_state = raw_open
            door_debounce_count = 0
            # Log door state change
    else:
        door_debounce_count = 0
```

### 5.2. Tamper Detection

```python
def is_tamper():
    """Cửa mở KHÔNG do lệnh unlock → Phá hoại"""
    return debounced_door_state == True  # cửa mở
           and hw.lock_active == False    # khóa không được mở hợp lệ
```

> **Giải thích:** Nếu Master gửi lệnh `unlock_door`, relay khóa sẽ active 5s → `lock_active=True`. Trong thời gian này cửa mở là hợp lệ. Nếu cửa mở mà `lock_active=False` → ai đó phá khóa → TAMPER.

### 5.3. Quạt tản nhiệt (Fan Hysteresis)

```
Ngưỡng BẬT (fan_high_temp): 45.0°C (450 × 0.1°C) — configurable via HR 0x0100
Ngưỡng TẮT (fan_low_temp):  38.0°C (380 × 0.1°C) — configurable via HR 0x0101
Anti-chatter: minimum ON time = 30 giây

Logic:
  if (fan_off AND temp >= fan_high_temp):
      fan_on = True
      fan_on_tick = current_tick
      relay["FAN"] = True
      
  elif (fan_on AND temp <= fan_low_temp):
      if (current_tick - fan_on_tick >= 30000):  # 30s minimum
          fan_on = False
          relay["FAN"] = False
```

### 5.4. Khóa cửa (Door Lock)

```
Trigger: Coil 0x0002 (unlock_door) = 0xFF00
Hành vi:
  1. relay["DOORLOCK"] = True (mở solenoid)
  2. lock_active = True
  3. lock_start_tick = current_tick
  
Auto-relock (gọi mỗi tick):
  if lock_active AND (current_tick - lock_start_tick >= 5000):  # 5s
      lock_active = False
      relay["DOORLOCK"] = False
      # Log: "Door Lock: tự động KHÓA lại"
```

### 5.5. Alarm Flags (Bitmask)

```
bit 0: OVERTEMP      — ntc_temp > overtemp_limit (HR 0x0106, default 700)
bit 1: DOOR_OPEN     — debounced_door_state = True
bit 2: TAMPER        — is_tamper() = True
bit 3: OVERPOWER     — power > max_power (HR 0x0102, default 7000W)
bit 4: VOLT_FAULT    — voltage < 1800 OR voltage > 2600
bit 5: ENERGY_LIMIT  — session_energy >= energy_limit
bit 6: LOW_CURRENT   — current < 50 (0.5A) liên tục 60s khi CHARGING
bit 7: COMM_FAIL     — master_alive = 2 (heartbeat timeout)
bit 8: GROUND_FAULT  — rò điện ra vỏ (reserved)
bit 9: CONNECTOR     — đầu cắm fault (reserved)
bit 10: OVERCURRENT  — dòng > current_limit (HR 0x010A)
```

### 5.6. Master Heartbeat Protocol

```
ESP32 Gateway (Master) ghi Holding Register 0x0109 mỗi 3-5 giây.
  → Slave đặt master_alive = 1, reset timer

Nếu > 10 giây KHÔNG nhận được ghi vào 0x0109:
  → master_alive = 2 (TIMEOUT)
  → Trigger ERROR state
  → alarm_flags bit 7 = COMM_FAIL
  → Ghi Error Log

Recovery:
  1. Master ghi lại HR 0x0109 → master_alive = 1
  2. Master ghi Coil 0x0003 (clear_error) → FSM reset về IDLE
```

### 5.7. Meter Monitor (DLT645 Giám sát)

```
Chạy liên tục trong main loop, đọc từ DLT645 meter:

┌─────────────────────────────────────────────────┐
│ Cảnh báo           │ Điều kiện        │ Kết quả │
├─────────────────────┼──────────────────┼─────────┤
│ Overpower           │ P > max_power_W  │ ERROR   │
│ Overload            │ P > 110% rated   │ ERROR   │
│                     │   liên tục 30s   │         │
│ Energy Limit        │ session >= limit  │ FINISH  │
│ Low Current         │ I < 0.5A @ CHG   │ FINISH  │
│                     │   liên tục 60s   │         │
│ Voltage Fault       │ V < 180V         │ ERROR   │
│                     │ V > 260V         │         │
└─────────────────────┴──────────────────┴─────────┘
```

### 5.8. Tích lũy điện năng khi sạc

```python
# Gọi mỗi giây (hoặc mỗi tick nếu đơn giản hóa)
if state == CHARGING:
    # Tính năng lượng tích lũy: P(W) × thời gian(s) = Ws → / 3600 = Wh
    session_energy += power / 3600  # mỗi giây
    total_energy += power / 3600
```

---

## 6. GIAO THỨC MODBUS RTU SLAVE

### 6.1. Cấu hình kết nối

| Thông số | Giá trị |
|----------|---------|
| Baudrate | 9600 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Slave Address | Từ DIP switch (mặc định 1) |
| Frame silence | 3.5 char time (~4ms @ 9600) |

### 6.2. Function Codes hỗ trợ

| FC | Tên | Hướng | Mô tả |
|----|------|-------|-------|
| 01 | Read Coils | Read | Luôn trả 0 (coils tự reset) |
| 02 | Read Discrete Inputs | Read | 8 boolean inputs |
| 03 | Read Holding Registers | Read | 14 registers (0x0100-0x010D) |
| 04 | Read Input Registers | Read | 45 registers (0x0000-0x002C) |
| 05 | Write Single Coil | Write | 7 trigger commands |
| 06 | Write Single Register | Write | Config parameters |
| 16 (0x10) | Write Multiple Registers | Write | Batch write |

### 6.3. CRC16 Modbus

```python
def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc  # Little-endian: CRC_LO first, CRC_HI second
```

### 6.4. FC04: Input Registers (Read-Only) — 45 registers

| Addr | Tên | Đơn vị | Nguồn dữ liệu |
|------|------|--------|----------------|
| 0x0000 | voltage | 0.1V | DLT645 Meter |
| 0x0001 | current | 0.01A | DLT645 Meter |
| 0x0002 | power | W | DLT645 Meter |
| 0x0003 | energy_hi | Wh (high 16-bit) | DLT645 Meter |
| 0x0004 | energy_lo | Wh (low 16-bit) | DLT645 Meter |
| 0x0005 | temperature | 0.1°C | NTC ADC |
| 0x0006 | fsm_state | enum (0-5) | App FSM |
| 0x0007 | relay_status | bitmask | Relay module |
| | | | bit0=CHARGER, bit1=SOCKET, bit2=FAN, bit3=DOORLOCK |
| 0x0008 | led_status | bitmask | LED module |
| | | | bit0=RED, bit1=GREEN, bit2=WHITE |
| 0x0009 | meter_alarm | enum | Meter Monitor |
| | | | 0=OK, 1=OVERPOWER, 2=OVERLOAD, 3=ENERGY_LIM, 4=LOW_CURR, 5=VOLT_FAULT |
| 0x000A | door_open | bool | Digital Input (debounced) |
| 0x000B | fan_state | bool | Fan Control |
| 0x000C | session_energy | Wh | Meter Monitor |
| 0x000D | fw_version | hex | Hardcoded 0x0100 = v1.0 |
| 0x000E | slave_addr | uint8 | DIP switch |
| 0x000F | uptime_hi | s (high 16-bit) | Diagnostics |
| 0x0010 | uptime_lo | s (low 16-bit) | Diagnostics |
| 0x0011 | heartbeat | uint16 | +1 mỗi giây |
| 0x0012 | temp_min | 0.1°C | Diagnostics (min ghi nhận) |
| 0x0013 | temp_max | 0.1°C | Diagnostics (max ghi nhận) |
| 0x0014 | error_count | uint16 | Diagnostics (RAM session) |
| 0x0015 | charge_count | uint16 | Diagnostics (RAM session) |
| 0x0016 | dlt645_ok | uint16 | Counter frame DLT645 OK |
| 0x0017 | dlt645_fail | uint16 | Counter frame DLT645 FAIL |
| 0x0018 | alarm_flags | bitmask | Build từ tất cả alarm |
| 0x0019 | session_duration | seconds | Thời gian phiên sạc |
| 0x001A | meter_valid | bool | 1=tươi, 0=stale (>10s) |
| 0x001B | boot_count | uint16 | Flash persistent |
| 0x001C | total_error_count | uint16 | Flash persistent |
| 0x001D | total_charge_count | uint16 | Flash persistent |
| 0x001E | total_energy_hi | Wh (high) | Flash persistent |
| 0x001F | total_energy_lo | Wh (low) | Flash persistent |
| 0x0020 | last_error_id | uint16 | Flash persistent |
| 0x0021 | last_error_type | enum | Xem bảng Error Types |
| 0x0022 | master_alive | enum | 0=chưa HB, 1=alive, 2=timeout |
| 0x0023 | meter_serial_1 | BCD | DLT645 serial (chữ 1-4) |
| 0x0024 | meter_serial_2 | BCD | DLT645 serial (chữ 5-8) |
| 0x0025 | meter_serial_3 | BCD | DLT645 serial (chữ 9-12) |
| 0x0026 | frequency | 0.01Hz | DLT645 (50.00Hz=5000) |
| 0x0027 | power_factor | 0.001 | DLT645 (PF=0.95→950) |
| 0x0028 | current_rms_raw | 0.001A | ADC raw |
| 0x0029 | session_id | uint16 | Auto-increment |
| 0x002A | last_stop_reason | enum | Lý do dừng sạc |
| 0x002B | connector_status | enum | 0=Unplugged, 1=Plugged, 2=Locked |
| 0x002C | ground_fault | bool | Rò điện |

### 6.5. FC02: Discrete Inputs (Read-Only, 1-bit)

| Addr | Tên | Logic |
|------|------|-------|
| 0x0000 | door_open | 1 = cửa mở |
| 0x0001 | is_charging | 1 = state == CHARGING |
| 0x0002 | is_error | 1 = state == ERROR |
| 0x0003 | fan_running | 1 = quạt đang chạy |
| 0x0004 | door_unlocked | 1 = khóa đang mở |
| 0x0005 | tamper | 1 = phá hoại |
| 0x0006 | connector_plugged | 1 = đầu sạc đã cắm |
| 0x0007 | ground_fault | 1 = rò điện |

### 6.6. FC05: Coils (Write-Only, Trigger)

> Ghi `0xFF00` để kích hoạt, tự auto-reset về 0 sau xử lý.

| Addr | Tên | Hành vi |
|------|------|---------|
| 0x0000 | start_charge | → CHARGING (nếu không ERROR) |
| 0x0001 | stop_charge | → FINISH (nếu đang CHARGING) |
| 0x0002 | unlock_door | Mở khóa cửa 5s |
| 0x0003 | clear_error | Reset ERROR → IDLE (nếu đủ ĐK) |
| 0x0004 | standby | IDLE/FINISH → STANDBY |
| 0x0005 | force_fan | Bật/tắt quạt bắt buộc |
| 0x0006 | fw_update | Reserved (chưa implement) |

### 6.7. FC03/06/16: Holding Registers (Read/Write)

| Addr | Tên | Default | Đơn vị | Mô tả |
|------|------|---------|--------|-------|
| 0x0100 | fan_high_temp | 450 | 0.1°C | Ngưỡng bật quạt (45.0°C) |
| 0x0101 | fan_low_temp | 380 | 0.1°C | Ngưỡng tắt quạt (38.0°C) |
| 0x0102 | max_power | 7000 | W | Công suất max tức thì |
| 0x0103 | rated_power | 3500 | W | Công suất định mức |
| 0x0104 | energy_limit_hi | 0 | Wh | Giới hạn energy (high 16-bit) |
| 0x0105 | energy_limit_lo | 20000 | Wh | Giới hạn energy (low 16-bit) |
| 0x0106 | overtemp_limit | 700 | 0.1°C | Ngưỡng quá nhiệt (70.0°C) |
| 0x0107 | lock_pulse_ms | 5000 | ms | Thời gian mở khóa |
| 0x0108 | comm_timeout_s | 30 | s | Timeout DLT645 |
| 0x0109 | master_heartbeat | 0 | - | **Master ghi mỗi 3-5s** |
| 0x010A | current_limit | 3200 | 0.01A | Dynamic load balancing (32.00A) |
| 0x010B | session_energy_lim | 0 | Wh | Giới hạn kWh/phiên (0=off) |
| 0x010C | time_sync_hi | 0 | - | Unix timestamp (high 16-bit) |
| 0x010D | time_sync_lo | 0 | - | Unix timestamp (low 16-bit) |

### 6.8. Error Types (register 0x0021)

| ID | Tên | Mô tả |
|----|------|--------|
| 1 | ERR_OVERTEMP | >70°C |
| 2 | ERR_TAMPER | Phá hoại |
| 3 | ERR_OVERPOWER | Quá CS tức thì |
| 4 | ERR_OVERLOAD | Quá tải 30s |
| 5 | ERR_VOLTAGE | Ngoài 180-260V |
| 6 | ERR_COMM_FAIL | Mất DLT645 / Master |
| 7 | ERR_DOOR_CHARGING | Cửa mở khi sạc |
| 8 | ERR_GROUND_FAULT | Rò dòng ra vỏ |
| 9 | ERR_CONNECTOR | Đầu cắm fault |
| 10 | ERR_OVERCURRENT | Dòng > current_limit |

### 6.9. Frame Format ví dụ

#### Request: Read 35 Input Registers từ addr 0
```
01 04 00 00 00 23 [CRC_LO] [CRC_HI]
│  │  └──┬──┘ └──┬──┘
│  │     │       └─ Quantity = 35
│  │     └── Start addr = 0x0000
│  └── FC = 04 (Read Input Registers)
└── Slave addr = 1
```

#### Response: (35 registers × 2 bytes = 70 bytes data)
```
01 04 46 [70 bytes data...] [CRC_LO] [CRC_HI]
│  │  │
│  │  └── Byte count = 0x46 (70)
│  └── FC = 04
└── Slave addr = 1
```

#### Write Coil: Start Charge
```
01 05 00 00 FF 00 [CRC_LO] [CRC_HI]
│  │  └──┬──┘ └──┬──┘
│  │     │       └─ Value = 0xFF00 (ON)
│  │     └── Coil addr = 0x0000 (start_charge)
│  └── FC = 05
└── Slave addr = 1
```

---

## 7. GIAO THỨC MQTT (ESP32 GATEWAY)

### 7.1. Topics

| Topic | Hướng | Mô tả |
|-------|-------|-------|
| `charging/st/{mac}/gw_status` | GW → Server | Trạng thái gateway, mỗi 60s |
| `charging/st/{mac}/post/{id}/tlm` | GW → Server | Telemetry, 30s khi sạc, 60s khi rảnh |
| `charging/st/{mac}/post/{id}/status` | GW → Server | Sự kiện sạc (start/stop) |
| `charging/st/{mac}/post/{id}/evt` | GW → Server | Alarm / recovery events |
| `charging/st/{mac}/post/{id}/cmd` | Server → GW | Lệnh điều khiển |
| `charging/st/{mac}/cmd` | Server → GW | Lệnh gateway (reboot) |

### 7.2. Telemetry Payload (GW → Server)

```json
{
    "state": 3,
    "voltage": 225.4,
    "current": 12.85,
    "power": 2896,
    "frequency": 50.0,
    "energy_total": 121.250,
    "energy_session": 0.700,
    "temperature": 45.5,
    "meter_serial": "012345678912",
    "timestamp": 1711284600
}
```

> **Quy đổi từ Modbus → MQTT:**
> - `voltage` = reg[0] × 0.1
> - `current` = reg[1] × 0.01
> - `power` = reg[2]
> - `energy_total` = ((reg[3] << 16) | reg[4]) × 0.001
> - `temperature` = reg[5] × 0.1

### 7.3. Command Payload (Server → GW)

```json
// Start charge
{"cmd": "start_charge", "params": {"min_current": 0.20, "max_coin_limit": 500.0}, "timestamp": 1711284500}

// Stop charge
{"cmd": "stop_charge", "params": {"reason": 3}, "timestamp": 1711285000}

// Unlock door
{"cmd": "unlock_door", "params": {"duration_ms": 5000}, "timestamp": 1711285050}

// Clear error
{"cmd": "clear_error", "timestamp": 1711285055}
```

### 7.4. Event Payload

```json
// Charging Started
{"message_type": 0, "event_code": 10, "start_kwh": 120.550, "timestamp": 1711284515}

// Session Completed  
{"message_type": 0, "event_code": 11, "start_kwh": 120.550, "end_kwh": 125.750, "total_consumed": 5.200, "reason": 3, "timestamp": 1711285005}

// Alarm (over-temperature)
{"message_type": 1, "event_code": 1, "value": 78.5, "timestamp": 1711285100}
```

### 7.5. Gateway Status Payload

```json
{
    "timestamp": 1711284000,
    "uptime": 3600,
    "wifi_rssi": -65,
    "stats": {
        "total_posts": 10,
        "idle": 5,
        "charging": 3,
        "error": 1,
        "offline": 1
    },
    "posts_state": [
        {"id": 1, "state": 2},
        {"id": 2, "state": 0}
    ]
}
```

---

## 8. DASHBOARD API

### 8.1. GET /api/state

Trả về JSON trạng thái toàn bộ hệ thống, để dashboard HTML poll mỗi 200-500ms.

```json
{
    "state": "CHARGING",
    "volt": 2200,
    "amp": 1500,
    "pwr": 3500,
    "kwh": 1250,
    "temp": 385,
    "up": 3600,
    "hb": 3600,
    "fan": 0,
    "lock": 0,
    "door": 0,
    "af": 0,
    "m_alive": 1,
    "m_hb_val": 12345,
    "l_w": 0,
    "l_g": 0,
    "l_r": 1,
    "r_c": 1,
    "r_s": 0
}
```

| Field | Type | Mô tả |
|-------|------|-------|
| `state` | string/int | FSM state hiện tại |
| `volt` | int | Voltage × 10 (2200 = 220.0V) |
| `amp` | int | Current × 100 (1500 = 15.00A) |
| `pwr` | int | Power W |
| `kwh` | int | Energy Wh |
| `temp` | int | Temperature × 10 (385 = 38.5°C) |
| `up` | int | Uptime seconds |
| `hb` | int | Heartbeat counter |
| `fan` | int | 0/1 fan state |
| `lock` | int | 0/1 door lock state |
| `door` | int | 0/1 door open |
| `af` | int | Alarm flags bitmask |
| `m_alive` | int | Master alive (0/1/2) |
| `l_w/l_g/l_r` | int | LED white/green/red state |
| `r_c/r_s` | int | Relay charger/socket state |

### 8.2. GET /api/set?key=value

Dùng cho Dashboard điều khiển simulator:

| Key | Value | Hành vi |
|-----|-------|---------|
| `door` | 0/1 | Set cửa đóng/mở |
| `temp` | int | Set nhiệt độ (0.1°C) |
| `volt` | int | Set điện áp (0.1V) |
| `pwr` | int | Set công suất (W) |
| `m_start` | 1 | Master: Start charge |
| `m_stop` | 1 | Master: Stop charge |
| `m_unlock` | 1 | Master: Unlock door |
| `m_standby` | 1 | Master: Standby |
| `clear` | 1 | Master: Clear error |
| `m_hb` | 0/1 | Toggle auto heartbeat |
| `state` | string | Force set state |

---

## 9. KỊCH BẢN TEST TỰ ĐỘNG

### TEST 1: Boot → IDLE (Khởi tạo cơ bản)

```
Mục đích: Kiểm tra firmware khởi tạo đúng
─────────────────────────────────────────
1. Reset simulator (init mới)
2. Set LED White = ON (boot indicator)
3. Chạy 1 giây
4. Kiểm tra:
   ✅ state == IDLE (1)
   ✅ LED White = ON, Green = OFF, Red = OFF
   ✅ Relay CHARGER = OFF
   ✅ Không có alarm
```

### TEST 2: Door → ERROR → Clear

```
Mục đích: Kiểm tra phát hiện cửa mở và clear error
───────────────────────────────────────────────────
1. Boot → IDLE (TEST 1)
2. Set door_sensor = False (mở cửa)
3. Chạy 100ms (đợi debounce)
4. Kiểm tra:
   ✅ state == ERROR (5)
   ✅ LED Red = ON solid
   ✅ alarm_flags bit1 (DOOR_OPEN) = 1
5. Set door_sensor = True (đóng cửa)
6. Chạy 100ms
7. Gửi clear_error
8. Chạy 100ms
9. Kiểm tra:
   ✅ state == IDLE (1)
   ✅ LED White = ON
```

### TEST 3: Full Charge Cycle

```
Mục đích: Kiểm tra chu trình sạc hoàn chỉnh IDLE → STANDBY → CHARGING → FINISH
──────────────────────────────────────────────────────────────────────────────────
1. Boot → IDLE
2. Set voltage=2200, power=3500, current=1500
3. Gửi standby (Coil 0x0004)
4. Chạy 1 giây
5. Kiểm tra:
   ✅ state == STANDBY (2)
   ✅ LED Green = BLINK
6. Gửi start_charge (Coil 0x0000)
7. Chạy 5 giây
8. Kiểm tra:
   ✅ state == CHARGING (3)
   ✅ Relay CHARGER = ON
   ✅ LED Red = BLINK
   ✅ session_energy > 0
9. Gửi stop_charge (Coil 0x0001)
10. Chạy 1 giây
11. Kiểm tra:
   ✅ state == FINISH (4)
   ✅ Relay CHARGER = OFF
   ✅ LED Green = ON solid
   ✅ charge_count >= 1
```

### TEST 4: TAMPER Detection

```
Mục đích: Kiểm tra phát hiện cửa bị phá
──────────────────────────────────────────
1. Boot → IDLE
2. Mở cửa (door_sensor=False) KHÔNG qua unlock
3. Chạy 100ms
4. Kiểm tra:
   ✅ is_tamper() == True
   ✅ state == ERROR (5)
   ✅ alarm_flags bit2 (TAMPER) = 1
```

### TEST 5: Quá nhiệt → Fan + ERROR

```
Mục đích: Kiểm tra logic fan hysteresis và overtemp protection
───────────────────────────────────────────────────────────────
1. Boot → IDLE
2. Set temp = 460 (46.0°C, > fan_high 45°C)
3. Chạy 1 giây
4. Kiểm tra:
   ✅ Fan = ON
   ✅ Relay FAN = True
5. Set temp = 720 (72.0°C, > overtemp 70°C)
6. Chạy 1 giây
7. Kiểm tra:
   ✅ state == ERROR (5)
   ✅ alarm_flags bit0 (OVERTEMP) = 1
8. Set temp = 350 (35.0°C)
9. Chạy 1 giây
10. Gửi clear_error
11. Chạy 1 giây
12. Kiểm tra:
    ✅ state == IDLE (1)
```

### TEST 6: Master Heartbeat Timeout

```
Mục đích: Kiểm tra cơ chế watchdog communication
──────────────────────────────────────────────────
1. Boot → IDLE
2. Ghi HR 0x0109 = bất kỳ (kích hoạt heartbeat)
3. Chạy 5 giây
4. Kiểm tra: master_alive == 1
5. KHÔNG ghi HR 0x0109 nữa
6. Chạy 11 giây (> 10s timeout)
7. Kiểm tra:
   ✅ master_alive == 2 (TIMEOUT)
   ✅ state == ERROR (5)
   ✅ alarm_flags bit7 (COMM_FAIL) = 1
8. Ghi lại HR 0x0109 → master_alive = 1
9. Ghi Coil 0x0003 (clear_error)
10. Kiểm tra:
    ✅ state == IDLE (1)
```

### TEST 7: Overpower Emergency Stop

```
Mục đích: Kiểm tra ngắt khẩn cấp khi quá công suất
────────────────────────────────────────────────────
1. Boot → IDLE → Start Charge (power=3500)
2. Chạy 2 giây, xác nhận CHARGING
3. Set power = 7500 (> max_power 7000W)
4. Chạy 1 giây
5. Kiểm tra:
   ✅ state == ERROR (5)
   ✅ Relay CHARGER = OFF
   ✅ alarm_flags bit3 (OVERPOWER) = 1
```

### TEST 8: Voltage Fault

```
Mục đích: Kiểm tra phát hiện mất pha / quá áp
────────────────────────────────────────────────
1. Boot → IDLE
2. Set voltage = 1500 (150V, < 180V)
3. Chạy 1 giây
4. Kiểm tra:
   ✅ state == ERROR (5)
   ✅ alarm_flags bit4 (VOLT_FAULT) = 1
```

### TEST 9: Door Lock Timing

```
Mục đích: Kiểm tra unlock 5s và auto-relock
────────────────────────────────────────────
1. Boot → IDLE
2. Gửi unlock_door (Coil 0x0002)
3. Kiểm tra ngay:
   ✅ lock_active == True
   ✅ Relay DOORLOCK = True (HIGH)
4. Chạy 4 giây
5. Kiểm tra: vẫn DOORLOCK = True
6. Chạy 2 giây (tổng > 5s)
7. Kiểm tra:
   ✅ lock_active == False
   ✅ Relay DOORLOCK = False (tự khóa)
```

### TEST 10: Energy Limit → Auto FINISH

```
Mục đích: Kiểm tra ngắt tự động khi đủ kWh
────────────────────────────────────────────
1. Boot → IDLE
2. Set energy_limit = 100 Wh (nhỏ để test nhanh)
3. Set power = 3600W
4. Start charge
5. Chạy 100+ giây (3600W × 100s / 3600 = 100Wh)
6. Kiểm tra:
   ✅ state == FINISH (4) (tự chuyển)
   ✅ session_energy ~= 100 Wh
```

---

## 10. HƯỚNG DẪN TRIỂN KHAI

### 10.1. Cấu trúc thư mục dự kiến

```
charger-simulator/
├── README.md
├── requirements.txt        # pymodbus, paho-mqtt
├── docker-compose.yml      # Mosquitto + Simulator
│
├── simulator/
│   ├── __init__.py
│   ├── hardware_model.py   # Class HardwareModel
│   ├── firmware_sim.py     # Class FirmwareSimulator (FSM + Logic)
│   ├── modbus_server.py    # Modbus RTU TCP Server (port 4321)
│   ├── gateway_sim.py      # ESP32 Gateway simulator (poll + MQTT)
│   ├── dashboard_api.py    # HTTP API server (port 4323)
│   └── cli.py              # Interactive CLI
│
├── web/
│   └── dashboard.html      # Giao diện trình duyệt
│
├── tests/
│   ├── test_fsm.py         # Unit test cho FSM
│   ├── test_modbus.py      # Test Modbus compliance
│   └── test_scenarios.py   # 10 kịch bản test tự động
│
├── docs/
│   ├── MODBUS_REGISTER_MAP.md
│   ├── MQTT_PAYLOAD_V3.3.md
│   └── SIMULATOR_SPECIFICATION.md  # File này
│
└── mosquitto/
    └── config/
        └── mosquitto.conf
```

### 10.2. Quy trình chạy

```bash
# 1. Cài dependencies
pip install pymodbus paho-mqtt

# 2. Chạy MQTT Broker (nếu cần test MQTT)
docker compose up -d mosquitto

# 3. Chạy Simulator
python -m simulator.main
# Hoặc individual:
python simulator/firmware_sim.py     # CLI mode
python simulator/modbus_server.py    # Modbus TCP server
python simulator/gateway_sim.py      # Gateway + MQTT
python simulator/dashboard_api.py    # HTTP API

# 4. Mở Dashboard
# → http://localhost:4323/dashboard.html

# 5. Kết nối Master thật (hoặc test tool)
python modbus_master.py --port tcp:127.0.0.1:4321 --addr 1
```

### 10.3. Docker Compose

```yaml
services:
  mosquitto:
    image: eclipse-mosquitto:latest
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto/config/mosquitto.conf:/mosquitto/config/mosquitto.conf:ro

  simulator:
    build: .
    ports:
      - "4321:4321"  # Modbus RTU over TCP
      - "4323:4323"  # Dashboard API
    environment:
      - MQTT_BROKER=mosquitto
    depends_on:
      - mosquitto
```

### 10.4. Checklist hoàn thành

- [ ] `HardwareModel` — Tất cả biến trạng thái phần cứng
- [ ] `FirmwareSimulator` — FSM 6 trạng thái + tất cả transition
- [ ] Debounce cửa (50ms)
- [ ] Tamper detection logic
- [ ] Fan hysteresis (bật ≥45°C, tắt ≤38°C, min 30s ON)
- [ ] Door lock auto-relock 5s
- [ ] Alarm flags bitmask (11 bits)
- [ ] Master heartbeat watchdog (10s timeout)
- [ ] Meter monitor (overpower, overload, energy limit, low current, volt fault)
- [ ] Session energy accumulation
- [ ] `ModbusSlaveServer` — FC01, FC02, FC03, FC04, FC05, FC06, FC16
- [ ] CRC16 Modbus (polynomial 0xA001)
- [ ] 45 Input Registers đầy đủ
- [ ] 14 Holding Registers + apply logic
- [ ] 8 Discrete Inputs
- [ ] 7 Coils + trigger actions
- [ ] Safety Interlock (RL1/RL2 mutual exclusion)
- [ ] `GatewaySimulator` — Poll Modbus → MQTT publish
- [ ] MQTT Telemetry, Status, Event payloads
- [ ] Master heartbeat ghi HR 0x0109
- [ ] `DashboardAPI` — /api/state, /api/set
- [ ] `DashboardUI` — LED, Relay, Stats, Alarms, Controls
- [ ] 10 kịch bản test tự động PASS

---

## PHỤ LỤC A: LED Behavior Matrix

| State | RED | GREEN | WHITE |
|-------|-----|-------|-------|
| INIT | OFF | OFF | OFF |
| IDLE | OFF | OFF | SOLID |
| STANDBY | OFF | BLINK 500ms | OFF |
| CHARGING | BLINK 500ms | OFF | OFF |
| FINISH | OFF | SOLID | OFF |
| ERROR | SOLID | OFF | OFF |

## PHỤ LỤC B: Relay Behavior Matrix

| State | RL1 (CHARGER) | RL2 (SOCKET) | RL3 (FAN) | RL4 (LOCK) |
|-------|---------------|--------------|-----------|------------|
| INIT | OFF | OFF | OFF | OFF |
| IDLE | OFF | OFF | Auto | Auto 5s |
| STANDBY | OFF | OFF | Auto | Auto 5s |
| CHARGING | **ON** | OFF | Auto | Auto 5s |
| FINISH | OFF | OFF | Auto | Auto 5s |
| ERROR | **FORCED OFF** | **FORCED OFF** | Auto | OFF |

> **Auto** = Controlled by fan_ctrl / door_lock modules independently

## PHỤ LỤC C: Concurrency Notes

```
Main Loop (mỗi 1ms):
  1. process_door()         — debounce GPIO
  2. process_fan()          — hysteresis control
  3. process_lock()         — auto-relock timer
  4. process_diag()         — uptime, heartbeat, min/max temp
  5. build_alarm_flags()    — tổng hợp tất cả alarm
  6. FSM process()          — state machine logic
  7. Modbus_Process()       — parse RX buffer, master HB watchdog
```

> Tất cả logic PHẢI non-blocking. Sử dụng tick-based timing (current_tick - start_tick >= duration), KHÔNG DÙNG sleep/delay trong main loop.

---

*Tài liệu này là nguồn tham chiếu DUY NHẤT. Mọi câu hỏi về logic nghiệp vụ đều trả lời được từ đây. Happy coding!* 🚀
