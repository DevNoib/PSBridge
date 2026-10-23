# EU VOU DEIXAR OS COMENTARIOS EM PORTUGUES PORQUE EU SOU 
# BRASILEIRO NESSE CARALHO ENTÃO VAI SE FUDER

import struct
import sys
from pathlib import Path

from allegrex import branch_names, decode, target_of
from analyze import find_callbacks, find_jump_tables, output_register
from elf_reader import read_code, read_word
from module_sexytion import add_image, add_info, add_map, write_map
from imports import find_module, read_imports, import_symbol, link_imports, read_imports
from r5900 import emit
from relocate import relocate

def read_game_code(elf, data, segments):
    base = min(vaddr - offset for offset, vaddr, *_ in segments)
    code = {}
    names = set()

    for raw_address, _ in read_code(elf):
        address = base + raw_address
        instruction = decode(address, read_word(data, segments, address))

        if instruction[0] == "word":
            raise ValueError(f"que porra de instrução é essa em 0x{address:08x}")

        code[address] = instruction
        names.add(instruction[0])

    return code, names, base


def emit_instruction(address, instruction, label, references, lower_address=None):
    target = references["high"].get(address)

    if target is not None:
        if instruction[0] != "lui":
            raise ValueError(f"o hi16 em 0x{address:08x} não é um lui")

        return [f"lui ${instruction[1]}, %hi({label(target)})"]

    target = references["low"].get(address)

    if target is not None:
        name, rt, rs = instruction[:3]

        if name in ("addiu", "ori"):
            return [f"{name} ${rt}, ${rs}, %lo({label(target)})"]

        memory = (
            "lb", "lbu", "lh", "lhu", "lwl", "lwr", "lw",
            "sb", "sh", "swl", "swr", "sw",
        )

        if name in memory:
            return [f"{name} ${rt}, %lo({label(target)})(${rs})"]

        raise ValueError(f"o lo16 em 0x{address:08x} caiu num {name}")

    return emit(instruction, label, lower_address or address)


def add_line(lines, line):
    lines.append(line if line.endswith(":") else f"    {line}")


def add_branch_with_long_delay(lines, address, instruction, delay, label, emit_at):
    likely = instruction[0].endswith("l")
    taken = f"lower_{address:08x}_taken"

    for line in emit_at(address, instruction, lambda _: taken):
        add_line(lines, line)

    add_line(lines, "nop")

    if not likely:
        for line in emit_at(address + 4, delay, label, address + 0x100000000):
            add_line(lines, line)

    add_line(lines, f"j {label(address + 8)}")
    add_line(lines, "nop")
    lines.append(f"{taken}:")

    for line in emit_at(address + 4, delay, label, address + 0x200000000):
        add_line(lines, line)

    add_line(lines, f"j {label(target_of(instruction))}")
    add_line(lines, "nop")


def add_jump_with_long_delay(lines, address, instruction, delay, label, emit_at):
    name = instruction[0]

    if name in ("jal", "jalr"):
        link = 31 if name == "jal" else instruction[1]
        source = None if name == "jal" else instruction[2]

        if source == link:
            raise ValueError(f"o jalr em 0x{address:08x} escreveu em cima do próprio alvo")

        add_line(lines, f"lui ${link}, %hi({label(address + 8)})")
        add_line(lines, f"addiu ${link}, ${link}, %lo({label(address + 8)})")
    else:
        source = None if name == "j" else instruction[1]

    if source is not None and output_register(delay) == source:
        raise ValueError(f"o delay slot em 0x{address + 4:08x} apagou o alvo do salto")

    for line in emit_at(address + 4, delay, label, address + 0x100000000):
        add_line(lines, line)

    if name in ("j", "jal"):
        if instruction[1] == 0:
            add_line(lines, ".word 0x08000000")
        else:
            add_line(lines, f"j {label(instruction[1])}")
    else:
        add_line(lines, f"jr ${source}")

    add_line(lines, "nop")


def recompile(input_path, output_path):
    print("recompilando... deixa o cara gozar")

    elf = input_path.read_bytes()
    data, segments, entry, relocations, references = relocate(elf)
    module = find_module(data, segments)
    imports = read_imports(data, segments, module)
    code, names, base = read_game_code(elf, data, segments)

    import_map = {
        item["stub"]: item
        for item in imports
        if item["kind"] == "function"
    }

    for stub in import_map:
        code.pop(stub, None)
        code.pop(stub + 4, None)

    addresses = set(code)
    missing = references["targets"] - addresses - set(import_map)

    if missing:
        address = min(missing)
        raise ValueError(f"o alvo 0x{address:08x} sumiu do código")

    missing_fixups = (
        set(references["high"]) | set(references["low"])
    ) - addresses

    if missing_fixups:
        address = min(missing_fixups)
        raise ValueError(f"o ajuste em 0x{address:08x} caiu fora do código")

    callbacks = find_callbacks(code, import_map)
    calls = {
        instruction[1]
        for instruction in code.values()
        if instruction[0] == "jal" and instruction[1] in addresses
    }
    starts = {entry} | callbacks | calls | (references["targets"] & addresses)
    direct_targets = {
        target
        for instruction in code.values()
        if (target := target_of(instruction)) not in (None, 0)
    }
    map_addresses = {entry} | callbacks | references["targets"] | direct_targets

    def label(address):
        if address in import_map:
            return import_symbol(import_map[address])

        name = "guest" if address in starts or address not in addresses else "code"
        return f"{name}_{address:08x}"

    external = (references["targets"] | direct_targets) - addresses
    unknown = external - set(import_map)

    if unknown:
        address = min(unknown)
        raise ValueError(f"o alvo externo 0x{address:08x} não é um import")

    word_locations = {base + offset for offset in references["words"]}
    jump_tables = find_jump_tables(code, word_locations)

    def emit_at(address, instruction, current_label=label, lower_address=None):
        return emit_instruction(
            address,
            instruction,
            current_label,
            references,
            lower_address,
        )

    lines = [".set noreorder", ".set noat", ".text", ".globl guest_entry"]

    for symbol in sorted({label(address) for address in external}):
        lines.append(f".extern {symbol}")

    for address in sorted(code):
        instruction = code[address]

        if address == entry:
            lines.append("guest_entry:")

        lines.append(f"{label(address)}:")
        delay = code.get(address + 4)
        delay_lines = emit_at(address + 4, delay) if delay else []

        if instruction[0] in branch_names and len(delay_lines) > 1:
            add_branch_with_long_delay(
                lines,
                address,
                instruction,
                delay,
                label,
                emit_at,
            )
            continue

        if instruction[0] in ("j", "jal", "jr", "jalr") and len(delay_lines) > 1:
            add_jump_with_long_delay(
                lines,
                address,
                instruction,
                delay,
                label,
                emit_at,
            )
            continue

        for line in emit_at(address, instruction):
            add_line(lines, line)

    binary_path = output_path.with_suffix(".bin")
    map_path = output_path.with_suffix(".map")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    binary_path.write_bytes(data)

    add_map(lines, map_addresses, label)
    file_size = add_image(
        lines,
        binary_path,
        data,
        segments,
        references["words"],
        label,
    )
    add_info(lines, base, len(data), file_size, module["gp"])

    output_path.write_text("\n".join(lines) + "\n")
    write_map(map_path, map_addresses, label)

    print(f"o assembly saiu do forno {output_path}")
    print(f"o não binário caiu no prato do carnivoro {binary_path}")
    print(f"o mapa dessa safada ficou em {map_path}")
    print(f"entradas de código na pista {len(starts)}")
    print(f"callbacks pegos no flagra com a julia do segundo ano do ensino medio no canto esquerdo da sala em são paulo que ela largou ele porque ela tava querendo focar nos estudos{len(callbacks)}")
    print(f"instruções que foram pra chapa {len(code)}")
    print(f"tipos de instrução que fizeram o decoder gozar {len(names)}")
    print(f"relocações que pararam de apontar pra puta que pariu {relocations}")
    print(f"ponteiros de código que acharam o rumo {len(references['targets'])}")
    print(f"tabelas de salto que pararam de se esconder {len(jump_tables)}")
    print(f"endereços que não vão mais se perder no mapa {len(map_addresses)}")
    print(f"imports amarrados na nega {len(external)}")
    print(f"dados do jogo no pacote {file_size} bytes")
    print(f"bss zerada bonitinha {len(data) - file_size} bytes")
    print("agora sim está de pé diferente do meu pa papel")


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
