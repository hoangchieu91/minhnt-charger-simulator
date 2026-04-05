import socket
import time
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(5.0)
s.connect(("127.0.0.1", 4321))
req = b"\x01\x04\x00\x00\x00\x23\xB0\x14"
s.sendall(req)
print(f"Sent {req.hex()}")
try:
    resp = s.recv(1024)
    print(f"Recv {resp.hex()}")
except Exception as e:
    print(f"Exception: {e}")
