import struct
import sys
from pathlib import Path

from allegrex import decode
from elf_reader import read_function, read_header, read_segments
from r5900 import emit


def direct_target(instruction):
    if instruction[0] in ("j", "jal"):
        return instruction[1]
    if instruction[0] in ("beq", "bne"):
        return instruction[3]

    return None


def recompile(input_path, output_path):
    print("recompiling... let him cook")
    data = input_path.read_bytes()
    _, entry, ph_offset, ph_size, ph_count = read_header(data)
    segments = read_segments(data, ph_offset, ph_size, ph_count)
    code = read_function(data, segments, entry)
    code = [(address, decode(address, word)) for address, word in code]
    addresses = {address for address, _ in code}

    def label(address):
        name = "code" if address in addresses else "guest"
        return f"{name}_{address:08x}"

    external = set()
    for _, instruction in code:
        target = direct_target(instruction)
        if target is not None and target not in addresses:
            external.add(target)

    lines = [".set noreorder", ".text", ".globl guest_entry"]

    for address in sorted(external):
        lines.append(f".extern {label(address)}")

    for address, instruction in code:
        if address == entry:
            lines.append("guest_entry:")
        lines.append(f"{label(address)}:")
        lines.append(f"    {emit(instruction, label)}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n")
    print(f"wrote {output_path}")
    print(f"instructions {len(code)}")
    print(f"external targets {len(external)}")
    print("we are so back")


def main():
    input_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")
    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/entry.s")

    try:
        recompile(input_path, output_path)
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"error {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
