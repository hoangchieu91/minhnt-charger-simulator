# DANH SÁCH TÍNH NĂNG — TRU V1.0

> **Cập nhật:** 23/03/2026

---

## F01 — Điều khiển LED trạng thái (RGW) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Hiển thị trạng thái trụ sạc qua 3 LED: Đỏ, Xanh lá, Trắng |
| GPIO | PA8 (Red), PA11 (Green), PA12 (White) |
| Chế độ | Sáng liên tục / Nhấp nháy 500ms |
| Ưu tiên | **Cao** |
| Status | `led_rgw.c/h` — TDD PASSED |

## F02 — Điều khiển Relay (4 kênh) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Đóng/ngắt 4 relay: Charger, Socket, Fan, Spare |
| GPIO | PB0, PB1, PB10, PB11 |
| An toàn | Khóa chéo RL1/RL2, dead-time 100ms |
| Ưu tiên | **Cao — Safety Critical** |
| Status | `relay_ctrl.c/h` — TDD PASSED |

## F03 — Đo nhiệt độ NTC20K ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Đọc nhiệt độ môi trường/cabinet qua cảm biến NTC20K |
| GPIO | PA0 (ADC_IN0) |
| Công thức | R_NTC = 10000 × ADC / (4095 - ADC), Steinhart-Hart lookup |
| Bộ lọc | Moving Average N=8 chống nhiễu |
| Ngưỡng cảnh báo | >70°C → ERROR (ngắt sạc) |
| Ưu tiên | **Cao** |
| Status | `ntc_temp.c/h` — TDD PASSED |

## F04 — Giao tiếp Modbus Slave (RS485_1)

| Mục | Chi tiết |
|---|---|
| Mô tả | Nhận lệnh từ Tủ trung tâm, trả dữ liệu Voltage/Current/Energy/Temp |
| Giao tiếp | UART1 (PA9/PA10), DE: PA1 |
| Baud rate | 9600-8-N-1 |
| Register | 0x0100–0x0105 |
| Ưu tiên | **Cao** |

## F05 — Giao tiếp đồng hồ DLT645-2007 (RS485_2) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Đọc dữ liệu điện năng từ đồng hồ chuẩn DLT645-2007 (Broadcast Point-to-Point) |
| Giao tiếp | UART2 (PA2/PA3), DE: PA4 |
| Baud rate | 9600-8-E-1 (hoặc theo cấu hình mặc định DLT645) |
| Ưu tiên | **Cao** |
| Status | `dlt645_meter.c/h` — TDD PASSED |

## F06 — Đọc địa chỉ Modbus từ DIP Switch ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Xác định Slave Address qua 3-bit DIP Switch (0–7) |
| GPIO | PB5 (ADD0), PB6 (ADD1), PB7 (ADD2) |
| Logic | Input Pullup, đọc 1 lần khi khởi động |
| Ưu tiên | Trung bình |
| Status | `digital_input.c/h` — TDD PASSED |

## F07 — Giám sát cửa (Door Sensor) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Phát hiện cửa cabinet mở trái phép → cảnh báo Error |
| GPIO | PB12 (Input Pullup) |
| Phản ứng | Debounce 50ms → LED Đỏ liên tục + ngắt toàn bộ Relay |
| Ưu tiên | Trung bình |
| Status | `digital_input.c/h` — TDD PASSED |

## F08 — Watchdog (IWDG)

| Mục | Chi tiết |
|---|---|
| Mô tả | Tự khởi động lại MCU nếu firmware bị treo > 2 giây |
| Cấu hình | IWDG, timeout 2s |
| Ưu tiên | **Cao — Safety Critical** |

## F09 — Máy trạng thái chính (Main State Machine v2) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Quản lý luồng hoạt động: Idle → Standby → Charging → Finish / Error |
| Input | Lệnh Modbus, cảm biến nhiệt, door sensor, meter alarm |
| Output | LED, Relay, Modbus response |
| Logic ngắt sạc | Quá nhiệt→ERROR, Cửa mở→ERROR, Quá công suất→ERROR, Đủ kWh→FINISH, Xe đầy→FINISH |
| Clear Error | Chỉ khi cửa đóng + nhiệt <70°C |
| Ưu tiên | **Cao** |
| Status | `app_main.c/h` — TDD PASSED |

## F10 — Điều khiển quạt thông minh (Fan Hysteresis) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Bật/tắt quạt tản nhiệt (RL3) theo nhiệt độ với vùng trễ |
| Logic | Bật ≥45°C, Tắt ≤38°C, giữ nguyên trong vùng trễ |
| Anti-chatter | Minimum ON time 30s bảo vệ động cơ |
| Set sai ngưỡng | Auto-clamp: gap ≥5°C, sàn 30°C, trần 80°C |
| Status | `fan_ctrl.c/h` — TDD PASSED |

## F11 — Giám sát công suất & năng lượng (Meter Monitor) ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Giám sát V/A/W/kWh từ DLT645 và đưa ra alarm |
| Overpower | >max_power_W → CẮT NGAY |
| Overload | >110% rated liên tục 30s → ERROR |
| Energy limit | consumed ≥ limit_Wh → FINISH (sạc xong) |
| Low current | <0.5A liên tục 60s → FINISH (xe đầy) |
| Voltage fault | <180V hoặc >260V → ERROR (mất pha/quá áp) |
| Status | `meter_monitor.c/h` — TDD PASSED |

## F12 — Web Dashboard Realtime ✅

| Mục | Chi tiết |
|---|---|
| Mô tả | Giao diện trình duyệt hiển thị realtime LED, Relay, Nhiệt độ, Quạt, V/A/W/kWh, Cửa |
| Truy cập | http://localhost:4322/dashboard.html |
| Cập nhật | Polling API mỗi 200ms |
| Status | `dashboard.html` + `sse_bridge.py` |
