import struct
import sys
from pathlib import Path


def signed(value):
    return value - 0x10000 if value & 0x8000 else value


def read_headers(data):
    ph_offset = struct.unpack_from("<I", data, 28)[0]
    sh_offset = struct.unpack_from("<I", data, 32)[0]
    ph_size, ph_count = struct.unpack_from("<HH", data, 42)
    sh_size, sh_count = struct.unpack_from("<HH", data, 46)

    programs = [
        struct.unpack_from("<IIIIIIII", data, ph_offset + index * ph_size)
        for index in range(ph_count)
    ]
    sections = [
        struct.unpack_from("<IIIIIIIIII", data, sh_offset + index * sh_size)
        for index in range(sh_count)
    ]
    return programs, sections


def build_image(data, programs):
    loads = [program for program in programs if program[0] == 1]

    if not loads:
        raise ValueError("não tem nenhum segmento pra carregar nessa porra")

    start = min(program[2] for program in loads)
    end = max(program[2] + program[5] for program in loads)
    image = bytearray(end - start)

    for program in loads:
        _, offset, address, _, file_size, _, _, _ = program
        image_offset = address - start
        image[image_offset:image_offset + file_size] = data[offset:offset + file_size]

    return image, start


def read_relocations(data, sections):
    tables = []

    for section in sections:
        kind = section[1]
        offset = section[4]
        size = section[5]

        if kind != 0x700000a0 or size == 0:
            continue
        if size % 8:
            raise ValueError("a tabela de relocação veio mais torta que minha pica")

        tables.append([
            struct.unpack_from("<II", data, offset + position)
            for position in range(0, size, 8)
        ])

    return tables


def patch_offset(programs, relocation, image_start):
    offset, info = relocation
    segment = (info >> 8) & 0xff

    if segment >= len(programs) or programs[segment][0] != 1:
        raise ValueError(f"a relocação apontou pro segmento do caralho {segment}")

    return programs[segment][2] + offset - image_start


def target_address(programs, relocation, base):
    target = (relocation[1] >> 16) & 0xff

    if target >= len(programs) or programs[target][0] != 1:
        raise ValueError(f"a relocação quer um segmento que nem existe {target}")

    return base + programs[target][2]


def apply_table(image, programs, table, base, image_start, counts):
    words = []

    for relocation in table:
        offset = patch_offset(programs, relocation, image_start)
        if offset < 0 or offset + 4 > len(image):
            raise ValueError(f"a relocação saiu passeando em 0x{offset:08x}")
        words.append(struct.unpack_from("<I", image, offset)[0])

    for index, relocation in enumerate(table):
        offset = patch_offset(programs, relocation, image_start)
        info = relocation[1]
        kind = info & 0xff
        word = words[index]
        target = target_address(programs, relocation, base)

        if kind == 2:
            patched = word + target
        elif kind == 4:
            patched = (word & 0xfc000000) | (((word & 0x03ffffff) + (target >> 2)) & 0x03ffffff)
        elif kind == 5:
            patched = None

            for next_index in range(index + 1, len(table)):
                next_relocation = table[next_index]
                next_kind = next_relocation[1] & 0xff

                if next_kind == 5:
                    continue
                if next_kind not in (1, 6):
                    continue
                if (next_relocation[1] >> 8) != (info >> 8):
                    continue

                value = ((word & 0xffff) << 16) + signed(words[next_index] & 0xffff) + target
                patched = (word & 0xffff0000) | (((value + 0x8000) >> 16) & 0xffff)
                break

            if patched is None:
                raise ValueError(f"o hi16 ficou solteiro em 0x{offset:08x}")
        elif kind in (1, 6):
            patched = (word & 0xffff0000) | (((word & 0xffff) + target) & 0xffff)
        else:
            raise ValueError(f"ainda não sei realocar o tipo {kind}")

        struct.pack_into("<I", image, offset, patched & 0xffffffff)
        counts[kind] = counts.get(kind, 0) + 1


def relocate(data, base=0x00400000):
    programs, sections = read_headers(data)
    image, image_start = build_image(data, programs)
    tables = read_relocations(data, sections)
    counts = {}

    for table in tables:
        apply_table(image, programs, table, base, image_start, counts)

    segments = []
    for kind, _, address, _, file_size, memory_size, flags, align in programs:
        if kind == 1:
            segments.append((
                address - image_start,
                base + address,
                file_size,
                memory_size,
                flags,
                align,
            ))

    entry = base + struct.unpack_from("<I", data, 24)[0]
    total = sum(counts.values())
    return bytes(image), segments, entry, total


def main():
    input_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")
    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/game.bin")

    try:
        image, _, entry, count = relocate(input_path.read_bytes())
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(image)
        print(f"entrada realocada 0x{entry:08x}")
        print(f"relocações {count}")
        print(f"escrevi {output_path}")
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"deu ruim {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
