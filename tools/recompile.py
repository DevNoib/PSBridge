import struct
import sys
from pathlib import Path

from allegrex import branch_names, decode, target_of
from elf_reader import read_code, read_function, read_header, read_segments
from r5900 import emit


def direct_target(instruction):
    return target_of(instruction)


def is_code(segments, address):
    for _, vaddr, file_size, _, flags, _ in segments:
        if flags & 1 and vaddr <= address <= vaddr + file_size - 4:
            return True
    return False


def find_functions(data, segments, entry):
    functions = {}
    pending = [entry]

    while pending:
        start = pending.pop()
        if start in functions:
            continue

        code = read_function(data, segments, start)
        instructions = []

        for address, word in code:
            instruction = decode(address, word)
            if instruction[0] == "word":
                raise ValueError(f"unsupported instruction at 0x{address:08x}")
            instructions.append((address, instruction))

            if instruction[0] == "jal":
                target = instruction[1]
                if is_code(segments, target) and target not in functions:
                    pending.append(target)

        functions[start] = instructions

    return functions


def check_instructions(data):
    names = set()

    for address, word in read_code(data):
        instruction = decode(address, word)
        if instruction[0] == "word":
            raise ValueError(f"unsupported instruction at 0x{address:08x}")
        names.add(instruction[0])

    return names


def add_line(lines, line):
    lines.append(line if line.endswith(":") else f"    {line}")


def add_branch_with_long_delay(lines, address, instruction, delay, label):
    name = instruction[0]
    likely = name.endswith("l")
    taken = f"lower_{address:08x}_taken"
    fallthrough = label(address + 8)
    target = label(target_of(instruction))

    for line in emit(instruction, lambda _: taken, address):
        add_line(lines, line)
    add_line(lines, "nop")

    if not likely:
        for line in emit(delay, label, address + 0x100000000):
            add_line(lines, line)
    add_line(lines, f"j {fallthrough}")
    add_line(lines, "nop")
    lines.append(f"{taken}:")
    for line in emit(delay, label, address + 0x200000000):
        add_line(lines, line)
    add_line(lines, f"j {target}")
    add_line(lines, "nop")


def recompile(input_path, output_path):
    print("recompiling... let him cook")
    data = input_path.read_bytes()
    _, entry, ph_offset, ph_size, ph_count = read_header(data)
    segments = read_segments(data, ph_offset, ph_size, ph_count)
    names = check_instructions(data)
    functions = find_functions(data, segments, entry)
    code = {}

    for instructions in functions.values():
        code.update(instructions)

    starts = set(functions)
    addresses = set(code)

    def label(address):
        name = "guest" if address in starts or address not in addresses else "code"
        return f"{name}_{address:08x}"

    external = set()
    for instruction in code.values():
        target = direct_target(instruction)
        if target is not None and target not in addresses:
            external.add(target)

    lines = [".set noreorder", ".text", ".globl guest_entry"]

    for address in sorted(external):
        lines.append(f".extern {label(address)}")

    for address in sorted(code):
        instruction = code[address]
        if address == entry:
            lines.append("guest_entry:")
        lines.append(f"{label(address)}:")
        delay = code.get(address + 4)
        delay_lines = emit(delay, label, address + 4) if delay else []

        if instruction[0] in branch_names and len(delay_lines) > 1:
            add_branch_with_long_delay(lines, address, instruction, delay, label)
            continue
        if instruction[0] in ("j", "jal", "jr", "jalr") and len(delay_lines) > 1:
            raise ValueError(f"long delay slot at 0x{address + 4:08x}")

        for line in emit(instruction, label, address):
            add_line(lines, line)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n")
    print(f"wrote {output_path}")
    print(f"functions {len(functions)}")
    print(f"instructions {len(code)}")
    print(f"instruction types {len(names)}")
    print(f"external targets {len(external)}")
    print("we are so back")


def main():
    input_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")
    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/game.s")

    try:
        recompile(input_path, output_path)
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"error {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
