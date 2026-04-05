# DANH SÁCH CÔNG VIỆC — DỰ ÁN TRỤ SẠC TRU V1.0

> **Cập nhật:** 23/03/2026 | **PM:** Nguyễn Tuấn Minh

---

## Giai đoạn 1: Thiết kế & Chuẩn bị

- [ ] Hoàn thiện sơ đồ nguyên lý TRU_V1.0_18032026
- [ ] Review sơ đồ nguyên lý (peer review)
- [ ] Thiết kế PCB layout
- [ ] Review PCB layout (DRC/ERC check)
- [ ] Đặt gia công PCB mẫu
- [ ] Lên BOM (Bill of Materials) và đặt linh kiện
- [ ] Chuẩn bị jig test và thiết bị đo lường

## Giai đoạn 2: Phát triển Firmware

- [ ] Khởi tạo project STM32CubeIDE / CubeMX
- [ ] Cấu hình clock (HSE 8MHz → 72MHz PLL)
- [ ] Cấu hình GPIO cho LED (PA8, PA11, PA12)
- [ ] Cấu hình GPIO cho Relay (PB0, PB1, PB10, PB11)
- [ ] Cấu hình GPIO Input Pullup cho DIP Switch (PB5, PB6, PB7)
- [ ] Cấu hình GPIO Input Pullup cho Door Sensor (PB12)
- [ ] Cấu hình ADC (PA0) cho NTC20K
- [ ] Cấu hình UART1 (PA9/PA10) + DE pin (PA1) cho RS485_1
- [ ] Cấu hình UART2 (PA2/PA3) + DE pin (PA4) cho RS485_2
- [ ] Cấu hình Timer 500ms cho LED nhấp nháy
- [ ] Cấu hình IWDG (timeout 2s)
- [x] Implement module LED State Machine
- [x] Implement module Relay Control + Safety Interlock
- [x] Implement module đọc nhiệt độ NTC20K (ADC → R_NTC → °C, Moving Average N=8)
- [x] Implement module đọc địa chỉ Modbus từ DIP Switch + Door Sensor (debounce 50ms)
- [x] Implement module Fan Controller (Hysteresis 45↑/38↓°C, min ON 30s, auto-clamp)
- [x] Implement module Meter Monitor (5 alarm: overpower, overload, energy limit, low current, voltage fault)
- [ ] Implement Modbus Slave stack (RS485_1)
- [x] Implement giao tiếp DLT645-2007 (RS485_2)
- [x] Implement logic chính (Main State Machine v2 — tích hợp NTC/Fan/Meter/Door)
- [x] Implement xử lý lỗi & cảnh báo (Error Handler + logic ngắt nguồn sạc)

## Giai đoạn 2.5: Native TDD & Renode Simulation

- [x] Khởi tạo thư mục `Tests/` và Makefile native
- [x] Viết UnitTest (TDD) cho module Relay Safety Interlock trên Ubuntu x86_64
- [x] Viết UnitTest (TDD) cho module State Machine
- [x] Viết `sim.resc` cho giả lập Renode
- [x] Chạy thử nghiệm nghiệm file .elf trên Renode
- [x] Khởi tạo giao diện đồ họa Web Dashboard Realtime
- [x] Viết UnitTest (TDD) cho NTC, Fan Hysteresis, Meter Monitor, Door Sensor (9/9 PASSED)
- [x] Nâng cấp Dashboard v2: hiển thị nhiệt độ, quạt, công suất, năng lượng, điện áp, dòng điện, cửa

## Giai đoạn 3: Kiểm thử

- [x] Unit test: Đọc ADC & chuyển đổi nhiệt độ (NTC basic + filter)
- [x] Unit test: Khóa chéo Relay (Safety Interlock)
- [x] Unit test: Fan Hysteresis + invalid thresholds
- [x] Unit test: Meter overpower, energy limit, low current, voltage fault
- [x] Unit test: Door debounce
- [ ] Integration test: Modbus Master → Slave giao tiếp
- [ ] Integration test: Đọc đồng hồ Psmart qua RS485_2
- [x] Test quá nhiệt → LED Error + ngắt Relay (via FSM v2)
- [x] Test cửa mở (DI1) → LED Error (via FSM v2)
- [ ] Test Watchdog recovery
- [ ] Test tải thực (Relay + bộ sạc thật)
- [ ] Test liên tục 24h (burn-in test)

## Giai đoạn 4: Triển khai & Bàn giao

- [ ] Hoàn thiện tài liệu đặc tả (bản cuối)
- [ ] Viết hướng dẫn vận hành cho kỹ thuật viên
- [ ] Viết hướng dẫn nạp firmware
- [ ] Đào tạo nhân sự vận hành
- [ ] Lắp đặt thử nghiệm tại site
- [ ] Nghiệm thu & bàn giao
