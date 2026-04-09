#ifndef METER_POLLING_H
#define METER_POLLING_H

#include <stdint.h>

void MeterPolling_Init(void);
void MeterPolling_Process(void);
void MeterPolling_UART2_IRQHandler(void);

/** @brief Get diagnostics data */
void MeterPolling_GetLastTX(uint8_t *buf);
void MeterPolling_GetLastRX(uint8_t *buf);
uint32_t MeterPolling_GetTxCnt(void);
uint32_t MeterPolling_GetRxCnt(void);
uint32_t MeterPolling_GetCrcErrCnt(void);
uint32_t MeterPolling_GetTimeoutCnt(void);

#endif // METER_POLLING_H
