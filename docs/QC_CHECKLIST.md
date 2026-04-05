# CHECKLIST KIỂM SOÁT CHẤT LƯỢNG — TRU V1.0

> **Cập nhật:** 23/03/2026

---

## A. Kiểm tra phần cứng (Hardware QC)

- [ ] Kiểm tra nguồn 3.3V ổn định (đo bằng oscilloscope, ripple < 50mV)
- [ ] Kiểm tra thạch anh 8MHz hoạt động (đo tần số output)
- [ ] Kiểm tra MCU boot thành công (SWD kết nối OK)
- [ ] Đo điện áp tại chân PA0 (ADC) khi có NTC20K
- [ ] Kiểm tra mạch RS485_1: TX/RX/DE hoạt động
- [ ] Kiểm tra mạch RS485_2: TX/RX/DE hoạt động
- [ ] Kiểm tra LED Red (PA8) sáng khi set HIGH
- [ ] Kiểm tra LED Green (PA11) sáng khi set HIGH
- [ ] Kiểm tra LED White (PA12) sáng khi set HIGH
- [ ] Kiểm tra Relay RL1 (PB0) đóng/ngắt
- [ ] Kiểm tra Relay RL2 (PB1) đóng/ngắt
- [ ] Kiểm tra Relay RL3 (PB10) đóng/ngắt
- [ ] Kiểm tra Relay RL4 (PB11) đóng/ngắt
- [ ] Kiểm tra DIP Switch ADD0/1/2 (PB5/6/7) đọc đúng giá trị
- [ ] Kiểm tra Door Sensor (PB12) phản hồi đúng

## B. Kiểm tra firmware (Firmware QC)

- [ ] Firmware nạp thành công qua SWD
- [ ] LED hiển thị đúng trạng thái Idle (Trắng sáng liên tục)
- [ ] LED nhấp nháy đúng chu kỳ 500ms (Standby/Charging)
- [ ] Chuyển trạng thái LED đúng theo bảng trạng thái
- [ ] ADC đọc nhiệt độ chính xác (sai số < ±2°C)
- [ ] Khóa chéo RL1/RL2 hoạt động đúng
- [ ] Dead-time 100ms khi chuyển đổi Relay
- [ ] Modbus Slave phản hồi đúng thanh ghi 0x0100–0x0105
- [ ] Đọc đồng hồ Psmart qua RS485_2 thành công
- [ ] Địa chỉ Modbus đọc đúng từ DIP Switch
- [ ] Door Sensor trigger Error state
- [ ] Watchdog reset MCU khi firmware treo
- [ ] Test liên tục 24h không lỗi

## C. Kiểm tra an toàn (Safety QC)

- [ ] Cách điện giữa mạch 3.3V và mạch lực (> 1kΩ @ 500VDC)
- [ ] Không xảy ra khóa chéo nguồn khi test stress
- [ ] Quá nhiệt → tự động ngắt Relay + LED Error
- [ ] Cửa mở → cảnh báo kịp thời
- [ ] Watchdog phục hồi hệ thống trong mọi trường hợp treo

## D. Kiểm tra trước bàn giao

- [ ] Tài liệu đặc tả hoàn chỉnh
- [ ] Hướng dẫn vận hành đã viết
- [ ] Hướng dẫn nạp firmware đã viết
- [ ] Source code đã lưu trữ (Git/SVN)
- [ ] BOM cuối cùng đã xác nhận
- [ ] Sơ đồ nguyên lý bản cuối đã ký duyệt
- [ ] Test report đã ký

---

> **Người kiểm tra:** ________________  
> **Ngày kiểm tra:** ___/___/______  
> **Kết quả:** ☐ ĐẠT &nbsp;&nbsp; ☐ KHÔNG ĐẠT
