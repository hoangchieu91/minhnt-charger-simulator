# PROMPT GIẢ LẬP FIRMWARE — TRU V1.0 Charger Slave (STM32F103C8T6)

> **Mục đích:** Tài liệu này chứa toàn bộ nghiệp vụ, logic, hằng số, và kịch bản hoạt động của firmware trụ sạc để bạn có thể **xây dựng giả lập (Simulator) bằng bất kỳ ngôn ngữ nào (Python, C#, JS...)** trên một máy khác mà không cần mã nguồn C gốc.
>
> **Ngày tạo:** 05/04/2026  
> **Firmware Version:** v1.0 Rev 2.0  
> **MCU thật:** STM32F103C8T6 (ARM Cortex-M3)

---

## MỤC LỤC

1. [Tổng quan hệ thống](#1-tổng-quan-hệ-thống)
2. [Mô hình phần cứng cần giả lập](#2-mô-hình-phần-cứng-cần-giả-lập)
3. [Máy trạng thái chính (FSM)](#3-máy-trạng-thái-chính-fsm)
4. [Module LED (led_rgw)](#4-module-led-led_rgw)
5. [Module Relay (relay_ctrl)](#5-module-relay-relay_ctrl)
6. [Module NTC cảm biến nhiệt (ntc_temp)](#6-module-ntc-cảm-biến-nhiệt-ntc_temp)
7. [Module Quạt (fan_ctrl)](#7-module-quạt-fan_ctrl)
8. [Module Giám sát công suất (meter_monitor)](#8-module-giám-sát-công-suất-meter_monitor)
9. [Module Đầu vào số (digital_input)](#9-module-đầu-vào-số-digital_input)
10. [Module Khóa cửa (door_lock)](#10-module-khóa-cửa-door_lock)
11. [Module Chẩn đoán (diagnostics)](#11-module-chẩn-đoán-diagnostics)
12. [Module Nhật ký lỗi (error_log)](#12-module-nhật-ký-lỗi-error_log)
13. [Giao thức Modbus Slave](#13-giao-thức-modbus-slave)
14. [Bảng Alarm Flags (bitmask)](#14-bảng-alarm-flags-bitmask)
15. [Kịch bản kiểm thử](#15-kịch-bản-kiểm-thử)
16. [Giao tiếp ESP32 Master (Context tổng)](#16-giao-tiếp-esp32-master-context-tổng)

---

## 1. Tổng quan hệ thống

Trụ sạc TRU V1.0 là một board mạch nhúng điều khiển việc cấp/ngắt nguồn điện xoay chiều (AC) cho xe điện/thiết bị. Board này **KHÔNG kết nối Internet trực tiếp**. Nó giao tiếp với một Gateway (ESP32) qua chuẩn **Modbus RTU RS485**.

```
[Cloud Server] ←─ MQTT ─→ [ESP32 Gateway / Master]
                                    │  RS485 (9600-8N1)
                                    ▼
                           [STM32 Slave / TRU V1.0]  ← BẠN GIẢ LẬP CÁI NÀY
                                    │
                                    ├── 4x Relay (Sạc, Ổ cắm, Quạt, Khóa cửa)
                                    ├── 3x LED (Đỏ, Xanh lá, Trắng)
                                    ├── 1x NTC20K (Cảm biến nhiệt)
                                    ├── 1x Door Sensor (Cảm biến cửa)
                                    ├── 1x DIP Switch 3-bit (Địa chỉ Modbus)
                                    └── RS485 #2 → [Đồng hồ điện DLT645]
```

### Vòng lặp chính (Main Loop)

Firmware chạy vòng lặp `while(1)` với tần số khoảng **1ms/tick**. Mỗi vòng lặp gọi tuần tự:

```
App_Process():
  1. LED_Process()         ← Xử lý nhấp nháy LED
  2. Relay_Process()       ← Xử lý interlock + dead-time relay
  3. NTC_Process()         ← Đọc ADC, lọc trung bình
  4. Fan_Process(temp)     ← Logic quạt hysteresis
  5. Meter_Process()       ← Kiểm tra alarm điện
  6. DI_Process()          ← Debounce cửa
  7. DoorLock_Process()    ← Tự khóa cửa sau 5s
  8. Diag_Process()        ← Uptime, heartbeat
  9. build_alarm_flags()   ← Tổng hợp cờ cảnh báo
  10. FSM switch/case      ← Logic chuyển trạng thái
```

---

## 2. Mô hình phần cứng cần giả lập

Tạo một object `HardwareModel` chứa các biến dưới đây. Simulator sẽ thao tác trực tiếp lên object này.

```python
class HardwareModel:
    # Clock
    tick: int = 0                    # Tăng 1 mỗi ms

    # --- GPIO Inputs ---
    door_sensor: bool = True         # True=đóng (pullup HIGH), False=mở (LOW)
    dip_switch: int = 1              # 3-bit (0-7), đọc 1 lần khi boot → Modbus slave addr

    # --- GPIO Outputs ---
    relay_charger: bool = False      # RL1 - PB0: Cấp nguồn bộ sạc
    relay_socket: bool = False       # RL2 - PB1: Cấp nguồn ổ cắm
    relay_fan: bool = False          # RL3 - PB10: Quạt
    relay_doorlock: bool = False     # RL4 - PB11: Khóa cửa solenoid

    led_red: int = 0                 # 0=OFF, 1=SOLID, 2=BLINK
    led_green: int = 0
    led_white: int = 0

    # --- ADC ---
    ntc_adc: int = 2048             # 12-bit (0-4095), ~25°C mặc định

    # --- Meter Data (nhận từ DLT645 hoặc giả lập) ---
    voltage: int = 2200             # Đơn vị 0.1V → 220.0V
    current: int = 0                # Đơn vị 0.01A → 0.00A
    power: int = 0                  # Đơn vị W
    energy: int = 0                 # Đơn vị Wh (tổng cộng tích lũy)
    meter_valid: bool = True        # DLT645 còn phản hồi?
```

---

## 3. Máy trạng thái chính (FSM)

### 3.1. Các trạng thái

| Mã | Tên | LED | Relay | Mô tả |
|----|-----|-----|-------|-------|
| 0 | `INIT` | — | Tất cả OFF | Chỉ thoáng qua lúc boot |
| 1 | `IDLE` | Trắng SOLID | Tất cả OFF | Rảnh, sẵn sàng |
| 2 | `STANDBY` | Xanh lá BLINK | Tất cả OFF | Chờ lệnh sạc |
| 3 | `CHARGING` | Đỏ BLINK | RL_CHARGER=ON | Đang cấp nguồn |
| 4 | `FINISH` | Xanh lá SOLID | Tất cả OFF | Sạc xong |
| 5 | `ERROR` | Đỏ SOLID | Tất cả OFF (cưỡng bức) | Lỗi khẩn cấp |

### 3.2. Sơ đồ chuyển trạng thái (Transitions)

```
                  ┌────────────────────────────┐
                  │          INIT (0)           │
                  └──────────┬─────────────────┘
                             │  App_Init()
                             ▼
             ┌───────────── IDLE (1) ◄──── App_TriggerClearError()
             │               │                     ▲
             │  standby      │ door/tamper/overtemp │ (chỉ khi: cửa đóng
             │  coil         │                      │  + temp<70°C
             ▼               ▼                      │  + không tamper)
        STANDBY (2) ──────► ERROR (5) ◄────────────┘
             │                  ▲      ▲
  start_charge coil             │      │
             │                  │      │
             ▼                  │      │
       CHARGING (3) ────────────┘      │
             │  overtemp/door/tamper   │
             │  overpower/voltage      │
             │                         │
             │  energy_limit           │
             │  low_current            │
             │  stop coil              │
             ▼                         │
        FINISH (4) ────────────────────┘
                  door/tamper
```

### 3.3. Quy tắc chuyển trạng thái (Chi tiết từ mã nguồn)

#### Vào STANDBY
```
Điều kiện: state hiện tại = IDLE hoặc FINISH
Action:
  - LED: White=OFF, Green=BLINK, Red=OFF
```

#### Vào CHARGING
```
Điều kiện: state hiện tại != ERROR và != CHARGING
Action:
  - LED: White=OFF, Green=OFF, Red=BLINK
  - Relay_SetTarget(RL_CHARGER, ON)
  - Meter_StartSession()        ← reset session energy counter
  - session_id++                ← tăng ID phiên (wrap 65535→1, skip 0)
  - charge_start_tick = now     ← bắt đầu đếm thời gian sạc
  - last_stop_reason = UNKNOWN
```

#### Vào FINISH (Sạc xong bình thường)
```
Điều kiện: state hiện tại = CHARGING
Action:
  - LED: Red=OFF, Green=SOLID
  - Relay_SetTarget(RL_CHARGER, OFF)
  - Diag_IncrementCharge()       ← +1 bộ đếm phiên sạc
  - Nếu chưa set reason → reason = REMOTE_STOP_USER (2)
```

#### Vào ERROR
```
Được gọi từ BẤT KỲ trạng thái nào.
Action:
  - LED: White=OFF, Green=OFF, Red=SOLID
  - Relay_SetTarget(RL_CHARGER, OFF)
  - Relay_SetTarget(RL_SOCKET, OFF)
  - Diag_IncrementError()         ← +1 bộ đếm lỗi
  - Nếu chưa set reason → reason = SAFETY_ALARM_STOP (4)
```

#### Clear Error (về IDLE)
```
Điều kiện CẦN đầy đủ cả 3:
  1. state hiện tại = ERROR
  2. Cửa ĐÓNG (DI_IsDoorOpen() == false)
  3. Nhiệt độ < 700 (70.0°C)
  4. KHÔNG tamper (DI_IsTamper() == false)
=> Nếu đủ → Gọi App_Init() → reset về IDLE
=> Nếu thiếu → KHÔNG LÀM GÌ (giữ ERROR)
```

### 3.4. Logic quét mỗi vòng FSM (trong App_Process)

#### Khi ở IDLE hoặc STANDBY:
```python
if tamper:              trigger_error()
elif door_open:         trigger_error()
elif temp > 700:        trigger_error()
```

#### Khi ở CHARGING:
```python
# a. Tamper → ERROR (ưu tiên cao nhất)
if tamper:              trigger_error()

# b. Quá nhiệt → ERROR  
elif temp > 700:        trigger_error()

# c. Cửa mở → ERROR
elif door_open:         trigger_error()

# d. Meter alarm
elif alarm in (OVERPOWER, OVERLOAD, VOLTAGE_FAULT):
    stop_reason = SAFETY_ALARM_STOP
    trigger_error()
elif alarm in (ENERGY_LIMIT, LOW_CURRENT):
    stop_reason = FINISHED_AUTO
    trigger_stop_charge()

# e. Overcurrent (Rev 2.0): dòng > 110% current_limit
elif current > current_limit * 110/100 and current_limit > 0:
    stop_reason = OVERCURRENT_STOP
    trigger_error()

# f. Session energy limit (Rev 2.0)
elif session_energy_limit > 0 and session_energy >= session_energy_limit:
    stop_reason = SESSION_ENERGY_EXCEEDED
    trigger_stop_charge()
```

#### Khi ở FINISH:
```python
if tamper:       trigger_error()
elif door_open:  trigger_error()
```

#### Khi ở ERROR:
```python
# Cưỡng bức relay OFF mỗi tick (phòng trường hợp bug set lại)
relay_charger = OFF
relay_socket = OFF
# Fan vẫn chạy theo hysteresis (tự quản)
```

---

## 4. Module LED (led_rgw)

### Hằng số
```
BLINK_INTERVAL_MS = 500
Có 3 LED: RED(0), GREEN(1), WHITE(2)
Có 3 mode: OFF(0), SOLID(1), BLINK(2)
```

### Logic
```python
class LED:
    modes = [OFF, OFF, OFF]     # 3 LED
    blink_state = False
    last_blink_tick = 0
    
    def set(color, mode):
        modes[color] = mode
        if mode == OFF:    hw.write_pin(color, 0)
        elif mode == SOLID: hw.write_pin(color, 1)
        # BLINK sẽ do process() xử lý
    
    def process():
        if tick - last_blink_tick >= 500:
            last_blink_tick = tick
            blink_state = !blink_state
            for i in range(3):
                if modes[i] == BLINK:
                    hw.write_pin(i, 1 if blink_state else 0)
```

---

## 5. Module Relay (relay_ctrl)

### Hằng số
```
INTERLOCK_DELAY_MS = 100   # Dead-time dập hồ quang: 100ms
Có 4 relay: RL_CHARGER(0), RL_SOCKET(1), RL_FAN(2), RL_DOORLOCK(3)
RL_CHARGER và RL_SOCKET là cặp INTERLOCK (không bao giờ cùng ON)
```

### Logic đầy đủ (QUAN TRỌNG — SAFETY CRITICAL)

```python
class RelayCtrl:
    target = [OFF, OFF, OFF, OFF]
    actual = [OFF, OFF, OFF, OFF]
    interlock_transitioning = False
    interlock_start_time = 0
    pending_relay_id = 0
    
    def set_target(relay_id, state):
        # === KHÓA CHÉO ===
        if state == ON:
            if relay_id == RL_CHARGER and target[RL_SOCKET] == ON:
                target[RL_SOCKET] = OFF    # Cưỡng bức tắt Socket
            elif relay_id == RL_SOCKET and target[RL_CHARGER] == ON:
                target[RL_CHARGER] = OFF   # Cưỡng bức tắt Charger
        target[relay_id] = state
    
    def process():
        # 1. Relay thường (Fan, DoorLock) → áp dụng NGAY
        for i in [2, 3]:   # RL_FAN, RL_DOORLOCK
            if target[i] != actual[i]:
                hw.write_pin(i, target[i])
                actual[i] = target[i]
        
        # 2. Interlock relay: TẮT thì tắt NGAY
        for i in [0, 1]:   # RL_CHARGER, RL_SOCKET
            if target[i] == OFF and actual[i] == ON:
                hw.write_pin(i, OFF)
                actual[i] = OFF
                if interlock_transitioning and pending_relay_id == i:
                    interlock_transitioning = False
        
        # 3. Interlock relay: BẬT thì phải chờ dead-time 100ms
        if not interlock_transitioning:
            for i in [0, 1]:
                if target[i] == ON and actual[i] == OFF:
                    other = 1 if i == 0 else 0
                    if actual[other] == OFF:      # relay kia phải đã tắt
                        interlock_transitioning = True
                        pending_relay_id = i
                        interlock_start_time = tick
                    break                         # chỉ xử lý 1 lúc
        else:
            # Đang chờ dead-time
            if (tick - interlock_start_time) >= 100:
                if target[pending_relay_id] == ON:
                    hw.write_pin(pending_relay_id, ON)
                    actual[pending_relay_id] = ON
                interlock_transitioning = False
```

---

## 6. Module NTC cảm biến nhiệt (ntc_temp)

### Hằng số
```
R_PULLUP = 10000    # 10kΩ
ADC_MAX = 4095      # 12-bit
NTC_FILTER_SIZE = 8 # Moving Average N=8
```

### Bảng tra Steinhart-Hart (NTC20K, B=3950)

| R_NTC (Ω) | Nhiệt độ (0.1°C) | Nhiệt độ (°C) |
|-----------|-------------------|----------------|
| 97900 | -100 | -10.0 |
| 67800 | 0 | 0.0 |
| 47600 | 100 | 10.0 |
| 33900 | 200 | 20.0 |
| 20000 | 250 | 25.0 |
| 17400 | 300 | 30.0 |
| 12700 | 350 | 35.0 |
| 10000 | 400 | 40.0 |
| 7500  | 450 | 45.0 |
| 5700  | 500 | 50.0 |
| 4400  | 550 | 55.0 |
| 3400  | 600 | 60.0 |
| 2700  | 650 | 65.0 |
| 2100  | 700 | 70.0 |
| 1700  | 750 | 75.0 |
| 1400  | 800 | 80.0 |
| 1100  | 850 | 85.0 |
| 900   | 900 | 90.0 |
| 740   | 950 | 95.0 |
| 620   | 1000 | 100.0 |

### Logic
```
1. Đọc ADC → filter Moving Average (buffer 8 mẫu)
2. Tính R_NTC = R_PULLUP × ADC / (4095 - ADC)
   - ADC = 0    → R = ∞ (hở mạch)
   - ADC = 4095 → R = 0 (chập GND)
3. Nội suy tuyến tính từ bảng lookup (R giảm khi T tăng)
4. Trả về đơn vị 0.1°C (VD: 250 = 25.0°C, 700 = 70.0°C)
```

> **Gợi ý cho Simulator:** Bạn có thể bỏ qua ADC + bảng lookup và trực tiếp set `ntc_temp = 450` nghĩa là 45.0°C. Module NTC chỉ quan trọng khi test với Renode / HIL.

---

## 7. Module Quạt (fan_ctrl)

### Hằng số
```
threshold_high = 450     # Bật quạt khi ≥ 45.0°C (đơn vị 0.1°C)
threshold_low  = 380     # Tắt quạt khi ≤ 38.0°C
FAN_MIN_ON_TIME_MS = 30000   # Tối thiểu 30s chạy (chống giật motor)
FAN_TEMP_FLOOR = 300     # Ngưỡng sàn 30°C (clamp)
FAN_TEMP_CEILING = 800   # Ngưỡng trần 80°C (clamp)
FAN_TEMP_GAP_MIN = 50    # Gap tối thiểu 5°C (high - low ≥ 5°C)
```

### Logic Hysteresis + Anti-chatter

```python
class FanCtrl:
    fan_state = OFF
    fan_on_tick = 0
    
    def process(temp):
        if fan_state == OFF:
            if temp >= threshold_high:    # ≥45°C
                fan_state = ON
                fan_on_tick = tick
                hw.relay_fan = ON
        
        else:  # fan đang ON
            elapsed = tick - fan_on_tick
            if elapsed >= 30000:          # Đã chạy ≥30s
                if temp <= threshold_low:  # ≤38°C
                    fan_state = OFF
                    hw.relay_fan = OFF
            # Nếu chưa đủ 30s HOẶC temp vẫn trong vùng trễ → giữ ON
```

> **Vùng trễ (Hysteresis):** Nhiệt lên 45°C → bật. PHẢI hạ xuống 38°C mới tắt. Vùng 38-45°C giữ nguyên trạng thái cũ.

> **Có thể thay đổi ngưỡng qua Modbus:** Ghi HR 0x0100 (high) và 0x0101 (low). Giá trị được auto-clamp: gap ≥5°C, sàn 30°C, trần 80°C.

---

## 8. Module Giám sát công suất (meter_monitor)

### Hằng số
```
OVERLOAD_DURATION_MS    = 30000   # 30s quá tải liên tục → ALARM
LOW_CURRENT_DURATION_MS = 60000   # 60s dòng thấp → xe đầy
LOW_CURRENT_THRESH_001A = 50      # 0.50A
VOLTAGE_MIN_01V         = 1800    # 180.0V
VOLTAGE_MAX_01V         = 2600    # 260.0V
METER_STALE_TIMEOUT_MS  = 10000   # 10s không update → data stale

# Config defaults (có thể thay qua Modbus HR):
max_power_W       = 7000    # Quá CS tức thì → CẮT NGAY
rated_power_W     = 3500    # 110% rated = 3850W → bắt đầu đếm 30s
energy_limit_Wh   = 20000   # Đạt 20kWh phiên → FINISH
```

### Enum MeterAlarm
```
METER_OK            = 0
METER_OVERPOWER     = 1    # power > max_power_W → CẮT NGAY
METER_OVERLOAD      = 2    # power > 110% rated liên tục 30s
METER_ENERGY_LIMIT  = 3    # session energy ≥ energy_limit → FINISH
METER_LOW_CURRENT   = 4    # current < 0.5A liên tục 60s → FINISH (xe đầy)
METER_VOLTAGE_FAULT = 5    # voltage < 180V hoặc > 260V
```

### Logic quét (Meter_Process)

```python
def meter_process():
    # 0. Kiểm tra data tươi (DLT645 còn phản hồi?)
    if meter_valid and (tick - last_update > 10000):
        meter_valid = False
        voltage = current = power = 0   # zero out (energy giữ nguyên)
    
    # 1. Quá áp / mất pha
    if voltage > 0 and (voltage < 1800 or voltage > 2600):
        alarm = VOLTAGE_FAULT
        return
    
    # 2. Quá công suất tức thì
    if power > max_power_W:
        alarm = OVERPOWER
        return
    
    # 3. Quá tải có thời gian (>110% rated liên tục 30s)
    overload_thresh = rated_power_W * 110 / 100    # = 3850
    if power > overload_thresh:
        if not overload_timing:
            overload_timing = True
            overload_start = tick
        elif tick - overload_start >= 30000:
            alarm = OVERLOAD
            return
    else:
        overload_timing = False   # Reset timer nếu power hạ xuống
    
    # 4. Limit năng lượng phiên
    session_energy = energy - energy_start
    if session_energy >= energy_limit_Wh:
        alarm = ENERGY_LIMIT
        return
    
    # 5. Dòng thấp (xe đầy pin) < 0.5A liên tục 60s
    if current < 50:    # 0.50A
        if not lowcurr_timing:
            lowcurr_timing = True
            lowcurr_start = tick
        elif tick - lowcurr_start >= 60000:
            alarm = LOW_CURRENT
            return
    else:
        lowcurr_timing = False
    
    alarm = OK
```

> **Lưu ý logic session:** Khi `Meter_StartSession()` được gọi, `energy_start = energy`. Session energy = energy - energy_start.

---

## 9. Module Đầu vào số (digital_input)

### Hằng số
```
DI_DEBOUNCE_MS = 50    # Debounce 50ms
```

### Logic Cửa (Door Sensor)
```python
# Phần cứng: Input Pullup → LOW = cửa mở, HIGH = cửa đóng
class DigitalInput:
    door_open = False           # Kết quả debounced
    door_raw_last = HIGH        # trạng thái raw trước đó
    door_debouncing = False
    door_change_tick = 0
    modbus_addr = 0             # Đọc 1 lần từ DIP switch khi boot
    
    def process():
        raw = hw.door_sensor    # True=HIGH=đóng, False=LOW=mở
        if raw != door_raw_last:
            door_raw_last = raw
            door_change_tick = tick
            door_debouncing = True
        
        if door_debouncing and (tick - door_change_tick >= 50):
            door_open = (door_raw_last == LOW)   # LOW = mở
            door_debouncing = False
```

### Logic Tamper (Phá hoại)
```python
def is_tamper():
    # Phá hoại = Khóa ĐANG CÀI (relay OFF) + Cửa MỞ
    return (doorlock_relay == OFF) and door_open
```

> **Giải thích:** Nếu cửa mở mà khóa vẫn cài → ai đó phá khóa vật lý → TAMPER → ERROR ngay lập tức.

---

## 10. Module Khóa cửa (door_lock)

### Hằng số
```
DOOR_LOCK_PULSE_MS = 5000   # Mở khóa 5 giây rồi tự đóng
```

### Logic
```python
class DoorLock:
    unlocked = False
    unlock_tick = 0
    
    def unlock():
        unlocked = True
        unlock_tick = tick
        hw.relay_doorlock = ON    # Mở khóa solenoid
    
    def process():
        if unlocked:
            if tick - unlock_tick >= 5000:
                unlocked = False
                hw.relay_doorlock = OFF   # Tự đóng khóa
    
    def is_unlocked():
        return unlocked
```

> **Lưu ý:** Chỉ mở khóa khi state != ERROR. Lệnh mở đến từ Modbus Coil 0x0002.

---

## 11. Module Chẩn đoán (diagnostics)

### Logic
```python
class Diagnostics:
    uptime_sec = 0
    heartbeat = 0           # Tăng +1 mỗi giây, Master đọc liên tục
    last_sec_tick = 0
    temp_min = 32767        # Nhiệt thấp nhất
    temp_max = -32768       # Nhiệt cao nhất
    error_count = 0         # Đếm lỗi session (RAM)
    charge_count = 0        # Đếm phiên sạc session (RAM)
    dlt645_ok = 0           # Frame DLT645 thành công
    dlt645_fail = 0         # Frame DLT645 thất bại
    alarm_flags = 0         # Bitmask 16-bit tổng hợp
    
    def process():
        if tick - last_sec_tick >= 1000:
            elapsed = (tick - last_sec_tick) / 1000
            uptime_sec += elapsed
            heartbeat += elapsed
            last_sec_tick += elapsed * 1000
    
    def update_temp(temp):
        if temp < temp_min: temp_min = temp
        if temp > temp_max: temp_max = temp
```

---

## 12. Module Nhật ký lỗi (error_log)

### Cấu trúc Flash Persistent
```python
# Header (32 bytes) — sống sót qua mất nguồn
class PersistentHeader:
    magic = 0x54525531       # "TRU1" 
    boot_count = 0           # +1 mỗi lần khởi động
    total_error_count = 0    # Tổng lỗi vĩnh viễn
    total_charge_count = 0   # Tổng phiên sạc vĩnh viễn
    total_energy_wh = 0      # Tổng kWh vĩnh viễn
    next_event_id = 1        # ID sự kiện tiếp theo
    log_index = 0            # Con trỏ circular

# Event (12 bytes) — circular buffer 80 events
class ErrorEvent:
    event_id: uint16         # ID tuần tự
    error_type: uint8        # ERR_OVERTEMP=1, ERR_TAMPER=2, ...
    state_when: uint8        # FSM state khi xảy ra
    uptime: uint32           # Giây
    temp: int16              # 0.1°C
    power: uint16            # W
```

### Error Type Enum
```
ERR_OVERTEMP        = 1    # >70°C
ERR_TAMPER          = 2    # Phá hoại
ERR_OVERPOWER       = 3    # Quá CS tức thì
ERR_OVERLOAD        = 4    # Quá tải 30s
ERR_VOLTAGE         = 5    # Ngoài 180-260V
ERR_COMM_FAIL       = 6    # Mất DLT645 hoặc Master
ERR_DOOR_CHARGING   = 7    # Cửa mở khi sạc
ERR_GROUND_FAULT    = 8    # Rò dòng (placeholder)
ERR_CONNECTOR_FAULT = 9    # Đầu cắm (placeholder)
ERR_OVERCURRENT     = 10   # Vượt current_limit +10%
```

> **Gợi ý Simulator:** Bạn có thể giả lập persistent bằng file JSON/pickle trên máy.

---

## 13. Giao thức Modbus Slave

### Cấu hình

```
Baudrate: 9600
Data: 8-N-1
Slave Address: Từ DIP switch (1-7)
Silence timeout: 200ms (phát hiện kết thúc frame)
CRC: CRC16 Modbus (polynomial 0xA001, init 0xFFFF)
```

### 13.1. FC04: Read Input Registers (Read-Only, 45 registers)

| Addr | Tên | Đơn vị | Nguồn dữ liệu |
|------|------|--------|---------------|
| 0x0000 | voltage | 0.1V | DLT645 → Meter_GetVoltage() |
| 0x0001 | current | 0.01A | DLT645 → Meter_GetCurrent() |
| 0x0002 | power | W | DLT645 → Meter_GetPower() |
| 0x0003 | energy_hi | Wh | (Meter_GetEnergy() >> 16) |
| 0x0004 | energy_lo | Wh | (Meter_GetEnergy() & 0xFFFF) |
| 0x0005 | temperature | 0.1°C | NTC_GetTempC() |
| 0x0006 | fsm_state | enum | App_GetState() [0-5] |
| 0x0007 | relay_status | bitmask | bit0=Charger, bit1=Socket, bit2=Fan, bit3=DoorLock |
| 0x0008 | led_status | bitmask | (reserved → 0) |
| 0x0009 | meter_alarm | enum | Meter_GetAlarm() [0-5] |
| 0x000A | door_open | bool | DI_IsDoorOpen() |
| 0x000B | fan_state | bool | Fan_GetState() |
| 0x000C | session_energy | Wh | Meter_GetSessionEnergy() |
| 0x000D | fw_version | hex | 0x0100 = v1.0 (cố định) |
| 0x000E | slave_addr | - | Từ DIP switch |
| 0x000F | uptime_hi | s | (Diag_GetUptime() >> 16) |
| 0x0010 | uptime_lo | s | (Diag_GetUptime() & 0xFFFF) |
| 0x0011 | heartbeat | - | Diag_GetHeartbeat() +1/s |
| 0x0012 | temp_min | 0.1°C | Nhiệt thấp nhất từ boot |
| 0x0013 | temp_max | 0.1°C | Nhiệt cao nhất từ boot |
| 0x0014 | error_count | - | Đếm lỗi session (RAM) |
| 0x0015 | charge_count | - | Đếm phiên sạc (RAM) |
| 0x0016 | dlt645_ok | - | Frame DLT645 OK |
| 0x0017 | dlt645_fail | - | Frame DLT645 fail |
| 0x0018 | alarm_flags | bitmask | Tổng hợp 8 bit alarm |
| 0x0019 | session_duration | s | Thời gian phiên sạc |
| 0x001A | meter_valid | bool | 1=data tươi, 0=stale |
| 0x001B | boot_count | - | Flash persistent |
| 0x001C | total_error_count | - | Flash persistent |
| 0x001D | total_charge_count | - | Flash persistent |
| 0x001E | total_energy_hi | Wh | Flash (high) |
| 0x001F | total_energy_lo | Wh | Flash (low) |
| 0x0020 | last_error_id | - | Flash |
| 0x0021 | last_error_type | enum | Flash |
| 0x0022 | master_alive | enum | 0=no HB, 1=alive, 2=timeout |
| 0x0023 | meter_serial_1 | BCD | (placeholder → 0) |
| 0x0024 | meter_serial_2 | BCD | (placeholder → 0) |
| 0x0025 | meter_serial_3 | BCD | (placeholder → 0) |
| 0x0026 | frequency | 0.01Hz | (placeholder → 0xFFFF) |
| 0x0027 | power_factor | 0.001 | (placeholder → 0xFFFF) |
| 0x0028 | current_rms_raw | 0.01A | Meter_GetCurrent() |
| 0x0029 | session_id | - | App_GetSessionId() |
| 0x002A | last_stop_reason | enum | App_GetLastStopReason() |
| 0x002B | connector_status | enum | (placeholder → 0xFFFF) |
| 0x002C | ground_fault | bool | (placeholder → 0) |

### 13.2. FC02: Discrete Inputs (Read-Only, 1-bit)

| Addr | Tên | Logic |
|------|------|-------|
| 0x0000 | door_open | DI_IsDoorOpen() |
| 0x0001 | is_charging | state == CHARGING |
| 0x0002 | is_error | state == ERROR |
| 0x0003 | fan_running | Fan_GetState() |
| 0x0004 | door_unlocked | DoorLock_IsUnlocked() |
| 0x0005 | tamper | DI_IsTamper() |
| 0x0006 | connector_present | (placeholder → 0) |
| 0x0007 | ground_fault | (placeholder → 0) |

### 13.3. FC05: Write Single Coil (Trigger)

Ghi **0xFF00** để kích hoạt. Coil tự reset (đọc lại = 0).

| Addr | Tên | Hành vi |
|------|------|---------|
| 0x0000 | start_charge | → App_TriggerStartCharge() |
| 0x0001 | stop_charge | → App_TriggerStopCharge() |
| 0x0002 | unlock_door | → App_TriggerUnlockDoor() |
| 0x0003 | clear_error | → App_TriggerClearError() |
| 0x0004 | standby | → App_TriggerStandby() |
| 0x0005 | force_fan_on | → App_ForceFanOn() / FanOff() |
| 0x0006 | enter_fw_update | NOT IMPLEMENTED (exception) |

### 13.4. FC03/06/16: Holding Registers (Read/Write Config)

| Addr | Tên | Default | Đơn vị | Side-effect |
|------|------|---------|--------|-------------|
| 0x0100 | fan_high_temp | 450 | 0.1°C | Fan_SetThresholds() |
| 0x0101 | fan_low_temp | 380 | 0.1°C | Fan_SetThresholds() |
| 0x0102 | max_power | 7000 | W | Meter_SetConfig() |
| 0x0103 | rated_power | 3500 | W | Meter_SetConfig() |
| 0x0104 | energy_limit_hi | 0 | Wh | Meter_SetConfig() |
| 0x0105 | energy_limit_lo | 20000 | Wh | Meter_SetConfig() |
| 0x0106 | overtemp_limit | 700 | 0.1°C | (chưa apply runtime) |
| 0x0107 | lock_pulse_ms | 5000 | ms | (chưa apply runtime) |
| 0x0108 | comm_timeout_s | 30 | s | (chưa apply runtime) |
| 0x0109 | **master_heartbeat** | 0 | - | **Reset HB watchdog, master_alive=1** |
| 0x010A | current_limit | 3200 | 0.01A | App_SetCurrentLimit() |
| 0x010B | session_energy_limit | 0 | Wh | App_SetSessionEnergyLimit() |
| 0x010C | time_sync_hi | 0 | - | App_SetUnixTimestamp() |
| 0x010D | time_sync_lo | 0 | - | App_SetUnixTimestamp() |

### 13.5. Master Heartbeat Watchdog

```
MASTER_HB_TIMEOUT_MS = 10000   # 10 giây

Master ghi HR 0x0109 mỗi 3-5s → master_alive = 1, reset timer

Nếu master_alive==1 và >10s không ghi:
  → master_alive = 2 (TIMEOUT)
  → App_TriggerError()
  → ErrLog_RecordError(ERR_COMM_FAIL)
  → alarm_flags |= COMM_FAIL

Recovery:
  → Master ghi lại HR 0x0109 → master_alive = 1
  → Master ghi Coil 0x0003 (clear_error) → FSM reset
```

### 13.6. CRC16 Modbus

```python
def modbus_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

# CRC append: [crc_lo, crc_hi] (little-endian)
```

### 13.7. Frame Format

```
Request:  [ADDR] [FC] [DATA...] [CRC_LO] [CRC_HI]
Response: [ADDR] [FC] [DATA...] [CRC_LO] [CRC_HI]
Exception: [ADDR] [FC|0x80] [EX_CODE] [CRC_LO] [CRC_HI]

FC04 Request:  [ADDR][04][START_HI][START_LO][QTY_HI][QTY_LO][CRC_LO][CRC_HI]
FC04 Response: [ADDR][04][BYTE_CNT][REG1_HI][REG1_LO]...[CRC_LO][CRC_HI]

FC05 Request:  [ADDR][05][COIL_HI][COIL_LO][0xFF][0x00][CRC_LO][CRC_HI]
FC05 Response: Echo request

FC06 Request:  [ADDR][06][REG_HI][REG_LO][VAL_HI][VAL_LO][CRC_LO][CRC_HI]
FC06 Response: Echo request
```

---

## 14. Bảng Alarm Flags (bitmask 16-bit)

Register `0x0018` tổng hợp mọi cảnh báo:

| Bit | Tên | Điều kiện kích |
|-----|------|---------------|
| 0 | OVERTEMP | temp > 700 (70.0°C) |
| 1 | DOOR_OPEN | DI_IsDoorOpen() == true |
| 2 | TAMPER | Khóa cài + cửa mở |
| 3 | OVERPOWER | meter_alarm == OVERPOWER or OVERLOAD |
| 4 | VOLTAGE_FAULT | meter_alarm == VOLTAGE_FAULT |
| 5 | ENERGY_LIMIT | meter_alarm == ENERGY_LIMIT |
| 6 | LOW_CURRENT | meter_alarm == LOW_CURRENT |
| 7 | COMM_FAIL | DLT645 không phản hồi >30s hoặc Master HB timeout |
| 8 | OVERCURRENT | Dòng > 110% current_limit (Rev 2.0) |

---

## 15. Kịch bản kiểm thử

### Test 1: Boot → IDLE
```
1. Khởi tạo (state=INIT)
2. App_Init() → state=IDLE
3. Kiểm tra: LED White=SOLID, tất cả Relay=OFF
4. Chạy 1s
5. Assert: state==IDLE, heartbeat==1, uptime==1
```

### Test 2: Door Open → ERROR → Close → Clear → IDLE
```
1. Từ IDLE, mở cửa (door_sensor = LOW)
2. Chạy 100ms (debounce)
3. Assert: state==ERROR, LED Red=SOLID
4. Đóng cửa (door_sensor = HIGH)
5. Chạy 100ms
6. Gửi Coil 0x0003 (clear_error)
7. Assert: state==IDLE, LED White=SOLID
```

### Test 3: Full Charge Cycle
```
1. Từ IDLE, set voltage=220V, power=3500W, current=15A
2. Gửi Coil 0x0004 (standby)
3. Assert: state==STANDBY, LED Green=BLINK
4. Gửi Coil 0x0000 (start_charge)
5. Assert: state==CHARGING, LED Red=BLINK, relay_charger=ON
6. Chạy 5s (session đang tích lũy energy)
7. Gửi Coil 0x0001 (stop_charge)
8. Assert: state==FINISH, LED Green=SOLID, relay_charger=OFF
9. charge_count == 1
```

### Test 4: TAMPER Detection
```
1. Từ IDLE, khóa cửa = CÀI (mặc định, doorlock relay=OFF)
2. Mở cửa (door_sensor=LOW)
3. Chạy 100ms
4. Assert: is_tamper()==True, state==ERROR, alarm_flags bit2 set
```

### Test 5: Overtemp → Fan + ERROR
```
1. Từ IDLE, set temp=46°C (460)
2. Chạy 1s
3. Assert: fan=ON
4. Set temp=72°C (720)
5. Chạy 1s
6. Assert: state==ERROR, alarm_flags bit0 set
7. Hạ temp=35°C (350), chạy 35s (fan min ON time)
8. Clear error
9. Assert: state==IDLE, fan=OFF (vì temp < 38°C)
```

### Test 6: Overpower → ERROR
```
1. Từ CHARGING (power=3500W)
2. Set power=8000W (>7000 max)
3. Chạy 100ms
4. Assert: state==ERROR, meter_alarm==OVERPOWER
```

### Test 7: Low Current → FINISH (xe đầy)
```
1. Từ CHARGING (current=15A)
2. Set current=30 (0.30A < 0.50A)
3. Chạy 59s
4. Assert: state==CHARGING (chưa đủ 60s)
5. Chạy thêm 2s
6. Assert: state==FINISH, stop_reason=FINISHED_AUTO
```

### Test 8: Master Heartbeat Timeout
```
1. Từ IDLE
2. Ghi HR 0x0109 (heartbeat) → master_alive=1
3. Chạy 5s → master_alive=1
4. KHÔNG ghi thêm, chạy 11s
5. Assert: master_alive=2, state==ERROR, alarm_flags bit7 set
```

### Test 9: Voltage Fault
```
1. Từ CHARGING
2. Set voltage=160V (1600) → dưới 180V
3. Chạy 100ms
4. Assert: state==ERROR, meter_alarm==VOLTAGE_FAULT
```

### Test 10: Overload 30s
```
1. Từ CHARGING, rated_power=3500
2. Set power=4000W (>3850 = 110% rated)
3. Chạy 29s
4. Assert: state==CHARGING (chưa đủ 30s)
5. Chạy thêm 2s
6. Assert: state==ERROR, meter_alarm==OVERLOAD
```

---

## 16. Giao tiếp ESP32 Master (Context tổng)

> Phần này cung cấp bối cảnh để bạn hiểu "ai nói chuyện với Slave và bằng gì".

### ESP32 Master sẽ:

1. **Polling FC04** mỗi ~1s: Đọc 45 Input Registers (0x0000-0x002C) từ mỗi Slave.
2. **Heartbeat FC06** mỗi ~3s: Ghi HR 0x0109 giá trị bất kỳ.
3. **Ghi Coil FC05** khi nhận lệnh từ Server: start(0x0000), stop(0x0001), clear(0x0003)...
4. **Ghi HR FC06/FC16** để thay đổi config: max_power, energy_limit, current_limit...
5. **Chuyển đổi dữ liệu Modbus → MQTT JSON** rồi publish lên Broker.

### Mapping Modbus → MQTT (cách Master dùng data)

```
Input Reg 0x0000 (voltage=2200)   → MQTT TLM: "voltage": 220.0    (÷10)
Input Reg 0x0001 (current=1650)   → MQTT TLM: "current": 16.50    (÷100)
Input Reg 0x0002 (power=3630)     → MQTT TLM: "power": 3630       (nguyên)
Input Reg 0x0003+0x0004 (energy)  → MQTT TLM: "energy_total": 121.250 (÷1000 → kWh)
Input Reg 0x0005 (temp=455)       → MQTT TLM: "temperature": 45.5  (÷10)
Input Reg 0x0006 (fsm_state=3)    → MQTT TLM: "state": 3
Input Reg 0x000C (session_e=700)  → MQTT TLM: "energy_session": 0.700 (÷1000 → kWh)
```

### StopReason Enum (cho MQTT `reason` field)

```
0 = REASON_UNKNOWN
1 = FINISHED_AUTO         # Xe đầy / energy limit
2 = REMOTE_STOP_USER      # User nhấn Stop qua App
3 = REMOTE_STOP_OUT_OF_COIN  # Hết tiền
4 = SAFETY_ALARM_STOP     # Lỗi phần cứng
5 = SESSION_ENERGY_EXCEEDED  # Vượt session limit
6 = OVERCURRENT_STOP      # Dòng vượt +10%
```

---

## PHỤ LỤC: Tham khảo nhanh cho Coder giả lập

### Các biến state tối thiểu cần trong Simulator

```python
# Clock
tick = 0                        # +1 mỗi ms

# FSM
state = IDLE                    # enum 0-5
state_enter_tick = 0

# Session
session_id = 0                  # ++ mỗi lần start_charge
charge_start_tick = 0
last_stop_reason = 0
current_limit_001A = 3200       # 32.00A default
session_energy_limit_wh = 0     # 0 = disabled

# Door
door_open = False               # debounced, True=mở
door_debounce_timer = 0

# Fan
fan_on = False
fan_on_tick = 0

# Meter
voltage = 2200
current = 0
power = 0
energy = 0
energy_start = 0                # set khi start session
meter_alarm = OK
meter_valid = True
overload_timing = False
overload_start = 0
lowcurr_timing = False
lowcurr_start = 0

# Diagnostics
uptime = 0
heartbeat = 0
temp_min = 32767
temp_max = -32768
error_count = 0
charge_count = 0
alarm_flags = 0

# Master Heartbeat
master_alive = 0                # 0=noHB, 1=alive, 2=timeout
master_last_hb_tick = 0

# Relay (target vs actual, interlock logic)
relay_target = [OFF, OFF, OFF, OFF]
relay_actual = [OFF, OFF, OFF, OFF]

# LED
led_mode = [OFF, OFF, OFF]      # RED, GREEN, WHITE → OFF/SOLID/BLINK
blink_state = False
last_blink_tick = 0

# Persistent (simulate with file)
boot_count = 0
total_errors = 0
total_charges = 0
total_energy = 0
```

### Thứ tự gọi OK cho mỗi tick (1ms)

```python
while running:
    tick += 1
    led_process()
    relay_process()
    ntc_process()           # hoặc skip, set temp trực tiếp
    fan_process(temp)
    meter_process()
    di_process()            # debounce door
    doorlock_process()
    diag_process()
    build_alarm_flags()
    fsm_process()           # switch case theo state
```

---

**Chúc bạn viết Simulator thành công! 🚀**
