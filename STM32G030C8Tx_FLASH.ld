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
    . = ALIGN(4); *(.text) *(.text*) *(.rodata) *(.rodata*) *(.glue_7) *(.glue_7t) *(.eh_frame) KEEP (*(.init)) KEEP (*(.fini)) . = ALIGN(4); _etext = .;
  } >FLASH
  
  .preinit_array : {
    . = ALIGN(4); PROVIDE_HIDDEN (__preinit_array_start = .); KEEP (*(.preinit_array*)) PROVIDE_HIDDEN (__preinit_array_end = .); . = ALIGN(4);
  } >FLASH
  .init_array : {
    . = ALIGN(4); PROVIDE_HIDDEN (__init_array_start = .); KEEP (*(SORT(.init_array.*))) KEEP (*(.init_array*)) PROVIDE_HIDDEN (__init_array_end = .); . = ALIGN(4);
  } >FLASH
  .fini_array : {
    . = ALIGN(4); PROVIDE_HIDDEN (__fini_array_start = .); KEEP (*(SORT(.fini_array.*))) KEEP (*(.fini_array*)) PROVIDE_HIDDEN (__fini_array_end = .); . = ALIGN(4);
  } >FLASH
  
  _sidata = LOADADDR(.data);
  .data : {
    . = ALIGN(4); _sdata = .; *(.data) *(.data*) . = ALIGN(4); _edata = .;
  } >RAM AT> FLASH
  .bss : {
    . = ALIGN(4); _sbss = .; *(.bss) *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .;
  } >RAM
}
