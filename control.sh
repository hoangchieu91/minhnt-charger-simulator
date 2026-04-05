#!/bin/bash

BASE_URL="http://localhost:4322"

usage() {
    echo "Sử dụng: ./control.sh [lệnh] [giá trị]"
    echo "Các lệnh hỗ trợ (Tiếng Việt):"
    echo "  nhiet [val]   - Chỉnh nhiệt độ (VD: 250 là 25.0°C, 750 là 75.0°C)"
    echo "  cua [0|1]     - Đóng (0) hoặc Mở (1) cửa"
    echo "  dienap [val]  - Chỉnh điện áp (VD: 2200 là 220V)"
    echo "  congsuat [val]- Chỉnh công suất (W)"
    echo "  dongdien [val]- Chỉnh dòng điện (VD: 1500 là 15.0A)"
    echo "  trangthai [S] - IDLE, CHARGING, FINISH, ERROR"
    echo "  xoa           - Xóa lỗi (nếu đã hết điều kiện lỗi)"
}

if [ -z "$1" ]; then
    usage
    exit 1
fi

case $1 in
    nhiet)   curl -s "${BASE_URL}/api/set?temp=$2" > /dev/null; echo "✅ Đặt nhiệt độ: $(($2/10)).$(($2%10))°C" ;;
    cua)     curl -s "${BASE_URL}/api/set?door=$2" > /dev/null; echo "✅ Cửa: $([ "$2" == "1" ] && echo "MỞ" || echo "ĐÓNG")" ;;
    dienap)  curl -s "${BASE_URL}/api/set?volt=$2" > /dev/null; echo "✅ Đặt điện áp: $(($2/10))V" ;;
    congsuat)curl -s "${BASE_URL}/api/set?pwr=$2" > /dev/null; echo "✅ Đặt công suất: ${2}W" ;;
    dongdien)curl -s "${BASE_URL}/api/set?amp=$2" > /dev/null; echo "✅ Đặt dòng điện: $(($2/100)).$(($2%100))A" ;;
    trangthai)curl -s "${BASE_URL}/api/set?state=$2" > /dev/null; echo "✅ Chuyển trạng thái: $2" ;;
    xoa)     curl -s "${BASE_URL}/api/set?clear=1" > /dev/null; echo "✅ Đã gửi lệnh xóa lỗi" ;;
    *) usage ;;
esac
