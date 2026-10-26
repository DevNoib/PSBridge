def file_end(segments):
    return max(offset + file_size for offset, _, file_size, _, _, _ in segments)


def quote_path(path):
    return str(path).replace("\\", "\\\\").replace('"', '\\"')


def add_image(lines, path, data, segments, pointers, label, image_base):
    file_size = file_end(segments)
    position = 0

    lines.extend([
        '.section .guest_image,"aw",@progbits',
        ".balign 1048576",
        f".space {image_base & 0xfffff}",
        ".globl guest_memory",
        ".globl guest_file_end",
        "guest_memory:",
    ])

    for offset, target in sorted(pointers.items()):
        if offset + 4 > file_size:
            raise ValueError(f"o ponteiro em 0x{offset:08x} caiu fora dos dados")
        if offset < position:
            raise ValueError(f"o ponteiro em 0x{offset:08x} caiu em cima de outro")

        if position < offset:
            lines.append(f'.incbin "{quote_path(path)}", {position}, {offset - position}')

        lines.append(f".word {label(target)}")
        position = offset + 4

    if position < file_size:
        lines.append(f'.incbin "{quote_path(path)}", {position}, {file_size - position}')

    lines.extend([
        "guest_file_end:",
        '.section .guest_bss,"aw",@nobits',
        ".balign 4",
        ".globl guest_bss",
        ".globl guest_memory_end",
        "guest_bss:",
        f".space {len(data) - file_size}",
        "guest_memory_end:",
    ])

    return file_size


def add_map(lines, addresses, label):
    addresses = sorted(addresses)
    lines.extend([
        '.section .rodata.guest_map,"a",@progbits',
        ".balign 4",
        ".globl guest_map_count",
        ".globl guest_map",
        ".globl guest_map_end",
        "guest_map_count:",
        f".word {len(addresses)}",
        "guest_map:",
    ])

    for address in addresses:
        lines.append(f".word 0x{address:08x}, {label(address)}")

    lines.append("guest_map_end:")


def add_overlay_map(lines, entries):
    lines.extend([
        '.section .rodata.overlay_map,"a",@progbits',
        ".balign 4",
        ".globl overlay_map_count",
        ".globl overlay_map",
        "overlay_map_count:",
        f".word {len(entries)}",
        "overlay_map:",
    ])

    for address, opcode, target in entries:
        lines.append(f".word 0x{address:08x}, 0x{opcode:08x}, {target}")


def add_info(lines, base, data_size, file_size, gp):
    lines.extend([
        '.section .rodata.guest_info,"a",@progbits',
        ".balign 4",
        ".globl guest_info",
        "guest_info:",
        ".word guest_memory",
        f".word 0x{base:08x}",
        f".word {data_size}",
        f".word {file_size}",
        f".word 0x{gp:08x}",
    ])


def write_map(path, addresses, label):
    text = "".join(
        f"0x{address:08x} {label(address)}\n"
        for address in sorted(addresses)
    )
    path.write_text(text)
