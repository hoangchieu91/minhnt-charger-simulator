#!/bin/bash
set -e

echo "1. Dọn dẹp HAL cũ (STM32F1xx)..."
rm -rf Drivers/STM32F1xx_HAL_Driver
rm -rf Drivers/CMSIS/Device/ST/STM32F1xx
rm -f setup_hal.sh
rm -f STM32F103C8Tx_FLASH.ld
rm -f Core/Src/startup_stm32f103xb.s
rm -f Core/Src/system_stm32f1xx.c

echo "2. Tải STM32G0xx HAL và CMSIS từ Github..."
mkdir -p Drivers/STM32G0xx_HAL_Driver Drivers/CMSIS/Device/ST/STM32G0xx/Include Drivers/CMSIS/Include Core/Inc Core/Src

if [ ! -d /tmp/hal_g0 ]; then
    git clone --depth 1 https://github.com/STMicroelectronics/stm32g0xx_hal_driver.git /tmp/hal_g0
    git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_g0.git /tmp/cmsis_dev_g0
    
    # CMSIS 5 is already installed in /tmp/cmsis_core from the previous run, but we will ensure it's there
    if [ ! -d /tmp/cmsis_core ]; then
        git clone --depth 1 https://github.com/ARM-software/CMSIS_5.git /tmp/cmsis_core
    fi
fi

cp -r /tmp/hal_g0/Inc Drivers/STM32G0xx_HAL_Driver/
cp -r /tmp/hal_g0/Src Drivers/STM32G0xx_HAL_Driver/
# Xóa các file template để tránh trùng lặp
rm -f Drivers/STM32G0xx_HAL_Driver/Src/*_template.c

cp -r /tmp/cmsis_dev_g0/Include/* Drivers/CMSIS/Device/ST/STM32G0xx/Include/
cp -r /tmp/cmsis_dev_g0/Source/Templates/system_stm32g0xx.c Core/Src/
cp -r /tmp/cmsis_dev_g0/Source/Templates/gcc/startup_stm32g030xx.s Core/Src/

# Cập nhật lại CMSIS core (hoặc copy nếu chưa có)
cp -r /tmp/cmsis_core/CMSIS/Core/Include/* Drivers/CMSIS/Include/ || true

echo "3. Chuẩn bị file stm32g0xx_hal_conf.h"
cat << 'EOF' > Core/Inc/stm32g0xx_hal_conf.h
#ifndef __STM32G0xx_HAL_CONF_H
#define __STM32G0xx_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#define HSE_VALUE    8000000U
#define HSI_VALUE    16000000U
#define LSI_VALUE    32000U
#define LSE_VALUE    32768U
#define TICK_INT_PRIORITY            0x00U

#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_hal_gpio.h"
#include "stm32g0xx_hal_dma.h"
#include "stm32g0xx_hal_cortex.h"
#include "stm32g0xx_hal_flash.h"
#include "stm32g0xx_hal_pwr.h"
#include "stm32g0xx_hal_uart.h"

#endif
EOF

echo "4. Chuẩn bị file Linker Script (STM32G030C8Tx_FLASH.ld)"
cat << 'EOF' > STM32G030C8Tx_FLASH.ld
ENTRY(Reset_Handler)
_estack = 0x20002000;
MEMORY {
  RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 8K
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
