import telnetlib
import time
import re

def read_mem(tn, addr, size):
    tn.write(f"sysbus ReadDoubleWord {addr}\n".encode())
    res = tn.read_until(b"\n", timeout=1).decode()
    # Renode output looks like: 0x20000818: 0x00000005
    match = re.search(r": (0x[0-9A-Fa-f]+)", res)
    if match:
        return int(match.group(1), 16)
    return 0

def main():
    HOST = "127.0.0.1"
    PORT = 1235
    
    # Addresses from 'nm'
    ADDR_SERIAL = 0x2000012c
    ADDR_RX_CNT = 0x20000818
    ADDR_TIMEOUT = 0x20000820
    
    print(f"🔍 Monitoring Renode RAM at {HOST}:{PORT}")
    try:
        tn = telnetlib.Telnet(HOST, PORT)
        # Wait for banner
        time.sleep(1)
        
        while True:
            rx_cnt = read_mem(tn, ADDR_RX_CNT, 4)
            timeouts = read_mem(tn, ADDR_TIMEOUT, 4)
            serial_lo = read_mem(tn, ADDR_SERIAL, 4)
            serial_hi = read_mem(tn, ADDR_SERIAL + 4, 4)
            
            print(f"--- [METER STATUS] ---")
            print(f"Valid Packets (RX): {rx_cnt}")
            print(f"Timeouts:          {timeouts}")
            print(f"Serial (RAM):      {hex(serial_hi)[2:].zfill(8)}{hex(serial_lo)[2:].zfill(4)}")
            print(f"----------------------")
            
            time.sleep(2)
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
