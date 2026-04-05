# Renode Debug Cheat Sheet — TRU V1.0

## Khởi động

```bash
cd /home/nxchieu/projects/minhnt_charger
make                         # Cross-compile → charger.elf
renode sim.resc              # Chạy simulation
```

## Macros có sẵn (gõ trong Renode Monitor)

| Macro | Mô tả |
|-------|-------|
| `runMacro openDoor` | Mở cửa cabinet (PB12=LOW) |
| `runMacro closeDoor` | Đóng cửa cabinet (PB12=HIGH) |
| `runMacro showRelays` | In trạng thái 4 relay |
| `runMacro showLeds` | In trạng thái 3 LED |
| `runMacro setAddr` | Set DIP address (đặt `$addr=3` trước) |

## GPIO Manual

### Đọc output (relay/LED)
```
sysbus.gpioPortB.ReadPin 0    # RL_CHARGER
sysbus.gpioPortB.ReadPin 1    # RL_SOCKET
sysbus.gpioPortB.ReadPin 10   # RL_FAN
sysbus.gpioPortB.ReadPin 11   # RL_DOORLOCK
sysbus.gpioPortA.ReadPin 8    # LED_RED
sysbus.gpioPortA.ReadPin 11   # LED_GREEN
sysbus.gpioPortA.ReadPin 12   # LED_WHITE
```

### Ghi input (giả lập sensor)
```
sysbus.gpioPortB.Set 12 false   # Door OPEN
sysbus.gpioPortB.Set 12 true    # Door CLOSE
sysbus.gpioPortB.Set 5 false    # DIP bit0 = ON
sysbus.gpioPortB.Set 5 true     # DIP bit0 = OFF
```

## Điều khiển thời gian

```
pause                           # Dừng MCU
start                           # Tiếp tục
machine.ElapsedVirtualTime      # Xem thời gian ảo
emulation RunFor "00:00:05"     # Chạy đúng 5 giây
emulation RunFor "00:01:00"     # Chạy 1 phút
```

## UART3 Monitor (telemetry)

```bash
# Terminal riêng — xem JSON output
nc localhost 4321
```

## Kịch bản test

### Test 1: Boot → Idle
```
# Sau boot, expect: LED_WHITE=ON, tất cả relay=OFF
runMacro showLeds       # PA12=1 (White ON)
runMacro showRelays     # Tất cả = 0
```

### Test 2: Door open → ERROR
```
runMacro openDoor                    # PB12=LOW
emulation RunFor "00:00:01"          # Đợi 1s (debounce)
runMacro showLeds                    # PA8=1 (Red SOLID)
runMacro showRelays                  # Tất cả relay OFF
runMacro closeDoor                   # Đóng cửa lại
```

### Test 3: TAMPER (khóa cài + cửa mở)
```
# DoorLock mặc định = LOCKED (PB11=LOW)
# Mở cửa khi khóa cài = TAMPER!
runMacro openDoor
emulation RunFor "00:00:01"
# Expect: alarm_flags bit2=1, state=ERROR
```
