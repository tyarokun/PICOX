MEMORY {
  FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2048K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

ENTRY(Reset_Handler)

SECTIONS {
  .boot2 ORIGIN(FLASH) : {
    KEEP(*(.boot2))
  } > FLASH

  .vectors : { /* boot2の直後にベクタテーブルを置く0x10000100~ */
    . = ALIGN(256); /* 256byteでアラインメント */
    KEEP(vector_table.o(.data))
  } > FLASH

  .text : {
    *(.text .text.*)
    . = ALIGN(4);
  } > FLASH

  .rodata : {
    _rodata_start = .;
    *(.rodata)
    *(.rodata.*)
    _rodata_end = .;
  } > FLASH

  .data : {
    . = ALIGN(4);
    _data_start = .;  /* {}>RAM によってこの.はRAMに割り当てられる(VMA) */
    *(.data .data.*)
    . = ALIGN(4);
    _data_end = .;
  } > RAM AT > FLASH  /* AT > FLASH によってFLASH続きから割り当てられる(LMA) */
  /* _rodata_end から.data領域分コピーすれば良い */

  .bss : {
    . = ALIGN(4);
    _bss_start = .;
    *(.bss .bss.* COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > RAM

  .userstack : {
    . = ALIGN(8);
    userstack = .;
    . += 24K;
    userstack_end = .;
  } > RAM

  _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
