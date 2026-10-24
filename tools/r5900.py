from allegrex import float_branches, one_reg_branches, two_reg_branches


def reg(index):
    return f"${index}"


def freg(index):
    return f"$f{index}"


def load_value(register, value):
    high = (value >> 16) & 0xffff
    low = value & 0xffff

    if high == 0:
        return [f"ori {reg(register)}, $0, 0x{low:04x}"]
    if low == 0:
        return [f"lui {reg(register)}, 0x{high:04x}"]
    return [
        f"lui {reg(register)}, 0x{high:04x}",
        f"ori {reg(register)}, {reg(register)}, 0x{low:04x}",
    ]


def emit(instruction, label, address):
    name = instruction[0]

    if name == "nop":
        return [name]

    if name in ("sll", "srl", "sra"):
        _, rd, rt, shift = instruction
        return [f"{name} {reg(rd)}, {reg(rt)}, {shift}"]
    if name in ("sllv", "srlv", "srav"):
        _, rd, rt, rs = instruction
        return [f"{name} {reg(rd)}, {reg(rt)}, {reg(rs)}"]

    if name == "jr":
        return [f"jr {reg(instruction[1])}"]
    if name == "jalr":
        return [f"jalr {reg(instruction[1])}, {reg(instruction[2])}"]

    if name in ("addu", "subu", "and", "or", "xor", "nor", "slt", "sltu", "movz", "movn"):
        _, rd, rs, rt = instruction
        return [f"{name} {reg(rd)}, {reg(rs)}, {reg(rt)}"]
    if name in ("min", "max"):
        _, rd, rs, rt = instruction
        host = "pminw" if name == "min" else "pmaxw"
        return [f"{host} {reg(rd)}, {reg(rs)}, {reg(rt)}", f"sll {reg(rd)}, {reg(rd)}, 0"]

    if name in ("mfhi", "mflo"):
        return [f"{name} {reg(instruction[1])}"]
    if name in ("mult", "multu", "madd"):
        return [f"{name} {reg(instruction[1])}, {reg(instruction[2])}"]
    if name in ("div", "divu"):
        return [f".word 0x{instruction[3]:08x}"]

    if name == "clz":
        _, rd, rs = instruction
        zero = f"lower_{address:08x}_zero"
        done = f"lower_{address:08x}_done"

        return [
            f"beq {reg(rs)}, $0, {zero}",
            f"srl {reg(rd)}, {reg(rs)}, 1",
            f"plzcw {reg(rd)}, {reg(rd)}",
            f"b {done}",
            "nop",
            f"{zero}:",
            f"ori {reg(rd)}, $0, 32",
            f"{done}:",
        ]

    if name == "break":
        return [f"break {instruction[1]}"]

    if name in ("j", "jal"):
        if instruction[1] == 0:
            word = 0x08000000 if name == "j" else 0x0c000000
            return [f".word 0x{word:08x}"]

        return [f"{name} {label(instruction[1])}"]

    if name in two_reg_branches:
        _, rs, rt, target = instruction
        return [f"{name} {reg(rs)}, {reg(rt)}, {label(target)}"]
    if name in one_reg_branches:
        _, rs, target = instruction
        return [f"{name} {reg(rs)}, {label(target)}"]
    if name in float_branches:
        return [f"{name} {label(instruction[1])}"]

    if name in ("addiu", "slti", "sltiu", "andi", "ori", "xori"):
        _, rt, rs, value = instruction
        shown = str(value) if name in ("addiu", "slti", "sltiu") else f"0x{value:x}"
        return [f"{name} {reg(rt)}, {reg(rs)}, {shown}"]
    if name == "lui":
        return [f"lui {reg(instruction[1])}, 0x{instruction[2]:x}"]

    if name in ("lb", "lbu", "lh", "lhu", "lwl", "lwr", "lw", "sb", "sh", "swl", "swr", "sw"):
        _, rt, rs, offset = instruction
        return [f"{name} {reg(rt)}, {offset}({reg(rs)})"]
    if name in ("lwc1", "swc1"):
        _, ft, rs, offset = instruction
        return [f"{name} {freg(ft)}, {offset}({reg(rs)})"]

    if name in ("mfc1", "mtc1"):
        _, rt, fs = instruction
        return [f"{name} {reg(rt)}, {freg(fs)}"]
    if name in ("add.s", "sub.s", "mul.s", "div.s"):
        _, fd, fs, ft = instruction
        return [f"{name} {freg(fd)}, {freg(fs)}, {freg(ft)}"]
    if name in ("mov.s", "trunc.w.s", "cvt.s.w"):
        _, fd, fs = instruction
        return [f"{name} {freg(fd)}, {freg(fs)}"]
    if name in ("c.eq.s", "c.lt.s", "c.le.s"):
        _, fs, ft = instruction
        return [f"{name} {freg(fs)}, {freg(ft)}"]

    if name == "cache":
        return ["nop"]

    if name == "ext":
        _, rt, rs, pos, size = instruction
        left = 32 - pos - size
        right = 32 - size
        lines = []

        if left:
            lines.append(f"sll {reg(rt)}, {reg(rs)}, {left}")
        elif rt != rs:
            lines.append(f"or {reg(rt)}, {reg(rs)}, $0")

        if right:
            lines.append(f"srl {reg(rt)}, {reg(rt)}, {right}")

        return lines or ["nop"]

    if name == "ins":
        _, rt, rs, pos, size = instruction

        if pos == 0 and size == 29:
            return [
                f"sll {reg(rt)}, {reg(rs)}, 3",
                f"srl {reg(rt)}, {reg(rt)}, 3",
            ]

        mask = ((1 << size) - 1) << pos if size < 32 else 0xffffffff
        scratch = [item for item in (26, 27, 1, 25, 24, 23, 22, 21) if item not in (rs, rt)][:2]
        first, second = scratch
        lines = [f"mthi1 {reg(first)}", f"mtlo1 {reg(second)}"]

        lines.extend(load_value(first, mask))
        lines.extend([
            f"nor {reg(second)}, {reg(first)}, $0",
            f"and {reg(rt)}, {reg(rt)}, {reg(second)}",
        ])

        if pos:
            lines.append(f"sll {reg(second)}, {reg(rs)}, {pos}")
        elif second != rs:
            lines.append(f"or {reg(second)}, {reg(rs)}, $0")

        lines.extend([
            f"and {reg(second)}, {reg(second)}, {reg(first)}",
            f"or {reg(rt)}, {reg(rt)}, {reg(second)}",
            f"mflo1 {reg(second)}",
            f"mfhi1 {reg(first)}",
        ])

        return lines

    raise ValueError(f"não sei cuspir {name} no r5900")
