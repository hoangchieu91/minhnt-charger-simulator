import serial
import time

def brute_force_baud(port="/dev/ttyUSB0"):
    bauds = [2400, 4800, 9600, 19200, 38400, 57600, 115200]
    test_data = b"\xde\xad\xbe\xef"
    
    for baud in bauds:
        print(f"📡 Testing {baud} baud...")
        try:
            s = serial.Serial(port, baud, timeout=1)
            s.write(test_data)
            resp = s.read(len(test_data))
            if resp:
                print(f"   RX ({baud}): {resp.hex()}")
                if resp == test_data:
                    print(f"🎯 FOUND CORRECT BAUDRATE: {baud}")
                    s.close()
                    return baud
            s.close()
        except:
            pass
    return None

if __name__ == "__main__":
    brute_force_baud()
