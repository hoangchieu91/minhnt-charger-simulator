#!/usr/bin/env python3
import http.server, socketserver, json, threading, time, socket
from urllib.parse import urlparse, parse_qs
from pymodbus.client import ModbusTcpClient

# NEW PORTS TO AVOID CONFLICTS
RENODE_UART_PORT = 4321
RENODE_MONITOR_PORT = 1235
HTTP_PORT = 4323

STATES = {0:"INIT", 1:"IDLE", 2:"CONNECTED", 3:"CHARGING", 4:"FINISH", 5:"ERROR", 10:"STANDBY"}
state_lock = threading.Lock()
current_data = {"state": "OFFLINE", "up": 0, "hb": 0, "volt": 0, "amp": 0, "pwr": 0, "kwh": 0.0, "temp": 0.0, "fan": 0, "lock": 0, "door": 0, "af": 0, "m_alive": 0, "m_hb_val": 0, "log": []}

def polling_loop():
    while True:
        client = ModbusTcpClient("127.0.0.1", port=RENODE_UART_PORT, framer="rtu", timeout=2)
        if not client.connect():
            with state_lock: current_data["state"] = "OFFLINE"
            time.sleep(3)
            continue
        while client.connected:
            try:
                rr = client.read_input_registers(0, count=35, device_id=1)
                if rr and not rr.isError():
                    regs = rr.registers
                    with state_lock:
                        relay_bits = regs[7]
                        current_data.update({
                            "volt":regs[0], "amp":regs[1], "pwr":regs[2], "kwh":((regs[3]<<16)|regs[4]), "temp":regs[5]*0.1,
                            "state":STATES.get(regs[6], f"UKN({regs[6]})"), "fan":1 if (relay_bits&0x04) else 0,
                            "lock":1 if (relay_bits&0x08) else 0, "door":regs[10], "up":((regs[15]<<16)|regs[16]), "hb":regs[17], "af":regs[24], "m_alive":regs[34],
                        })
                        if current_data["state"] == "OFFLINE": current_data["state"] = "IDLE (FW OK)"
                else:
                    print(f"Modbus read error: {rr}")
                    break
            except Exception as e:
                print(f"Modbus exception: {e}")
                break
            time.sleep(0.5)
        client.close()
        time.sleep(1)

threading.Thread(target=polling_loop, daemon=True).start()

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args): pass
    def do_GET(self):
        url = urlparse(self.path)
        if url.path == '/api/state':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            with state_lock: self.wfile.write(json.dumps(current_data).encode())
        else: super().do_GET()

socketserver.TCPServer.allow_reuse_address = True
try:
    with socketserver.TCPServer(("0.0.0.0", HTTP_PORT), Handler) as httpd:
        print(f"[*] Dashboard API on port {HTTP_PORT}")
        httpd.serve_forever()
except Exception as e: print(f"Error: {e}")
