def reg(index):
    return f"${index}"


def emit(instruction, label):
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
        return f"{name} {label(instruction[1])}"
    if name in ("beq", "bne"):
        _, rs, rt, target = instruction
        return f"{name} {reg(rs)}, {reg(rt)}, {label(target)}"
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
