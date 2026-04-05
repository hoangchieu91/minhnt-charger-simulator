#ifndef LED_RGW_H
#define LED_RGW_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { LED_RED = 0, LED_GREEN = 1, LED_WHITE = 2 } LedColor_t;
typedef enum { LED_OFF = 0, LED_SOLID = 1, LED_BLINK = 2 } LedMode_t;

typedef struct {
    void     (*write_pin)(LedColor_t color, uint8_t state);
    uint32_t (*get_tick)(void);
} LedHardwareConfig_t;

void LED_Init(LedHardwareConfig_t *config);
void LED_Set(LedColor_t color, LedMode_t mode);
void LED_Process(void); // Cần gọi liên tục ở hàm main

#endif // LED_RGW_H
