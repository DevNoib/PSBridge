from allegrex import branch_names, decode
from elf_reader import read_function


create_thread = "ThreadManForUser", 0x446d8de6

writes_register = {
    "sll", "srl", "sra", "sllv", "srlv", "srav", "addu", "subu",
    "and", "or", "xor", "nor", "slt", "sltu", "movz", "movn",
    "min", "max", "clz", "mfhi", "mflo", "addiu", "slti", "sltiu",
    "andi", "ori", "xori", "lui", "lb", "lbu", "lh", "lhu", "lwl",
    "lwr", "lw", "mfc1", "ext", "ins",
}


def is_code(segments, address):
    for _, vaddr, file_size, _, flags, _ in segments:
        if flags & 1 and vaddr <= address <= vaddr + file_size - 4:
            return True
    return False


def output_register(instruction):
    if instruction[0] in writes_register:
        return instruction[1]
    if instruction[0] == "jal":
        return 31
    if instruction[0] == "jalr":
        return instruction[1]
    return None


def result_value(instructions, index, instruction, seen):
    name = instruction[0]

    if name == "lui":
        return instruction[2] << 16
    if name in ("addiu", "ori"):
        source = register_value(instructions, index, instruction[2], seen.copy())
        if source is None:
            return None
        if name == "addiu":
            return (source + instruction[3]) & 0xffffffff
        return source | instruction[3]
    if name in ("addu", "or"):
        left = register_value(instructions, index, instruction[2], seen.copy())
        right = register_value(instructions, index, instruction[3], seen.copy())
        if left is None or right is None:
            return None
        if name == "addu":
            return (left + right) & 0xffffffff
        return left | right
    if name == "sll":
        value = register_value(instructions, index, instruction[2], seen.copy())
        if value is not None:
            return (value << instruction[3]) & 0xffffffff

    return None


def register_value(instructions, index, register, seen=None):
    if register == 0:
        return 0

    seen = seen or set()
    key = index, register
    if key in seen:
        return None
    seen.add(key)

    controls = ("j", "jal", "jr", "jalr") + branch_names

    for position in range(index - 1, max(-1, index - 32), -1):
        instruction = instructions[position][1]

        if instruction[0] in controls:
            return None
        if output_register(instruction) == register:
            return result_value(instructions, position, instruction, seen)

    return None


def callback_value(instructions, index, register):
    value = register_value(instructions, index, register)
    call_address = instructions[index][0]

    if index + 1 >= len(instructions):
        return value

    delay_address, delay = instructions[index + 1]
    if delay_address != call_address + 4 or output_register(delay) != register:
        return value

    return result_value(instructions, index, delay, set())


def find_functions(data, segments, entry, import_map):
    functions = {}
    callbacks = set()
    pending = [entry]

    while pending:
        start = pending.pop()
        if start in functions:
            continue

        instructions = []
        for address, word in read_function(data, segments, start, import_map):
            instruction = decode(address, word)
            if instruction[0] == "word":
                raise ValueError(f"que porra de instrução é essa em 0x{address:08x}")
            instructions.append((address, instruction))

            if instruction[0] != "jal":
                continue

            target = instruction[1]
            if target not in import_map and is_code(segments, target) and target not in functions:
                pending.append(target)

        for index, (_, instruction) in enumerate(instructions):
            if instruction[0] != "jal" or instruction[1] not in import_map:
                continue

            imported = import_map[instruction[1]]
            if (imported["library"], imported["nid"]) != create_thread:
                continue

            callback = callback_value(instructions, index, 5)
            if callback is None or not is_code(segments, callback):
                continue

            callbacks.add(callback)
            if callback not in functions:
                pending.append(callback)

        functions[start] = instructions

    return functions, callbacks
