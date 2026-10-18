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
        return "nop",
    if op == 0 and funct == 0:
        return "sll", rd, rt, shift
    if op == 0 and funct == 2:
        return "srl", rd, rt, shift
    if op == 0 and funct == 8:
        return "jr", rs
    if op == 0 and funct == 9:
        return "jalr", rd, rs
    if op == 0 and funct == 0x21:
        return "addu", rd, rs, rt
    if op == 0 and funct == 0x25:
        return "or", rd, rs, rt
    if op == 0 and funct == 0x26:
        return "xor", rd, rs, rt
    if op in (2, 3):
        target = ((address + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)
        return ("j" if op == 2 else "jal"), target
    if op in (4, 5):
        target = (address + 4 + (signed(value) << 2)) & 0xffffffff
        return ("beq" if op == 4 else "bne"), rs, rt, target
    if op == 9:
        return "addiu", rt, rs, signed(value)
    if op == 12:
        return "andi", rt, rs, value
    if op == 13:
        return "ori", rt, rs, value
    if op == 14:
        return "xori", rt, rs, value
    if op == 15:
        return "lui", rt, value
    if op == 35:
        return "lw", rt, rs, signed(value)
    if op == 40:
        return "sb", rt, rs, signed(value)
    if op == 41:
        return "sh", rt, rs, signed(value)
    if op == 43:
        return "sw", rt, rs, signed(value)

    return "word", word


def format_instruction(instruction):
    name = instruction[0]

    if name == "nop":
        return name
    if name in ("sll", "srl"):
        _, rd, rt, shift = instruction
        return f"{name} {reg(rd)}, {reg(rt)}, {shift}"
    if name == "jr":
        return f"jr {reg(instruction[1])}"
    if name == "jalr":
        return f"jalr {reg(instruction[1])}, {reg(instruction[2])}"
    if name in ("addu", "or", "xor"):
        _, rd, rs, rt = instruction
        return f"{name} {reg(rd)}, {reg(rs)}, {reg(rt)}"
    if name in ("j", "jal"):
        return f"{name} 0x{instruction[1]:08x}"
    if name in ("beq", "bne"):
        _, rs, rt, target = instruction
        return f"{name} {reg(rs)}, {reg(rt)}, 0x{target:08x}"
    if name in ("addiu", "andi", "ori", "xori"):
        _, rt, rs, value = instruction
        value = str(value) if name == "addiu" else f"0x{value:x}"
        return f"{name} {reg(rt)}, {reg(rs)}, {value}"
    if name == "lui":
        return f"lui {reg(instruction[1])}, 0x{instruction[2]:x}"
    if name in ("lw", "sb", "sh", "sw"):
        _, rt, rs, offset = instruction
        return f"{name} {reg(rt)}, {offset}({reg(rs)})"

    return f".word 0x{instruction[1]:08x}"
