file charger.elf
target extended-remote localhost:2331
monitor halt
break Reset_Handler
break HardFault_Handler
c &
shell sleep 5
interrupt
bt
detach
quit
