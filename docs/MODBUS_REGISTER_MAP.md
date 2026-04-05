# TRU V1.0 — Modbus Register Map (Rev 2.0)

## Thông tin kết nối
- **Baudrate:** 9600  
- **Parity:** None  
- **Data bits:** 8  
- **Stop bits:** 1  
- **Slave addr:** Từ DIP switch (mặc định 1)

---

## Quy ước FSM State (CHUẨN DUY NHẤT)

> [!IMPORTANT]
> Bảng enum này là **nguồn sự thật duy nhất** cho toàn bộ hệ thống (STM32, ESP32, Server MQTT). Mọi document khác phải tham chiếu bảng này.

| Giá trị | Tên State | Mô tả |
|---------|-----------|-------|
| 0 | `INIT` | Khởi tạo hệ thống, chưa sẵn sàng |
| 1 | `IDLE` | Rảnh, sẵn sàng nhận lệnh sạc |
| 2 | `STANDBY` | Chờ cắm súng sạc / xác nhận |
| 3 | `CHARGING` | Relay đóng, đang cấp nguồn |
| 4 | `FINISH` | Hoàn tất phiên sạc, chờ phản hồi |
| 5 | `ERROR` | Lỗi khẩn cấp (HW/SW/Comm) |
| 255 | `OFFLINE` | *(Chỉ phía Gateway)* Mất kết nối Modbus với Slave |

---

## FC04: Input Registers (Read-Only)

| Addr | Tên | Đơn vị | Mô tả |
|------|------|--------|--------|
| 0x0000 | voltage | 0.1V | Điện áp AC (220.0V = 2200) |
| 0x0001 | current | 0.01A | Dòng điện (16.50A = 1650) |
| 0x0002 | power | W | Công suất tức thì |
| 0x0003 | energy_hi | Wh | Năng lượng tổng (high 16-bit) |
| 0x0004 | energy_lo | Wh | Năng lượng tổng (low 16-bit) |
| 0x0005 | temperature | 0.1°C | NTC nhiệt độ (72.0°C = 720) |
| 0x0006 | fsm_state | enum | Xem bảng **Quy ước FSM State** ở trên |
| 0x0007 | relay_status | bitmask | bit0=RL_CHARGER, bit1=RL_SOCKET, bit2=FAN, bit3=DOORLOCK |
| 0x0008 | led_status | bitmask | (reserved) |
| 0x0009 | meter_alarm | enum | 0=OK, 1=OVERPOWER, 2=OVERLOAD, 3=ENERGY_LIMIT, 4=LOW_CURRENT, 5=VOLTAGE_FAULT |
| 0x000A | door_open | bool | 1=mở, 0=đóng |
| 0x000B | fan_state | bool | 1=chạy, 0=dừng |
| 0x000C | session_energy | Wh | Năng lượng phiên hiện tại |
| 0x000D | fw_version | hex | 0x0100 = v1.0 |
| 0x000E | slave_addr | - | Địa chỉ Modbus |
| 0x000F | uptime_hi | s | Uptime (high 16-bit) |
| 0x0010 | uptime_lo | s | Uptime (low 16-bit) |
| 0x0011 | heartbeat | - | Tăng +1/s, bằng nhau 2 lần = board treo |
| 0x0012 | temp_min | 0.1°C | Nhiệt độ thấp nhất từng ghi |
| 0x0013 | temp_max | 0.1°C | Nhiệt độ cao nhất từng ghi |
| 0x0014 | error_count | - | Số lỗi session (RAM) |
| 0x0015 | charge_count | - | Số phiên sạc session (RAM) |
| 0x0016 | dlt645_ok | - | Đếm frame DLT645 thành công |
| 0x0017 | dlt645_fail | - | Đếm frame DLT645 thất bại |
| 0x0018 | alarm_flags | bitmask | bit0=OVERTEMP, bit1=DOOR, bit2=TAMPER, bit3=OVERPOWER, bit4=VOLTAGE, bit5=ENERGY, bit6=LOW_CURRENT, bit7=COMM_FAIL, bit8=GROUND_FAULT, bit9=CONNECTOR, bit10=OVERCURRENT |
| 0x0019 | session_duration | s | Thời gian phiên sạc |
| 0x001A | meter_valid | bool | 1=data tươi, 0=stale (mất DLT645 >10s) |
| 0x001B | boot_count | - | Tổng lần khởi động (**Flash persistent**) |
| 0x001C | total_error_count | - | Tổng lỗi vĩnh viễn (**Flash**) |
| 0x001D | total_charge_count | - | Tổng phiên sạc vĩnh viễn (**Flash**) |
| 0x001E | total_energy_hi | Wh | Tổng energy vĩnh viễn (high) (**Flash**) |
| 0x001F | total_energy_lo | Wh | Tổng energy vĩnh viễn (low) (**Flash**) |
| 0x0020 | last_error_id | - | ID event lỗi gần nhất (**Flash**) |
| 0x0021 | last_error_type | enum | Loại lỗi gần nhất (bảng error_type) |
| 0x0022 | master_alive | enum | 0=chưa HB, 1=alive, 2=timeout (>10s) |
| 0x0023 | meter_serial_1 | BCD | Số serial (chữ số 1-4, vd 0x0123) |
| 0x0024 | meter_serial_2 | BCD | Số serial (chữ số 5-8, vd 0x4567) |
| 0x0025 | meter_serial_3 | BCD | Số serial (chữ số 9-12, vd 0x8912) |
| **0x0026** | **frequency** | **0.01Hz** | **Tần số lưới điện (50.00Hz = 5000)** |
| **0x0027** | **power_factor** | **0.001** | **Hệ số công suất (PF=0.95 → 950). Quan trọng cho billing.** |
| **0x0028** | **current_rms_raw** | **0.001A** | **Dòng RMS gốc ADC. Phát hiện rò điện khi IDLE.** |
| **0x0029** | **session_id** | **-** | **ID phiên sạc (tự tăng). Liên kết START↔STOP trên Server.** |
| **0x002A** | **last_stop_reason** | **enum** | **Lý do dừng sạc gần nhất (bảng reason enum).** |
| **0x002B** | **connector_status** | **enum** | **0=Unplugged, 1=Plugged, 2=Locked.** |
| **0x002C** | **ground_fault** | **bool** | **1=phát hiện rò điện ra vỏ (IEC 61851).** |

> Tổng: 45 Input Registers (0x0000 → 0x002C). Master đọc FC04 từ 0x0000, num_regs = 45.

---

## FC02: Discrete Inputs (Read-Only, 1-bit)

| Addr | Tên | Mô tả |
|------|------|--------|
| 0x0000 | door_open | 1=cửa mở |
| 0x0001 | is_charging | 1=đang sạc |
| 0x0002 | is_error | 1=ERROR state |
| 0x0003 | fan_running | 1=quạt chạy |
| 0x0004 | door_unlocked | 1=khóa mở |
| 0x0005 | tamper | 1=phá hoại |
| **0x0006** | **connector_plugged** | **1=đầu sạc đã cắm** |
| **0x0007** | **ground_fault_active** | **1=rò dòng ra vỏ** |

---

## FC05: Coils (Write-Only, Trigger)

Ghi 0xFF00 để kích hoạt (tự reset → 0).

| Addr | Tên | Mô tả |
|------|------|--------|
| 0x0000 | start_charge | Bắt đầu sạc |
| 0x0001 | stop_charge | Dừng sạc |
| 0x0002 | unlock_door | Mở khóa cửa |
| 0x0003 | clear_error | Xóa lỗi |
| 0x0004 | standby | Chuyển STANDBY |
| **0x0005** | **force_fan_on** | **Bật quạt bắt buộc (bảo trì/test)** |
| **0x0006** | **enter_fw_update** | **Đưa STM32 vào chế độ Bootloader DFU qua RS485** |

---

## FC03/06/16: Holding Registers (Read/Write)

| Addr | Tên | Default | Đơn vị | Mô tả |
|------|------|---------|--------|--------|
| 0x0100 | fan_high_temp | 450 | 0.1°C | Nhiệt bật quạt (45.0°C) |
| 0x0101 | fan_low_temp | 380 | 0.1°C | Nhiệt tắt quạt (38.0°C) |
| 0x0102 | max_power | 7000 | W | Công suất max tức thì |
| 0x0103 | rated_power | 3500 | W | Công suất định mức |
| 0x0104 | energy_limit_hi | 0 | Wh | Giới hạn energy (high) |
| 0x0105 | energy_limit_lo | 20000 | Wh | Giới hạn energy (low) |
| 0x0106 | overtemp_limit | 700 | 0.1°C | Ngưỡng quá nhiệt (70.0°C) |
| 0x0107 | lock_pulse_ms | 5000 | ms | Thời gian mở khóa |
| 0x0108 | comm_timeout_s | 30 | s | Timeout DLT645 |
| 0x0109 | **master_heartbeat** | 0 | - | **Master ghi mỗi 3-5s, timeout 10s → ERROR** |
| **0x010A** | **current_limit** | **3200** | **0.01A** | **Dynamic Load Balancing: Gateway set dòng tối đa cho từng trạm (32.00A default).** |
| **0x010B** | **session_energy_limit** | **0** | **Wh** | **Giới hạn kWh/phiên (0=disable). Server set trước start_charge.** |
| **0x010C** | **time_sync_hi** | **0** | **-** | **Unix timestamp high 16-bit. Gateway đẩy xuống Slave định kỳ.** |
| **0x010D** | **time_sync_lo** | **0** | **-** | **Unix timestamp low 16-bit.** |

---

## Error Types (cho register 0x0021)

| ID | Tên | Mô tả |
|----|------|--------|
| 1 | ERR_OVERTEMP | >70°C |
| 2 | ERR_TAMPER | Phá hoại |
| 3 | ERR_OVERPOWER | Quá CS tức thì |
| 4 | ERR_OVERLOAD | Quá tải 30s |
| 5 | ERR_VOLTAGE | Ngoài 180-260V |
| 6 | ERR_COMM_FAIL | Mất DLT645 hoặc Master |
| 7 | ERR_DOOR_CHARGING | Cửa mở khi sạc |
| **8** | **ERR_GROUND_FAULT** | **Rò dòng ra vỏ (IEC 61851)** |
| **9** | **ERR_CONNECTOR_FAULT** | **Đầu cắm kẹt hoặc trạng thái bất thường** |
| **10** | **ERR_OVERCURRENT** | **Dòng vượt ngưỡng current_limit** |

---

## Master Heartbeat Protocol

```
Master ghi HR 0x0109 mỗi 3-5s (giá trị bất kỳ)
  → Slave set master_alive=1, reset timer

Nếu >10s không ghi:
  → Slave set master_alive=2 (TIMEOUT)
  → App_TriggerError() → STATE_ERROR → đèn Đỏ + cắt relay
  → ErrLog ghi event (ERR_COMM_FAIL)
  → alarm_flags bit7 = COMM_FAIL

Để recovery:
  → Master ghi lại HR 0x0109 → master_alive=1
  → Master ghi Coil 0x0003 (clear_error) → FSM reset
```

---

## Dynamic Load Balancing Protocol (MỚI)

```
Khi Gateway phát hiện tổng dòng các trạm > ngưỡng nguồn chung:
  → Gateway tính toán current_limit mới cho từng Slave
  → Ghi FC06 vào HR 0x010A cho từng Slave
  → Slave điều chỉnh relay/PWM phù hợp

Slave tự ngắt nếu dòng thực tế > current_limit + 10%:
  → Trigger ERR_OVERCURRENT
  → alarm_flags bit10 = OVERCURRENT
```

---

## Time Sync Protocol (MỚI)

```
Gateway ghi FC16 (Multiple Registers) vào HR 0x010C-0x010D:
  → time_sync_hi = (uint16_t)(unix_time >> 16)
  → time_sync_lo = (uint16_t)(unix_time & 0xFFFF)
  → Chu kỳ: mỗi 60 giây hoặc sau khi NTP sync thành công
  → Slave dùng timestamp này cho Flash Event Log
```
