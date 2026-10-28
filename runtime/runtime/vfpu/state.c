#include <math.h>
#include <string.h>

#include "vfpu.h"

u32 vfpu_registers[VFPU_REGISTERS];
u32 vfpu_controls[VFPU_CONTROLS];

u32 vfpu_prefix_s = 0x000000e4;
u32 vfpu_prefix_t = 0x000000e4;
u32 vfpu_prefix_d;

u8 vfpu_have_s;
u8 vfpu_have_t;
u8 vfpu_have_d;

float vfpu_float(u32 bits)
{
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

u32 vfpu_bits(float value)
{
    u32 bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int vfpu_vector_lane(int register_id, int size, int lane)
{
    int matrix = (register_id >> 2) & 7;
    int column = register_id & 3;
    int row;

    if (size == 1)
        row = (register_id >> 5) & 3;
    else if (size == 3)
        row = (register_id >> 6) & 1;
    else
        row = (register_id >> 5) & 2;

    if (size != 1 && (register_id & 0x20))
        return matrix * 4 + ((row + lane) & 3) + column * 32;

    return matrix * 4 + column + ((row + lane) & 3) * 32;
}

int vfpu_matrix_lane(int register_id, int size, int column, int row)
{
    int matrix = (register_id >> 2) & 7;
    int first_column = register_id & 3;
    int first_row;

    if (size == 3)
        first_row = (register_id >> 6) & 1;
    else
        first_row = (register_id >> 5) & 2;

    if (register_id & 0x20) {
        return matrix * 4
            + ((first_row + row) & 3)
            + ((first_column + column) & 3) * 32;
    }

    return matrix * 4
        + ((first_column + column) & 3)
        + ((first_row + row) & 3) * 32;
}

void vfpu_read_vector(int register_id, int size, float output[VFPU_VECTOR_SIZE])
{
    int index;

    for (index = 0; index < size; index++) {
        output[index] = vfpu_float(
            vfpu_registers[vfpu_vector_lane(register_id, size, index)]
        );
    }
}

void vfpu_read_prefix(
    int register_id,
    int size,
    float output[VFPU_VECTOR_SIZE],
    u32 prefix,
    int enabled
)
{
    static const float constants[8] = {
        0.0f, 1.0f, 2.0f, 0.5f,
        3.0f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f
    };

    float original[VFPU_VECTOR_SIZE] = { 0 };
    int index;

    vfpu_read_vector(register_id, size, original);

    if (!enabled) {
        memcpy(output, original, sizeof(original));
        return;
    }

    for (index = 0; index < size; index++) {
        int component = (prefix >> (index * 2)) & 3;
        float value = original[component < size ? component : 0];

        if (prefix & (1u << (12 + index))) {
            int constant = component | (((prefix >> (8 + index)) & 1) * 4);
            value = constants[constant];
        } else if (prefix & (1u << (8 + index))) {
            value = fabsf(value);
        }

        if (prefix & (1u << (16 + index)))
            value = -value;

        output[index] = value;
    }
}

void vfpu_write_vector(
    int register_id,
    int size,
    const float input[VFPU_VECTOR_SIZE],
    int use_prefix
)
{
    int index;

    for (index = 0; index < size; index++) {
        float value = input[index];

        if (use_prefix && vfpu_have_d) {
            int saturation = (vfpu_prefix_d >> (index * 2)) & 3;

            if (saturation == 1) {
                if (value < 0)
                    value = 0;
                if (value > 1)
                    value = 1;
            }

            if (saturation == 3) {
                if (value < -1)
                    value = -1;
                if (value > 1)
                    value = 1;
            }

            if (vfpu_prefix_d & (1u << (8 + index)))
                continue;
        }

        vfpu_registers[vfpu_vector_lane(register_id, size, index)] = vfpu_bits(value);
    }
}

void vfpu_finish_prefix(void)
{
    vfpu_have_s = 0;
    vfpu_have_t = 0;
    vfpu_have_d = 0;
}

void vfpu_read_matrix(int register_id, int size, float output[4][4])
{
    int column;
    int row;

    for (column = 0; column < size; column++) {
        for (row = 0; row < size; row++) {
            output[column][row] = vfpu_float(
                vfpu_registers[vfpu_matrix_lane(register_id, size, column, row)]
            );
        }
    }
}

void vfpu_write_matrix(int register_id, int size, float input[4][4])
{
    int column;
    int row;

    for (column = 0; column < size; column++) {
        for (row = 0; row < size; row++) {
            vfpu_registers[vfpu_matrix_lane(register_id, size, column, row)] =
                vfpu_bits(input[column][row]);
        }
    }
}
