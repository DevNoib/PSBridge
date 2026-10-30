SECTIONS {
    .guest_image ALIGN(0x100000) : {
        *(.guest_image)
    }

    .guest_bss ALIGN(4) (NOLOAD) : {
        *(.guest_bss)
    } :text
}

INSERT AFTER .sdata;

SECTIONS {
    .psbridge_heap (NOLOAD) : {
        . = ALIGN(4096);
        __psbridge_heap_start = .;
        . += 4;
    } :text
    .psbridge_heap_limit 0x01f00000 (NOLOAD) : {
        __psbridge_heap_end = .;
        . += 4;
        _end = .;
        _end_bss = .;
    } :text
    ASSERT(__psbridge_heap_end >= __psbridge_heap_start + 0x400000, "o ELF gozou na RAM do PS2")
}

INSERT AFTER .bss;
