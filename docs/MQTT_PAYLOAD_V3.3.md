# CẤU TRÚC BẢN TIN MQTT GIAO TIẾP TRẠM SẠC (V3.3)

Tài liệu này mô tả chi tiết các Payload JSON thực tế, bao gồm luồng giao tiếp Server ↔ ESP32 Gateway ↔ Trụ Sạc (Modbus RTU). 

**Quy ước chung:**
- `{mac}`: Địa chỉ MAC của ESP32 (Đóng vai trò Gateway).
- `{post_id}`: Địa chỉ Slave Modbus của từng điểm sạc (1, 2, 3...).
- Tất cả các bản tin sự kiện, trạng thái đều phải được thiết lập cờ **RETAIN = true**.

---

## 1. Bản tin Trạng thái Gateway & Thống kê điểm sạc (MỚI)
**Topic:** `charging/st/{mac}/gw_status`  
**Hướng:** ESP32 (Gateway) -> Server  
**Chu kỳ:** Định kỳ (1 phút / lần) hoặc cập nhật ngay khi một trạm con chuyển trạng thái.  
**Mục đích:** Báo cáo sức khỏe của ESP32 và tổng hợp thống kê toàn bộ các post đang quản lý.

```json
{
  "timestamp": 1711284000,
  "uptime": 3600,            // Thời gian Gateway đã chạy (giây)
  "wifi_rssi": -65,          // Cường độ tín hiệu Wi-Fi (dBm)
  "stats": {                 // Thống kê nhanh trạng thái các điểm sạc
    "total_posts": 10,       // Tổng số trụ sạc đang đấu nối (Modbus node)
    "idle": 5,               // Số trụ sạc rảnh
    "charging": 3,           // Số trụ sạc đang cấp nguồn
    "error": 1,              // Số trụ sạc bị lỗi
    "offline": 1             // Số trụ sạc mất kết nối Modbus
  },
  "posts_state": [           // Mảng tóm tắt trạng thái từng post
    {"id": 1, "state": 2},
    {"id": 2, "state": 0},
    {"id": 3, "state": 4}
  ]
}
```

---

## 2. Bản tin Điều khiển (Command)
**Topic:** `charging/st/{mac}/post/{post_id}/cmd`  
**Hướng:** Server -> ESP32  

### 2.1. Lệnh Bắt đầu sạc
```json
{
  "cmd": "start_charge",
  "params": {
    "min_current": 0.20,     // Dòng điện tối thiểu để xác nhận đang sạc (A)
    "max_coin_limit": 500.0  // Giới hạn số dư an toàn (Charge Coin)
  },
  "timestamp": 1711284500
}
```

### 2.2. Lệnh Dừng sạc
```json
{
  "cmd": "stop_charge",
  "params": {
    "reason": 3              // 3: REMOTE_STOP_OUT_OF_COIN
  },
  "timestamp": 1711285000
}
```

### 2.3. Lệnh Mở khóa cửa (Bảo trì)
```json
{
  "cmd": "unlock_door",
  "params": {
    "duration_ms": 5000      // Thời gian mở khóa tính bằng mili-giây
  },
  "timestamp": 1711285050
}
```

### 2.4. Lệnh Xóa lỗi (Clear Error)
```json
{
  "cmd": "clear_error",
  "timestamp": 1711285055
}
```

### 2.5. Lệnh Khởi động lại Gateway
**Topic:** `charging/st/{mac}/cmd` (Đi thẳng vào Gateway thay vì vào từng Post)
```json
{
  "cmd": "gateway_reboot",
  "timestamp": 1711285060
}
```

---

## 3. Bản tin Telemetry định kỳ (TLM)
**Topic:** `charging/st/{mac}/post/{post_id}/tlm`  
**Hướng:** ESP32 -> Server  
**Chu kỳ:** 30s khi đang sạc, 60s khi nhàn rỗi, hoặc tiêu thụ được 0.1kWh.

```json
{
  "state": 2,              // 2: CHARGING
  "voltage": 225.4,        // Điện áp (V)
  "current": 12.85,        // Dòng điện (A)
  "power": 2896.3,         // Công suất (W)
  "frequency": 50.0,       // Tần số (Hz)
  "energy_total": 121.250, // Chỉ số tổng công tơ (kWh)
  "energy_session": 0.700, // Điện năng phiên hiện tại (kWh)
  "temperature": 45.5,     // (MỚI) Nhiệt độ hiện tại của trụ sạc (°C)
  "meter_serial": "012345678912", // (MỚI) Định danh đồng hồ điện
  "timestamp": 1711284600
}
```

---

## 4. Bản tin Sự kiện Sạc (Status)
**Topic:** `charging/st/{mac}/post/{post_id}/status`  
**Hướng:** ESP32 -> Server  

### 4.1. Sự kiện Bắt đầu (Chốt số công tơ đầu kỳ)
```json
{
  "message_type": 0,    
  "event_code": 10,        // 10: CHARGING_STARTED
  "start_kwh": 120.550,    // Chỉ số công tơ lúc bắt đầu sạc
  "timestamp": 1711284515 
}
```

### 4.2. Sự kiện Kết thúc (Chốt số công tơ cuối kỳ)
```json
{
  "message_type": 0,    
  "event_code": 11,        // 11: SESSION_COMPLETED
  "start_kwh": 120.550, 
  "end_kwh": 125.750,      // Số điện chốt cuối cùng
  "total_consumed": 5.200, 
  "reason": 3,             // Lý do kết thúc (Trùng với mã Stop)
  "timestamp": 1711285005 
}
```

---

## 5. Bản tin Cảnh báo Sự cố & Khôi phục (EVT)
**Topic:** `charging/st/{mac}/post/{post_id}/evt`  
**Hướng:** ESP32 -> Server  

*Lưu ý: Payload dưới đây được dùng chung, chỉ thay đổi `event_code` và các giá trị đi kèm.*

```json
{
  "message_type": 1,       // 1: ALARM, 0: EVENT (Khôi phục)
  "event_code": 1,         // Xem bảng mã lỗi bên dưới
  "value": 78.5,           // Giá trị thực tế tại thời điểm lỗi (Tùy chọn)
  "recovered_from": null,  // Nếu message_type = 0, điền mã lỗi vừa được khôi phục
  "timestamp": 1711285100 
}
```

---

## 6. Phụ lục: Bảng tra cứu Enum

### 6.1. Thuộc tính `cmd` (Lệnh từ Server)
| Tên lệnh | Ý nghĩa | Hành vi hệ thống |
|----------|---------|------------------|
| `start_charge` | Bắt đầu sạc | Mở Relay, khởi tạo session sạc dựa theo tham số dòng và mức tiền. |
| `stop_charge` | Dừng sạc | Ngắt Relay, phát sự kiện SESSION_COMPLETED với ID lý do tương ứng. |
| `clear_error` | Xóa lỗi | Hoàn tác trạng thái lỗi của trạm nếu điều kiện chạm ngưỡng an toàn. |
| `unlock_door` | Mở cửa tủ | Kích hoạt rơ le khóa cửa bảo trì trong thời gian thiết lập. |
| `gateway_reboot` | Khởi động lại | Reset cứng toàn bộ thiết bị Gateway ESP32. |

### 6.2. Thuộc tính `state` (Trạng thái trụ sạc)
| Mã | Tên trạng thái | Mô tả |
|----|----------------|-------|
| 0 | `IDLE` | Trống, rảnh, sẵn sàng sạc. |
| 1 | `WAITING` | Đang lấy mã sạc hoặc chờ cắm súng sạc. |
| 2 | `CHARGING` | Relay đang đóng lệnh dẫn điện. |
| 3 | `FINISHED` | Hoàn tất phiên, chờ người dùng phản hồi. |
| 4 | `ERROR` | Lỗi khẩn cấp (Hardware, nhiệt độ, Modbus). |
| 5 | `OFFLINE` | Mất kết nối giao tiếp Modbus với điểm sạc. |

### 6.3. Thuộc tính `event_code` (Mã sự kiện / báo lỗi)
| Mã | Phân loại | Tên sự kiện | Điều kiện kích hoạt |
|----|-----------|-------------|---------------------|
| **1** | ALARM | `CRITICAL_OVERHEAT` | Nhiệt độ NTC > 75°C (Trạm sẽ tự ngắt sạc). |
| **2** | ALARM | `HIGH_TEMP_WARNING` | Nhiệt độ NTC > 45°C (Trạm tự bật quạt làm mát). |
| **3** | ALARM | `DOOR_OPEN_ALARM` | Phát hiện cửa mở trái phép. |
| **4** | ALARM | `METER_OFFLINE` | Mất liên lạc giao thức DLT645 với đồng hồ con. |
| **5** | ALARM | `RELAY_STUCK_FAULT` | Có dòng tiêu thụ rò rỉ (> 0.1A) khi đang IDLE. |
| **6** | ALARM | `COMM_FAIL` | Mất kết nối Modbus giữa ESP32 và Điểm sạc (Thêm mới). |
| **10** | EVENT | `CHARGING_STARTED` | Đã xác nhận Relay đóng và dòng khởi sinh. |
| **11** | EVENT | `SESSION_COMPLETED` | Phiên sạc hoàn tất. |
| **12** | EVENT | `NORMAL_STATE` | Sự cố khôi phục về trạng thái an toàn. |

### 6.4. Thuộc tính `reason` (Lý do dừng sạc)
| Mã | Tên lý do | Chi tiết |
|----|-----------|----------|
| 1 | `FINISHED_AUTO` | Sạc tự động hoàn tất do ắc quy/xe đầy (dòng tiêu thụ giảm mạn). |
| 2 | `REMOTE_STOP_USER`| Người dùng chủ động nhấn Dừng trên App. |
| 3 | `REMOTE_STOP_OUT_OF_COIN` | Hệ thống ngắt do hết tiền / coin từ Server. |
| 4 | `SAFETY_ALARM_STOP` | Nhảy Relay khẩn cấp do lỗi nhiệt độ, lỗi điện áp. |

---

## 7. Yêu cầu Cấu hình Network & Local Web Dashboard (ESP32)
Để đảm bảo tính công nghiệp, chống nhiễu và dễ dàng triển khai, Gateway ESP32 cần tuân thủ cấu trúc mạng và tích hợp Server nội bộ như sau:

### 7.1. Chuẩn kết nối mạng (Ethernet-First)
- **Hardwire Network:** Sử dụng kết nối cáp mạng LAN thông qua module PHY Ethernet (vd: **LAN8720** qua RMII hoặc **W5500** qua SPI). Tuyệt đối **KHÔNG dùng Wi-Fi** làm kết nối chính để tránh nhiễu sống từ cuộn dây contactor/rơ-le công suất lớn trong trạm sạc.
- **Tính năng Fallback:**
  - Khi có dây LAN, ESP32 sẽ phát DHCP Request hoặc dùng IP tĩnh cấu hình sẵn.
  - Nếu mất mạng (Cable disconnected), ESP32 vẫn duy trì mạng nội bộ Modbus bình thường và lưu Event vào Flash đợi khôi phục.
- **Fail-safe AP:** Trong trường hợp trạm mới tinh chưa có IP, ESP32 sẽ tạm phát một sóng Wi-Fi (Ví dụ: `CHARGER_GW_MAC`) để thợ kỹ thuật dùng điện thoại kết nối vào cấu hình IP tĩnh mạng LAN.

### 7.2. Giao diện Web Dashboard Quản trị nội bộ (Local Web UI)
ESP32 cần chạy một HTTP Web Server hoạt động ở Port 80, cho phép kỹ thuật viên truy cập qua IP LAN (Ví dụ: `http://192.168.1.100`) để khai báo.

**Các thành phần trang Dashboard bắt buộc:**
1. **Trang Giám sát (Overview - Monitoring):**
   - Xem tổng quan tất cả các Trụ sạc (Cabinets) trực thuộc Gateway.
   - Thẻ hiển thị Trạng thái (Online/Offline), Dòng điện, Điện áp, Lỗi từ thanh ghi Modbus ở thời gian thực.
2. **Trang Cấu hình Mạng & MQTT (Network Config):**
   - Đặt IP tĩnh tĩnh, Gateway, Subnet mask.
   - Mật khẩu truy cập Web admin.
   - Setup Host IP Broker MQTT, Port (1883), Username, Password.
3. **Trang Quản lý Tủ sạc Modbus (Modbus Deployment):**
   - **Khai báo "Số lượng tủ" (`total_posts`):** Quy định Gateway này sẽ loop hỏi dữ liệu của bao nhiêu súng sạc. (Ví dụ nhập: 4 súng sạc).
   - Thiết lập Polling Rate (thời gian xoay vòng hỏi từng node Modbus).
   - Tùy chọn Start ID (Ví dụ địa chỉ Modbus bắt đầu từ Slave 1).
4. **Trang Công cụ gỡ lỗi (Diagnostics/Tools):**
   - Có nút nhấn kiểm tra ping ra Internet.
   - Form gửi lệnh thủ công (Manual Override): Unlock cửa tủ số N, Xóa lỗi trạm N thủ công.
