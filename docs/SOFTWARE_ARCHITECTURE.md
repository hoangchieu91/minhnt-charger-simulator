# KIẾN TRÚC PHẦN MỀM — TRU V1.0

> **Cập nhật:** 23/03/2026 | **Tác giả:** AI Assistant

Tài liệu này quy chuẩn cấu trúc mã nguồn (Source Code) và nguyên tắc lập trình nhúng cho dự án Board Client TRU V1.0 (STM32F103C8T6), nhằm mục tiêu: **Dễ quản lý, Không dính Spaghetti Code, Đảm bảo an toàn phần cứng**.

---

## 1. PHÂN LỚP KIẾN TRÚC (4 LAYERS)

Dự án được phân chia nghiêm ngặt thành 4 lớp. Lớp trên chỉ gọi hàm của lớp dưới, KHÔNG gọi ngược.

### Layer 1: Hardware Abstraction (Do CubeMX sinh)
- Chứa các driver giao tiếp trực tiếp với thanh ghi (HAL/LL Drivers).
- Nằm trong thư mục `Drivers/`.
- Ít khi đụng vào.

### Layer 2: Board Support Package (BSP) / Core
- Các hàm khởi tạo ngoại vi cụ thể cho board (Clock, GPIO init).
- Nằm trong `Core/Inc` và `Core/Src`.
- Tệp chính: `main.c` (chứa Hardware Init) và `stm32f1xx_it.c` (chứa ISR - Ngắt).

### Layer 3: Middleware & Modules (Logic phần cứng)
- Tách rời hoàn toàn khỏi chip, chỉ nhận đầu vào/đầu ra rõ ràng.
- Nằm trong `Modules/Inc` và `Modules/Src`.
- Bao gồm:
  - `ntc20k.c/h`: Module nội suy ADC ra độ C (Bảng Lookup).
  - `dlt645_meter.c/h`: Giao tiếp đồng hồ chuẩn DLT-645-2007 (Broadcast 1-1).
  - `relay_ctrl.c/h`: Module đóng ngắt Relay (Chứa logic **Safety Interlock** & **Dead-time**).
  - `led_rgw.c/h`: Module xử lý state của LED (Nhấp nháy không blocking).
  - `modbus_slave.c/h`: Thư viện xử lý chuỗi RTU (Checksum, Decode, Encode).

### Layer 4: Application (Logic nguyên lý vận hành)
- Nằm trong `App/Inc` và `App/Src`.
- Bao gồm:
  - `app_main.c`: **Máy trạng thái chính (FSM)** & **Error Monitor**:
    Trạng thái: INIT $\rightarrow$ IDLE $\rightarrow$ STANDBY $\rightarrow$ CHARGING $\rightarrow$ FINISH / ERROR.
    Hàm `App_Process()` quét toàn bộ hệ thống (Quá nhiệt, cửa mở, tamper) ở đầu mỗi vòng lặp để tự kích hoạt cờ lỗi cấp cao nhất.

---

## 2. CÁC NGUYÊN TẮC CODE CHUẨN

1. **KHÔNG dùng `HAL_Delay()` trong Main Loop:** 
   Các task nhấp nháy LED, delay đóng relay phải dùng `HAL_GetTick()` để tạo Non-blocking delay. Chặn hệ thống sẽ làm mất gói tin Modbus.

2. **Khóa chéo Relay ở sâu nhất:**
   Lớp `App` chỉ gửi lệnh "Charging Mode". Mọi thao tác kiểm tra an toàn (bật RL1 thì phải tắt RL2) phải nằm cứng trong hàm của `relay_ctrl.c`.

3. **Ngắt (ISR) phải trống lỏng:**
   Trong ngắt UART chỉ được đẩy data vào Ring Buffer. Việc phân tích Byte nào là Header, Address nằm ở `modbus_slave.c` gọi từ hàm Main Loop.

4. **Watchdog là phòng tuyến cuối:**
   `HAL_IWDG_Refresh()` chỉ được gọi **đúng một lần** ở cuối file `main.c` (chỗ kết thúc tất cả logic). Nếu vòng lặp kẹt ở đâu đó $\rightarrow$ Không feed được dog $\rightarrow$ MCU tự Reset sau 2s.

## 3. CẤU TRÚC THƯ MỤC CHUẨN

```text
minhnt_charger/
├── App/                (Layer 4)
│   ├── Inc/
│   │   └── app_main.h
│   └── Src/
│       └── app_main.c
├── Modules/            (Layer 3)
│   ├── Inc/
│   │   ├── relay_ctrl.h
│   │   ├── led_rgw.h
│   │   ├── ntc20k.h
│   │   ├── ntc20k.h
│   │   ├── modbus_slave.h
│   │   └── dlt645_meter.h
│   └── Src/
│       ├── relay_ctrl.c
│       ├── led_rgw.c
│       ├── ntc20k.c
│       ├── modbus_slave.c
│       └── dlt645_meter.c
├── Core/               (Layer 2)
│   ├── Inc/
│   └── Src/
│       └── main.c      (Entry point, System Clock, CubeMX init)
└── Makefile            (Build system)
```
