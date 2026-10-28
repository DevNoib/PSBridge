#include <stdint.h>

#include "psbridge.h"
#include "psbridge_log.h"
#include "vfpu.h"

void *vfpu_pointer(u32 raw_address, u32 bytes)
{
    u32 address = raw_address & 0x3fffffff;

    if ((address & 3) || !address || bytes > 0x10000)
        goto invalid;

    if (address < 0x02000000 &&
        address + bytes >= address &&
        address + bytes <= 0x02000000)
        return (void *)(uintptr_t)address;

    if (address >= 0x04000000 &&
        address <= 0x04180000 &&
        bytes <= 0x04180000 - address) {
        return (u8 *)psbridge_video_memory() + (address - 0x04000000);
    }

    if (address >= guest_info.base &&
        address - guest_info.base <= guest_info.memory_size &&
        bytes <= guest_info.memory_size - (address - guest_info.base)) {
        return psbridge_game_pointer((const void *)(uintptr_t)address);
    }

invalid:
    PSB_LOG(VFPU, "vfpu tentou lamber memoria torta %08lx tamanho %lu",
        (unsigned long)raw_address,
        (unsigned long)bytes
    );
    return NULL;
}

int vfpu_execute_memory(u32 opcode, int major, u32 *gpr)
{
    int base;
    int size;
    int register_id;
    int offset;
    int index;
    u32 address;
    u32 *memory;

    if (major != 50 && major != 54 && major != 58 && major != 62)
        return 0;

    base = (opcode >> 21) & 31;
    size = (major == 54 || major == 62) ? 4 : 1;
    register_id = ((opcode >> 16) & 31) | ((opcode & (size == 4 ? 1 : 3)) << 5);
    offset = (int)(short)(opcode & 0xfffc);
    address = gpr[base] + offset;
    memory = (u32 *)vfpu_pointer(address, size * 4);

    if (!memory)
        return 1;

    for (index = 0; index < size; index++) {
        int lane = vfpu_vector_lane(register_id, size, index);

        if (major == 50 || major == 54)
            vfpu_registers[lane] = memory[index];
        else
            memory[index] = vfpu_registers[lane];
    }

    return 1;
}

int vfpu_execute_prefix(u32 opcode)
{
    if ((opcode & 0xff000000) == 0xdc000000) {
        vfpu_prefix_s = opcode & 0xfffff;
        vfpu_have_s = 1;
        vfpu_controls[0] = vfpu_prefix_s;
        return 1;
    }

    if ((opcode & 0xff000000) == 0xdd000000) {
        vfpu_prefix_t = opcode & 0xfffff;
        vfpu_have_t = 1;
        vfpu_controls[1] = vfpu_prefix_t;
        return 1;
    }

    if ((opcode & 0xff000000) == 0xde000000) {
        vfpu_prefix_d = opcode & 0xfff;
        vfpu_have_d = 1;
        vfpu_controls[2] = vfpu_prefix_d;
        return 1;
    }

    return 0;
}

int vfpu_execute_immediate(u32 opcode)
{
    float result[VFPU_VECTOR_SIZE] = { 0 };
    int register_id;

    if ((opcode & 0xff800000) == 0xdf000000) {
        register_id = (opcode >> 16) & 127;
        result[0] = (float)(short)(opcode & 0xffff);
        vfpu_write_vector(register_id, 1, result, 0);
        return 1;
    }

    if ((opcode & 0xff800000) == 0xdf800000) {
        u32 half = opcode & 0xffff;
        u32 sign = (half & 0x8000) << 16;
        int exponent = (half >> 10) & 31;
        u32 bits;

        if (exponent == 0)
            bits = sign;
        else if (exponent == 31)
            bits = sign | 0x7f800000u | ((half & 1023) << 13);
        else
            bits = sign | ((exponent + 112u) << 23) | ((half & 1023) << 13);

        register_id = (opcode >> 16) & 127;
        result[0] = vfpu_float(bits);
        vfpu_write_vector(register_id, 1, result, 0);
        return 1;
    }

    return 0;
}

int vfpu_execute_transfer(u32 opcode, u32 *gpr)
{
    u32 direction = opcode & 0xffe00000;
    int target;
    int control;
    int register_id;

    if (direction != 0x48600000 && direction != 0x48e00000)
        return 0;

    target = (opcode >> 16) & 31;
    control = (opcode & 0x80) != 0;
    register_id = opcode & 127;

    if (direction == 0x48600000) {
        if (target) {
            if (control)
                gpr[target] = vfpu_controls[register_id & 15];
            else
                gpr[target] = vfpu_registers[vfpu_vector_lane(register_id, 1, 0)];
        }
    } else {
        if (control)
            vfpu_controls[register_id & 15] = gpr[target];
        else
            vfpu_registers[vfpu_vector_lane(register_id, 1, 0)] = gpr[target];
    }

    return 1;
}
