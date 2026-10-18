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


def decode(address, word):
    op = word >> 26
    rs = (word >> 21) & 31
    rt = (word >> 16) & 31
    rd = (word >> 11) & 31
    shift = (word >> 6) & 31
    funct = word & 63
    value = word & 0xffff

    if word == 0:
        return "nop"
    if op == 0 and funct == 0:
        return f"sll {reg(rd)}, {reg(rt)}, {shift}"
    if op == 0 and funct == 2:
        return f"srl {reg(rd)}, {reg(rt)}, {shift}"
    if op == 0 and funct == 8:
        return f"jr {reg(rs)}"
    if op == 0 and funct == 9:
        return f"jalr {reg(rd)}, {reg(rs)}"
    if op == 0 and funct == 0x21:
        return f"addu {reg(rd)}, {reg(rs)}, {reg(rt)}"
    if op == 0 and funct == 0x25:
        return f"or {reg(rd)}, {reg(rs)}, {reg(rt)}"
    if op == 0 and funct == 0x26:
        return f"xor {reg(rd)}, {reg(rs)}, {reg(rt)}"
    if op == 2 or op == 3:
        target = ((address + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)
        name = "j" if op == 2 else "jal"
        return f"{name} 0x{target:08x}"
    if op == 4 or op == 5:
        target = (address + 4 + (signed(value) << 2)) & 0xffffffff
        name = "beq" if op == 4 else "bne"
        return f"{name} {reg(rs)}, {reg(rt)}, 0x{target:08x}"
    if op == 9:
        return f"addiu {reg(rt)}, {reg(rs)}, {signed(value)}"
    if op == 12:
        return f"andi {reg(rt)}, {reg(rs)}, 0x{value:x}"
    if op == 13:
        return f"ori {reg(rt)}, {reg(rs)}, 0x{value:x}"
    if op == 14:
        return f"xori {reg(rt)}, {reg(rs)}, 0x{value:x}"
    if op == 15:
        return f"lui {reg(rt)}, 0x{value:x}"
    if op == 35:
        return f"lw {reg(rt)}, {signed(value)}({reg(rs)})"
    if op == 40:
        return f"sb {reg(rt)}, {signed(value)}({reg(rs)})"
    if op == 41:
        return f"sh {reg(rt)}, {signed(value)}({reg(rs)})"
    if op == 43:
        return f"sw {reg(rt)}, {signed(value)}({reg(rs)})"

    return f".word 0x{word:08x}"
