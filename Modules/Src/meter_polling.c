#include "meter_polling.h"
#include "dlt645_meter.h"
#include "meter_monitor.h"
#include "led_rgw.h"
#include "stm32g0xx_hal.h"
#include <string.h>

#define POLL_INTERVAL_MS 1000
#define POLL_TIMEOUT_MS  500

/* Broadcast address for DLT645-2007: AAAAAAAAAAAA */
static const uint8_t METER_ADDR[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

/* UART buffers */
static volatile uint8_t  dlt_tx_buf[32];
static volatile uint16_t dlt_tx_len = 0;
static volatile uint16_t dlt_tx_idx = 0;

static volatile uint8_t  dlt_rx_buf[64];
static volatile uint16_t dlt_rx_idx = 0;
static volatile uint32_t dlt_rx_tick = 0;

/* Diagnostics */
static uint8_t last_tx_frame[32];
static uint8_t last_rx_frame[32];
static uint32_t tx_cnt = 0, rx_cnt = 0, crc_err_cnt = 0, timeout_cnt = 0;

/* State Machine */
typedef enum {
    STATE_IDLE = 0,
    STATE_DISCOVERY,
    STATE_WAIT_ADDR,
    STATE_WAIT_V,
    STATE_WAIT_I,
    STATE_WAIT_P,
    STATE_WAIT_E,
    STATE_WAIT_F,
    STATE_WAIT_PF
} PollState_t;

static PollState_t state = STATE_IDLE;
static uint32_t state_tick = 0;

/* Temporary cache for batch variables */
static uint16_t current_v = 0;
static uint16_t current_i = 0;
static uint16_t current_p = 0;
static uint32_t current_e = 0;
static uint16_t current_f = 0;
static uint16_t current_pf = 0;

/* Start asynchronous transmission */
static void DLT_Send(const uint8_t *data, uint16_t len) {
    if (len > sizeof(dlt_tx_buf)) return;
    
    // Copy to volatile buffer safely
    for(uint16_t i=0; i<len; i++) dlt_tx_buf[i] = data[i];
    
    dlt_tx_len = len;
    dlt_tx_idx = 0;
    dlt_rx_idx = 0; // Reset RX on new TX
    
    // Log TX frame for diagnostics
    memset(last_tx_frame, 0, sizeof(last_tx_frame));
    for(uint16_t i=0; i<len && i<sizeof(last_tx_frame); i++) last_tx_frame[i] = data[i];
    tx_cnt++;
    
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
    
    /* Errors: Noise, Overrun, Frame, Parity */
    if (USART2->ISR & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) {
        USART2->ICR = 0x3F; // Clear all error flags
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
        case STATE_DISCOVERY: // Handled the same as IDLE start
            if (now - state_tick >= POLL_INTERVAL_MS) {
                // If serial is unknown (all 0), go to discovery first
                uint8_t s[6];
                Meter_GetSerial(s);
                bool known = false;
                for(int i=0; i<6; i++) if(s[i] != 0) known = true;

                if (!known) {
                    req_len = DLT645_BuildAddrRequest(req);
                    DLT_Send(req, req_len);
                    state = STATE_WAIT_ADDR;
                } else {
                    req_len = DLT645_BuildReadRequest(METER_ADDR, DLT_DI_VOLTAGE, req);
                    DLT_Send(req, req_len);
                    state = STATE_WAIT_V;
                }
                state_tick = now;
            }
            break;

        case STATE_WAIT_ADDR:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                // Log RX frame for diagnostics
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];
                
                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd)) {
                    Meter_SetSerial(pd.addr);
                    rx_cnt++;
                    LED_Set(LED_WHITE, LED_SOLID); // Pulse ON
                } else {
                    crc_err_cnt++;
                    LED_Set(LED_WHITE, LED_OFF);
                }
                state = STATE_IDLE; // Back to idle to start normal poll with new serial
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                timeout_cnt++;
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_V:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];

                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_v = (uint16_t)(pd.voltage * 10.0f); // 0.1V 
                    Meter_SetSerial(pd.addr);
                    rx_cnt++;
                } else {
                    crc_err_cnt++;
                }
                // Instantly queue next
                req_len = DLT645_BuildReadRequest(METER_ADDR, DLT_DI_CURRENT, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_I;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_I:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];

                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_i = (uint16_t)(pd.current * 100.0f); // 0.01A
                    rx_cnt++;
                } else {
                    crc_err_cnt++;
                }
                req_len = DLT645_BuildReadRequest(METER_ADDR, DLT_DI_POWER, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_P;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                timeout_cnt++;
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_P:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];

                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_p = (uint16_t)(pd.power * 1000.0f); // Converting kW to W
                    rx_cnt++;
                } else {
                    crc_err_cnt++;
                }
                req_len = DLT645_BuildReadRequest(METER_ADDR, DLT_DI_ENERGY, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_E;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                timeout_cnt++;
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_E:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];

                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_e = (uint32_t)(pd.energy * 1000.0f); // kWh to Wh
                    rx_cnt++;
                } else {
                    crc_err_cnt++;
                }
                req_len = DLT645_BuildReadRequest(METER_ADDR, DLT_DI_FREQ, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_F;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                timeout_cnt++;
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_F:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];

                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_f = (uint16_t)(pd.frequency * 10.0f); // 0.1Hz
                    rx_cnt++;
                } else {
                    crc_err_cnt++;
                }
                req_len = DLT645_BuildReadRequest(METER_ADDR, DLT_DI_PF, req);
                DLT_Send(req, req_len);
                state = STATE_WAIT_PF;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                timeout_cnt++;
                state = STATE_IDLE;
                state_tick = now;
            }
            break;

        case STATE_WAIT_PF:
            if (dlt_rx_idx > 0 && (now - dlt_rx_tick > 50)) {
                memset(last_rx_frame, 0, sizeof(last_rx_frame));
                for(uint16_t i=0; i<dlt_rx_idx && i<sizeof(last_rx_frame); i++) last_rx_frame[i] = dlt_rx_buf[i];

                if (DLT645_ParseFrame((uint8_t*)dlt_rx_buf, dlt_rx_idx, &pd) && pd.is_valid) {
                    current_pf = (uint16_t)(pd.power_factor * 1000.0f); // 0.001
                    rx_cnt++;
                    
                    // Visual Heartbeat: Toggle LED on successful full cycle
                    static uint8_t hb = 0;
                    hb = !hb;
                    LED_Set(LED_WHITE, hb ? LED_SOLID : LED_OFF);
                } else {
                    crc_err_cnt++;
                }
                
                // Final push to monitor
                Meter_Update(current_v, current_i, current_p, current_e, current_f, current_pf);
                
                state = STATE_IDLE;
                state_tick = now;
            } else if (now - state_tick >= POLL_TIMEOUT_MS) {
                timeout_cnt++;
                state = STATE_IDLE;
                state_tick = now;
            }
            break;
    }
}

void MeterPolling_GetLastTX(uint8_t *buf) {
    if(buf) memcpy(buf, (uint8_t*)last_tx_frame, 16);
}
void MeterPolling_GetLastRX(uint8_t *buf) {
    if(buf) memcpy(buf, (uint8_t*)last_rx_frame, 16);
}
uint32_t MeterPolling_GetTxCnt(void) { return tx_cnt; }
uint32_t MeterPolling_GetRxCnt(void) { return rx_cnt; }
uint32_t MeterPolling_GetCrcErrCnt(void) { return crc_err_cnt; }
uint32_t MeterPolling_GetTimeoutCnt(void) { return timeout_cnt; }
