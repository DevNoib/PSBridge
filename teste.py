import struct
import sys
from pathlib import Path

def read_header(data):
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError("bad girl")
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


def inspect(path):
    data = path.read_bytes()
    elf_type, entry, offset, size, count = read_header(data)
    segments = read_segments(data, offset, size, count)

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
