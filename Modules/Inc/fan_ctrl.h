#ifndef FAN_CTRL_H
#define FAN_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Fan Controller với Hysteresis + Anti-chatter
 * 
 * - Bật khi temp >= high_temp
 * - Tắt khi temp <= low_temp
 * - Giữ nguyên khi low_temp < temp < high_temp (hysteresis)
 * - Minimum ON time: 30s (bảo vệ động cơ)
 * - Auto-clamp nếu set sai ngưỡng
 */

#define FAN_MIN_ON_TIME_MS   30000U   /* Quạt bật tối thiểu 30 giây */
#define FAN_TEMP_GAP_MIN     50       /* Gap tối thiểu 5.0°C (đơn vị 0.1°C) */
#define FAN_TEMP_FLOOR       300      /* Sàn high_temp: 30.0°C */
#define FAN_TEMP_CEILING     800      /* Trần high_temp: 80.0°C */

typedef struct {
    void     (*write_fan)(uint8_t state);
    uint32_t (*get_tick)(void);
} FanHardwareConfig_t;

void Fan_Init(FanHardwareConfig_t *config);

/**
 * @brief Đặt ngưỡng bật/tắt quạt (đơn vị 0.1°C)
 * @param high_temp_01c Nhiệt độ bật quạt (VD: 450 = 45.0°C)
 * @param low_temp_01c  Nhiệt độ tắt quạt (VD: 380 = 38.0°C)
 * @note Tự động clamp nếu giá trị không hợp lệ
 */
void Fan_SetThresholds(int16_t high_temp_01c, int16_t low_temp_01c);

/**
 * @brief Xử lý logic quạt (gọi liên tục trong main loop)
 * @param current_temp_01c Nhiệt độ hiện tại (0.1°C)
 */
void Fan_Process(int16_t current_temp_01c);

/** @brief Lấy trạng thái quạt hiện tại (0=OFF, 1=ON) */
uint8_t Fan_GetState(void);

/** @brief Lấy ngưỡng hiện tại (sau clamp) */
int16_t Fan_GetHighTemp(void);
int16_t Fan_GetLowTemp(void);

#endif // FAN_CTRL_H
