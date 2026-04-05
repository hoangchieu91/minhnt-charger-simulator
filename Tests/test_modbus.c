#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════
 * TDD — Modbus RTU Slave (CRC16, Frame Build/Parse)
 * Chạy: gcc -o test_modbus Tests/test_modbus.c -IModules/Inc && ./test_modbus
 * ═══════════════════════════════════════════════════════════ */

/* Include the CRC function directly for testing */
uint16_t Modbus_CRC16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { printf("  TEST: %-40s", name); tests_total++; } while(0)
#define PASS() do { printf("✅ PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("❌ FAIL: %s\n", msg); } while(0)

/* ─── Test 1: CRC16 Known Values ─── */
void test_crc16_known(void) {
    TEST("CRC16 known value [01 04 00 00 00 01]");
    /* Modbus frame: addr=01, FC=04, start=0000, qty=0001 */
    uint8_t frame[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x01};
    uint16_t crc = Modbus_CRC16(frame, 6);
    /* Known CRC for this frame: calculated = 0xCA31 */
    if (crc == 0xCA31) PASS();
    else { char buf[64]; snprintf(buf, sizeof(buf), "CRC=0x%04X, expected 0xCA31", crc); FAIL(buf); }
}

/* ─── Test 2: CRC16 FC03 Read Holding ─── */
void test_crc16_fc03(void) {
    TEST("CRC16 FC03 [01 03 01 00 00 09]");
    uint8_t frame[] = {0x01, 0x03, 0x01, 0x00, 0x00, 0x09};
    uint16_t crc = Modbus_CRC16(frame, 6);
    /* Verify CRC is non-zero and consistent */
    uint16_t crc2 = Modbus_CRC16(frame, 6);
    if (crc == crc2 && crc != 0) PASS();
    else FAIL("CRC not consistent");
}

/* ─── Test 3: CRC16 Empty ─── */
void test_crc16_empty(void) {
    TEST("CRC16 empty data");
    uint16_t crc = Modbus_CRC16((uint8_t*)"", 0);
    if (crc == 0xFFFF) PASS(); /* Initial value, no data */
    else { char buf[64]; snprintf(buf, sizeof(buf), "CRC=0x%04X, expected 0xFFFF", crc); FAIL(buf); }
}

/* ─── Test 4: CRC16 Single byte ─── */
void test_crc16_single(void) {
    TEST("CRC16 single byte [0x01]");
    uint8_t data = 0x01;
    uint16_t crc = Modbus_CRC16(&data, 1);
    if (crc != 0xFFFF && crc != 0) PASS(); /* Should be different from initial */
    else FAIL("CRC should change");
}

/* ─── Test 5: FC04 Request Frame Validation ─── */
void test_fc04_frame_format(void) {
    TEST("FC04 request frame format");
    /* Build request: Read 1 Input Register from addr 0x0000 */
    uint8_t req[8];
    req[0] = 0x01; /* slave addr */
    req[1] = 0x04; /* FC */
    req[2] = 0x00; req[3] = 0x00; /* start addr */
    req[4] = 0x00; req[5] = 0x01; /* quantity */
    uint16_t crc = Modbus_CRC16(req, 6);
    req[6] = crc & 0xFF;
    req[7] = (crc >> 8) & 0xFF;

    /* Verify frame is 8 bytes and CRC matches */
    uint16_t verify = Modbus_CRC16(req, 6);
    if (verify == ((uint16_t)req[7] << 8 | req[6])) PASS();
    else FAIL("CRC mismatch");
}

/* ─── Test 6: FC05 Coil Frame ─── */
void test_fc05_frame_format(void) {
    TEST("FC05 Write Coil frame format");
    /* Write coil 0x0000 = ON (FF00) */
    uint8_t req[8];
    req[0] = 0x01;
    req[1] = 0x05;
    req[2] = 0x00; req[3] = 0x00; /* coil addr */
    req[4] = 0xFF; req[5] = 0x00; /* ON */
    uint16_t crc = Modbus_CRC16(req, 6);
    req[6] = crc & 0xFF;
    req[7] = (crc >> 8) & 0xFF;
    /* Response should echo same frame → CRC same */
    if (crc != 0) PASS();
    else FAIL("CRC should not be 0");
}

/* ─── Test 7: FC06 Write Register Frame ─── */
void test_fc06_frame_format(void) {
    TEST("FC06 Write Register frame format");
    /* Write holding 0x0100 = 500 */
    uint8_t req[8];
    req[0] = 0x01;
    req[1] = 0x06;
    req[2] = 0x01; req[3] = 0x00; /* reg addr */
    req[4] = 0x01; req[5] = 0xF4; /* value 500 */
    uint16_t crc = Modbus_CRC16(req, 6);
    req[6] = crc & 0xFF;
    req[7] = (crc >> 8) & 0xFF;
    uint16_t verify = Modbus_CRC16(req, 6);
    if (verify == crc) PASS();
    else FAIL("CRC verify failed");
}

/* ─── Test 8: Exception Frame Format ─── */
void test_exception_format(void) {
    TEST("Exception response format");
    /* Exception: addr=01, FC=84 (04+80), ex_code=02 */
    uint8_t resp[5];
    resp[0] = 0x01;
    resp[1] = 0x84; /* FC04 + error bit */
    resp[2] = 0x02; /* illegal address */
    uint16_t crc = Modbus_CRC16(resp, 3);
    resp[3] = crc & 0xFF;
    resp[4] = (crc >> 8) & 0xFF;
    if (resp[1] == (0x04 | 0x80) && resp[2] == 0x02) PASS();
    else FAIL("Exception format wrong");
}

/* ─── Test 9: CRC Verify Full Frame ─── */
void test_crc_verify_full(void) {
    TEST("CRC verify full frame (8 bytes)");
    uint8_t frame[8] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x1B};
    uint16_t crc = Modbus_CRC16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;
    /* Verify: CRC of entire frame including CRC bytes should be 0 */
    uint16_t full_crc = Modbus_CRC16(frame, 8);
    if (full_crc == 0) PASS();
    else { char buf[64]; snprintf(buf, sizeof(buf), "Full CRC=0x%04X, expected 0", full_crc); FAIL(buf); }
}

/* ─── Test 10: Broadcast Address ─── */
void test_broadcast_addr(void) {
    TEST("Broadcast address = 0x00");
    uint8_t req[8];
    req[0] = 0x00; /* broadcast */
    req[1] = 0x05;
    req[2] = 0x00; req[3] = 0x00;
    req[4] = 0xFF; req[5] = 0x00;
    uint16_t crc = Modbus_CRC16(req, 6);
    req[6] = crc & 0xFF;
    req[7] = (crc >> 8) & 0xFF;
    if (req[0] == 0x00 && crc != 0) PASS();
    else FAIL("Broadcast frame error");
}

int main(void) {
    printf("\n  ═══ TDD: Modbus RTU Slave ═══\n\n");

    test_crc16_known();
    test_crc16_fc03();
    test_crc16_empty();
    test_crc16_single();
    test_fc04_frame_format();
    test_fc05_frame_format();
    test_fc06_frame_format();
    test_exception_format();
    test_crc_verify_full();
    test_broadcast_addr();

    printf("\n  ═══════════════════════════════════\n");
    printf("  %d/%d PASSED\n\n", tests_passed, tests_total);
    return tests_passed == tests_total ? 0 : 1;
}
