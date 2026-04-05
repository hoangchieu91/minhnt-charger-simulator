#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "relay_ctrl.h"

// --- Hardware Mocking ---
static uint8_t mock_pins[4] = {0, 0, 0, 0};
static uint32_t mock_tick = 0;

void mock_write_pin(uint8_t relay_id, uint8_t state) {
    mock_pins[relay_id] = state;
}

uint32_t mock_get_tick(void) {
    return mock_tick;
}

RelayHardwareConfig_t test_hw = {
    .write_pin = mock_write_pin,
    .get_tick  = mock_get_tick
};
// ------------------------

void process_advancing_time(uint32_t ms_to_advance) {
    // Simulate main loop running perfectly every 1ms
    for(uint32_t i=0; i<ms_to_advance; i++) {
        mock_tick++;
        Relay_Process();
    }
}

int main() {
    printf("==========================================\n");
    printf("= Starting Relay Interlock Native Tests  =\n");
    printf("==========================================\n\n");
    
    Relay_Init(&test_hw);

    /* TEST 1: Delay ON for Interlock Relays */
    printf("TEST 1: Delay ON mechanism for RL_CHARGER...\n");
    Relay_SetTarget(RL_CHARGER, RELAY_ON);
    Relay_Process(); // Process immediately at mock_tick=0 to capture start_time correctly
    
    // Run for 50ms - should still be OFF
    process_advancing_time(50);
    assert(mock_pins[RL_CHARGER] == RELAY_OFF);
    
    // Run for another 50ms - should turn ON (reach 100ms dead-time)
    process_advancing_time(50);
    assert(mock_pins[RL_CHARGER] == RELAY_ON);
    printf("--> [PASS] Charger turned ON after exactly 100ms.\n\n");

    /* TEST 2: Active Interlock Override */
    printf("TEST 2: Force Socket ON while Charger is ON...\n");
    // System current state: Charger = ON
    mock_tick = 1000; // time jumps ahead
    
    Relay_SetTarget(RL_SOCKET, RELAY_ON);
    
    // Just 1 tick later
    process_advancing_time(1);
    assert(mock_pins[RL_CHARGER] == RELAY_OFF); // Charger MUST shut off immediately!
    assert(mock_pins[RL_SOCKET] == RELAY_OFF);  // Socket MUST wait for dead-time
    printf("--> [PASS] Charger disconnected immediately to prevent short-circuit.\n");
    
    // Wait for dead-time
    process_advancing_time(100);
    assert(mock_pins[RL_SOCKET] == RELAY_ON);
    printf("--> [PASS] Socket turned ON safely after 100ms dead-time.\n\n");

    /* TEST 3: Non-interlocked devices */
    printf("TEST 3: Non-interlocked relay (Fan) response...\n");
    Relay_SetTarget(RL_FAN, RELAY_ON);
    process_advancing_time(1); // 1 tick
    assert(mock_pins[RL_FAN] == RELAY_ON); // Turns on immediately
    printf("--> [PASS] Fan turned ON immediately (no dead-time).\n\n");

    printf("==========================================\n");
    printf("=> ALL TESTS PASSED SUCCESSFULLY! \n");
    printf("==========================================\n");

    return 0;
}
