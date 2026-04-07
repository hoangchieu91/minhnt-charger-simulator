# Minhnt Charger — STM32G030 Firmware

Firmware điều khiển trạm sạc xe điện trên nền tảng **STM32G030C8T6** (Cortex-M0+).

## Tính năng chính
- **Modbus RTU Slave** (COM1 — USART1, 9600 baud): Giao tiếp với ESP32 Gateway
- **DLT645 Meter Polling** (COM2 — USART2, 2400 baud): Đọc đồng hồ điện tự động, non-blocking
- **FSM 5 trạng thái**: IDLE → STANDBY → CHARGING → FINISH → ERROR
- **Bảo vệ đa tầng**: Quá nhiệt, Quá tải, Quá dòng, Tamper, Mất kết nối
- **Dynamic Load Balancing**: Gateway điều chỉnh dòng giới hạn từ xa
- **Session Energy Billing**: Giới hạn số điện theo ví người dùng, hỗ trợ nạp thêm giữa phiên

## Cấu trúc thư mục
```
Core/Src/          — main.c, startup, system clock, IRQ handlers
App/Src/           — app_main.c (FSM logic, session management)
Modules/Src/       — Các module ngoại vi (LED, Relay, NTC, Fan, Modbus, DLT645, Meter...)
Drivers/           — STM32G0xx HAL + CMSIS
docs/              — Tài liệu kỹ thuật (HTML + Markdown)
```

## Build & Flash
```bash
# Build
make clean && make -j4

# Flash qua J-Link GDB
/opt/SEGGER/JLink/JLinkGDBServer -device STM32G030C8 -if SWD -speed 4000 &
gdb-multiarch charger.elf -ex "target remote :2331" -ex "load" -ex "monitor reset" -ex "monitor go"
```

## Tài liệu
- **[Firmware Reference (HTML)](docs/FIRMWARE_REFERENCE_STM32G0.html)** — Kiến trúc, Giải thuật, Register Map (mở bằng trình duyệt)
- **[Firmware Reference (MD)](docs/FIRMWARE_REFERENCE_STM32G0.md)** — Bản Markdown cho AI/developer
- **[Bàn giao 07/04](docs/HANDOVER_20250407.md)** — Tóm tắt phiên migration

## Hardware Pinout
| Chức năng | Chân MCU |
|-----------|----------|
| COM1 TX/RX/DE (Modbus) | PA9 / PA10 / PA1 |
| COM2 TX/RX/DE (DLT645) | PA2 / PA3 / PA4 |
| Relay Charger / Socket | PB0 / PB1 |
| Relay Fan / DoorLock | PB10 / PB11 |
| NTC ADC | PA0 |
| Door Sensor | PB12 |
| LED R/G/W | PA8 / PA11 / PA12 |
