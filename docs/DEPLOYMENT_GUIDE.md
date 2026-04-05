# TÀI LIỆU TRIỂN KHAI & BÀN GIAO — TRU V1.0

> **Phiên bản:** 1.0  
> **Ngày:** 23/03/2026  
> **Người lập:** AI Assistant | **Phê duyệt:** Nguyễn Tuấn Minh

---

## 1. TỔNG QUAN HỆ THỐNG

### 1.1. Mô tả

Board TRU V1.0 là bộ điều khiển nhúng dành cho trụ sạc xe điện, sử dụng MCU STM32F103C8T6. Board đảm nhiệm:

- Nhận lệnh từ Tủ trung tâm qua RS485 (Modbus Slave)
- Đọc dữ liệu điện năng từ đồng hồ Psmart qua RS485
- Điều khiển Relay cấp nguồn cho bộ sạc / ổ cắm
- Giám sát nhiệt độ và trạng thái cửa
- Hiển thị trạng thái qua LED RGW

### 1.2. Sơ đồ kết nối hệ thống

```
┌──────────────┐     RS485_1      ┌──────────────┐
│  Tủ trung tâm │◄──────────────►│  Board TRU   │
│  (Master)     │   UART1 9600   │  V1.0        │
└──────────────┘                  │              │
                                  │  STM32F103   │──► LED (R/G/W)
┌──────────────┐     RS485_2      │              │──► Relay (×4)
│  Đồng hồ     │◄──────────────►│              │◄── NTC20K
│  Psmart       │   UART2 9600   │              │◄── Door Sensor
└──────────────┘                  │              │◄── DIP Switch (×3)
                                  └──────────────┘
```

---

## 2. YÊU CẦU THIẾT BỊ TRIỂN KHAI

### 2.1. Công cụ nạp firmware

| Thiết bị | Mô tả |
|---|---|
| ST-Link V2 | Programmer/Debugger SWD |
| STM32CubeProgrammer | Phần mềm nạp firmware (v2.14+) |
| Cáp SWD 4 dây | SWDIO, SWCLK, GND, 3.3V |

### 2.2. Công cụ kiểm tra

| Thiết bị | Mục đích |
|---|---|
| Multimeter | Đo điện áp nguồn, kiểm tra relay |
| USB-RS485 Converter | Test giao tiếp Modbus |
| Phần mềm Modbus Poll | Gửi/nhận frame Modbus |
| Oscilloscope (tùy chọn) | Kiểm tra tín hiệu UART, ripple nguồn |

---

## 3. QUY TRÌNH NẠP FIRMWARE

### Bước 1: Kết nối phần cứng
```
ST-Link V2          Board TRU V1.0
──────────          ──────────────
SWDIO  ──────────►  SWDIO
SWCLK  ──────────►  SWCLK
GND    ──────────►  GND
3.3V   ──────────►  3.3V (hoặc cấp nguồn ngoài)
```

### Bước 2: Nạp firmware
1. Mở **STM32CubeProgrammer**
2. Chọn giao tiếp **SWD**
3. Nhấn **Connect** → MCU phải nhận diện STM32F103C8T6
4. Chọn file firmware `.hex` hoặc `.bin`
5. Nhấn **Download** → chờ hoàn tất
6. Nhấn **Disconnect** → rút cáp SWD

### Bước 3: Xác nhận
- LED Trắng phải sáng liên tục (trạng thái Idle)
- Thử gửi lệnh Modbus qua RS485_1 → phải có phản hồi

---

## 4. QUY TRÌNH LẮP ĐẶT TẠI SITE

### 4.1. Trước khi lắp đặt

1. **Kiểm tra QC:** Đảm bảo board đã pass toàn bộ QC_CHECKLIST
2. **Cài DIP Switch:** Set địa chỉ Modbus phù hợp (0–7)
3. **Kiểm tra firmware:** Phiên bản firmware đúng

### 4.2. Kết nối tại trụ sạc

| Kết nối | Đầu A | Đầu B | Cáp |
|---|---|---|---|
| RS485_1 | Board (A+, B-) | Tủ trung tâm (A+, B-) | Cáp xoắn đôi có chống nhiễu |
| RS485_2 | Board (A+, B-) | Đồng hồ Psmart (A+, B-) | Cáp xoắn đôi |
| NTC20K | Board (PA0, GND) | Cảm biến | Cáp 2 dây |
| Door Sensor | Board (PB12, GND) | Công tắc cửa | Cáp 2 dây |
| Relay Out | Board (RL1–RL4) | Thiết bị lực | Theo sơ đồ lực |
| Nguồn | Board (VIN, GND) | Nguồn DC | Theo spec nguồn |

### 4.3. Kiểm tra sau lắp đặt

1. Cấp nguồn → LED Trắng sáng (Idle) ✓
2. Gửi lệnh Modbus từ Tủ → Board phản hồi ✓
3. Đọc nhiệt độ → giá trị hợp lý (20–40°C) ✓
4. Test đóng/ngắt từng Relay → hoạt động ✓
5. Mở cửa → LED Đỏ sáng (Error) ✓

---

## 5. XỬ LÝ SỰ CỐ

| Sự cố | Nguyên nhân có thể | Xử lý |
|---|---|---|
| Board không lên nguồn | Hở mạch nguồn, IC ổn áp hỏng | Kiểm tra điện áp VIN, 3.3V |
| LED không sáng | Transistor hỏng, firmware lỗi | Đo GPIO, nạp lại firmware |
| RS485 không giao tiếp | Đấu ngược A/B, sai baud rate, chưa có điện trở terminal | Kiểm tra đấu dây, cấu hình |
| Relay không đóng | Driver hỏng, firmware lỗi | Đo tín hiệu GPIO, kiểm tra driver |
| Nhiệt độ đọc sai | NTC đứt/chập, lỗi ADC | Đo điện trở NTC, kiểm tra mạch chia áp |
| Board tự reset | Watchdog timeout, nguồn không ổn | Kiểm tra firmware, đo nguồn |

---

## 6. THÔNG TIN BÀN GIAO

| Hạng mục | Nội dung |
|---|---|
| Phần cứng | Board TRU V1.0 (số lượng: ___) |
| Firmware | File .hex phiên bản: ___ |
| Source code | Repo: ___ (branch: main) |
| Tài liệu | Đặc tả, Task List, Feature List, QC Checklist, Deployment Guide |
| Sơ đồ | TRU_V1.0_18032026 (Altium/KiCad) |
| BOM | File BOM cuối cùng |

> **Bên giao:** ________________ &nbsp;&nbsp; **Ngày:** ___/___/______  
> **Bên nhận:** ________________ &nbsp;&nbsp; **Ngày:** ___/___/______
