SECTIONS {
    .guest_image ALIGN(16) : {
        *(.guest_image)
    } :text

    .guest_bss ALIGN(4) (NOLOAD) : {
        *(.guest_bss)
    } :text

    _end = .;
}

INSERT BEFORE .spad;