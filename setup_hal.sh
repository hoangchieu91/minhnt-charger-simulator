#!/bin/bash
set -e

echo "1. Tải STM32F1xx HAL và CMSIS từ Github..."
mkdir -p Drivers/STM32F1xx_HAL_Driver Drivers/CMSIS/Device/ST/STM32F1xx/Include Drivers/CMSIS/Include Core/Inc Core/Src

if [ ! -d /tmp/hal ]; then
    git clone --depth 1 https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git /tmp/hal
    git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_f1.git /tmp/cmsis_dev
    git clone --depth 1 https://github.com/ARM-software/CMSIS_5.git /tmp/cmsis_core
fi

cp -r /tmp/hal/Inc Drivers/STM32F1xx_HAL_Driver/
cp -r /tmp/hal/Src Drivers/STM32F1xx_HAL_Driver/
# Xóa các file template để tránh trùng lặp
rm -f Drivers/STM32F1xx_HAL_Driver/Src/*_template.c

cp -r /tmp/cmsis_dev/Include/* Drivers/CMSIS/Device/ST/STM32F1xx/Include/
cp -r /tmp/cmsis_dev/Source/Templates/system_stm32f1xx.c Core/Src/
cp -r /tmp/cmsis_dev/Source/Templates/gcc/startup_stm32f103xb.s Core/Src/
cp -r /tmp/cmsis_core/CMSIS/Core/Include/* Drivers/CMSIS/Include/

echo "2. Chuẩn bị file stm32f1xx_hal_conf.h"
cat << 'EOF' > Core/Inc/stm32f1xx_hal_conf.h
#ifndef __STM32F1xx_HAL_CONF_H
#define __STM32F1xx_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#define HSE_VALUE    8000000U
#define HSI_VALUE    8000000U
#define LSI_VALUE    40000U
#define LSE_VALUE    32768U
#define TICK_INT_PRIORITY            0x00U

#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_cortex.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_pwr.h"
#include "stm32f1xx_hal_uart.h"

#endif
EOF

echo "3. Chuẩn bị file Linker Script (STM32F103C8Tx_FLASH.ld)"
cat << 'EOF' > STM32F103C8Tx_FLASH.ld
ENTRY(Reset_Handler)
_estack = 0x20005000;
MEMORY {
  RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 20K
  FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = 64K
}
SECTIONS {
  .isr_vector : {
    . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4);
  } >FLASH
  .text : {
    . = ALIGN(4); *(.text) *(.text*) *(.rodata) *(.rodata*) . = ALIGN(4); _etext = .;
  } >FLASH
  _sidata = LOADADDR(.data);
  .data : {
    . = ALIGN(4); _sdata = .; *(.data) *(.data*) . = ALIGN(4); _edata = .;
  } >RAM AT> FLASH
  .bss : {
    . = ALIGN(4); _sbss = .; *(.bss) *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .;
  } >RAM
}
EOF

echo "Thành công!"
