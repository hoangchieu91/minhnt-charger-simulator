import serial
import time

def echo_test(port="/dev/ttyUSB0", baud=9600):
    print(f"📡 Testing Serial Echo on {port} @ {baud}...")
    try:
        s = serial.Serial(port, baud, timeout=2)
        test_data = b"\xde\xad\xbe\xef"
        print(f"TX: {test_data.hex()}")
        s.write(test_data)
        
        # Read back
        resp = s.read(len(test_data))
        if resp:
            print(f"RX: {resp.hex()}")
            if resp == test_data:
                print("✅ ECHO SUCCESS! Board is alive and echoing.")
                return True
            else:
                print("⚠️ ECHO MISMATCH (Likely baudrate or noise issue).")
        else:
            print("❌ NO RESPONSE (Board is silent or RX/TX reversed).")
        s.close()
    except Exception as e:
        print(f"❌ Error: {e}")
    return False

if __name__ == "__main__":
    echo_test()
