import struct
import sys
from pathlib import Path

from allegrex import branch_names, decode, format_instruction, target_of


def read_header(data):
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError("esse arquivo tá de sacanagem")

    if data[4] != 1 or data[5] != 1:
        raise ValueError("essa porra tem que ser elf 32-bit little-endian")

    elf_type, machine = struct.unpack_from("<HH", data, 16)
    entry, ph_offset = struct.unpack_from("<II", data, 24)
    ph_size, ph_count = struct.unpack_from("<HH", data, 42)

    if machine != 8:
        raise ValueError("essa porra tem que ser mips")

    if ph_size < 32:
        raise ValueError("o cabeçalho do programa veio mais fudido que prisioneiro em prisão")

    if ph_offset + ph_size * ph_count > len(data):
        raise ValueError("a tabela de segmentos veio toda fudida")

    return elf_type, entry, ph_offset, ph_size, ph_count


def read_segments(data, ph_offset, ph_size, ph_count):
    segments = []

    for index in range(ph_count):
        offset = ph_offset + index * ph_size
        segment = struct.unpack_from("<IIIIIIII", data, offset)
        kind, file_off, vaddr, _, file_size, memory_size, flags, align = segment

        if kind != 1:
            continue

        if file_off + file_size > len(data):
            raise ValueError(f"o segmento {index} veio fudido")

        segments.append((file_off, vaddr, file_size, memory_size, flags, align))

    return segments


def read_code(data):
    section_offset = struct.unpack_from("<I", data, 32)[0]
    section_size, section_count = struct.unpack_from("<HH", data, 46)
    code = []

    if section_size < 40 or section_offset + section_size * section_count > len(data):
        raise ValueError("a tabela de seções veio toda fudida")

    for index in range(section_count):
        offset = section_offset + index * section_size
        _, kind, flags, address, file_offset, size = struct.unpack_from("<IIIIII", data, offset)

        if not flags & 4 or kind == 8:
            continue

        if file_offset + size > len(data):
            raise ValueError(f"a seção {index} veio fudida")

        for position in range(0, size - 3, 4):
            word = struct.unpack_from("<I", data, file_offset + position)[0]
            code.append((address + position, word))

    return code


def read_word(data, segments, address):
    for file_off, vaddr, file_size, _, flags, _ in segments:
        if not flags & 1:
            continue

        if not vaddr <= address <= vaddr + file_size - 4:
            continue

        offset = file_off + address - vaddr
        return struct.unpack_from("<I", data, offset)[0]

    raise ValueError(f"cadê o código em 0x{address:08x}")


def read_function(data, segments, entry, stops=None, limit=4096):
    code = {}
    pending = [entry]
    stops = stops or set()

    while pending:
        address = pending.pop()

        while address not in code:
            if address in stops:
                break

            if len(code) >= limit:
                raise ValueError("essa função tá grande pra caralho")

            word = read_word(data, segments, address)
            code[address] = word
            instruction = decode(address, word)
            name = instruction[0]

            if name not in ("j", "jal", "jr", "jalr") + branch_names:
                address += 4
                continue

            delay = address + 4
            if delay not in code:
                code[delay] = read_word(data, segments, delay)

            if name in branch_names:
                pending.append(address + 8)
                pending.append(target_of(instruction))
            elif name == "j":
                pending.append(instruction[1])
            elif name in ("jal", "jalr"):
                pending.append(address + 8)

            break

    return sorted(code.items())


def inspect(path):
    data = path.read_bytes()
    elf_type, entry, offset, size, count = read_header(data)
    segments = read_segments(data, offset, size, count)
    code = read_function(data, segments, entry)

    print(f"arquivo {path}")
    print(f"tipo {'psp prx' if elf_type == 0xFFA0 else hex(elf_type)}")
    print(f"entrada 0x{entry:08x}")
    print(f"segmentos carregados {len(segments)}")

    for index, segment in enumerate(segments):
        file_off, vaddr, file_size, memory_size, flags, align = segment
        access = "".join("rwx"[i] if flags & bit else "-" for i, bit in enumerate((4, 2, 1)))
        print(
            f"[{index}] posição=0x{file_off:08x} endereço=0x{vaddr:08x} "
            f"arquivo=0x{file_size:x} memória=0x{memory_size:x} "
            f"permissões={access} alinhamento=0x{align:x}"
        )

    print("código da entrada")
    for address, word in code:
        instruction = decode(address, word)
        print(f"0x{address:08x} 0x{word:08x} {format_instruction(instruction)}")


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")

    try:
        inspect(path)
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"deu ruim {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
