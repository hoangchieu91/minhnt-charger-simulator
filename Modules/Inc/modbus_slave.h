#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stdint.h>

/**
 * @brief Modbus RTU Slave — TRU V1.0
 * 
 * Xử lý frame Modbus RTU nhận qua UART1 (RS485_1)
 * Hỗ trợ: FC01, FC02, FC03, FC04, FC05, FC06, FC16
 * 
 * Luồng:  RX byte → buffer → silence 3.5 char → parse → respond TX
 */

#define MODBUS_MAX_FRAME   256
#define MODBUS_SILENCE_MS  200    /* Tăng lên 200ms để ổn định trong mô phỏng HIL/Renode */

/* Exception codes */
#define MODBUS_EX_ILLEGAL_FUNCTION   0x01
#define MODBUS_EX_ILLEGAL_ADDRESS    0x02
#define MODBUS_EX_ILLEGAL_VALUE      0x03

/* Hardware abstraction */
typedef struct {
    void     (*send_bytes)(const uint8_t *data, uint16_t len);
    uint32_t (*get_tick)(void);
} ModbusHardwareConfig_t;

/**
 * @brief Init Modbus slave
 * @param config  Hardware callbacks
 * @param addr    Slave address (1-247)
 */
void Modbus_Init(ModbusHardwareConfig_t *config, uint8_t addr);

/**
 * @brief Nhận 1 byte từ UART RX interrupt
 * Gọi trong ISR hoặc polling loop
 */
void Modbus_ReceiveByte(uint8_t byte);

/**
 * @brief Xử lý frame (gọi trong main loop)
 * Kiểm tra silence timeout → parse → respond
 */
void Modbus_Process(void);

/**
 * @brief Thay đổi slave address (VD: từ DIP switch)
 */
void Modbus_SetAddress(uint8_t addr);

/**
 * @brief CRC16 Modbus (public cho TDD)
 */
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len);

#endif // MODBUS_SLAVE_H
