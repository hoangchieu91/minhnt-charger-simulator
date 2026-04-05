#include "stm32f1xx_hal.h"
#include "app_main.h"
#include "led_rgw.h"
#include "relay_ctrl.h"
#include "ntc_temp.h"
#include "fan_ctrl.h"
#include "meter_monitor.h"
#include "digital_input.h"
#include "door_lock.h"
#include "diagnostics.h"
#include "modbus_slave.h"
#include "error_log.h"

/* Không dùng UART3 — xung đột PB10(RL_FAN)/PB11(RL_DOORLOCK)
 * Debug qua GDB server (tương đương SWD)
 * Monitor qua UART1 Modbus RTU (tương đương RS485_1) */

/* ═══════════════════════════════════════════════════════════
 * UART1 — RS485_1 Modbus RTU (PA9=TX, PA10=RX, PA1=DE)
 * ═══════════════════════════════════════════════════════════ */

static void UART1_SendBytes(const uint8_t *data, uint16_t len) {
    /* DE pin HIGH = transmit */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    for (uint16_t i = 0; i < len; i++) {
        /* Timeout for Renode: TXE wait */
        for (volatile int t = 0; t < 1000 && !(USART1->SR & USART_SR_TXE); t++) {}
        USART1->DR = data[i];
    }
    /* Timeout for Renode: TC wait */
    for (volatile int t = 0; t < 1000 && !(USART1->SR & USART_SR_TC); t++) {}
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); /* DE=LOW = receive */
}

void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFF);
        USART1->DR = byte; /* HIL DEBUG ECHO */
        Modbus_ReceiveByte(byte);
        /* Diagnostic: Toggle PB12 LED on every received byte in Renode */
        GPIOB->ODR ^= GPIO_PIN_12;
    }
}

/* ═══════════════════════════════════════════════════════════
 * HARDWARE ABSTRACTION — Renode đọc/ghi GPIO thật
 * Trên Renode, GPIO read/write đi qua peripheral model
 * ═══════════════════════════════════════════════════════════ */

/* LED: PA8(Red), PA11(Green), PA12(White) */
static void hw_write_led(LedColor_t c, uint8_t s) {
    GPIO_TypeDef *port = GPIOA;
    uint16_t pin;
    switch (c) {
        case LED_RED:   pin = GPIO_PIN_8;  break;
        case LED_GREEN: pin = GPIO_PIN_11; break;
        case LED_WHITE: pin = GPIO_PIN_12; break;
        default: return;
    }
    HAL_GPIO_WritePin(port, pin, s ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Relay: PB0(Charger), PB1(Socket), PB10(Fan), PB11(DoorLock) */
static void hw_write_relay(uint8_t r_id, uint8_t s) {
    uint16_t pin;
    switch (r_id) {
        case RL_CHARGER:  pin = GPIO_PIN_0;  break;
        case RL_SOCKET:   pin = GPIO_PIN_1;  break;
        case RL_FAN:      pin = GPIO_PIN_10; break;
        case RL_DOORLOCK: pin = GPIO_PIN_11; break;
        default: return;
    }
    HAL_GPIO_WritePin(GPIOB, pin, s ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* NTC: PA0 ADC_IN0 */
static uint16_t hw_read_adc(void) {
    /* Renode: ADC channel 0. Inject via: sysbus.ADC1 SetDefaultConversionValue 0 2048 */
    ADC1->CR2 |= ADC_CR2_SWSTART;
    /* Renode often doesn't set EOC. Skip polling to avoid log bloat and slowness. */
    return (uint16_t)(ADC1->DR & 0xFFF);
}

/* Fan relay: dùng Relay_SetTarget(RL_FAN, x) */
static void hw_write_fan(uint8_t s) {
    hw_write_relay(RL_FAN, s);
}

/* Door sensor: PB12 Input Pullup (LOW=mở) */
static uint8_t hw_read_door(void) {
    return (uint8_t)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12);
}

/* DIP Switch: PB5(bit0), PB6(bit1), PB7(bit2) */
static uint8_t hw_read_dip(void) {
    uint8_t v = 0;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) v |= 1;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_RESET) v |= 2;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET) v |= 4;
    return v;
}

/* Door Lock relay: PB11 */
static void hw_write_lock(uint8_t s) {
    hw_write_relay(RL_DOORLOCK, s);
}

uint32_t hw_get_tick(void) { return HAL_GetTick(); }

/* ═══════════════════════════════════════════════════════════
 * GPIO INIT cho Renode
 * ═══════════════════════════════════════════════════════════ */

static void GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g;

    /* LED outputs: PA8, PA11, PA12 */
    g.Pin = GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);

    /* Relay outputs: PB0, PB1, PB10, PB11 */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_11;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &g);

    /* UART1 pins: PA9=TX(AF_PP), PA10=RX(Input) */
    g.Pin = GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_10;
    g.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &g);

    /* RS485 DE: PA1, PA4 */
    g.Pin = GPIO_PIN_1 | GPIO_PIN_4;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &g);

    /* Door sensor input: PB12 */
    g.Pin = GPIO_PIN_12;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);

    /* DIP Switch inputs: PB5, PB6, PB7 */
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
}

static void ADC_Init(void) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    ADC1->CR2 = ADC_CR2_ADON;     /* Bật ADC */
    ADC1->SQR3 = 0;               /* Channel 0 (PA0) */
    ADC1->CR2 |= ADC_CR2_CAL;     /* Calibrate */
    /* Skip calibration wait for Renode */
}

/* ═══════════════════════════════════════════════════════════
 * HARDWARE CONFIG STRUCTS
 * ═══════════════════════════════════════════════════════════ */

LedHardwareConfig_t led_hw = { hw_write_led, hw_get_tick };
RelayHardwareConfig_t relay_hw = { hw_write_relay, hw_get_tick };
NTC_HardwareConfig_t ntc_hw = { hw_read_adc };
FanHardwareConfig_t fan_hw = { hw_write_fan, hw_get_tick };
DI_HardwareConfig_t di_hw = { hw_read_door, hw_read_dip, hw_get_tick };
DoorLockHardwareConfig_t lock_hw = { hw_write_lock, hw_get_tick };
DiagHardwareConfig_t diag_hw = { hw_get_tick };

/* Flash HAL for Error Log */
static int hw_flash_erase(uint32_t addr) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = addr;
    erase.NbPages = 1;
    uint32_t error;
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &error);
    HAL_FLASH_Lock();
    return (st == HAL_OK) ? 0 : -1;
}
static int hw_flash_write(uint32_t addr, const uint8_t *data, uint16_t len) {
    HAL_FLASH_Unlock();
    /* STM32F103 writes in 16-bit halfwords */
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t hw = data[i];
        if (i + 1 < len) hw |= ((uint16_t)data[i+1] << 8);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, hw);
    }
    HAL_FLASH_Lock();
    return 0;
}
static void hw_flash_read(uint32_t addr, uint8_t *data, uint16_t len) {
    /* Direct read from Flash memory-mapped region */
    for (uint16_t i = 0; i < len; i++) {
        data[i] = *(volatile uint8_t*)(addr + i);
    }
}
static ErrLogFlashConfig_t errlog_flash = { hw_flash_erase, hw_flash_write, hw_flash_read };
static uint32_t last_save_tick = 0;



/* ═══════════════════════════════════════════════════════════
 * MAIN — Tất cả module init + superloop thật
 * ═══════════════════════════════════════════════════════════ */

int main(void) {
    HAL_Init();

    GPIO_Init();
    ADC_Init();

    /* USART1 — RS485_1 Modbus RTU Slave (9600-8-N-1) */
    __HAL_RCC_USART1_CLK_ENABLE();
    /* SystemCoreClock = 8MHz (HSI default, per system_stm32f1xx.c)
     * BRR = SystemCoreClock / baud = 8000000 / 9600 = 833 = 0x0341
     * Note: Renode's STM32_UART model ignores BRR for byte transport */
    USART1->BRR = SystemCoreClock / 9600;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_SetPriority(USART1_IRQn, 1);

    /* Init TẤT CẢ module */
    LED_Init(&led_hw);
    Relay_Init(&relay_hw);
    NTC_Init(&ntc_hw);
    Fan_Init(&fan_hw);
    DI_Init(&di_hw);
    DoorLock_Init(&lock_hw);
    Diag_Init(&diag_hw);
    App_Init();

    /* Modbus Slave — Force addr 1 for HIL reliability */
    static ModbusHardwareConfig_t modbus_hw;
    modbus_hw.send_bytes = UART1_SendBytes;
    modbus_hw.get_tick = hw_get_tick;
    Modbus_Init(&modbus_hw, 1);

    /* Error Log — persistent Flash storage */
    ErrLog_Init(&errlog_flash);
    last_save_tick = HAL_GetTick();

    /* Boot marker — non-blocking write */
    USART1->DR = 0xBB;

    while (1) {
        /* Superloop — firmware logic */
        App_Process();

        /* Modbus RTU slave processing (CRC check & silence detector) */
        Modbus_Process();

        /* Error Log flush every 60s */
        if (HAL_GetTick() - last_save_tick >= 60000) {
            last_save_tick = HAL_GetTick();
            ErrLog_Save();
        }
    }
}

void HAL_MspInit(void) {
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void SystemClock_Config(void) { /* HSI 8MHz default */ }
void Error_Handler(void) { while(1); }
