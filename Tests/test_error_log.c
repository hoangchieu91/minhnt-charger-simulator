#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════
 * TDD — Error Log (Flash mock in RAM)
 * gcc -o test_errlog Tests/test_error_log.c Modules/Src/error_log.c -IModules/Inc -Wall
 * ═══════════════════════════════════════════════════════════ */

/* Mock Flash — RAM buffer simulating 2 pages */
static uint8_t mock_flash[2048];  /* 2 × 1KB pages */
#define MOCK_BASE 0x0800F000U

static int mock_erase(uint32_t addr) {
    uint32_t offset = addr - MOCK_BASE;
    if (offset < 2048) {
        memset(&mock_flash[offset], 0xFF, 1024);
        return 0;
    }
    return -1;
}

static int mock_write(uint32_t addr, const uint8_t *data, uint16_t len) {
    uint32_t offset = addr - MOCK_BASE;
    if (offset + len <= 2048) {
        memcpy(&mock_flash[offset], data, len);
        return 0;
    }
    return -1;
}

static void mock_read(uint32_t addr, uint8_t *data, uint16_t len) {
    uint32_t offset = addr - MOCK_BASE;
    if (offset + len <= 2048) {
        memcpy(data, &mock_flash[offset], len);
    }
}

#include "error_log.h"

static ErrLogFlashConfig_t mock_cfg = { mock_erase, mock_write, mock_read };

static int tests_passed = 0, tests_total = 0;
#define TEST(name) do { printf("  TEST: %-45s", name); tests_total++; } while(0)
#define PASS() do { printf("✅ PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("❌ FAIL: %s\n", msg); } while(0)

/* ─── Test 1: Init fresh ─── */
void test_init_fresh(void) {
    TEST("Init fresh (no existing data)");
    memset(mock_flash, 0xFF, sizeof(mock_flash));
    ErrLog_Init(&mock_cfg);
    if (ErrLog_GetBootCount() == 1 && ErrLog_GetTotalErrors() == 0
        && ErrLog_GetNextEventId() == 1) PASS();
    else FAIL("boot_count or next_event_id wrong");
}

/* ─── Test 2: Boot count increments ─── */
void test_boot_count(void) {
    TEST("Boot count increments on re-init");
    /* Don't clear flash — re-init should increment */
    ErrLog_Init(&mock_cfg);
    if (ErrLog_GetBootCount() == 2) PASS();
    else { char b[64]; snprintf(b,sizeof(b),"boot=%u", (unsigned)ErrLog_GetBootCount()); FAIL(b); }
}

/* ─── Test 3: Record error ─── */
void test_record_error(void) {
    TEST("Record error event");
    ErrLog_RecordError(1, 3, 720, 3200, 3600); /* OVERTEMP, CHARGING, 72°C, 3200W, 1h */
    if (ErrLog_GetTotalErrors() == 1 && ErrLog_GetNextEventId() == 2) PASS();
    else FAIL("error count or event_id wrong");
}

/* ─── Test 4: Get last event ─── */
void test_last_event(void) {
    TEST("Get last event data");
    ErrorEvent_t *ev = ErrLog_GetLastEvent();
    if (ev && ev->event_id == 1 && ev->error_type == 1
        && ev->temp == 720 && ev->power == 3200) PASS();
    else FAIL("event data mismatch");
}

/* ─── Test 5: Multiple errors ─── */
void test_multiple_errors(void) {
    TEST("Multiple errors have sequential IDs");
    ErrLog_RecordError(2, 3, 250, 0, 3700);    /* TAMPER */
    ErrLog_RecordError(5, 0, 250, 0, 3800);    /* VOLTAGE */
    ErrorEvent_t *ev = ErrLog_GetLastEvent();
    if (ev && ev->event_id == 3 && ev->error_type == 5
        && ErrLog_GetTotalErrors() == 3) PASS();
    else FAIL("sequential ID or count wrong");
}

/* ─── Test 6: Charge counter ─── */
void test_charge_counter(void) {
    TEST("Charge counter increments");
    ErrLog_IncrementCharge();
    ErrLog_IncrementCharge();
    if (ErrLog_GetTotalCharges() == 2) PASS();
    else FAIL("charge count wrong");
}

/* ─── Test 7: Energy accumulation ─── */
void test_energy(void) {
    TEST("Energy accumulates");
    ErrLog_AddEnergy(5000);
    ErrLog_AddEnergy(3000);
    if (ErrLog_GetTotalEnergy() == 8000) PASS();
    else FAIL("energy wrong");
}

/* ─── Test 8: Persistence across reboot ─── */
void test_persistence(void) {
    TEST("Persistence across simulated reboot");
    ErrLog_Save();
    /* Simulate reboot — re-init from same flash */
    ErrLog_Init(&mock_cfg);
    if (ErrLog_GetBootCount() == 3  /* was 2, now 3 */
        && ErrLog_GetTotalErrors() == 3
        && ErrLog_GetTotalCharges() == 2
        && ErrLog_GetTotalEnergy() == 8000) PASS();
    else {
        char b[128]; snprintf(b,sizeof(b),"boot=%u err=%u chg=%u nrg=%u",
            (unsigned)ErrLog_GetBootCount(), (unsigned)ErrLog_GetTotalErrors(),
            (unsigned)ErrLog_GetTotalCharges(), (unsigned)ErrLog_GetTotalEnergy());
        FAIL(b);
    }
}

/* ─── Test 9: Circular overflow ─── */
void test_circular(void) {
    TEST("Circular buffer overflow (>80 events)");
    memset(mock_flash, 0xFF, sizeof(mock_flash));
    ErrLog_Init(&mock_cfg);
    for (int i = 0; i < 85; i++) {
        ErrLog_RecordError(1, 0, 250, 0, (uint32_t)i);
    }
    /* Should have 85 total, event_id=86 next */
    ErrorEvent_t *ev = ErrLog_GetLastEvent();
    if (ErrLog_GetTotalErrors() == 85 && ev && ev->event_id == 85) PASS();
    else {
        char b[64]; snprintf(b,sizeof(b),"err=%u evid=%u",
            (unsigned)ErrLog_GetTotalErrors(), ev ? ev->event_id : 0);
        FAIL(b);
    }
}

/* ─── Test 10: No flash (null config) ─── */
void test_no_flash(void) {
    TEST("Init with NULL flash (RAM only)");
    ErrLog_Init((void*)0);
    ErrLog_RecordError(1, 0, 250, 0, 100);
    if (ErrLog_GetTotalErrors() == 1) PASS();
    else FAIL("should work in RAM");
}

int main(void) {
    printf("\n  ═══ TDD: Error Log ═══\n\n");

    test_init_fresh();
    test_boot_count();
    test_record_error();
    test_last_event();
    test_multiple_errors();
    test_charge_counter();
    test_energy();
    test_persistence();
    test_circular();
    test_no_flash();

    printf("\n  ═══════════════════════════════════════════\n");
    printf("  %d/%d PASSED\n\n", tests_passed, tests_total);
    return tests_passed == tests_total ? 0 : 1;
}
