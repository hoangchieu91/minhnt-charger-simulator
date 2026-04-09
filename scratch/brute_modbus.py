from pymodbus.client import ModbusSerialClient
import time

def brute_modbus(port="/dev/ttyUSB0", slave=1):
    bauds = [2400, 4800, 9600, 19200, 38400, 57600, 115200]
    
    for baud in bauds:
        print(f"📡 Testing Modbus @ {baud}...")
        client = ModbusSerialClient(port=port, baudrate=baud, timeout=0.5)
        if not client.connect():
            continue
            
        # Read Meter Serial 1 (0x0023)
        res = client.read_input_registers(address=0x0023, count=1, device_id=slave)
        if not res.isError():
            print(f"🎯 SUCCESS! Found active Modbus at {baud} baud.")
            print(f"📊 Reg 0x0023 value: {res.registers[0]:04X}")
            client.close()
            return baud
        else:
            # Check for raw response even if parse error
            pass
        client.close()
    return None

if __name__ == "__main__":
    brute_modbus()
