#ifndef DLT645_METER_H
#define DLT645_METER_H

#include <stdint.h>
#include <stdbool.h>

/* Data Identifiers (DI) defined in DLT645-2007 */
#define DLT_DI_VOLTAGE    0x00010102
#define DLT_DI_CURRENT    0x00010202
#define DLT_DI_POWER      0x00010302
#define DLT_DI_ENERGY     0x00000100

typedef struct {
    float voltage;  /* Volts */
    float current;  /* Amps */
    float power;    /* kW */
    float energy;   /* kWh */
    bool  is_valid;
} DLT645_Data_t;

/**
 * @brief Constructs a Point-to-Point broadcast Read frame
 * @param di The Data Identifier (e.g., DLT_DI_VOLTAGE)
 * @param out_buffer The buffer to store the constructed frame Request (must be >= 18 bytes)
 * @return The length of the constructed frame
 */
uint16_t DLT645_BuildReadRequest(uint32_t di, uint8_t *out_buffer);

/**
 * @brief Parses an incoming frame into workable floating-point data
 * @param frame The raw byte array arriving from RS485 RX
 * @param length The number of bytes received
 * @param parsed_data Struct pointer to be updated with Extracted values
 * @return true if cleanly parsed and checksum matches, false otherwise
 */
bool DLT645_ParseFrame(const uint8_t *frame, uint16_t length, DLT645_Data_t *parsed_data);

#endif // DLT645_METER_H
