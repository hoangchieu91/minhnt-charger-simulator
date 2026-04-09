#ifndef DLT645_METER_H
#define DLT645_METER_H

#include <stdint.h>
#include <stdbool.h>

/* Data Identifiers (DI) - DEC-Psmart (SF80C-20) Verified */
#define DLT_DI_VOLTAGE    0x02010100  /* Phase A Voltage */
#define DLT_DI_CURRENT    0x02020100  /* Phase A Current */
#define DLT_DI_POWER      0x02030000  /* Total Active Power */
#define DLT_DI_ENERGY     0x00010000  /* Total Forward Active Energy */
#define DLT_DI_FREQ       0x02800002  /* Grid Frequency */
#define DLT_DI_PF         0x02060000  /* Total Power Factor */

/* Control Codes */
#define DLT_CMD_READ        0x11     /* Master: Read data */
#define DLT_CMD_READ_ADDR   0x13     /* Master: Read address */
#define DLT_RESP_READ_OK    0x91     /* Slave: Response OK (read) */
#define DLT_RESP_READ_ADDR_OK 0x93   /* Slave: Response OK (read addr) */
#define DLT_RESP_READ_ERR   0xD1     /* Slave: Response Error */

/* Broadcast Address for DLT645-2007 (6 bytes of 0xAA) */
#define DLT_BROADCAST_ADDR  {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}

typedef struct {
    uint8_t addr[6];    /* Source address from response */
    float voltage;      /* Volts */
    float current;      /* Amps */
    float power;        /* kW */
    float energy;       /* kWh */
    float frequency;    /* Hz */
    float power_factor; /* - */
    bool  is_valid;
} DLT645_Data_t;

/**
 * @brief Constructs a Point-to-Point broadcast Read frame
 * @param di The Data Identifier (e.g., DLT_DI_VOLTAGE)
 * @param out_buffer The buffer to store the constructed frame Request (must be >= 18 bytes)
 * @return The length of the constructed frame
 */
uint16_t DLT645_BuildReadRequest(const uint8_t *addr, uint32_t di, uint8_t *out_buffer);

/**
 * @brief Constructs a Read Address frame (0x13) to discover meter serial
 * @param out_buffer Output buffer (>= 12 bytes)
 * @return Frame length
 */
uint16_t DLT645_BuildAddrRequest(uint8_t *out_buffer);

/**
 * @brief Parses an incoming frame into workable floating-point data
 * @param frame The raw byte array arriving from RS485 RX
 * @param length The number of bytes received
 * @param parsed_data Struct pointer to be updated with Extracted values
 * @return true if cleanly parsed and checksum matches, false otherwise
 */
bool DLT645_ParseFrame(const uint8_t *frame, uint16_t length, DLT645_Data_t *parsed_data);

#endif // DLT645_METER_H
