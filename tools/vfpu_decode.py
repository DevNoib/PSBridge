VFPU_SIZES = {
    "s": 0x0000,
    "p": 0x0080,
    "t": 0x8000,
    "q": 0x8080,
}

VFPU_PATTERNS = []


def add_vfpu_opcode(name, opcode, mask):
    VFPU_PATTERNS.append((name, int(opcode, 16), int(mask, 16)))


def add_vfpu_variants(name, opcode, mask, sizes):
    base_opcode = int(opcode, 16)
    for size in sizes:
        variant_opcode = base_opcode | VFPU_SIZES[size]
        add_vfpu_opcode(f"{name}.{size}", f"{variant_opcode:08X}", mask)



VFPU_CONTROL_AND_MEMORY = (
    ("bvf", "49000000", "FFE30000"),
    ("bvfl", "49020000", "FFE30000"),
    ("bvt", "49010000", "FFE30000"),
    ("bvtl", "49030000", "FFE30000"),
    ("lv.q", "D8000000", "FC000002"),
    ("lv.s", "C8000000", "FC000000"),
    ("lvl.q", "D4000000", "FC000002"),
    ("lvr.q", "D4000002", "FC000002"),
    ("sv.q", "F8000000", "FC000002"),
    ("sv.s", "E8000000", "FC000000"),
    ("svl.q", "F4000000", "FC000002"),
    ("svr.q", "F4000002", "FC000002"),
    ("vwb.q", "F8000002", "FC000002"),
    ("mfv", "48600000", "FFE0FF80"),
    ("mfvc", "48600000", "FFE0FF00"),
    ("mtv", "48E00000", "FFE0FF80"),
    ("mtvc", "48E00000", "FFE0FF00"),
    ("vpfxs", "DC000000", "FF000000"),
    ("vpfxt", "DD000000", "FF000000"),
    ("vpfxd", "DE000000", "FF000000"),
    ("viim.s", "DF000000", "FF800000"),
    ("vfim.s", "DF800000", "FF800000"),
    ("vnop", "FFFF0000", "FFFFFFFF"),
    ("vsync", "FFFF0320", "FFFFFFFF"),
    ("vflush", "FFFF040D", "FFFFFFFF"),
)


for name, opcode, mask in VFPU_CONTROL_AND_MEMORY:
    add_vfpu_opcode(name, opcode, mask)


VFPU_ARITHMETIC = (
    ("vadd", "60000000", "FF808080", "sptq"),
    ("vsub", "60800000", "FF808080", "sptq"),
    ("vsbn", "61000000", "FF808080", "s"),
    ("vdiv", "63800000", "FF808080", "sptq"),
    ("vmul", "64000000", "FF808080", "sptq"),
    ("vdot", "64800000", "FF808080", "ptq"),
    ("vscl", "65000000", "FF808080", "ptq"),
    ("vhdp", "66000000", "FF808080", "ptq"),
    ("vcrs", "66800000", "FF808080", "t"),
    ("vdet", "67000000", "FF808080", "p"),
    ("vcmp", "6C000000", "FF8080F0", "sptq"),
    ("vmin", "6D000000", "FF808080", "sptq"),
    ("vmax", "6D800000", "FF808080", "sptq"),
    ("vscmp", "6E800000", "FF808080", "sptq"),
    ("vsge", "6F000000", "FF808080", "sptq"),
    ("vslt", "6F800000", "FF808080", "sptq"),
)


for name, opcode, mask, sizes in VFPU_ARITHMETIC:
    add_vfpu_variants(name, opcode, mask, sizes)


VFPU_UNARY = (
    ("vmov", "D0000000", "FFFF8080", "sptq"),
    ("vabs", "D0010000", "FFFF8080", "sptq"),
    ("vneg", "D0020000", "FFFF8080", "sptq"),
    ("vidt", "D0030000", "FFFFFF80", "pq"),
    ("vsat0", "D0040000", "FFFF8080", "sptq"),
    ("vsat1", "D0050000", "FFFF8080", "sptq"),
    ("vzero", "D0060000", "FFFFFF80", "sptq"),
    ("vone", "D0070000", "FFFFFF80", "sptq"),
    ("vrcp", "D0100000", "FFFF8080", "sptq"),
    ("vrsq", "D0110000", "FFFF8080", "sptq"),
    ("vsin", "D0120000", "FFFF8080", "sptq"),
    ("vcos", "D0130000", "FFFF8080", "sptq"),
    ("vexp2", "D0140000", "FFFF8080", "sptq"),
    ("vlog2", "D0150000", "FFFF8080", "sptq"),
    ("vsqrt", "D0160000", "FFFF8080", "sptq"),
    ("vasin", "D0170000", "FFFF8080", "sptq"),
    ("vnrcp", "D0180000", "FFFF8080", "sptq"),
    ("vnsin", "D01A0000", "FFFF8080", "sptq"),
    ("vrexp2", "D01C0000", "FFFF8080", "sptq"),
    ("vrndi", "D0210000", "FFFFFF80", "sptq"),
    ("vrndf1", "D0220000", "FFFFFF80", "sptq"),
    ("vrndf2", "D0230000", "FFFFFF80", "sptq"),
    ("vf2h", "D0320000", "FFFF8080", "pq"),
    ("vh2f", "D0330000", "FFFF8080", "sp"),
    ("vsbz", "D0360000", "FFFF8080", "s"),
    ("vlgb", "D0370000", "FFFF8080", "s"),
    ("vus2i", "D03A0000", "FFFF8080", "sp"),
    ("vs2i", "D03B0000", "FFFF8080", "sp"),
    ("vi2uc", "D03C0000", "FFFF8080", "q"),
    ("vi2c", "D03D0000", "FFFF8080", "q"),
    ("vi2us", "D03E0000", "FFFF8080", "pq"),
    ("vi2s", "D03F0000", "FFFF8080", "pq"),
    ("vsrt1", "D0400000", "FFFF8080", "q"),
    ("vsrt2", "D0410000", "FFFF8080", "q"),
    ("vbfy1", "D0420000", "FFFF8080", "pq"),
    ("vbfy2", "D0430000", "FFFF8080", "q"),
    ("vocp", "D0440000", "FFFF8080", "sptq"),
    ("vsocp", "D0450000", "FFFF8080", "sp"),
    ("vfad", "D0460000", "FFFF8080", "ptq"),
    ("vavg", "D0470000", "FFFF8080", "ptq"),
    ("vsrt3", "D0480000", "FFFF8080", "q"),
    ("vsrt4", "D0490000", "FFFF8080", "q"),
    ("vsgn", "D04A0000", "FFFF8080", "sptq"),
    ("vt4444", "D0590000", "FFFF8080", "q"),
    ("vt5551", "D05A0000", "FFFF8080", "q"),
    ("vt5650", "D05B0000", "FFFF8080", "q"),
)


for name, opcode, mask, sizes in VFPU_UNARY:
    add_vfpu_variants(name, opcode, mask, sizes)


for name, opcode, mask in (
    ("vrnds.s", "D0200000", "FFFF80FF"),
    ("vmfvc", "D0500000", "FFFF0080"),
    ("vmtvc", "D0510000", "FFFF8000"),
):
    add_vfpu_opcode(name, opcode, mask)

add_vfpu_variants("vcst", "D0600000", "FFE0FF80", "sptq")


VFPU_CONVERSIONS_AND_MATRICES = (
    ("vf2in", "D2000000", "FFE08080", "sptq"),
    ("vf2iz", "D2200000", "FFE08080", "sptq"),
    ("vf2iu", "D2400000", "FFE08080", "sptq"),
    ("vf2id", "D2600000", "FFE08080", "sptq"),
    ("vi2f", "D2800000", "FFE08080", "sptq"),
    ("vcmovt", "D2A00000", "FFF88080", "sptq"),
    ("vcmovf", "D2A80000", "FFF88080", "sptq"),
    ("vmmul", "F0000000", "FF808080", "ptq"),
    ("vmscl", "F2000000", "FF808080", "ptq"),
    ("vcrsp", "F2800000", "FF808080", "t"),
    ("vqmul", "F2800000", "FF808080", "q"),
    ("vmmov", "F3800000", "FFFF8080", "ptq"),
    ("vmidt", "F3830000", "FFFFFF80", "ptq"),
    ("vmzero", "F3860000", "FFFFFF80", "ptq"),
    ("vmone", "F3870000", "FFFFFF80", "ptq"),
    ("vrot", "F3A00000", "FFE08080", "ptq"),
)


for name, opcode, mask, sizes in VFPU_CONVERSIONS_AND_MATRICES:
    add_vfpu_variants(name, opcode, mask, sizes)


VFPU_TRANSFORMS = (
    ("vhtfm2.p", "F0800000"),
    ("vtfm2.p", "F0800080"),
    ("vhtfm3.t", "F1000080"),
    ("vtfm3.t", "F1008000"),
    ("vhtfm4.q", "F1808000"),
    ("vtfm4.q", "F1808080"),
)


for name, opcode in VFPU_TRANSFORMS:
    add_vfpu_opcode(name, opcode, "FF808080")

add_vfpu_opcode("vuc2ifs.s", "D0380000", "FFFF8080")
add_vfpu_opcode("vwbn.s", "D3000000", "FF008080")


def identify_vfpu(instruction_word):
    for name, opcode, mask in VFPU_PATTERNS:
        if instruction_word & mask == opcode:
            return name
    return None
