#ifndef NTC_TEMP_H
#define NTC_TEMP_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief NTC20K Temperature Sensor Module
 * Mạch: R_up = 10kΩ pullup to Vcc=3.3V, NTC20K nối GND
 * ADC 12-bit → R_NTC → Nhiệt độ °C (Steinhart-Hart lookup)
 * Bộ lọc Moving Average N=8 chống nhiễu
 */

#define NTC_FILTER_SIZE 8

typedef struct {
    uint16_t (*read_adc)(void);  // Dependency Injection cho TDD
} NTC_HardwareConfig_t;

/**
 * @brief Khởi tạo module NTC
 * @param config Con trỏ tới cấu hình phần cứng (không NULL)
 */
void NTC_Init(NTC_HardwareConfig_t *config);

/**
 * @brief Lấy mẫu ADC mới nhất và đẩy vào bộ lọc (gọi liên tục trong main loop)
 */
void NTC_Process(void);

/**
 * @brief Lấy giá trị ADC thô mới nhất
 * @return Giá trị 0–4095
 */
uint16_t NTC_GetRawADC(void);

/**
 * @brief Lấy giá trị điện trở NTC (đã lọc)
 * @return Ohm (VD: 20000 = 20kΩ tại 25°C)
 */
uint32_t NTC_GetResistance(void);

/**
 * @brief Lấy nhiệt độ đã quy đổi
 * @return int16_t đơn vị 0.1°C (VD: 455 = 45.5°C, -100 = -10.0°C)
 */
int16_t NTC_GetTempC(void);

#endif // NTC_TEMP_H
