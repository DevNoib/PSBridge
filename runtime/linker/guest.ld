SECTIONS {
    .guest_image ALIGN(64) : {
        *(.guest_image)
    }

    .guest_bss ALIGN(4) (NOLOAD) : {
        *(.guest_bss)
    } :text
}

INSERT AFTER .sdata;
