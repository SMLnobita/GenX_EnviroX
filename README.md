# GenX_EnviroX 🌡️💨

> Hệ Thống Giám Sát Môi Trường Thông Minh với Kết Nối IoT

## 📋 Tổng Quan

Hệ thống giám sát môi trường thời gian thực đo **nhiệt độ**, **độ ẩm** và **nồng độ khí gas** sử dụng vi điều khiển STM32F407 với truyền dữ liệu không dây qua ESP8266.

## 🚀 Tính Năng

- 🌡️ **Giám sát nhiệt độ & độ ẩm** (DHT11)
- 💨 **Phát hiện khí gas** với các mức cảnh báo (MQ2)
- 📺 **Hiển thị thời gian thực** trên màn hình OLED
- 📡 **Kết nối IoT** qua ESP8266
- 🚨 **Đèn LED báo hiệu** trạng thái hệ thống
- ⏰ **Truyền dữ liệu thời gian thực** mỗi 2 giây
- 🔒 **Bảo vệ timeout** và xử lý lỗi

## 🛠️ Linh Kiện Phần Cứng

| Linh Kiện | Model | Chức Năng |
|-----------|-------|-----------|
| Vi điều khiển | STM32F407VGT6 | Đơn vị xử lý chính |
| Cảm biến nhiệt độ/độ ẩm | DHT11 | Cảm nhận môi trường |
| Cảm biến khí gas | MQ2 | Phát hiện nồng độ khí |
| Module WiFi | ESP8266 | Truyền dữ liệu IoT |
| Board phát triển | STM32F4 Discovery | Nền tảng |

## 📌 Cấu Hình Chân

### Cảm Biến
- **DHT11**: `PA1` (Chân dữ liệu)
- **MQ2**: `PA0` (Đầu vào analog)

### Giao Tiếp
- **ESP8266 UART5**: `PC12` (TX), `PD2` (RX)
- **OLED I2C**: `PB6` (SCL), `PB7` (SDA)

### Đèn LED Báo Trạng Thái
- **LED UART**: `PD13` (Cam) - Báo truyền dữ liệu
- **LED DHT11**: `PD15` (Xanh dương) - Trạng thái DHT11
- **LED Cảnh Báo MQ2**: `PD14` (Đỏ) - Cảnh báo mức gas

## ⚙️ Ngoại Vi Sử Dụng

### Timer
- **TIM2**: Ngắt 1Hz để kích hoạt ADC
- **TIM4**: Delay microsecond cho giao thức DHT11

### Giao Tiếp
- **ADC1**: Đọc cảm biến gas MQ2
- **UART5**: Giao tiếp ESP8266 (115200 baud)
- **I2C1**: Giao tiếp OLED display (400kHz)

### GPIO
- **Output**: Đèn LED báo hiệu, điều khiển DHT11
- **Input**: Dữ liệu DHT11, giám sát hệ thống

## 📊 Định Dạng Dữ Liệu

**UART Transmission (every 2s):**
```
Temperature: XX.X°C, Humidity: XX.X%, Gas: XXXX ppm, Level: NORMAL/WARNING/DANGER
```

**OLED Display:**
```
┌─────────────────┐
│ EnviroX Monitor │
├─────────────────┤
│ Temp: 25.3°C    │
│ Humi: 60.2%     │
│ Gas:  245 ppm   │
└─────────────────┘
```

## 🚨 Hệ Thống Cảnh Báo

### Ngưỡng Mức Gas
- 🟢 **BÌNH THƯỜNG**: < 300 ppm (LED TẮT)
- 🟡 **CẢNH BÁO**: 300-1000 ppm (LED nhấp nháy 500ms)
- 🔴 **NGUY HIỂM**: > 1000 ppm (LED nhấp nháy 200ms)

### Đèn LED Báo Trạng Thái
- **LED Cam (PD13)**: Chớp khi truyền dữ liệu
- **LED Xanh Dương (PD15)**: Trạng thái DHT11 (SẮN=OK, Nhấp Nháy=Lỗi)
- **LED Đỏ (PD14)**: Mức cảnh báo gas

## 🔄 Hoạt Động Hệ Thống

1. **Khởi Tạo**: Cấu hình ngoại vi và cảm biến
2. **Thu Thập Dữ Liệu**: 
   - DHT11 đọc nhiệt độ/độ ẩm
   - MQ2 đo nồng độ gas qua ADC
3. **Hiển Thị**: Cập nhật dữ liệu lên màn hình OLED
4. **Xử Lý**: Xác thực dữ liệu và xác định mức cảnh báo
5. **Truyền Tải**: Gửi dữ liệu đến ESP8266 mỗi 2 giây
6. **Cập Nhật Trạng Thái**: Cập nhật đèn LED báo hiệu

## 💻 Môi Trường Phát Triển

- **IDE**: STM32CubeIDE
- **Thư Viện HAL**: STM32F4xx HAL
- **Debugger**: ST-Link
- **Ngôn Ngữ**: C

## 📈 Thông Số Kỹ Thuật

| Thông Số | Giá Trị |
|-----------|---------|
| Xung Clock Hệ Thống | 8 MHz (HSI) |
| Độ Phân Giải ADC | 12-bit |
| Độ Chính Xác DHT11 | ±2°C, ±5%RH |
| Tốc Độ UART | 115200 bps |
| Tốc Độ I2C | 400 kHz (Fast Mode) |
| Độ Phân Giải OLED | 128x64 pixels (1.3") |
| Tần Suất Cập Nhật | 2 giây |
| Nguồn Cung Cấp | 5V qua USB |

## 🚀 Bắt Đầu

### Yêu Cầu Tiên Quyết
- STM32CubeIDE đã cài đặt
- Board STM32F4 Discovery
- Module DHT11, MQ2, ESP8266, OLED 1.3"
- Dây nối và breadboard

### Cài Đặt
1. Clone repository này
2. Mở project trong STM32CubeIDE
3. Kết nối phần cứng theo cấu hình chân
4. Build và flash vào STM32F407
5. Cấu hình ESP8266 cho mạng WiFi của bạn

### Sử Dụng
1. Bật nguồn hệ thống
2. Đợi khởi tạo (đèn LED báo hiệu)
3. Xem dữ liệu trực tiếp trên màn hình OLED
4. Theo dõi serial output để xem truyền dữ liệu
5. Kiểm tra nền tảng IoT để nhận dữ liệu

----
*Được xây dựng với ❤️ và STM32F407*
