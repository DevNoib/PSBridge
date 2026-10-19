# EU VOU DEIXAR OS COMENTARIOS EM PORTUGUES PORQUE EU SOU BRASILEIRO NESSE CARALHO ENTÃO VAI SE FUDER

import struct
import sys
from pathlib import Path

from allegrex import branch_names, decode, target_of
from elf_reader import read_code, read_function
from imports import find_module, read_imports
from r5900 import emit
from relocate import relocate


def direct_target(instruction):
    return target_of(instruction)


def is_code(segments, address):
    for _, vaddr, file_size, _, flags, _ in segments:
        if flags & 1 and vaddr <= address <= vaddr + file_size - 4:
            return True
    return False


def find_functions(data, segments, entry, import_map):
    functions = {}
    pending = [entry]

    while pending:
        start = pending.pop()
        if start in functions:
            continue

        code = read_function(data, segments, start, import_map)
        instructions = []

        for address, word in code:
            instruction = decode(address, word)
            if instruction[0] == "word":
                raise ValueError(f"que porra de instrução é essa em 0x{address:08x}")
            instructions.append((address, instruction))

            if instruction[0] == "jal":
                target = instruction[1]
                if target not in import_map and is_code(segments, target) and target not in functions:
                    pending.append(target)

        functions[start] = instructions

    return functions


def import_symbol(item):
    library = "".join(char if char.isalnum() or char == "_" else "_" for char in item["library"])
    return f"psp_{library}_{item['nid']:08x}"


def check_instructions(data):
    names = set()

    for address, word in read_code(data):
        instruction = decode(address, word)
        if instruction[0] == "word":
            raise ValueError(f"que porra de instrução é essa em 0x{address:08x}")
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
    print("recompilando... deixa o cara gozar")
    elf = input_path.read_bytes()
    names = check_instructions(elf)
    data, segments, entry, relocations = relocate(elf)

    module = find_module(data, segments)

    imports = read_imports(data, segments, module)
    import_map = {
        item["stub"]: item
        for item in imports
        if item["kind"] == "function"
    }

    functions = find_functions(data, segments, entry, import_map)
    code = {}

    for instructions in functions.values():
        code.update(instructions)

    starts = set(functions)
    addresses = set(code)

    def label(address):
        if address in import_map:
            return import_symbol(import_map[address])
        name = "guest" if address in starts or address not in addresses else "code"
        return f"{name}_{address:08x}"

    external = set()
    for instruction in code.values():
        target = direct_target(instruction)
        if target is not None and target not in addresses:
            external.add(target)

    lines = [".set noreorder", ".text", ".globl guest_entry"]

    for symbol in sorted({label(address) for address in external}):
        lines.append(f".extern {symbol}")

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
            raise ValueError(f"esse delay slot ficou grande pra caralho em 0x{address + 4:08x}")

        for line in emit(instruction, label, address):
            add_line(lines, line)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n")
    print(f"escrevi {output_path}")
    print(f"funções {len(functions)}")
    print(f"instruções {len(code)}")
    print(f"tipos de instrução {len(names)}")
    print(f"relocações {relocations}")
    print(f"imports usados {sum(address in import_map for address in external)}")
    print(f"alvos externos {len(external)}")
    print("voltamo porra")


def main():
    input_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")
    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/game.s")

    try:
        recompile(input_path, output_path)
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"deu ruim {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
