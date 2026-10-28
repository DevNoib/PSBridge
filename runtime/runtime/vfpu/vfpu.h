#ifndef PSBRIDGE_VFPU_H
#define PSBRIDGE_VFPU_H

#include <tamtypes.h>

#define VFPU_REGISTERS 128
#define VFPU_CONTROLS 16
#define VFPU_VECTOR_SIZE 4

extern u32 vfpu_registers[VFPU_REGISTERS];
extern u32 vfpu_controls[VFPU_CONTROLS];
extern u32 vfpu_prefix_s;
extern u32 vfpu_prefix_t;
extern u32 vfpu_prefix_d;
extern u8 vfpu_have_s;
extern u8 vfpu_have_t;
extern u8 vfpu_have_d;

float vfpu_float(u32 bits);
u32 vfpu_bits(float value);
int vfpu_vector_lane(int register_id, int size, int lane);
int vfpu_matrix_lane(int register_id, int size, int column, int row);
void vfpu_read_vector(int register_id, int size, float output[VFPU_VECTOR_SIZE]);
void vfpu_read_prefix(
    int register_id,
    int size,
    float output[VFPU_VECTOR_SIZE],
    u32 prefix,
    int enabled
);
void vfpu_write_vector(
    int register_id,
    int size,
    const float input[VFPU_VECTOR_SIZE],
    int use_prefix
);
void vfpu_finish_prefix(void);
void vfpu_read_matrix(int register_id, int size, float output[4][4]);
void vfpu_write_matrix(int register_id, int size, float input[4][4]);
void *vfpu_pointer(u32 address, u32 bytes);

int vfpu_execute_memory(u32 opcode, int major, u32 *gpr);
int vfpu_execute_prefix(u32 opcode);
int vfpu_execute_immediate(u32 opcode);
int vfpu_execute_transfer(u32 opcode, u32 *gpr);
int vfpu_execute_arithmetic(
    u32 opcode,
    int major,
    int subgroup,
    int size,
    int destination,
    int source,
    int target
);
int vfpu_execute_unary(u32 opcode, int size, int destination, int source);
int vfpu_execute_conversion(u32 opcode, int size, int destination, int source);
int vfpu_execute_matrix(
    u32 opcode,
    int size,
    int destination,
    int source,
    int target
);

#endif
