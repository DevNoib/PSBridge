SECTIONS {
    .guest_image 0x00400000 : {
        *(.guest_image)
    }

    .guest_bss ALIGN(4) (NOLOAD) : {
        *(.guest_bss)
    } :text

}

INSERT AFTER .sdata;