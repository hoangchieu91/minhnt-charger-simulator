#ifndef RELAY_CTRL_H
#define RELAY_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/* Relay Identifiers */
#define RL_CHARGER 0
#define RL_SOCKET  1
#define RL_FAN     2
#define RL_DOORLOCK 3

#define RELAY_OFF  0
#define RELAY_ON   1

/**
 * @brief Hardware Abstraction Layer structure for Relay Control
 * We use function pointers so this module can be unit-tested seamlessly
 * on a PC without STM32 dependencies.
 */
typedef struct {
    void     (*write_pin)(uint8_t relay_id, uint8_t state);
    uint32_t (*get_tick)(void);
} RelayHardwareConfig_t;

/**
 * @brief Initialize the relay control module with hardware wrappers
 * @param config Pointer to the hardware functions (MUST not be NULL)
 */
void Relay_Init(RelayHardwareConfig_t *config);

/**
 * @brief Request a relay to change state (Safe Operation)
 * @param relay_id RL_CHARGER, RL_SOCKET, RL_FAN, RL_SPARE
 * @param state RELAY_ON or RELAY_OFF
 */
void Relay_SetTarget(uint8_t relay_id, uint8_t state);

/**
 * @brief Get the actual current physical state of a relay
 * @param relay_id Relay identifier
 * @return RELAY_ON or RELAY_OFF
 */
uint8_t Relay_GetActual(uint8_t relay_id);

/**
 * @brief Process relay transitions (must be called continuously in main loop)
 * This handles the non-blocking dead-time delay for safety interlock.
 */
void Relay_Process(void);

#endif // RELAY_CTRL_H
