#include <math.h>

#include "vfpu.h"

static int compare_condition(int condition, float source, float target)
{
    switch (condition) {
        case 0:  return 0;
        case 1:  return source == target;
        case 2:  return source < target;
        case 3:  return source <= target;
        case 4:  return 1;
        case 5:  return source != target;
        case 6:  return source >= target;
        case 7:  return source > target;
        case 8:  return source == 0;
        case 9:  return source != 0;
        case 10: return source < 0;
        case 11: return source >= 0;
        case 12: return isinf(source) || isnan(source);
        case 13: return isfinite(source);
        case 14: return isnan(source);
        case 15: return !isnan(source);
    }

    return 0;
}

int vfpu_execute_arithmetic(
    u32 opcode,
    int major,
    int subgroup,
    int size,
    int destination,
    int source,
    int target
)
{
    float s[VFPU_VECTOR_SIZE] = { 0 };
    float t[VFPU_VECTOR_SIZE] = { 0 };
    float result[VFPU_VECTOR_SIZE] = { 0 };
    int index;

    if (major < 24 || major > 27)
        return 0;

    vfpu_read_prefix(source, size, s, vfpu_prefix_s, vfpu_have_s);
    vfpu_read_prefix(target, size, t, vfpu_prefix_t, vfpu_have_t);

    switch (major) {
        case 24:
            switch (subgroup) {
                case 0:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] + t[index];
                    break;

                case 1:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] - t[index];
                    break;

                case 2:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] * exp2f(t[index]);
                    break;

                case 7:
                    for (index = 0; index < size; index++)
                        result[index] = t[index] == 0 ? 0 : s[index] / t[index];
                    break;

                default:
                    return 0;
            }
            break;

        case 25:
            switch (subgroup) {
                case 0:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] * t[index];
                    break;

                case 1: {
                    float dot = 0;

                    for (index = 0; index < size; index++)
                        dot += s[index] * t[index];

                    result[0] = dot;
                    size = 1;
                    break;
                }

                case 2:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] * t[0];
                    break;

                case 4: {
                    float dot = 0;

                    for (index = 0; index < size - 1; index++)
                        dot += s[index] * t[index];

                    result[0] = dot + t[size - 1];
                    size = 1;
                    break;
                }

                case 5:
                    if (size != 3)
                        return 0;

                    result[0] = s[1] * t[2];
                    result[1] = s[2] * t[0];
                    result[2] = s[0] * t[1];
                    break;

                case 6:
                    if (size != 2)
                        return 0;

                    result[0] = s[0] * t[1] - s[1] * t[0];
                    size = 1;
                    break;

                default:
                    return 0;
            }
            break;

        case 26:
            return 0;

        case 27:
            if (subgroup == 0) {
                u32 flags = 0;
                u32 mask = (1u << size) - 1;
                int condition = opcode & 15;

                for (index = 0; index < size; index++) {
                    int match = compare_condition(condition, s[index], t[index]);
                    flags |= (match ? 1u : 0u) << index;
                }

                vfpu_controls[3] = flags
                    | ((flags & mask) ? 0x10 : 0)
                    | ((flags & mask) == mask ? 0x20 : 0);

                vfpu_finish_prefix();
                return 1;
            }

            switch (subgroup) {
                case 2:
                    for (index = 0; index < size; index++)
                        result[index] = fminf(s[index], t[index]);
                    break;

                case 3:
                    for (index = 0; index < size; index++)
                        result[index] = fmaxf(s[index], t[index]);
                    break;

                case 5:
                    for (index = 0; index < size; index++) {
                        result[index] = s[index] > t[index] ? 1.0f
                            : s[index] < t[index] ? -1.0f : 0.0f;
                    }
                    break;

                case 6:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] >= t[index] ? 1 : 0;
                    break;

                case 7:
                    for (index = 0; index < size; index++)
                        result[index] = s[index] < t[index] ? 1 : 0;
                    break;

                default:
                    return 0;
            }
            break;
    }

    vfpu_write_vector(destination, size, result, 1);
    vfpu_finish_prefix();
    return 1;
}

int vfpu_execute_unary(u32 opcode, int size, int destination, int source)
{
    float s[VFPU_VECTOR_SIZE] = { 0 };
    float result[VFPU_VECTOR_SIZE] = { 0 };
    int kind;
    int index;

    if ((opcode & 0xff000000) != 0xd0000000)
        return 0;

    kind = (opcode >> 16) & 255;
    vfpu_read_prefix(source, size, s, vfpu_prefix_s, vfpu_have_s);

    for (index = 0; index < size; index++) {
        float value = s[index];

        switch (kind) {
            case 0:  result[index] = value; break;
            case 1:  result[index] = fabsf(value); break;
            case 2:  result[index] = -value; break;
            case 3:  result[index] = ((destination >> 5) & 3) == index ? 1 : 0; break;
            case 4:  result[index] = fminf(1, fmaxf(0, value)); break;
            case 5:  result[index] = fminf(1, fmaxf(-1, value)); break;
            case 6:  result[index] = 0; break;
            case 7:  result[index] = 1; break;
            case 16: result[index] = value ? 1 / value : 0; break;
            case 17: result[index] = value > 0 ? 1 / sqrtf(value) : 0; break;
            case 18: result[index] = sinf(value * 1.57079632679f); break;
            case 19: result[index] = cosf(value * 1.57079632679f); break;
            case 20: result[index] = exp2f(value); break;
            case 21: result[index] = value > 0 ? log2f(value) : 0; break;
            case 22: result[index] = sqrtf(fmaxf(value, 0)); break;
            case 23:
                result[index] = asinf(fmaxf(-1, fminf(1, value))) / 1.57079632679f;
                break;
            case 24: result[index] = value ? -1 / value : 0; break;
            case 26: result[index] = -sinf(value * 1.57079632679f); break;
            case 28: result[index] = exp2f(-value); break;
            case 68: result[index] = 1 - value; break;
            case 70: result[index] = 0; break;
            case 71: result[index] = value; break;
            case 74: result[index] = value < 0 ? -1 : value > 0 ? 1 : 0; break;
            default: return 0;
        }
    }

    vfpu_write_vector(destination, size, result, 1);
    vfpu_finish_prefix();
    return 1;
}

int vfpu_execute_conversion(u32 opcode, int size, int destination, int source)
{
    float s[VFPU_VECTOR_SIZE] = { 0 };
    float result[VFPU_VECTOR_SIZE] = { 0 };
    int kind;
    int scale;
    int index;

    if ((opcode & 0xff000000) != 0xd2000000 &&
        (opcode & 0xffe00000) != 0xd2000000 &&
        (opcode & 0xffe00000) != 0xd2200000 &&
        (opcode & 0xffe00000) != 0xd2400000 &&
        (opcode & 0xffe00000) != 0xd2600000 &&
        (opcode & 0xffe00000) != 0xd2800000)
        return 0;

    kind = (opcode >> 21) & 7;
    scale = (opcode >> 16) & 31;
    vfpu_read_prefix(source, size, s, vfpu_prefix_s, vfpu_have_s);

    if (kind <= 3) {
        for (index = 0; index < size; index++) {
            float value = ldexpf(s[index], scale);
            u32 converted;

            if (isnan(value)) {
                converted = 0x7fffffff;
            } else if (value >= 2147483647.0f) {
                converted = 0x7fffffff;
            } else if (value <= -2147483648.0f) {
                converted = 0x80000000;
            } else {
                float rounded;

                if (kind == 0)
                    rounded = nearbyintf(value);
                else if (kind == 1)
                    rounded = truncf(value);
                else if (kind == 2)
                    rounded = ceilf(value);
                else
                    rounded = floorf(value);

                converted = (u32)(s32)rounded;
            }

            vfpu_registers[vfpu_vector_lane(destination, size, index)] = converted;
        }
    } else if (kind == 4) {
        for (index = 0; index < size; index++) {
            result[index] = ldexpf(
                (float)(s32)vfpu_registers[vfpu_vector_lane(source, size, index)],
                -scale
            );
        }

        vfpu_write_vector(destination, size, result, 1);
    } else if (kind == 5) {
        int bit = (opcode >> 16) & 7;
        int truth = bit < 6
            ? ((vfpu_controls[3] >> bit) & 1)
            : ((vfpu_controls[3] >> 4) & 1);
        int move_when_false = (opcode & 0x00080000) != 0;
        float previous[VFPU_VECTOR_SIZE] = { 0 };

        vfpu_read_vector(destination, size, previous);

        for (index = 0; index < size; index++) {
            result[index] = (truth ^ move_when_false)
                ? s[index] : previous[index];
        }

        vfpu_write_vector(destination, size, result, 1);
    } else {
        return 0;
    }

    vfpu_finish_prefix();
    return 1;
}

int vfpu_execute_matrix(
    u32 opcode,
    int size,
    int destination,
    int source,
    int target
)
{
    float a[4][4] = { { 0 } };
    float b[4][4] = { { 0 } };
    float output[4][4] = { { 0 } };
    float result[VFPU_VECTOR_SIZE] = { 0 };
    int subtype;
    int column;
    int row;
    int index;

    if ((opcode & 0xff000000) != 0xf0000000 &&
        (opcode & 0xff000000) != 0xf1000000 &&
        (opcode & 0xff000000) != 0xf2000000 &&
        (opcode & 0xff000000) != 0xf3000000)
        return 0;

    subtype = (opcode >> 23) & 7;

    if ((opcode & 0xff800000) == 0xf0800000 ||
        (opcode & 0xff800000) == 0xf1000000 ||
        (opcode & 0xff800000) == 0xf1800000) {
        float input[VFPU_VECTOR_SIZE] = { 0 };
        int homogeneous = subtype == size - 1;

        vfpu_read_matrix(source, size, a);
        vfpu_read_vector(target, size, input);

        for (row = 0; row < size; row++) {
            float sum = 0;

            for (index = 0; index < size; index++) {
                sum += a[index][row]
                    * (homogeneous && index == size - 1 ? 1 : input[index]);
            }

            result[row] = sum;
        }

        vfpu_write_vector(destination, size, result, 1);
        vfpu_finish_prefix();
        return 1;
    }

    if ((opcode & 0xff800000) == 0xf0000000) {
        vfpu_read_matrix(source, size, a);
        vfpu_read_matrix(target, size, b);

        for (column = 0; column < size; column++) {
            for (row = 0; row < size; row++) {
                for (index = 0; index < size; index++)
                    output[column][row] += a[index][row] * b[column][index];
            }
        }

        vfpu_write_matrix(destination, size, output);
        vfpu_finish_prefix();
        return 1;
    }

    if ((opcode & 0xff800000) == 0xf2000000) {
        float scale[VFPU_VECTOR_SIZE] = { 0 };

        vfpu_read_matrix(source, size, a);
        vfpu_read_vector(target, 1, scale);

        for (column = 0; column < size; column++) {
            for (row = 0; row < size; row++)
                output[column][row] = a[column][row] * scale[0];
        }

        vfpu_write_matrix(destination, size, output);
        vfpu_finish_prefix();
        return 1;
    }

    if ((opcode & 0xffff0000) == 0xf3800000) {
        vfpu_read_matrix(source, size, a);
        vfpu_write_matrix(destination, size, a);
        vfpu_finish_prefix();
        return 1;
    }

    if ((opcode & 0xffff0000) == 0xf3830000 ||
        (opcode & 0xffff0000) == 0xf3860000 ||
        (opcode & 0xffff0000) == 0xf3870000) {
        int kind = (opcode >> 16) & 255;

        for (column = 0; column < size; column++) {
            for (row = 0; row < size; row++) {
                output[column][row] = kind == 0x83
                    ? (column == row) : kind == 0x87 ? 1 : 0;
            }
        }

        vfpu_write_matrix(destination, size, output);
        vfpu_finish_prefix();
        return 1;
    }

    return 0;
}
