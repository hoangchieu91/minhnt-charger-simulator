#include "relay_ctrl.h"
#include <stddef.h>

#define INTERLOCK_DELAY_MS 100 // Thoi gian tre (Dead-time) de dap ho quang: 100ms

static RelayHardwareConfig_t *hw = NULL;

static uint8_t target_state[4] = {0, 0, 0, 0};
static uint8_t actual_state[4] = {0, 0, 0, 0};

// State variables for interlock dead-time tracking
static bool interlock_transitioning = false;
static uint32_t interlock_start_time = 0;
static uint8_t pending_relay_id = 0;

void Relay_Init(RelayHardwareConfig_t *config) {
    if (config) {
        hw = config;
        for (int i = 0; i < 4; i++) {
            hw->write_pin((uint8_t)i, RELAY_OFF);
            target_state[i] = RELAY_OFF;
            actual_state[i] = RELAY_OFF;
        }
        interlock_transitioning = false;
    }
}

void Relay_SetTarget(uint8_t relay_id, uint8_t state) {
    if (relay_id > 3) return;
    
    // Safety Interlock Enforcement (Only for RL_CHARGER and RL_SOCKET)
    if (state == RELAY_ON) {
        if (relay_id == RL_CHARGER && target_state[RL_SOCKET] == RELAY_ON) {
            target_state[RL_SOCKET] = RELAY_OFF; // Force turn off Socket immediately
        } else if (relay_id == RL_SOCKET && target_state[RL_CHARGER] == RELAY_ON) {
            target_state[RL_CHARGER] = RELAY_OFF; // Force turn off Charger immediately
        }
    }
    
    target_state[relay_id] = state;
}

uint8_t Relay_GetActual(uint8_t relay_id) {
    if (relay_id > 3) return RELAY_OFF;
    return actual_state[relay_id];
}

void Relay_Process(void) {
    if (!hw) return;
    
    // 1. Handle non-interlock relays immediately (Fan, Spare)
    for (int i = 2; i < 4; i++) {
        if (target_state[i] != actual_state[i]) {
            hw->write_pin((uint8_t)i, target_state[i]);
            actual_state[i] = target_state[i];
        }
    }
    
    // 2. Shut off interlock relays immediately if requested
    for (int i = 0; i < 2; i++) {
        if (target_state[i] == RELAY_OFF && actual_state[i] == RELAY_ON) {
            hw->write_pin((uint8_t)i, RELAY_OFF);
            actual_state[i] = RELAY_OFF;
            // If it was transitioning to this state, cancel it
            if (interlock_transitioning && pending_relay_id == i) {
                interlock_transitioning = false; 
            }
        }
    }
    
    // 3. Handle turning ON interlock relays with dead-time constraint
    if (!interlock_transitioning) {
        // Find if any interlock relay needs to turn ON
        for (int i = 0; i < 2; i++) {
            if (target_state[i] == RELAY_ON && actual_state[i] == RELAY_OFF) {
                uint8_t other = (i == RL_CHARGER) ? RL_SOCKET : RL_CHARGER;
                
                // Must ensure the other relay is physically OFF before counting delay
                if (actual_state[other] == RELAY_OFF) {
                    interlock_transitioning = true;
                    pending_relay_id = i;
                    interlock_start_time = hw->get_tick();
                }
                break; // Only start one transition at a time
            }
        }
    } else {
        // We are waiting for the dead-time delay
        if ((hw->get_tick() - interlock_start_time) >= INTERLOCK_DELAY_MS) {
            // Delay passed, execute the turn ON command if it hasn't been cancelled
            if (target_state[pending_relay_id] == RELAY_ON) {
                hw->write_pin(pending_relay_id, RELAY_ON);
                actual_state[pending_relay_id] = RELAY_ON;
            }
            interlock_transitioning = false;
        }
    }
}
