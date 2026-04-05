#include "dlt645_meter.h"
#include <stddef.h>

/**
 * @brief Convert from BCD Format to Decimal
 */
static uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

uint16_t DLT645_BuildReadRequest(uint32_t di, uint8_t *out_buffer) {
    if (!out_buffer) return 0;
    
    uint8_t i = 0;
    out_buffer[i++] = 0x68;              // Sync byte 1
    
    // Broadcast Address: AA AA AA AA AA AA
    for(int j=0; j<6; j++) {
        out_buffer[i++] = 0xAA;
    }
    
    out_buffer[i++] = 0x68;              // Sync byte 2
    out_buffer[i++] = 0x11;              // Control Code: Read Data (From Master)
    out_buffer[i++] = 0x04;              // Length: 4 bytes for DI
    
    // Data Mask +0x33 rule (Transmit LSB first)
    out_buffer[i++] = (uint8_t)((di & 0xFF) + 0x33);
    out_buffer[i++] = (uint8_t)(((di >> 8) & 0xFF) + 0x33);
    out_buffer[i++] = (uint8_t)(((di >> 16) & 0xFF) + 0x33);
    out_buffer[i++] = (uint8_t)(((di >> 24) & 0xFF) + 0x33);
    
    // Checksum: Sum(mod 256) of bytes starting from first 0x68
    uint8_t cs = 0;
    for(int j=0; j<i; j++) {
        cs += out_buffer[j];
    }
    out_buffer[i++] = cs;
    
    out_buffer[i++] = 0x16;              // End byte
    
    return i; // Total length
}

bool DLT645_ParseFrame(const uint8_t *frame, uint16_t length, DLT645_Data_t *parsed_data) {
    if (!frame || !parsed_data || length < 12) return false;
    
    // Hunt for the first 0x68 sync byte
    uint16_t start = 0;
    while(start < length && frame[start] != 0x68) start++;
    if (start + 12 > length) return false; // Incomplete frame
    
    // Verify the second 0x68 sync byte
    if (frame[start+7] != 0x68) return false;
    
    uint8_t ctrl = frame[start+8];
    uint8_t L    = frame[start+9];
    
    // Boundary check + Validate Ending Sequence 0x16
    if (start + 10 + L + 2 > length) return false; 
    if (frame[start + 10 + L + 1] != 0x16) return false;
    
    // Validate CS (Checksum)
    uint8_t cs = 0;
    for(uint16_t i = start; i < start + 10 + L; i++) {
        cs += frame[i];
    }
    if (cs != frame[start + 10 + L]) return false;
    
    // Accept specifically Normal Data Response (0x91)
    if (ctrl == 0x91 && L >= 6) { 
        // Reverse Data Mask (-0x33) for Data Identifier (DI)
        uint32_t rx_di = 0;
        rx_di |= (uint32_t)(frame[start+10] - 0x33);
        rx_di |= (uint32_t)(frame[start+11] - 0x33) << 8;
        rx_di |= (uint32_t)(frame[start+12] - 0x33) << 16;
        rx_di |= (uint32_t)(frame[start+13] - 0x33) << 24;
        
        // Voltage Decode: XXX.X (2 bytes)
        if (rx_di == DLT_DI_VOLTAGE && L == 6) { 
            uint8_t buf[2];
            for(int k=0; k<2; k++) buf[k] = frame[start+14+k] - 0x33;
            // BCD LSB -> MSB
            parsed_data->voltage = (float)(bcd_to_dec(buf[1])*100 + bcd_to_dec(buf[0])) / 10.0f;
            parsed_data->is_valid = true;
            return true;
        }
        
        // Current Decode: XXX.XXX (3 bytes)
        if (rx_di == DLT_DI_CURRENT && L == 7) { 
            uint8_t buf[3];
            for(int k=0; k<3; k++) buf[k] = frame[start+14+k] - 0x33;
            parsed_data->current = (float)(bcd_to_dec(buf[2])*10000 + bcd_to_dec(buf[1])*100 + bcd_to_dec(buf[0])) / 1000.0f;
            parsed_data->is_valid = true;
            return true;
        }
        
        // Energy Decode: XXXXXX.XX (4 bytes)
        if (rx_di == DLT_DI_ENERGY && L == 8) { 
            uint8_t buf[4];
            for(int k=0; k<4; k++) buf[k] = frame[start+14+k] - 0x33;
            parsed_data->energy = (float)(bcd_to_dec(buf[3])*1000000 + bcd_to_dec(buf[2])*10000 + bcd_to_dec(buf[1])*100 + bcd_to_dec(buf[0])) / 100.0f;
            parsed_data->is_valid = true;
            return true;
        }
    }
    
    return false;
}
