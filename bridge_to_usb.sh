#!/bin/bash

# Cấu hình mặc định
PORT=${1:-"/dev/ttyUSB0"}
BAUD=9600
TCP_PORT=4321

# Kiểm tra socat
if ! command -v socat &> /dev/null; then
    echo "❌ Lỗi: Chưa cài đặt 'socat'. Hãy chạy: sudo apt install socat"
    exit 1
fi

# Kiểm tra quyền truy cập port (dialout group)
if [ ! -w "$PORT" ]; then
    echo "⚠️ Cảnh báo: Không có quyền ghi vào $PORT. Hãy thử chạy với 'sudo' hoặc thêm user vào group dialout."
fi

echo "🚀 Đang thiết lập cầu nối (Bridge):"
echo "   Simulator (TCP:$TCP_PORT) <---> Phần cứng ($PORT @ $BAUD)"
echo "   --------------------------------------------------------"
echo "   Bây giờ bạn có thể dùng Modbus Master (PLC, Modbus Poll) "
echo "   cắm vào cổng $PORT để điều khiển Simulator."
echo ""
echo "Nhấn Ctrl+C để dừng..."

# socat bridge
# - TCP: Kết nối tới UART1 của Renode
# - Serial: Kết nối tới cổng USB vật lý
sudo socat -v TCP:localhost:$TCP_PORT,forever,reuseaddr "${PORT},raw,echo=0,baudrate=${BAUD}"
