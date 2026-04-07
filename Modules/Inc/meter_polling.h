#ifndef METER_POLLING_H
#define METER_POLLING_H

#include <stdint.h>

void MeterPolling_Init(void);
void MeterPolling_Process(void);
void MeterPolling_UART2_IRQHandler(void);

#endif // METER_POLLING_H
