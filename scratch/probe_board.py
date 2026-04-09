from pymodbus.client import ModbusSerialClient
import time
import serial

def check_noise(port="/dev/ttyUSB0", baud=9600):
    print(f"🕵️ Monitoring noise on {port} at {baud}...")
    try:
        s = serial.Serial(port, baud, timeout=2)
        data = s.read(32)
        if data:
            print(f"📡 Detected incoming data: {data.hex()}")
            return True
        else:
            print("🤫 Line is silent.")
            return False
        s.close()
    except Exception as e:
        print(f"❌ Error opening port: {e}")
        return False

def try_probe(port, baud, slave=1):
    print(f"🔄 Attempting Modbus probe at {baud} baud...")
    client = ModbusSerialClient(port=port, baudrate=baud, timeout=1)
    if not client.connect():
        return False
    
    # Read UpTime (0x000F-0x0010) - Very stable register
    res = client.read_input_registers(address=0x000F, count=2, device_id=slave)
    if not res.isError():
        hi, lo = res.registers
        uptime = (hi << 16) | lo
        print(f"✅ SUCCESS! Board Uptime: {uptime}s")
        
        # Now read Serial
        res = client.read_input_registers(address=0x0023, count=3, device_id=slave)
        if not res.isError():
            s1, s2, s3 = res.registers
            print(f"📟 Meter Serial in RAM: {s1:04X}{s2:04X}{s3:04X}")
        
        client.close()
        return True
    
    client.close()
    return False

def main():
    port = "/dev/ttyUSB0"
    
    # 1. Check for noise first
    has_noise = check_noise(port, 9600)
    
    # 2. Try 9600
    if try_probe(port, 9600):
        return
        
    # 3. Try 115200 (Common for PLL-enabled boards)
    if try_probe(port, 115200):
        return
        
    print("❌ Could not communicate with the board at 9600 or 115200.")
    print("💡 Please check if the USB-to-RS485 adapter is connected to the CORRECT pins (PA9/PA10).")

if __name__ == "__main__":
    main()
