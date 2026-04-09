import serial
import time

def sniff():
    try:
        ser = serial.Serial('/dev/ttyUSB0', 2400, parity=serial.PARITY_EVEN, stopbits=serial.STOPBITS_ONE, timeout=1)
        print("Sniffing on /dev/ttyUSB0 (2400, 8, E, 1)... Press Ctrl+C to stop.")
        
        while True:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                hex_str = ' '.join(['{:02X}'.format(b) for b in data])
                print(f"[{time.strftime('%H:%M:%S')}] RX: {hex_str}")
            time.sleep(0.1)
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    sniff()
