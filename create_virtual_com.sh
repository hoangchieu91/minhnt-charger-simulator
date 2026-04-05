#!/bin/bash

# Configuration
TCP_PORT=4321
VIRTUAL_COM="/tmp/ttyTRU"

# Check for socat
if ! command -v socat &> /dev/null; then
    echo "❌ Lỗi: Chưa cài đặt 'socat'. Hãy chạy: sudo apt install socat"
    exit 1
fi

echo "🚀 Đang tạo COM ảo (PTY) từ VXL Renode..."
echo "--------------------------------------------------------"
echo "Simulator (TCP:$TCP_PORT) <---> COM ảo ($VIRTUAL_COM)"
echo "--------------------------------------------------------"
echo "Bây giờ bạn có thể kết nối bất kỳ Modbus Master tool nào (Python, Modbus Poll) "
echo "vào Port: $VIRTUAL_COM"
echo ""
echo "Nhấn Ctrl+C để dừng..."

# socat bridge:
# PTY: Tạo một pseudo-terminal
# link: Tạo symlink /tmp/ttyTRU để dễ truy cập
# raw,echo=0: Chế độ binary không echo (cần cho Modbus RTU)
# TCP: Kết nối tới Renode UART1
socat -d -d PTY,link=$VIRTUAL_COM,raw,echo=0 TCP:localhost:$TCP_PORT
