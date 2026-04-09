#!/usr/bin/env python3
"""
TRU V1.0 — Modbus RTU Master Simulator
Giả lập Tủ trung tâm giao tiếp với board TRU V1.0 qua RS485

Sử dụng: python3 modbus_master.py [--port /dev/ttyUSB0] [--addr 1] [--baud 9600]

Yêu cầu: pip install pymodbus pyserial
"""

import argparse
import time
import sys
import os

try:
    from pymodbus.client import ModbusSerialClient
except ImportError:
    print("❌ Cần cài pymodbus: pip install pymodbus pyserial")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════
# REGISTER DEFINITIONS (theo MODBUS_REGISTER_MAP.md)
# ═══════════════════════════════════════════════════════════

STATE_NAMES = {0: "INIT", 1: "IDLE", 2: "STANDBY", 3: "CHARGING", 4: "FINISH", 5: "ERROR"}
ALARM_NAMES = {0: "OK", 1: "OVERPOWER", 2: "OVERLOAD", 3: "ENERGY_LIMIT", 4: "LOW_CURRENT", 5: "VOLTAGE_FAULT"}

ALARM_FLAG_BITS = [
    (0, "OVERTEMP"),
    (1, "DOOR_OPEN"),
    (2, "⚠️TAMPER"),
    (3, "OVERPOWER"),
    (4, "VOLT_FAULT"),
    (5, "ENERGY_LIM"),
    (6, "LOW_CURR"),
    (7, "COMM_FAIL"),
]

# Input Register addresses (FC=04)
IR = {
    0x0000: ("voltage",      "0.1V",  lambda v: f"{v/10:.1f} V"),
    0x0001: ("current",      "0.01A", lambda v: f"{v/100:.2f} A"),
    0x0002: ("power",        "W",     lambda v: f"{v} W"),
    0x0003: ("energy_hi",    "Wh",    lambda v: f"{v}"),
    0x0004: ("energy_lo",    "Wh",    lambda v: f"{v}"),
    0x0005: ("temperature",  "0.1°C", lambda v: f"{v/10:.1f} °C"),
    0x0006: ("system_state", "enum",  lambda v: STATE_NAMES.get(v, f"?{v}")),
    0x0007: ("relay_status", "bits",  lambda v: decode_relay(v)),
    0x0008: ("led_status",   "bits",  lambda v: decode_led(v)),
    0x0009: ("alarm_code",   "enum",  lambda v: ALARM_NAMES.get(v, f"?{v}")),
    0x000A: ("door_sensor",  "bool",  lambda v: "🚪 MỞ" if v else "✅ Đóng"),
    0x000B: ("fan_state",    "bool",  lambda v: "🌀 Chạy" if v else "Tắt"),
    0x000C: ("session_energy","Wh",   lambda v: f"{v} Wh"),
    0x000D: ("fw_version",   "",      lambda v: f"v{(v>>8)&0xFF}.{v&0xFF}"),
    0x000E: ("modbus_addr",  "",      lambda v: f"Addr={v}"),
    0x000F: ("uptime_hi",    "s",     lambda v: f"{v}"),
    0x0010: ("uptime_lo",    "s",     lambda v: f"{v}"),
    0x0011: ("heartbeat",    "",      lambda v: f"{v}"),
    0x0012: ("temp_min",     "0.1°C", lambda v: f"{v/10:.1f} °C"),
    0x0013: ("temp_max",     "0.1°C", lambda v: f"{v/10:.1f} °C"),
    0x0014: ("error_count",  "",      lambda v: f"{v}"),
    0x0015: ("charge_count", "",      lambda v: f"{v}"),
    0x0016: ("dlt645_ok",    "",      lambda v: f"{v}"),
    0x0017: ("dlt645_fail",  "",      lambda v: f"{v}"),
    0x0018: ("alarm_flags",  "bits",  lambda v: decode_alarm_flags(v)),
    0x0019: ("session_dur",  "s",     lambda v: fmt_time(v)),
    0x001A: ("meter_valid",  "bool",  lambda v: "✅ Tươi" if v else "❌ Stale"),
    0x001B: ("boot_count",   "",      lambda v: f"{v}"),
    0x001C: ("tot_err_cnt",  "",      lambda v: f"{v}"),
    0x001D: ("tot_chg_cnt",  "",      lambda v: f"{v}"),
    0x001E: ("tot_egy_hi",   "Wh",    lambda v: f"{v}"),
    0x001F: ("tot_egy_lo",   "Wh",    lambda v: f"{v}"),
    0x0020: ("last_err_id",  "",      lambda v: f"{v}"),
    0x0021: ("last_err_typ", "enum",  lambda v: f"{v}"),
    0x0022: ("master_alive", "enum",  lambda v: {0:"Chưa HB", 1:"Alive", 2:"Timeout"}.get(v, f"?{v}")),
    0x0023: ("meter_serial_1", "BCD", lambda v: f"{v:04X}"),
    0x0024: ("meter_serial_2", "BCD", lambda v: f"{v:04X}"),
    0x0025: ("meter_serial_3", "BCD", lambda v: f"{v:04X}"),
    0x0026: ("frequency",      "Hz",  lambda v: f"{v}"),
    0x0027: ("power_factor",   "%",   lambda v: f"{v}"),
    0x0028: ("current_rms_raw","",    lambda v: f"{v}"),
    0x0029: ("session_id",     "",    lambda v: f"{v}"),
    0x002A: ("last_stop_reason","",   lambda v: f"{v}"),
    0x002B: ("connector_status","",   lambda v: f"{v}"),
    0x002C: ("ground_fault",   "",    lambda v: f"{v}"),
}

# Holding Register addresses (FC=03, base=0x0100)
HR = {
    0x0100: ("fan_high_temp",  "0.1°C", 450,  lambda v: f"{v/10:.1f} °C"),
    0x0101: ("fan_low_temp",   "0.1°C", 380,  lambda v: f"{v/10:.1f} °C"),
    0x0102: ("max_power",      "W",     7000, lambda v: f"{v} W"),
    0x0103: ("rated_power",    "W",     3500, lambda v: f"{v} W"),
    0x0104: ("energy_limit_hi","Wh",    0,    lambda v: f"{v}"),
    0x0105: ("energy_limit_lo","Wh",    20000,lambda v: f"{v} Wh"),
    0x0106: ("overtemp_limit", "0.1°C", 700,  lambda v: f"{v/10:.1f} °C"),
    0x0107: ("lock_pulse_ms",  "ms",    5000, lambda v: f"{v} ms"),
    0x0108: ("comm_timeout_s", "s",     30,   lambda v: f"{v} s"),
    0x0109: ("master_heartbeat","",     0,    lambda v: f"{v}"),
    0x010A: ("current_limit",  "0.01A", 3200, lambda v: f"{v/100:.2f} A"),
    0x010B: ("sess_energy_lim","Wh",    0,    lambda v: f"{v} Wh"),
    0x010C: ("time_sync_hi",   "",      0,    lambda v: f"{v}"),
    0x010D: ("time_sync_lo",   "",      0,    lambda v: f"{v}"),
}

# Coil addresses (FC=05)
COILS = {
    0: "cmd_start_charge",
    1: "cmd_stop_charge",
    2: "cmd_unlock_door",
    3: "cmd_clear_error",
    4: "cmd_standby",
    5: "cmd_force_fan",
    6: "cmd_fw_update",
}

# Discrete Input addresses (FC=02)
DI = {
    0: "di_door_open",
    1: "di_charging",
    2: "di_error",
    3: "di_fan_running",
    4: "di_door_unlocked",
    5: "di_tamper",
    6: "di_connector_plugged",
    7: "di_ground_fault",
}

# ═══════════════════════════════════════════════════════════
# DECODE HELPERS
# ═══════════════════════════════════════════════════════════

def decode_relay(v):
    bits = []
    if v & 1: bits.append("⚡Charger")
    if v & 2: bits.append("🔌Socket")
    if v & 4: bits.append("🌀Fan")
    if v & 8: bits.append("🔓Lock")
    return " | ".join(bits) if bits else "ALL OFF"

def decode_led(v):
    bits = []
    if v & 1: bits.append("🔴Red")
    if v & 2: bits.append("🟢Green")
    if v & 4: bits.append("⚪White")
    return " | ".join(bits) if bits else "ALL OFF"

def decode_alarm_flags(v):
    if v == 0: return "✅ OK"
    active = []
    for bit, name in ALARM_FLAG_BITS:
        if v & (1 << bit): active.append(name)
    return " | ".join(active)

def fmt_time(s):
    if s < 60: return f"{s}s"
    if s < 3600: return f"{s//60}m {s%60}s"
    return f"{s//3600}h {(s%3600)//60}m"

# ═══════════════════════════════════════════════════════════
# MODBUS CLIENT
# ═══════════════════════════════════════════════════════════

class TRU_ModbusMaster:
    def __init__(self, port, slave_addr, baudrate=9600):
        self.slave = slave_addr
        if port.startswith("tcp:"):
            from pymodbus.client import ModbusTcpClient
            host, tcp_port = port[4:].split(":")
            self.client = ModbusTcpClient(host, port=int(tcp_port), framer="rtu", timeout=2)
            if not self.client.connect():
                print(f"❌ Không thể kết nối TCP {host}:{tcp_port}")
                sys.exit(1)
            print(f"✅ Kết nối TCP {host}:{tcp_port}, slave={slave_addr}")
        else:
            self.client = ModbusSerialClient(
                port=port,
                baudrate=baudrate,
                bytesize=8,
                parity='N',
                stopbits=1,
                timeout=1,
            )
            if not self.client.connect():
                print(f"❌ Không thể kết nối {port}")
                sys.exit(1)
            print(f"✅ Kết nối {port} @ {baudrate} baud, slave={slave_addr}")

    def close(self):
        self.client.close()

    # ─── FC04: Read Input Registers ───
    def read_all_inputs(self):
        """Đọc toàn bộ 45 Input Registers"""
        print("\n" + "═"*65)
        print("  📊 INPUT REGISTERS (FC=04, Read-Only)")
        print("═"*65)

        result = self.client.read_input_registers(address=0x0000, count=45, device_id=self.slave)
        if result.isError():
            print(f"  ❌ Lỗi đọc: {result}")
            return None

        regs = result.registers
        data = {}

        for i, (addr, (name, unit, fmt)) in enumerate(IR.items()):
            if i < len(regs):
                val = regs[i]
                decoded = fmt(val)
                print(f"  0x{addr:04X} │ {name:18s} │ {val:6d} │ {decoded}")
                data[name] = val

        # Combine 32-bit values
        if 'energy_hi' in data and 'energy_lo' in data:
            total_energy = (data['energy_hi'] << 16) | data['energy_lo']
            print(f"  {'':6s} │ {'→ total_energy':18s} │ {total_energy:6d} │ {total_energy/1000:.2f} kWh")
        if 'uptime_hi' in data and 'uptime_lo' in data:
            uptime = (data['uptime_hi'] << 16) | data['uptime_lo']
            print(f"  {'':6s} │ {'→ total_uptime':18s} │ {uptime:6d} │ {fmt_time(uptime)}")
        if 'meter_serial_1' in data and 'meter_serial_2' in data and 'meter_serial_3' in data:
            serial_str = f"{data['meter_serial_1']:04X}{data['meter_serial_2']:04X}{data['meter_serial_3']:04X}"
            print(f"  {'':6s} │ {'→ meter_serial':18s} │ {'------':6s} │ S/N: {serial_str}")

        return data

    # ─── FC03: Read Holding Registers ───
    def read_all_holdings(self):
        """Đọc toàn bộ 14 Holding Registers"""
        print("\n" + "═"*65)
        print("  ⚙️  HOLDING REGISTERS (FC=03, Read-Write)")
        print("═"*65)

        result = self.client.read_holding_registers(address=0x0100, count=14, device_id=self.slave)
        if result.isError():
            print(f"  ❌ Lỗi đọc: {result}")
            return None

        regs = result.registers
        data = {}
        for i, (addr, (name, unit, default, fmt)) in enumerate(HR.items()):
            if i < len(regs):
                val = regs[i]
                decoded = fmt(val)
                mark = " ✏️" if val != default else ""
                print(f"  0x{addr:04X} │ {name:18s} │ {val:6d} │ {decoded} (default={default}){mark}")
                data[name] = val
        return data

    # ─── FC02: Read Discrete Inputs ───
    def read_discrete_inputs(self):
        """Đọc 8 Discrete Inputs"""
        print("\n" + "═"*65)
        print("  🔘 DISCRETE INPUTS (FC=02, Read-Only)")
        print("═"*65)

        result = self.client.read_discrete_inputs(address=0x0000, count=8, device_id=self.slave)
        if result.isError():
            print(f"  ❌ Lỗi đọc: {result}")
            return None

        bits = result.bits[:8]
        icons = ["🚪", "⚡", "🚨", "🌀", "🔓", "⚠️", "🔌", "🔌"]
        for i, (addr, name) in enumerate(DI.items()):
            val = bits[i] if i < len(bits) else False
            icon = icons[i] if i < len(icons) else "❓"
            state = "🔴 TRUE" if val else "⚪ false"
            print(f"  0x{addr:04X} │ {icon} {name:20s} │ {state}")
        return {DI[i]: bits[i] for i in range(min(len(bits), len(DI)))}

    # ─── FC01: Read Coils ───
    def read_coils(self):
        """Đọc 7 Coil Registers"""
        print("\n" + "═"*65)
        print("  🎛️  COIL REGISTERS (FC=01, Read-Write)")
        print("═"*65)

        result = self.client.read_coils(address=0x0000, count=7, device_id=self.slave)
        if result.isError():
            print(f"  ❌ Lỗi đọc: {result}")
            return None

        bits = result.bits[:5]
        for i, (addr, name) in enumerate(COILS.items()):
            val = bits[i] if i < len(bits) else False
            state = "🔵 ACTIVE" if val else "⚪ idle"
            print(f"  0x{addr:04X} │ {name:25s} │ {state}")

    # ─── FC05: Write Single Coil ───
    def write_coil(self, coil_addr, value=True):
        """Ghi coil (FC=05): value=True → 0xFF00"""
        name = COILS.get(coil_addr, f"coil_{coil_addr}")
        print(f"\n  🎯 Ghi Coil 0x{coil_addr:04X} ({name}) → {'FF00' if value else '0000'}...")
        result = self.client.write_coil(address=coil_addr, value=value, device_id=self.slave)
        if result.isError():
            print(f"  ❌ Lỗi: {result}")
            return False
        print(f"  ✅ OK — Coil {name} triggered!")
        return True

    # ─── FC06: Write Single Holding Register ───
    def write_holding(self, addr, value):
        """Ghi 1 Holding Register (FC=06)"""
        name = HR.get(addr, (f"reg_{addr:04X}", "", 0, str))[0]
        print(f"\n  📝 Ghi Holding 0x{addr:04X} ({name}) = {value}...")
        result = self.client.write_register(address=addr, value=value, device_id=self.slave)
        if result.isError():
            print(f"  ❌ Lỗi: {result}")
            return False
        print(f"  ✅ OK — {name} = {value}")
        return True

    # ─── FULL SCAN ───
    def full_scan(self):
        """Đọc toàn bộ register 1 lần"""
        print("\n" + "╔" + "═"*63 + "╗")
        print("║  🔍 TRU V1.0 — FULL REGISTER SCAN" + " "*28 + "║")
        print("║  " + time.strftime("%Y-%m-%d %H:%M:%S") + " "*42 + "║")
        print("╚" + "═"*63 + "╝")
        self.read_all_inputs()
        self.read_all_holdings()
        self.read_discrete_inputs()
        self.read_coils()
        print()

    # ─── HEALTH CHECK ───
    def health_check(self):
        """Kiểm tra board sống qua heartbeat"""
        print("\n  💓 Health Check — đọc heartbeat 2 lần cách 2s...")
        r1 = self.client.read_input_registers(address=0x0011, count=1, device_id=self.slave)
        if r1.isError():
            print(f"  ❌ Không đọc được heartbeat: {r1}")
            return
        hb1 = r1.registers[0]
        print(f"  Heartbeat #1: {hb1}")
        time.sleep(2)
        r2 = self.client.read_input_registers(address=0x0011, count=1, device_id=self.slave)
        hb2 = r2.registers[0]
        print(f"  Heartbeat #2: {hb2}")
        if hb2 > hb1:
            print(f"  ✅ Board SỐNG! (delta = {hb2-hb1})")
        else:
            print(f"  🚨 Board TREO hoặc KHÔNG PHẢN HỒI!")

    # ─── AUTO MONITOR ───
    def auto_monitor(self, interval=2):
        """Tự động đọc Input mỗi N giây"""
        print(f"\n  📡 Auto Monitor (mỗi {interval}s, Ctrl+C để dừng)")
        try:
            while True:
                os.system('clear' if os.name != 'nt' else 'cls')
                print(f"  📡 TRU V1.0 Monitor — {time.strftime('%H:%M:%S')} (Ctrl+C = dừng)\n")
                
                # Gửi heartbeat
                hb_val = int(time.time() % 65535)
                self.client.write_register(address=0x0109, value=hb_val, device_id=self.slave)
                
                self.read_all_inputs()
                self.read_discrete_inputs()
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\n  ⏹️ Dừng monitor")


# ═══════════════════════════════════════════════════════════
# INTERACTIVE CLI
# ═══════════════════════════════════════════════════════════

MENU = """
╔═══════════════════════════════════════════════════════╗
║  🔌 TRU V1.0 — Modbus RTU Master Simulator           ║
╠═══════════════════════════════════════════════════════╣
║  [1] Full Scan (đọc tất cả registers)                ║
║  [2] Đọc Input Registers (FC=04)                     ║
║  [3] Đọc Holding Registers (FC=03)                   ║
║  [4] Đọc Discrete Inputs (FC=02)                     ║
║  [5] Đọc Coils (FC=01)                               ║
║  ─────────── LỆNH ĐIỀU KHIỂN ───────────             ║
║  [s] Start Charge                                     ║
║  [t] Stop Charge                                      ║
║  [u] Unlock Door (mở khóa 5s)                        ║
║  [c] Clear Error                                      ║
║  [b] Standby                                          ║
║  ─────────── CẤU HÌNH ──────────────                  ║
║  [w] Ghi Holding Register                             ║
║  ─────────── TIỆN ÍCH ──────────────                  ║
║  [h] Health Check (heartbeat)                         ║
║  [m] Auto Monitor (live)                              ║
║  ─────────── TEST TỰ ĐỘNG ──────────                  ║
║  [a] Auto Test Sequence (IDLE→CHARGE→FINISH)          ║
║  [q] Quit                                             ║
╚═══════════════════════════════════════════════════════╝
"""

def auto_test_sequence(master):
    """Chạy chuỗi test tự động"""
    print("\n  🤖 AUTO TEST SEQUENCE")
    print("  " + "─"*50)

    print("\n  Step 1: Full scan ban đầu...")
    master.full_scan()
    time.sleep(1)

    print("\n  Step 2: Gửi lệnh Standby...")
    master.write_coil(4, True)
    time.sleep(2)
    master.read_all_inputs()

    print("\n  Step 3: Gửi lệnh Start Charge...")
    master.write_coil(0, True)
    time.sleep(2)
    master.read_all_inputs()

    print("\n  Step 4: Unlock Door (mở khóa 5s)...")
    master.write_coil(2, True)
    time.sleep(1)
    master.read_all_inputs()

    print("\n  Step 5: Đợi 6s rồi kiểm tra khóa đã đóng lại...")
    time.sleep(6)
    master.read_all_inputs()

    print("\n  Step 6: Gửi lệnh Stop Charge...")
    master.write_coil(1, True)
    time.sleep(2)
    master.full_scan()

    print("\n  ✅ AUTO TEST HOÀN TẤT!")


def interactive(master):
    """CLI tương tác"""
    while True:
        print(MENU)
        choice = input("  Lệnh > ").strip().lower()

        if choice == '1': master.full_scan()
        elif choice == '2': master.read_all_inputs()
        elif choice == '3': master.read_all_holdings()
        elif choice == '4': master.read_discrete_inputs()
        elif choice == '5': master.read_coils()
        elif choice == 's': master.write_coil(0, True)
        elif choice == 't': master.write_coil(1, True)
        elif choice == 'u': master.write_coil(2, True)
        elif choice == 'c': master.write_coil(3, True)
        elif choice == 'b': master.write_coil(4, True)
        elif choice == 'w':
            try:
                addr = int(input("  Địa chỉ (hex, VD 0x0100): "), 16)
                val = int(input("  Giá trị (uint16): "))
                master.write_holding(addr, val)
            except (ValueError, EOFError):
                print("  ❌ Sai định dạng")
        elif choice == 'h': master.health_check()
        elif choice == 'm': master.auto_monitor()
        elif choice == 'a': auto_test_sequence(master)
        elif choice == 'q':
            print("  👋 Bye!")
            break
        else:
            print(f"  ❓ Không hiểu lệnh '{choice}'")

        if choice not in ('q', 'm'):
            input("\n  ⏎ Enter để tiếp...")


# ═══════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="TRU V1.0 Modbus RTU Master Simulator")
    parser.add_argument("--port", "-p", default="/dev/ttyUSB0", help="Cổng serial (default: /dev/ttyUSB0)")
    parser.add_argument("--addr", "-a", type=int, default=1, help="Slave address (default: 1)")
    parser.add_argument("--baud", "-b", type=int, default=9600, help="Baudrate (default: 9600)")
    parser.add_argument("--scan", action="store_true", help="Full scan 1 lần rồi thoát")
    parser.add_argument("--monitor", "-m", type=int, metavar="SEC", help="Auto monitor mỗi N giây")
    args = parser.parse_args()

    master = TRU_ModbusMaster(args.port, args.addr, args.baud)

    try:
        if args.scan:
            master.full_scan()
        elif args.monitor:
            master.auto_monitor(args.monitor)
        else:
            interactive(master)
    except KeyboardInterrupt:
        print("\n  ⏹️ Interrupted")
    finally:
        master.close()
