#!/usr/bin/env python3
"""
TRU V1.0 — Firmware Logic Simulator
Giả lập logic firmware THẬT trên PC, cho phép debug tương tác

Chạy: python3 firmware_sim.py
"""

import time
import threading
import sys
import os

# ═══════════════════════════════════════════════════════════
# MÔ PHỎNG PHẦN CỨNG
# ═══════════════════════════════════════════════════════════

class HardwareModel:
    """Mô phỏng GPIO, ADC, Relay, LED của board TRU V1.0"""
    def __init__(self):
        self.tick = 0
        # GPIO Inputs
        self.door_sensor = True    # True=đóng (pullup), False=mở
        self.dip_switch = 1        # Địa chỉ Modbus (0-7)
        # GPIO Outputs
        self.relays = {"CHARGER": False, "SOCKET": False, "FAN": False, "DOORLOCK": False}
        self.leds = {"RED": False, "GREEN": False, "WHITE": False}
        self.led_blink = {"RED": False, "GREEN": False, "WHITE": False}
        # ADC
        self.ntc_adc = 2048        # ~25°C mặc định
        self.ntc_temp = 250        # 0.1°C → 25.0°C
        # Meter DLT645
        self.voltage = 2200        # 0.1V → 220.0V
        self.current = 0           # 0.01A
        self.power = 0             # W
        self.energy = 0            # Wh (tổng)
        self.session_energy = 0    # Wh (phiên)
        # Lock timing
        self.lock_start_tick = 0
        self.lock_active = False

    def get_tick(self):
        return self.tick

# ═══════════════════════════════════════════════════════════
# FSM — Logic firmware thật (copy từ app_main.c)
# ═══════════════════════════════════════════════════════════

STATES = {0:"INIT", 1:"IDLE", 2:"STANDBY", 3:"CHARGING", 4:"FINISH", 5:"ERROR"}
ALARM_NAMES = ["OVERTEMP","DOOR_OPEN","TAMPER","OVERPOWER","VOLT_FAULT","ENERGY_LIM","LOW_CURR","COMM_FAIL"]

OVERTEMP_LIMIT = 700  # 70.0°C
FAN_HIGH = 450        # 45.0°C
FAN_LOW = 380         # 38.0°C
LOCK_PULSE_MS = 5000

class FirmwareSimulator:
    def __init__(self):
        self.hw = HardwareModel()
        self.state = 1  # IDLE
        self.state_enter_tick = 0
        # Fan
        self.fan_on = False
        self.fan_on_tick = 0
        # Diagnostics
        self.uptime = 0
        self.heartbeat = 0
        self.last_second_tick = 0
        self.temp_min = 9999
        self.temp_max = -9999
        self.error_count = 0
        self.charge_count = 0
        self.dlt645_ok = 0
        self.dlt645_fail = 0
        self.alarm_flags = 0
        # Session
        self.charge_start_tick = 0
        # Debounce
        self.door_debounce_count = 0
        self.door_state = False  # debounced: False=đóng, True=mở
        # Log
        self.log_lines = []

    def log(self, msg):
        ts = f"[{self.hw.tick/1000:.1f}s]"
        line = f"{ts} {msg}"
        self.log_lines.append(line)
        if len(self.log_lines) > 100:
            self.log_lines.pop(0)
        print(f"  {line}")

    # ─── Debounce Door ───
    def process_door(self):
        raw_open = not self.hw.door_sensor  # sensor LOW = mở
        if raw_open != self.door_state:
            self.door_debounce_count += 1
            if self.door_debounce_count >= 50:  # 50ms debounce
                old = self.door_state
                self.door_state = raw_open
                self.door_debounce_count = 0
                if old != raw_open:
                    self.log(f"🚪 Door {'MỞ' if raw_open else 'ĐÓNG'}!")
        else:
            self.door_debounce_count = 0

    # ─── Tamper Detection ───
    def is_tamper(self):
        return self.door_state and not self.hw.lock_active  # cửa mở + khóa cài

    # ─── Fan Hysteresis ───
    def process_fan(self):
        temp = self.hw.ntc_temp
        if not self.fan_on and temp >= FAN_HIGH:
            self.fan_on = True
            self.fan_on_tick = self.hw.tick
            self.hw.relays["FAN"] = True
            self.log(f"🌀 Fan BẬT (temp={temp/10:.1f}°C ≥ {FAN_HIGH/10:.1f}°C)")
        elif self.fan_on and temp <= FAN_LOW:
            if self.hw.tick - self.fan_on_tick >= 30000:  # min 30s ON
                self.fan_on = False
                self.hw.relays["FAN"] = False
                self.log(f"🌀 Fan TẮT (temp={temp/10:.1f}°C ≤ {FAN_LOW/10:.1f}°C)")

    # ─── Door Lock ───
    def process_lock(self):
        if self.hw.lock_active:
            if self.hw.tick - self.hw.lock_start_tick >= LOCK_PULSE_MS:
                self.hw.lock_active = False
                self.hw.relays["DOORLOCK"] = False
                self.log("🔒 Door Lock: tự động KHÓA lại")

    # ─── Diagnostics ───
    def process_diag(self):
        if self.hw.tick - self.last_second_tick >= 1000:
            elapsed_sec = (self.hw.tick - self.last_second_tick) // 1000
            self.last_second_tick = self.hw.tick
            self.uptime += elapsed_sec
            self.heartbeat += elapsed_sec

        temp = self.hw.ntc_temp
        if temp < self.temp_min: self.temp_min = temp
        if temp > self.temp_max: self.temp_max = temp

    # ─── Build Alarm Flags ───
    def build_alarm_flags(self):
        flags = 0
        if self.hw.ntc_temp > OVERTEMP_LIMIT: flags |= (1 << 0)  # OVERTEMP
        if self.door_state: flags |= (1 << 1)                     # DOOR_OPEN
        if self.is_tamper(): flags |= (1 << 2)                    # TAMPER
        # bit3-6: meter alarms (simplified)
        if self.hw.power > 7000: flags |= (1 << 3)               # OVERPOWER
        v = self.hw.voltage
        if v < 1800 or v > 2600: flags |= (1 << 4)               # VOLT_FAULT
        self.alarm_flags = flags

    # ─── State Transitions ───
    def enter_state(self, new_state):
        old_name = STATES.get(self.state, "?")
        new_name = STATES.get(new_state, "?")
        self.log(f"📌 State: {old_name} → {new_name}")
        self.state = new_state
        self.state_enter_tick = self.hw.tick

    def trigger_error(self):
        self.enter_state(5)  # ERROR
        self.hw.leds = {"RED": True, "GREEN": False, "WHITE": False}
        self.hw.led_blink = {"RED": False, "GREEN": False, "WHITE": False}
        self.hw.relays["CHARGER"] = False
        self.hw.relays["SOCKET"] = False
        self.error_count += 1
        self.log(f"🚨 ERROR! (total errors: {self.error_count})")

    def trigger_start_charge(self):
        if self.state not in (5, 3):  # not ERROR, not CHARGING
            self.enter_state(3)  # CHARGING
            self.hw.leds = {"RED": True, "GREEN": False, "WHITE": False}
            self.hw.led_blink = {"RED": True, "GREEN": False, "WHITE": False}
            self.hw.relays["CHARGER"] = True
            self.charge_start_tick = self.hw.tick
            self.hw.session_energy = 0
            self.log("⚡ START CHARGE — Relay Charger ON")

    def trigger_stop_charge(self):
        if self.state == 3:  # CHARGING
            self.enter_state(4)  # FINISH
            self.hw.leds = {"RED": False, "GREEN": True, "WHITE": False}
            self.hw.led_blink = {"RED": False, "GREEN": False, "WHITE": False}
            self.hw.relays["CHARGER"] = False
            self.charge_count += 1
            self.log(f"✅ FINISH — Charge #{self.charge_count} done, energy={self.hw.session_energy}Wh")

    def trigger_standby(self):
        if self.state in (1, 4):  # IDLE or FINISH
            self.enter_state(2)  # STANDBY
            self.hw.leds = {"RED": False, "GREEN": True, "WHITE": False}
            self.hw.led_blink = {"RED": False, "GREEN": True, "WHITE": False}
            self.log("💤 STANDBY — Chờ lệnh sạc")

    def trigger_unlock(self):
        if self.state != 5:  # not ERROR
            self.hw.lock_active = True
            self.hw.lock_start_tick = self.hw.tick
            self.hw.relays["DOORLOCK"] = True
            self.log("🔓 Door UNLOCK — mở 5 giây")

    def trigger_clear_error(self):
        if self.state == 5:  # ERROR
            if not self.door_state and not self.is_tamper() and self.hw.ntc_temp < OVERTEMP_LIMIT:
                self.enter_state(1)  # IDLE
                self.hw.leds = {"RED": False, "GREEN": False, "WHITE": True}
                self.hw.led_blink = {"RED": False, "GREEN": False, "WHITE": False}
                self.log("✅ Error cleared → IDLE")
            else:
                reasons = []
                if self.door_state: reasons.append("cửa còn mở")
                if self.is_tamper(): reasons.append("tamper")
                if self.hw.ntc_temp >= OVERTEMP_LIMIT: reasons.append("quá nhiệt")
                self.log(f"❌ Không clear được: {', '.join(reasons)}")

    # ─── Main Process (1 tick = 1ms) ───
    def process(self):
        self.process_door()
        self.process_fan()
        self.process_lock()
        self.process_diag()
        self.build_alarm_flags()

        # FSM
        if self.state in (1, 2):  # IDLE / STANDBY
            if self.is_tamper(): self.trigger_error(); return
            if self.door_state: self.trigger_error(); return
            if self.hw.ntc_temp > OVERTEMP_LIMIT: self.trigger_error()

        elif self.state == 3:  # CHARGING
            if self.is_tamper(): self.trigger_error(); return
            if self.hw.ntc_temp > OVERTEMP_LIMIT: self.trigger_error(); return
            if self.door_state: self.trigger_error(); return
            if self.hw.power > 7000: self.trigger_error()
            # Simulate energy accumulation
            self.hw.session_energy += self.hw.power // 3600  # Wh per tick at 1s intervals

        elif self.state == 4:  # FINISH
            if self.is_tamper(): self.trigger_error(); return
            if self.door_state: self.trigger_error()

        elif self.state == 5:  # ERROR
            self.hw.relays["CHARGER"] = False
            self.hw.relays["SOCKET"] = False

    # ─── Run N milliseconds ───
    def run_ms(self, ms):
        for _ in range(ms):
            self.hw.tick += 1
            self.process()

    def run_seconds(self, sec):
        for s in range(sec):
            self.run_ms(1000)

    # ─── Display ───
    def display(self):
        t = self.hw
        st_name = STATES.get(self.state, "?")

        # Relay bitmask display
        relay_str = " ".join(
            f"{'⚡' if v else '·'}{k[:3]}" for k, v in t.relays.items()
        )

        # LED display
        led_str = ""
        for name, on in t.leds.items():
            blink = t.led_blink.get(name, False)
            if blink:
                led_str += f" ✨{name}"  # blinking
            elif on:
                led_str += f" 🔴{name}" if name == "RED" else (f" 🟢{name}" if name == "GREEN" else f" ⚪{name}")
            else:
                led_str += f" ·{name}"

        # Alarm flags
        active_alarms = []
        for i, name in enumerate(ALARM_NAMES):
            if self.alarm_flags & (1 << i):
                active_alarms.append(f"⚠️{name}")

        sess_dur = (t.tick - self.charge_start_tick) // 1000 if self.state == 3 else 0

        print()
        print("  ╔═══════════════════════════════════════════════════════╗")
        print(f"  ║  TRU V1.0 @ {t.tick/1000:.1f}s │ State: {st_name:10s}            ║")
        print("  ╠═══════════════════════════════════════════════════════╣")
        print(f"  ║  LED: {led_str:46s}  ║")
        print(f"  ║  Relay: {relay_str:44s}  ║")
        print(f"  ║  🌡️  Temp: {t.ntc_temp/10:.1f}°C (min={self.temp_min/10:.1f} max={self.temp_max/10:.1f})       ║")
        print(f"  ║  ⚡ V={t.voltage/10:.0f}V I={t.current/100:.1f}A P={t.power}W              ║")
        print(f"  ║  🔋 Session: {t.session_energy}Wh  Dur: {sess_dur}s                  ║")
        print(f"  ║  🚪 Door: {'MỞ' if self.door_state else 'Đóng':4s}  Lock: {'🔓Mở' if t.lock_active else '🔒Cài'}  Tamper: {'⚠️CÓ' if self.is_tamper() else 'Không'} ║")
        print(f"  ║  💓 HB={self.heartbeat} Up={self.uptime}s Err={self.error_count} Chg={self.charge_count}        ║")
        if active_alarms:
            al_str = " ".join(active_alarms)
            print(f"  ║  🚨 {al_str:50s} ║")
        else:
            print(f"  ║  ✅ No alarms                                        ║")
        print("  ╚═══════════════════════════════════════════════════════╝")


# ═══════════════════════════════════════════════════════════
# INTERACTIVE CLI
# ═══════════════════════════════════════════════════════════

HELP = """
  ╔═══════════════════════════════════════════════════════╗
  ║  🔧 TRU V1.0 Firmware Simulator — Lệnh               ║
  ╠═══════════════════════════════════════════════════════╣
  ║  THEO DÕI                                             ║
  ║    d / display     Hiển thị trạng thái hiện tại       ║
  ║    log             Xem 20 dòng log gần nhất            ║
  ║                                                        ║
  ║  CHẠY THỜI GIAN                                       ║
  ║    r <giây>        Chạy N giây (VD: r 5)              ║
  ║    r               Chạy 1 giây                         ║
  ║                                                        ║
  ║  GIẢ LẬP PHẦN CỨNG                                   ║
  ║    door open       Mở cửa cabinet                     ║
  ║    door close      Đóng cửa cabinet                   ║
  ║    temp <0.1°C>    Set nhiệt độ (VD: temp 550 = 55°C) ║
  ║    volt <0.1V>     Set điện áp (VD: volt 2200 = 220V) ║
  ║    power <W>       Set công suất (VD: power 3500)     ║
  ║    current <0.01A> Set dòng điện (VD: current 1500)   ║
  ║                                                        ║
  ║  LỆNH MODBUS (giả lập Tủ trung tâm)                  ║
  ║    start           Lệnh bắt đầu sạc                  ║
  ║    stop            Lệnh dừng sạc                      ║
  ║    unlock          Mở khóa cửa 5s                     ║
  ║    clear           Clear error                        ║
  ║    standby         Chuyển standby                     ║
  ║                                                        ║
  ║  KỊCH BẢN TỰ ĐỘNG                                    ║
  ║    test1           Boot → Idle (kiểm tra init)        ║
  ║    test2           Door open → ERROR → Clear          ║
  ║    test3           Charge cycle đầy đủ                ║
  ║    test4           TAMPER detection                   ║
  ║    test5           Quá nhiệt → Fan + ERROR            ║
  ║    testall         Chạy tất cả 5 test                 ║
  ║                                                        ║
  ║    help / h        Hiển thị menu này                  ║
  ║    q               Thoát                              ║
  ╚═══════════════════════════════════════════════════════╝
"""

def run_test1(sim):
    """Test 1: Boot → IDLE"""
    print("\n  ═══ TEST 1: Boot → IDLE ═══")
    sim.__init__()
    sim.hw.leds["WHITE"] = True
    sim.run_seconds(1)
    sim.display()
    ok = sim.state == 1 and sim.hw.leds["WHITE"] and not sim.hw.relays["CHARGER"]
    print(f"  → {'✅ PASS' if ok else '❌ FAIL'}: State=IDLE, White=ON, Charger=OFF")
    return ok

def run_test2(sim):
    """Test 2: Door open → ERROR → Close → Clear"""
    print("\n  ═══ TEST 2: Door → ERROR → Clear ═══")
    sim.__init__()
    sim.hw.leds["WHITE"] = True
    sim.run_seconds(1)

    print("  → Mở cửa...")
    sim.hw.door_sensor = False  # door open
    sim.run_ms(100)  # debounce
    sim.display()

    ok1 = sim.state == 5  # ERROR
    print(f"  → {'✅' if ok1 else '❌'} State=ERROR sau mở cửa")

    print("  → Đóng cửa rồi clear...")
    sim.hw.door_sensor = True
    sim.run_ms(100)
    sim.trigger_clear_error()
    sim.run_ms(100)
    sim.display()

    ok2 = sim.state == 1  # IDLE
    print(f"  → {'✅' if ok2 else '❌'} Clear → IDLE")
    return ok1 and ok2

def run_test3(sim):
    """Test 3: Full charge cycle"""
    print("\n  ═══ TEST 3: Full Charge Cycle ═══")
    sim.__init__()
    sim.hw.leds["WHITE"] = True
    sim.hw.voltage = 2200
    sim.hw.power = 3500
    sim.hw.current = 1500
    sim.run_seconds(1)

    print("  → Standby...")
    sim.trigger_standby()
    sim.run_seconds(1)
    ok1 = sim.state == 2
    print(f"  → {'✅' if ok1 else '❌'} State=STANDBY")

    print("  → Start charge...")
    sim.trigger_start_charge()
    sim.run_seconds(5)
    sim.display()
    ok2 = sim.state == 3 and sim.hw.relays["CHARGER"]
    print(f"  → {'✅' if ok2 else '❌'} State=CHARGING, Relay=ON")

    print("  → Stop charge...")
    sim.trigger_stop_charge()
    sim.run_seconds(1)
    sim.display()
    ok3 = sim.state == 4 and not sim.hw.relays["CHARGER"]
    print(f"  → {'✅' if ok3 else '❌'} State=FINISH, Relay=OFF, charges={sim.charge_count}")
    return ok1 and ok2 and ok3

def run_test4(sim):
    """Test 4: TAMPER detection"""
    print("\n  ═══ TEST 4: TAMPER Detection ═══")
    sim.__init__()
    sim.hw.leds["WHITE"] = True
    sim.run_seconds(1)

    print("  → Khóa cài (mặc định), mở cửa = TAMPER!")
    sim.hw.door_sensor = False
    sim.run_ms(100)
    sim.display()

    tamper = sim.is_tamper()
    error = sim.state == 5
    af = sim.alarm_flags & (1 << 2)
    print(f"  → {'✅' if tamper else '❌'} is_tamper()={tamper}")
    print(f"  → {'✅' if error else '❌'} State=ERROR")
    print(f"  → {'✅' if af else '❌'} alarm_flags bit2 (TAMPER) set")
    return tamper and error and bool(af)

def run_test5(sim):
    """Test 5: Overtemp → Fan + ERROR"""
    print("\n  ═══ TEST 5: Overtemp → Fan + ERROR ═══")
    sim.__init__()
    sim.hw.leds["WHITE"] = True
    sim.run_seconds(1)

    print("  → Set temp=46°C → Fan ON")
    sim.hw.ntc_temp = 460
    sim.run_seconds(1)
    ok1 = sim.fan_on
    print(f"  → {'✅' if ok1 else '❌'} Fan={'ON' if sim.fan_on else 'OFF'}")

    print("  → Set temp=72°C → ERROR")
    sim.hw.ntc_temp = 720
    sim.run_seconds(1)
    sim.display()
    ok2 = sim.state == 5
    ok3 = sim.alarm_flags & (1 << 0)
    print(f"  → {'✅' if ok2 else '❌'} State=ERROR")
    print(f"  → {'✅' if ok3 else '❌'} OVERTEMP flag set")

    print("  → Hạ nhiệt 35°C → clear error")
    sim.hw.ntc_temp = 350
    sim.run_seconds(1)
    sim.trigger_clear_error()
    sim.run_seconds(1)
    ok4 = sim.state == 1
    print(f"  → {'✅' if ok4 else '❌'} Clear → IDLE")
    return ok1 and ok2 and bool(ok3) and ok4


def main():
    sim = FirmwareSimulator()
    sim.hw.leds["WHITE"] = True  # Boot state
    sim.log("🔌 TRU V1.0 Boot OK — All modules initialized")
    sim.run_seconds(1)

    print(HELP)
    sim.display()

    while True:
        try:
            cmd = input("\n  TRU> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\n  👋 Bye!")
            break

        if not cmd:
            continue
        elif cmd in ('h', 'help'):
            print(HELP)
        elif cmd in ('d', 'display'):
            sim.display()
        elif cmd == 'log':
            print("\n  ─── LOG (20 gần nhất) ───")
            for l in sim.log_lines[-20:]:
                print(f"  {l}")
        elif cmd.startswith('r'):
            parts = cmd.split()
            sec = int(parts[1]) if len(parts) > 1 else 1
            print(f"  ⏩ Chạy {sec} giây...")
            sim.run_seconds(sec)
            sim.display()
        elif cmd == 'door open':
            sim.hw.door_sensor = False
            print("  🚪 Door sensor → OPEN (LOW)")
            sim.run_ms(100)
            sim.display()
        elif cmd == 'door close':
            sim.hw.door_sensor = True
            print("  🚪 Door sensor → CLOSED (HIGH)")
            sim.run_ms(100)
            sim.display()
        elif cmd.startswith('temp '):
            val = int(cmd.split()[1])
            sim.hw.ntc_temp = val
            print(f"  🌡️ Temp set → {val/10:.1f}°C")
            sim.run_ms(100)
            sim.display()
        elif cmd.startswith('volt '):
            val = int(cmd.split()[1])
            sim.hw.voltage = val
            print(f"  📐 Voltage set → {val/10:.1f}V")
        elif cmd.startswith('power '):
            val = int(cmd.split()[1])
            sim.hw.power = val
            print(f"  ⚡ Power set → {val}W")
        elif cmd.startswith('current '):
            val = int(cmd.split()[1])
            sim.hw.current = val
            print(f"  🔌 Current set → {val/100:.2f}A")
        elif cmd == 'start':
            sim.trigger_start_charge()
            sim.run_ms(100)
            sim.display()
        elif cmd == 'stop':
            sim.trigger_stop_charge()
            sim.run_ms(100)
            sim.display()
        elif cmd == 'unlock':
            sim.trigger_unlock()
            sim.run_ms(100)
            sim.display()
        elif cmd == 'clear':
            sim.trigger_clear_error()
            sim.run_ms(100)
            sim.display()
        elif cmd == 'standby':
            sim.trigger_standby()
            sim.run_ms(100)
            sim.display()
        elif cmd == 'test1': run_test1(sim)
        elif cmd == 'test2': run_test2(sim)
        elif cmd == 'test3': run_test3(sim)
        elif cmd == 'test4': run_test4(sim)
        elif cmd == 'test5': run_test5(sim)
        elif cmd == 'testall':
            results = {}
            for name, fn in [("Boot→IDLE", run_test1), ("Door→ERROR", run_test2),
                              ("Charge Cycle", run_test3), ("TAMPER", run_test4),
                              ("Overtemp", run_test5)]:
                results[name] = fn(sim)
            print("\n  ═══════════════════════════════════")
            for name, ok in results.items():
                print(f"  {'✅' if ok else '❌'} {name}")
            total = sum(results.values())
            print(f"  ═══════════════════════════════════")
            print(f"  {total}/{len(results)} PASSED")
        elif cmd == 'q':
            print("  👋 Bye!")
            break
        else:
            print(f"  ❓ Không hiểu '{cmd}'. Gõ 'help' để xem menu.")


if __name__ == "__main__":
    main()
