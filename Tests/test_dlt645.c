#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "dlt645_meter.h"

void print_hex(const char* label, const uint8_t* buf, uint16_t len) {
    printf("%s: ", label);
    for(uint16_t i=0; i<len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

int main() {
    printf("==========================================\n");
    printf("= Starting DLT645-2007 TDD Native Tests  =\n");
    printf("==========================================\n\n");

    /* TEST 1: Frame Generation (Voltage) */
    printf("TEST 1: Generate Read Request for Voltage (0x00010102)\n");
    uint8_t tx_buf[32];
    uint16_t tx_len = DLT645_BuildReadRequest(DLT_DI_VOLTAGE, tx_buf);
    
    // Expected Frame:
    // Sync(68) Addr(AA AA AA AA AA AA) Sync(68) Ctrl(11) L(04)
    // DI with +0x33 LSB first: 0x02+33=35, 0x01+33=34, 0x01+33=34, 0x00+33=33 -> (35 34 34 33) 
    // CS = sum % 256
    // End(16)
    print_hex("Generated TX", tx_buf, tx_len);
    assert(tx_len == 16);
    assert(tx_buf[0] == 0x68);
    assert(tx_buf[1] == 0xAA && tx_buf[6] == 0xAA);
    assert(tx_buf[7] == 0x68);
    assert(tx_buf[8] == 0x11);
    assert(tx_buf[9] == 0x04);
    assert(tx_buf[10] == 0x35 && tx_buf[11] == 0x34 && tx_buf[12] == 0x34 && tx_buf[13] == 0x33);
    assert(tx_buf[15] == 0x16);
    printf("--> [PASS] Read Request frame matches standard specifications.\n\n");

    /* TEST 2: Frame Parsing (Voltage : 220.5 V) */
    printf("TEST 2: Parse Voltage Response Frame (220.5V)\n");
    
    DLT645_Data_t m_data = {0};
    
    // Building a raw response frame manually
    // 220.5V -> "22 05" BCD.
    // LSB first: 0x05, 0x22
    // Masked (+0x33): 0x05+0x33=0x38, 0x22+0x33=0x55
    uint8_t rx_voltage[] = {
        0x68, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x68, // Header
        0x91, 0x06,                                     // Normal Read Response, Length=6 (4 DI + 2 Data)
        0x35, 0x34, 0x34, 0x33,                         // DI (+0x33)
        0x38, 0x55,                                     // Data(+0x33)
        0x00, 0x16                                      // Checksum (tbd), End
    };
    
    // Compute checksum manually for our mock
    uint8_t cs = 0;
    for(int i=0; i<16; i++) cs += rx_voltage[i];
    rx_voltage[16] = cs;

    print_hex("Incoming RX ", rx_voltage, 18);
    
    bool ok = DLT645_ParseFrame(rx_voltage, 18, &m_data);
    assert(ok == true);
    assert(m_data.is_valid == true);
    printf("Extracted Voltage: %.1f\n", m_data.voltage);
    assert(m_data.voltage == 220.5f);
    printf("--> [PASS] Correctly decoded Voltage from raw bytes.\n\n");


    /* TEST 3: Frame Parsing (Energy : 123456.78 kWh) */
    printf("TEST 3: Parse Total Energy Response Frame (123456.78 kWh)\n");
    m_data.is_valid = false;
    
    // 123456.78 -> BCD split: 12 34 56 78
    // LSB first: 78 56 34 12
    // Masked:  78+33=AB, 56+33=89, 34+33=67, 12+33=45
    // DI Energy (00 00 01 00): 00 01 00 00 -> +0x33: 33 34 33 33
    uint8_t rx_energy[] = {
        0x68, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x68, 
        0x91, 0x08,                                     
        0x33, 0x34, 0x33, 0x33,                         
        0xAB, 0x89, 0x67, 0x45,                                     
        0x00, 0x16                                      
    };
    cs = 0;
    for(int i=0; i<18; i++) cs += rx_energy[i];
    rx_energy[18] = cs;

    ok = DLT645_ParseFrame(rx_energy, 20, &m_data);
    assert(ok == true);
    printf("Extracted Energy : %.2f kWh\n", m_data.energy);
    assert(m_data.energy == 123456.78f);
    printf("--> [PASS] Correctly decoded 4-byte Energy BCD with +0x33 offset.\n\n");

    printf("==========================================\n");
    printf("=> ALL DLT645 PARSER TESTS PASSED! \n");
    printf("==========================================\n");

    return 0;
}
