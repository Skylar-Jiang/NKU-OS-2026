ENTRY(_start)
SECTIONS {
 . = 0x80000000;
 .text : { *(.text.entry) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) *(.sdata*) }
 . = 0x80200000;
 .bss : { *(.bss*) *(.sbss*) *(COMMON) }
}
