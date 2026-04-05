#!/usr/bin/env python3
import http.server
import socketserver

# Configuration from Environment
BROKER = os.getenv('MQTT_BROKER', 'localhost')
PORT = 1883
GATEWAY_MAC = "C0:4E:30:12:34:56"
MODBUS_PORT = os.getenv('MODBUS_PORT', '/tmp/ttyTRU')
HTTP_PORT = 4323  # Dashboard API Port
POLLING_INTERVAL = 3  # Seconds

# ... [rest of the imports and definitions remains the same] ...

class GatewaySimulator:
    def __init__(self):
        self.mb_client = ModbusSerialClient(port=MODBUS_PORT, baudrate=9600, timeout=1)
        self.mqtt_client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2, f"gw_{GATEWAY_MAC}")
        self.running = True
        self.last_state = -1
        self.state_lock = threading.Lock()
        self.current_data = {
            "state": "OFFLINE", "up": 0, "hb": 0, "volt": 0, "amp": 0, "pwr": 0, "kwh": 0.0, "temp": 0.0, 
            "fan": 0, "lock": 0, "door": 0, "af": 0, "m_alive": 0, "m_hb_val": 0, "l_w": 0, "l_g": 0, "l_r": 0, "r_c": 0, "r_s": 0
        }

    # ... [connect_mqtt and on_mqtt_message methods] ...

    def poll_modbus(self):
        while self.running:
            if not self.mb_client.connected:
                self.mb_client.connect()
                time.sleep(1)
                continue

            try:
                # Read Input Registers (FC04)
                result = self.mb_client.read_input_registers(0, 35, slave=1)
                if not result.isError():
                    regs = result.registers
                    relay_bits = regs[7]
                    led_bits = regs[8]
                    
                    with self.state_lock:
                        self.current_data.update({
                            "volt": regs[0], "amp": regs[1], "pwr": regs[2], 
                            "kwh": ((regs[3] << 16) | regs[4]), "temp": regs[5],
                            "state": regs[6], "door": regs[10], "hb": regs[17], "af": regs[24],
                            "up": ((regs[15] << 16) | regs[16]), "m_alive": regs[34],
                            "fan": 1 if (relay_bits & 0x04) else 0,
                            "lock": 1 if (relay_bits & 0x08) else 0,
                            "r_c": 1 if (relay_bits & 0x01) else 0,
                            "r_s": 1 if (relay_bits & 0x02) else 0,
                            "l_r": 1 if (led_bits & 0x01) else 0,
                            "l_g": 1 if (led_bits & 0x02) else 0,
                            "l_w": 1 if (led_bits & 0x04) else 0,
                        })

                    # Publish MQTT Telemetry
                    tlm = {
                        "state": regs[6],
                        "voltage": regs[0] * 0.1,
                        "current": regs[1] * 0.01,
                        "power": regs[2],
                        "energy_total": ((regs[3] << 16) | regs[4]) * 0.001,
                        "temperature": regs[5] * 0.1,
                        "timestamp": int(time.time())
                    }
                    self.mqtt_client.publish(TOPIC_POST_TLM, json.dumps(tlm))
                else:
                    logger.warning(f"Modbus Read Error: {result}")
            except Exception as e:
                logger.error(f"Modbus Polling Error: {e}")
            time.sleep(POLLING_INTERVAL)

    def start_http_server(self):
        parent = self
        class DashboardHandler(http.server.SimpleHTTPRequestHandler):
            def log_message(self, format, *args): pass
            def do_GET(self):
                if self.path == '/api/state':
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Access-Control-Allow-Origin', '*')
                    self.end_headers()
                    with parent.state_lock:
                        self.wfile.write(json.dumps(parent.current_data).encode())
                else:
                    super().do_GET()

        socketserver.TCPServer.allow_reuse_address = True
        with socketserver.TCPServer(("0.0.0.0", HTTP_PORT), DashboardHandler) as httpd:
            logger.info(f"🌐 Dashboard API server started on port {HTTP_PORT}")
            httpd.serve_forever()

    def start(self):
        self.connect_mqtt()
        self.mqtt_client.loop_start()
        
        threading.Thread(target=self.poll_modbus, daemon=True).start()
        threading.Thread(target=self.start_http_server, daemon=True).start()

        try:
            while True: time.sleep(1)
        except KeyboardInterrupt:
            self.running = False
            logger.info("👋 Shutting down simulator.")


if __name__ == "__main__":
    sim = GatewaySimulator()
    sim.start()
