# ĐẶC TẢ CHI TIẾT BOARD MẠCH ĐIỀU KHIỂN TRỤ SẠC
## MODEL: TRU V1.0 | Sơ đồ: TRU_V1.0_18032026

> **Người phê duyệt:** Nguyễn Tuấn Minh  
> **Ngày cập nhật:** 23/03/2026

---

## 1. THÔNG SỐ PHẦN CỨNG & SƠ ĐỒ CHÂN

### 1.1. Vi xử lý — STM32F103C8T6

| Thông số | Giá trị |
|---|---|
| Kiến trúc | ARM® 32-bit Cortex®-M3 |
| Tốc độ tối đa | 72 MHz (Thạch anh 8MHz + PLL) |
| Flash | 64 KB |
| SRAM | 20 KB |
| Điện áp | 2.0V – 3.6V (Ổn áp 3.3V) |
| Đóng gói | LQFP48 |

**Ngoại vi sử dụng:**
- **2x ADC 12-bit:** 1 kênh cho NTC20K (PA0)
- **3x USART:** USART1 → RS485_1, USART2 → RS485_2
- **Timer:** PWM cho LED / Timer ngắt 1ms
- **IWDG:** Watchdog timeout 2 giây

### 1.2. Khối LED trạng thái (RGW)

| Chức năng | Chân MCU | Kiểu | Ghi chú |
|---|---|---|---|
| LED_RED | PA8 | Output Push-Pull | Kích HIGH qua Transistor |
| LED_GREEN | PA11 | Output Push-Pull | Kích HIGH qua Transistor |
| LED_WHITE | PA12 | Output Push-Pull | Kích HIGH qua Transistor |

### 1.3. Khối Relay

| Relay | Chân MCU | Chức năng |
|---|---|---|
| RL1 (Charger) | PB0 | Cấp nguồn bộ sạc |
| RL2 (Socket) | PB1 | Cấp nguồn ổ cắm |
| RL3 (Fan) | PB10 | Điều khiển quạt |
| RL4 (DoorLock) | PB11 | Khóa cửa solenoid 12V (kích 5s mở, mặc định đóng) |

> [!CAUTION]
> **Khóa chéo nguồn (Safety Interlock):** PB0 (RL1) và PB1 (RL2) KHÔNG BAO GIỜ được đồng thời HIGH. Dead-time tối thiểu 100ms khi chuyển đổi.

### 1.4. Giao tiếp & Cảm biến

| Chức năng | Chân MCU | Kiểu | Ghi chú |
|---|---|---|---|
| RS485_1 TX | PA9 | UART1 | Giao tiếp Tủ trung tâm |
| RS485_1 RX | PA10 | UART1 | |
| RS485_1 EN | PA1 | Output | Điều hướng DE/RE |
| RS485_2 TX | PA2 | UART2 | Giao tiếp đồng hồ DLT645 |
| RS485_2 RX | PA3 | UART2 | |
| RS485_2 EN | PA4 | Output | Điều hướng DE/RE |
| NTC_TEMP | PA0 | ADC_IN0 | Cảm biến NTC20K |
| ADD0 | PB5 | Input_Pullup | Địa chỉ Modbus (bit 0) |
| ADD1 | PB6 | Input_Pullup | Địa chỉ Modbus (bit 1) |
| ADD2 | PB7 | Input_Pullup | Địa chỉ Modbus (bit 2) |
| DI1 (Door) | PB12 | Input_Pullup | Giám sát trạng thái cửa |

---

## 2. LOGIC VẬN HÀNH

### 2.1. Bảng trạng thái LED

| Trạng thái | Màu | Hiệu ứng | Mô tả |
|---|---|---|---|
| Idle (Chờ) | Trắng | Sáng liên tục | Hệ thống sẵn sàng |
| Standby (Chờ sạc) | Xanh lá | Nhấp nháy 500ms | Đã nhận lệnh, chờ kết nối xe |
| Charging (Đang sạc) | Đỏ | Nhấp nháy 500ms | Đang cấp nguồn |
| Finish (Sạc xong) | Xanh lá | Sáng liên tục | Chu trình kết thúc |
| Error (Lỗi) | Đỏ | Sáng liên tục | Quá nhiệt / Cửa mở / Lỗi RS485 |

### 2.2. Tính toán nhiệt độ NTC20K

- Mạch: Cầu chia áp với R_up = 10kΩ, Vcc = 3.3V
- **Công thức:**
  ```
  R_NTC = 10000 × ADC_val / (4095 - ADC_val)
  ```
- ADC: 12-bit, thời gian lấy mẫu ≥ 1.5µs

### 2.3. Khóa chéo nguồn (Safety Interlock)

```
if (RL1 == ON) then RL2 = OFF (bắt buộc)
if (RL2 == ON) then RL1 = OFF (bắt buộc)
Khi chuyển đổi: delay tối thiểu 100ms
```

---

## 3. GIAO THỨC MODBUS SLAVE (RS485_1)

**Cấu hình UART:** 9600-8-N-1

> Bảng quy hoạch đầy đủ (Input/Holding/Coil/Discrete) → xem [MODBUS_REGISTER_MAP.md](./MODBUS_REGISTER_MAP.md)

---

## 4. GIAO THỨC ĐỒNG HỒ DLT645-2007 (RS485_2)

- **Cấu hình kết nối**: Point-to-Point (1 đồng hồ duy nhất trên bus RS485_2).
- **Địa chỉ Broadcast**: Sử dụng `AAAAAAAAAAAA` (12 ký tự 'A' HEX) để đọc mà không cần biết địa chỉ thật.
- **Data Identifier (DI)**: Voltage (`00 01 01 02`), Current (`00 01 02 02`), Power (`00 01 03 02`), Energy (`00 00 01 00`).
- **Data Mask**: Khi đóng gói frame phải cộng thêm `0x33` vào byte dữ liệu/DI.

---

## 5. CHỈ DẪN LẬP TRÌNH

| Hạng mục | Cấu hình |
|---|---|
| UART1 | 9600-8-N-1, Master Link (Tủ trung tâm) |
| UART2 | 9600-8-N-1, DLT645 Meter |
| Timer nhấp nháy | Chu kỳ 500ms, Toggle GPIO PA8/PA11 |
| Ngắt UART | Ưu tiên cao, nhận lệnh Modbus tức thời |
| IWDG | Timeout 2 giây, tự reset nếu vòng lặp bị đứng |
