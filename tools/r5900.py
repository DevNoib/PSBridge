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


def resolve_jump(source):
    lines = [
        "sw $25, -4($sp)",
        "sw $31, -8($sp)",
        f"or $25, {reg(source)}, $0",
        "addiu $sp, $sp, -8",
        "jal psbridge_resolve_guest",
        "nop",
    ]

    if source != 31:
        lines.append("lw $31, 0($sp)")

    lines.append(f"or {reg(source)}, $25, $0")

    if source != 25:
        lines.append("lw $25, 4($sp)")

    lines.append("addiu $sp, $sp, 8")
    return lines


def emit_vfpu_dispatch(opcode, address):
    return [
        "addiu $sp, $sp, -16",
        "sw $26, 0($sp)",
        "sw $27, 4($sp)",
        "sw $31, 8($sp)",
        *load_value(26, opcode),
        *load_value(27, address & 0xffffffff),
        "jal psbridge_vfpu_entry",
        "nop",
        "lw $26, 0($sp)",
        "lw $27, 4($sp)",
        "lw $31, 8($sp)",
        "addiu $sp, $sp, 16"
    ]


def emit(instruction, label, address):
    name = instruction[0]

    if name == "vfpu_call":
        return emit_vfpu_dispatch(instruction[1], address)

    if name == "nop":
        return [name]

    if name in ("sll", "srl", "sra"):
        _, rd, rt, shift = instruction
        return [f"{name} {reg(rd)}, {reg(rt)}, {shift}"]
    if name in ("sllv", "srlv", "srav"):
        _, rd, rt, rs = instruction
        return [f"{name} {reg(rd)}, {reg(rt)}, {reg(rs)}"]

    if name == "rotr":
        _, destination, source, shift = instruction

        if destination == 0:
            return ["nop"]
        if shift == 0:
            return [f"or {reg(destination)}, {reg(source)}, $0"]

        scratch = next(
            item for item in (26, 27, 1, 25, 24, 23, 22, 21)
            if item not in (destination, source)
        )

        return [
            f"mthi1 {reg(scratch)}",
            f"or {reg(scratch)}, {reg(source)}, $0",
            f"srl {reg(destination)}, {reg(scratch)}, {shift}",
            f"sll {reg(scratch)}, {reg(scratch)}, {32 - shift}",
            f"or {reg(destination)}, {reg(destination)}, {reg(scratch)}",
            f"mfhi1 {reg(scratch)}",
        ]

    if name == "rotrv":
        _, destination, source, shift = instruction

        if destination == 0:
            return ["nop"]

        scratch = [
            item for item in (26, 27, 1, 25, 24, 23, 22, 21)
            if item not in (destination, source, shift)
        ][:2]
        value, complement = scratch

        return [
            f"mthi1 {reg(value)}",
            f"mtlo1 {reg(complement)}",
            f"or {reg(value)}, {reg(source)}, $0",
            f"andi {reg(complement)}, {reg(shift)}, 31",
            f"srlv {reg(destination)}, {reg(value)}, {reg(complement)}",
            f"subu {reg(complement)}, $0, {reg(complement)}",
            f"andi {reg(complement)}, {reg(complement)}, 31",
            f"sllv {reg(value)}, {reg(value)}, {reg(complement)}",
            f"or {reg(destination)}, {reg(destination)}, {reg(value)}",
            f"mflo1 {reg(complement)}",
            f"mfhi1 {reg(value)}",
        ]

    if name == "jr":
        source = instruction[1]

        if source == 31:
            return [f"jr {reg(source)}"]

        return resolve_jump(source) + [f"jr {reg(source)}"]
    if name == "jalr":
        _, link, source = instruction
        return resolve_jump(source) + [f"jalr {reg(link)}, {reg(source)}"]

    if name in ("addu", "subu", "and", "or", "xor", "nor", "slt", "sltu", "movz", "movn"):
        _, rd, rs, rt = instruction
        return [f"{name} {reg(rd)}, {reg(rs)}, {reg(rt)}"]
    if name in ("min", "max"):
        _, rd, rs, rt = instruction
        host = "pminw" if name == "min" else "pmaxw"
        return [f"{host} {reg(rd)}, {reg(rs)}, {reg(rt)}", f"sll {reg(rd)}, {reg(rd)}, 0"]

    if name in ("mfhi", "mflo", "mthi", "mtlo"):
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

    if name in ("addi", "addiu", "slti", "sltiu", "andi", "ori", "xori"):
        _, rt, rs, value = instruction
        shown = str(value) if name in ("addi", "addiu", "slti", "sltiu") else f"0x{value:x}"
        return [f"{name} {reg(rt)}, {reg(rs)}, {shown}"]
    if name == "lui":
        return [f"lui {reg(instruction[1])}, 0x{instruction[2]:x}"]

    if name in ("lb", "lbu", "lh", "lhu", "lwl", "lwr", "lw", "sb", "sh", "swl", "swr", "sw"):
        _, rt, rs, offset = instruction
        return [f"{name} {reg(rt)}, {offset}({reg(rs)})"]
    if name in ("lwc1", "swc1"):
        _, ft, rs, offset = instruction
        return [f"{name} {freg(ft)}, {offset}({reg(rs)})"]

    if name in ("mfc1", "mtc1", "cfc1", "ctc1"):
        _, rt, fs = instruction
        return [f"{name} {reg(rt)}, {freg(fs)}"]
    if name in ("add.s", "sub.s", "mul.s", "div.s"):
        _, fd, fs, ft = instruction
        return [f"{name} {freg(fd)}, {freg(fs)}, {freg(ft)}"]
    if name in (
        "mov.s", "trunc.w.s", "cvt.s.w", "sqrt.s",
        "abs.s", "neg.s", "floor.w.s",
    ):
        _, fd, fs = instruction
        return [f"{name} {freg(fd)}, {freg(fs)}"]
    if name in ("c.eq.s", "c.lt.s", "c.le.s"):
        _, fs, ft = instruction
        return [f"{name} {freg(fs)}, {freg(ft)}"]

    if name in ("seb", "seh"):
        _, destination, source = instruction
        if destination == 0:
            return ["nop"]
        shift = 24 if name == "seb" else 16
        return [
            f"sll {reg(destination)}, {reg(source)}, {shift}",
            f"sra {reg(destination)}, {reg(destination)}, {shift}",
        ]

    if name == "bitrev":
        _, destination, source = instruction
        if destination == 0:
            return ["nop"]

        temporaries = [index for index in (26, 27, 1, 25, 24, 23, 22, 21) if index not in (destination, source)][:3]
        current, shifted, mask_register = temporaries
        lines = [
            "addiu $sp, $sp, -16",
            f"sw {reg(current)}, 0($sp)",
            f"sw {reg(shifted)}, 4($sp)",
            f"sw {reg(mask_register)}, 8($sp)",
        ]

        if source == 29:
            lines.append(f"addiu {reg(current)}, $sp, 16")
        else:
            lines.append(f"or {reg(current)}, {reg(source)}, $0")

        for count, mask in (
            (1, 0x55555555),
            (2, 0x33333333),
            (4, 0x0f0f0f0f),
            (8, 0x00ff00ff),
            (16, 0x0000ffff),
        ):
            lines.extend(load_value(mask_register, mask))
            lines.extend([
                f"srl {reg(shifted)}, {reg(current)}, {count}",
                f"xor {reg(shifted)}, {reg(shifted)}, {reg(current)}",
                f"and {reg(shifted)}, {reg(shifted)}, {reg(mask_register)}",
                f"xor {reg(current)}, {reg(current)}, {reg(shifted)}",
                f"sll {reg(shifted)}, {reg(shifted)}, {count}",
                f"xor {reg(current)}, {reg(current)}, {reg(shifted)}",
            ])

        if destination == 29:
            lines.append(f"sw {reg(current)}, 12($sp)")
        else:
            lines.append(f"or {reg(destination)}, {reg(current)}, $0")

        lines.extend([
            f"lw {reg(current)}, 0($sp)",
            f"lw {reg(shifted)}, 4($sp)",
            f"lw {reg(mask_register)}, 8($sp)",
        ])
        if destination == 29:
            lines.append("lw $sp, 12($sp)")
        else:
            lines.append("addiu $sp, $sp, 16")
        return lines

    if name == "wsbw":
        _, rd, rt = instruction
        if rd == 0:
            return ["nop"]

        scratch = [item for item in (26, 27, 1, 25, 24, 23, 22, 21) if item not in (rd, rt)][:2]
        first, second = scratch
        return [
            f"mthi1 {reg(first)}",
            f"mtlo1 {reg(second)}",
            f"sll {reg(first)}, {reg(rt)}, 8",
            f"lui {reg(second)}, 0xff00",
            f"ori {reg(second)}, {reg(second)}, 0xff00",
            f"and {reg(first)}, {reg(first)}, {reg(second)}",
            f"srl {reg(rd)}, {reg(rt)}, 8",
            f"nor {reg(second)}, {reg(second)}, $0",
            f"and {reg(rd)}, {reg(rd)}, {reg(second)}",
            f"or {reg(rd)}, {reg(rd)}, {reg(first)}",
            f"sll {reg(first)}, {reg(rd)}, 16",
            f"srl {reg(rd)}, {reg(rd)}, 16",
            f"or {reg(rd)}, {reg(rd)}, {reg(first)}",
            f"mflo1 {reg(second)}",
            f"mfhi1 {reg(first)}",
        ]

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
