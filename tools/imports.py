import struct
import sys
from pathlib import Path

from elf_reader import read_header, read_segments


def file_offset(segments, address, size=1):
    for offset, vaddr, file_size, _, _, _ in segments:
        if vaddr <= address and address + size <= vaddr + file_size:
            return offset + address - vaddr

    raise ValueError(f"esse endereço sumiu do mapa 0x{address:08x}")


def read_string(data, segments, address):
    offset = file_offset(segments, address)
    end = data.find(b"\0", offset)

    if end == -1:
        raise ValueError(f"essa string não aguentou e gozou em 0x{address:08x}")

    return data[offset:end].decode("ascii")


def valid_address(segments, address):
    if address == 0:
        return True

    for _, vaddr, _, memory_size, _, _ in segments:
        if vaddr <= address <= vaddr + memory_size:
            return True

    return False


def find_module(data, segments):
    found = []

    for segment_offset, _, file_size, _, _, _ in segments:
        end = segment_offset + file_size - 52

        for offset in range(segment_offset, end + 1, 4):
            attributes, version = struct.unpack_from("<HH", data, offset)
            raw_name = data[offset + 4:offset + 32]

            if version != 0x0101 or b"\0" not in raw_name:
                continue

            name = raw_name.split(b"\0", 1)[0]
            if not name or not all(32 <= char < 127 for char in name):
                continue

            gp, export_start, export_end, import_start, import_end = struct.unpack_from(
                "<IIIII", data, offset + 32
            )
            addresses = (export_start, export_end, import_start, import_end)

            if not all(valid_address(segments, address) for address in addresses):
                continue
            if export_start > export_end or import_start > import_end:
                continue
            if (import_end - import_start) % 20:
                continue

            found.append({
                "name": name.decode("ascii"),
                "attributes": attributes,
                "gp": gp,
                "export_start": export_start,
                "export_end": export_end,
                "import_start": import_start,
                "import_end": import_end,
            })

    if not found:
        raise ValueError("o módulo do psp foi pra puta que pariu")

    return max(found, key=lambda module: module["import_end"] - module["import_start"])


def read_imports(data, segments, module):
    imports = []
    address = module["import_start"]

    while address < module["import_end"]:
        offset = file_offset(segments, address, 20)
        name_address, _, _, length, _, functions, nid_address, stub_address = struct.unpack_from(
            "<IHHBBHII", data, offset
        )

        if length < 5:
            raise ValueError(f"esse import tá mais torto que minha pica em 0x{address:08x}")

        library = read_string(data, segments, name_address)

        for index in range(functions):
            nid_offset = file_offset(segments, nid_address + index * 4, 4)
            nid = struct.unpack_from("<I", data, nid_offset)[0]
            imports.append({
                "library": library,
                "nid": nid,
                "stub": stub_address + index * 8,
            })

        address += length * 4

    return imports


def inspect(path):
    data = path.read_bytes()
    _, _, ph_offset, ph_size, ph_count = read_header(data)
    segments = read_segments(data, ph_offset, ph_size, ph_count)
    module = find_module(data, segments)
    imports = read_imports(data, segments, module)
    libraries = sorted({item["library"] for item in imports})

    print("gozei nas parada do psp")
    print(f"arquivo {path}")
    print(f"módulo {module['name']}")
    print(f"bibliotecas {len(libraries)}")
    print(f"funções {len(imports)}")

    for library in libraries:
        print()
        print(library)

        for item in imports:
            if item["library"] == library:
                print(f"  stub 0x{item['stub']:08x}  nid 0x{item['nid']:08x}")


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("jogo.elf")

    try:
        inspect(path)
        return 0
    except (OSError, UnicodeDecodeError, ValueError, struct.error) as error:
        print(f"deu ruim {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
