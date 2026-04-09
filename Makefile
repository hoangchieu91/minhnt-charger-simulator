TARGET = charger
CC = arm-none-eabi-gcc
CP = arm-none-eabi-objcopy
SZ = arm-none-eabi-size

CFLAGS = -mcpu=cortex-m0plus -mthumb -TSTM32G030C8Tx_FLASH.ld -Wall -O2 
CFLAGS += -DUSE_HAL_DRIVER -DSTM32G030xx
CFLAGS += -ICore/Inc -IDrivers/STM32G0xx_HAL_Driver/Inc -IDrivers/STM32G0xx_HAL_Driver/Inc/Legacy
CFLAGS += -IDrivers/CMSIS/Device/ST/STM32G0xx/Include -IDrivers/CMSIS/Include
CFLAGS += -IApp/Inc -IModules/Inc

HAL_SRCS = \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_cortex.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash_ex.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr_ex.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c \
Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_uart.c

SRCS = Core/Src/main.c Core/Src/system_stm32g0xx.c Core/Src/stm32g0xx_it.c \
App/Src/app_main.c \
Modules/Src/dlt645_meter.c Modules/Src/led_rgw.c Modules/Src/relay_ctrl.c \
Modules/Src/ntc_temp.c Modules/Src/fan_ctrl.c Modules/Src/meter_monitor.c \
Modules/Src/digital_input.c Modules/Src/door_lock.c Modules/Src/diagnostics.c \
Modules/Src/modbus_slave.c Modules/Src/error_log.c Modules/Src/meter_polling.c

SRCS += $(HAL_SRCS)

ASM_SRCS = Core/Src/startup_stm32g030xx.s

OBJS = $(SRCS:.c=.o) $(ASM_SRCS:.s=.o)

all: $(TARGET).elf $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ -Wl,--gc-sections -Wl,-Map=$(TARGET).map --specs=nano.specs --specs=nosys.specs
	$(SZ) $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

$(TARGET).bin: $(TARGET).elf
	$(CP) -O binary $< $@

flash: $(TARGET).bin
	st-flash write $< 0x08000000

test_modbus:
	gcc -o /tmp/test_modbus Tests/test_modbus.c -IModules/Inc -Wall
	/tmp/test_modbus

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin
