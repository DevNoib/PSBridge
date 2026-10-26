SECTIONS {
    .guest_image ALIGN(0x100000) : {
        *(.guest_image)
    }

    .guest_bss ALIGN(4) (NOLOAD) : {
        *(.guest_bss)
    } :text
}

INSERT AFTER .sdata;
