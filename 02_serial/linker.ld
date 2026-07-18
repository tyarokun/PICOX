MEMORY{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 2048K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(Reset_Handler)

SECTIONS{
    /* 先頭256byte */
    .boot2 : {
        _boot_start = .;
        KEEP(*(.boot2)) /* KEEP : 最適化による削除（不要なコードの破棄）を無効化し、必ず配置するようリンカに指示 */
        _boot_end = .;
    } > FLASH

    /* boot2の直後にベクタテーブルを置く0x10000100~ */
    .vectors : {
        . = ALIGN(256); /* 256byteでアラインメント */
        KEEP(vector_table.o(.data))
    } > FLASH

    .text : {
        _text_start = .;
        *(.text)
        *(.text.*)
        _text_end = .;
    } > FLASH

    .rodata : {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    } > FLASH

    .data : {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM AT > FLASH

    .bss : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    . = ALIGN(4);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}