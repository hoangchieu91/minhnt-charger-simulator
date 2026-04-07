#include "meter_polling.h"
#include "dlt645_meter.h"
#include "meter_monitor.h"
#include "stm32g0xx_hal.h"
#include <string.h>

#define POLL_INTERVAL_MS 1000
#define POLL_TIMEOUT_MS  500

/* DLT645 Constants */
static const uint32_t DI_VOLTAGE = 0x02010100; // DLT_DI_VOLTAGE 
static const uint32_t DI_CURRENT = 0x02020100; // DLT_DI_CURRENT
static const uint32_t DI_ENERGY  = 0x00000000; // DLT_DI_ENERGY/Total active energy

/* UART buffers */
static volatile uint8_t  dlt_tx_buf[32];
static volatile uint16_t dlt_tx_len = 0;
static volatile uint16_t dlt_tx_idx = 0;

static volatile uint8_t  dlt_rx_buf[64];
static volatile uint16_t dlt_rx_idx = 0;
static volatile uint32_t dlt_rx_tick = 0;

/* State Machine */
typedef enum {
    STATE_IDLE = 0,
    STATE_WAIT_V,
    STATE_WAIT_I,
    STATE_WAIT_E
} PollState_t;

static PollState_t state = STATE_IDLE;
static uint32_t state_tick = 0;

/* Temporary cache for the 4 batch variables */
static uint16_t current_v = 0;
static uint16_t current_i = 0;
static uint16_t current_p = 0;
static uint32_t current_e = 0;

/* Start asynchronous transmission */
static void DLT_Send(const uint8_t *data, uint16_t len) {
    if (len > sizeof(dlt_tx_buf)) return;
    
    // Copy to volatile buffer safely
    for(uint16_t i=0; i<len; i++) dlt_tx_buf[i] = data[i];
    
    dlt_tx_len = len;
    dlt_tx_idx = 0;
    dlt_rx_idx = 0; // Reset RX on new TX
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); /* DE HIGH */
    USART2->CR1 |= USART_CR1_TXEIE_TXFNFIE;             /* Enable TXE interrupt */
}

/* Must be called inside USART2_IRQHandler */
void MeterPolling_UART2_IRQHandler(void) {
    /* Receive Buffer Not Empty */
    if (USART2->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t byte = (uint8_t)(USART2->RDR & 0xFF);
        if (dlt_rx_idx < sizeof(dlt_rx_buf)) {
            dlt_rx_buf[dlt_rx_idx++] = byte;
            dlt_rx_tick = HAL_GetTick();
        }
    }
    
    /* Transmit Register Empty */
    if ((USART2->CR1 & USART_CR1_TXEIE_TXFNFIE) && (USART2->ISR & USART_ISR_TXE_TXFNF)) {
        if (dlt_tx_idx < dlt_tx_len) {
            USART2->TDR = dlt_tx_buf[dlt_tx_idx++];
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE_TXFNFIE; /* Disable TXE */
            USART2->CR1 |= USART_CR1_TCIE;           /* Enable TC */
        }
    }
    
    /* Transmit Complete */
    if ((USART2->CR1 & USART_CR1_TCIE) && (USART2->ISR & USART_ISR_TC)) {
        USART2->CR1 &= ~USART_CR1_TCIE;          /* Disable TC */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); /* DE LOW */
    }
}

void MeterPolling_Init(void) {
    state = STATE_IDLE;
    state_tick = HAL_GetTick();
    dlt_tx_idx = dlt_tx_len = 0;
    dlt_rx_idx = 0;
}

void MeterPolling_Process(void) {
    uint32_t now = HAL_GetTick();
    uint8_t req[32];
    uint16_t req_len;
    DLT645_Data_t pd;

    switch (state) {
        case STATE_IDLE:
            if (now - state_tick >= POLL_INTERVAL_MS) {
                req_len = DLT645_BuildReadRequest(DLT_DI_VOLTAGE, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_V;
                state_tick = now;
            }
            break;

        case STATE_WAIT_V:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                // Inter-byte timeout indicates frame complete
                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_v = (uint16_t)(pd.voltage * 10.0f); // 0.1V 
                }
                // Instantly queue next
                req_len = DLT645_BuildReadRequest(DLT_DI_CURRENT, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_I;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                // Timeout, restart round
                state = STATE_IDLE;
                state_tick = now;
            }
            break;
            
        case STATE_WAIT_I:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_i = (uint16_t)(pd.current * 100.0f); // 0.01A
                }
                req_len = DLT645_BuildReadRequest(DLT_DI_ENERGY, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_E;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_E:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_e = (uint32_t)(pd.energy); // Wh
                }
                
                // Calculate pseudo-power based on V*I if meter doesn't provide P directly
                // Power (W) = V(0.1V) * I(0.01A) / 1000
                current_p = (uint16_t)(((uint32_t)current_v * (uint32_t)current_i) / 1000);
                
                // Push batch to meter monitor
                Meter_Update(current_v, current_i, current_p, current_e);
                
                state = STATE_IDLE;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                state = STATE_IDLE;
                state_tick = now;
            }
            break;
    }
}
