registers = (
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
)


def reg(index):
    return f"${registers[index]}"


def signed(value):
    return value - 0x10000 if value & 0x8000 else value


def decode(word):
    op = word >> 26
    rs = (word >> 21) & 31
    rt = (word >> 16) & 31
    rd = (word >> 11) & 31
    funct = word & 63
    value = word & 0xffff

    if op == 0 and funct == 0x21:
        return f"addu {reg(rd)}, {reg(rs)}, {reg(rt)}"
    if op == 9:
        return f"addiu {reg(rt)}, {reg(rs)}, {signed(value)}"
    if op == 13:
        return f"ori {reg(rt)}, {reg(rs)}, 0x{value:x}"
    if op == 15:
        return f"lui {reg(rt)}, 0x{value:x}"
    if op == 43:
        return f"sw {reg(rt)}, {signed(value)}({reg(rs)})"

    return f".word 0x{word:08x}"
