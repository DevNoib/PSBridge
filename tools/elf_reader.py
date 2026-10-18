import struct
import sys
from pathlib import Path

from allegrex import decode, format_instruction


def read_header(data):
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError("bad boy")
    if data[4] != 1 or data[5] != 1:
        raise ValueError("elf must be 32-bit little-endian")

    elf_type, machine = struct.unpack_from("<HH", data, 16)
    entry, ph_offset = struct.unpack_from("<II", data, 24)
    ph_size, ph_count = struct.unpack_from("<HH", data, 42)

    if machine != 8:
        raise ValueError("elf must be mips")
    if ph_size < 32:
        raise ValueError("bad program header")
    if ph_offset + ph_size * ph_count > len(data):
        raise ValueError("bad segment table")

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
            raise ValueError(f"bad segment {index}")

        segments.append((file_off, vaddr, file_size, memory_size, flags, align))

    return segments


def read_word(data, segments, address):
    for file_off, vaddr, file_size, _, flags, _ in segments:
        if not flags & 1:
            continue
        if not vaddr <= address <= vaddr + file_size - 4:
            continue

        offset = file_off + address - vaddr
        return struct.unpack_from("<I", data, offset)[0]

    raise ValueError(f"code not found at 0x{address:08x}")


def read_function(data, segments, entry, limit=4096):
    code = {}
    pending = [entry]

    while pending:
        address = pending.pop()

        while address not in code:
            if len(code) >= limit:
                raise ValueError("function is too large")

            word = read_word(data, segments, address)
            code[address] = word
            op = word >> 26
            funct = word & 63

            if op not in (2, 3, 4, 5) and not (op == 0 and funct in (8, 9)):
                address += 4
                continue

            delay = address + 4
            if delay not in code:
                code[delay] = read_word(data, segments, delay)

            if op in (4, 5):
                value = word & 0xffff
                offset = value - 0x10000 if value & 0x8000 else value
                pending.append(address + 8)
                pending.append((address + 4 + offset * 4) & 0xffffffff)
            elif op == 2:
                target = ((address + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)
                pending.append(target)
            elif op == 3 or funct == 9:
                pending.append(address + 8)

            break

    return sorted(code.items())


def inspect(path):
    data = path.read_bytes()
    elf_type, entry, offset, size, count = read_header(data)
    segments = read_segments(data, offset, size, count)
    code = read_function(data, segments, entry)

    print(f"file {path}")
    print(f"type {'psp prx' if elf_type == 0xFFA0 else hex(elf_type)}")
    print(f"entry 0x{entry:08x}")
    print(f"load segments {len(segments)}")

    for index, segment in enumerate(segments):
        file_off, vaddr, file_size, memory_size, flags, align = segment
        access = "".join("rwx"[i] if flags & bit else "-" for i, bit in enumerate((4, 2, 1)))
        print(
            f"[{index}] offset=0x{file_off:08x} address=0x{vaddr:08x} "
            f"file=0x{file_size:x} memory=0x{memory_size:x} "
            f"flags={access} align=0x{align:x}"
        )

    print("entry code")
    for address, word in code:
        instruction = decode(address, word)
        print(f"0x{address:08x} 0x{word:08x} {format_instruction(instruction)}")


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")

    try:
        inspect(path)
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"error {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
