import socket
import time

def calculate_cs(data):
    return sum(data) % 256

def handle_dlt645(data):
    # Find start of frame (0x68) after the FE preambles
    try:
        start_idx = data.index(0x68)
    except ValueError:
        return None
    
    if len(data) < start_idx + 10:
        return None
        
    ctrl = data[start_idx + 8]
    L = data[start_idx + 9]
    
    # Discovery CMD 0x13
    if ctrl == 0x13:
        print("MOCK: Received Discovery (0x13)")
        # Response: 68 ADDR(6) 68 93 06 ADDR(6) CS 16
        addr = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66] # BCD Serial 665544332211
        resp = [0x68] + addr + [0x68, 0x93, 0x06] + [a + 0x33 for a in addr]
        resp.append(calculate_cs(resp))
        resp.append(0x16)
        return bytes([0xFE, 0xFE, 0xFE, 0xFE] + resp)
    
    # Read Data CMD 0x11
    if ctrl == 0x11:
        di_raw = data[start_idx + 10 : start_idx + 14]
        di = [d - 0x33 for d in di_raw]
        # DI is LSB first: [0x00, 0x01, 0x01, 0x02] -> 02010100 (Voltage)
        
        print(f"MOCK: Received Read Data (0x11) for DI: {di[::-1]}")
        
        # Static addresses 
        addr = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66]
        
        # Response data (each +0x33)
        data_payload = []
        if di == [0x00, 0x01, 0x01, 0x02]: # Voltage 220.0V -> [0x00, 0x22] -> [0x33, 0x55]
            data_payload = [0x00 + 0x33, 0x22 + 0x33]
        elif di == [0x00, 0x02, 0x01, 0x02]: # Current 10.500A -> [0x00, 0x50, 0x10] -> [0x33, 0x83, 0x43]
            data_payload = [0x00 + 0x33, 0x50 + 0x33, 0x10 + 0x33]
        else:
            data_payload = [0x00 + 0x33] * 4 # Default 0
            
        header = [0x68] + addr + [0x68, 0x91, len(di) + len(data_payload)]
        resp_body = di_raw + data_payload
        cs_data = header + resp_body
        resp = cs_data + [calculate_cs(cs_data), 0x16]
        return bytes([0xFE, 0xFE, 0xFE, 0xFE] + resp)
        
    return None

def main():
    HOST = '127.0.0.1'
    PORT = 4322
    
    print(f"🚀 Mock DLT645 Meter listening on {HOST}:{PORT}")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Connected by {addr}")
                while True:
                    data = conn.recv(1024)
                    if not data:
                        break
                    print(f"RX: {data.hex()}")
                    resp = handle_dlt645(list(data))
                    if resp:
                        print(f"TX: {resp.hex()}")
                        conn.sendall(resp)

if __name__ == "__main__":
    main()
