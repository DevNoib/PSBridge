#include <stdint.h>

#include "psbridge_log.h"
#include "vfpu.h"

u32 psbridge_mixer_sp;
u32 psbridge_mixer_pointer;

void psbridge_mixer_probe(u32 library, u32 nid, u32 callsite)
{
    static u32 last_library;
    static u32 last_nid;
    static u32 last_callsite;
    u32 pointer;

    if (!psbridge_mixer_sp)
        return;

    pointer = *(u32 *)(uintptr_t)(psbridge_mixer_sp + 20508);

    if (pointer != psbridge_mixer_pointer) {
        PSB_LOG(
            VFPU,
            "import gozou a pilha anterior %08lx %08lx %08lx agora %08lx %08lx %08lx bagulho %08lx era %08lx",
            (unsigned long)last_library,
            (unsigned long)last_nid,
            (unsigned long)last_callsite,
            (unsigned long)library,
            (unsigned long)nid,
            (unsigned long)callsite,
            (unsigned long)pointer,
            (unsigned long)psbridge_mixer_pointer
        );

        psbridge_mixer_pointer = pointer;
    }

    last_library = library;
    last_nid = nid;
    last_callsite = callsite;
}

static void probe_mixer(u32 pc, u32 *gpr)
{
    u32 stack;
    u32 pointer;

    if (pc < 0x0883abc8 || pc > 0x0883e270)
        return;

    stack = gpr[29];

    if (!stack || stack >= 0x02000000)
        return;

    if (stack + 20512 < stack || stack + 20512 >= 0x02000000)
        return;

    pointer = *(u32 *)(uintptr_t)(stack + 20508);

    if (!psbridge_mixer_sp) {
        psbridge_mixer_sp = stack;
        psbridge_mixer_pointer = pointer;

        PSB_LOG(
            VFPU,
            "vfpu cheirou a pilha %08lx e a mulher dj tava %08lx",
            (unsigned long)stack,
            (unsigned long)pointer
        );
    }
}

void psbridge_vfpu_dispatch(u32 opcode, u32 pc, u32 *gpr)
{
    static u32 last_unknown;
    int size = 1 + ((opcode >> 7) & 1) + (((opcode >> 15) & 1) << 1);
    int destination = opcode & 127;
    int source = (opcode >> 8) & 127;
    int target = (opcode >> 16) & 127;
    int major = opcode >> 26;
    int subgroup = (opcode >> 23) & 7;

    probe_mixer(pc, gpr);

    if (vfpu_execute_memory(opcode, major, gpr))
        return;

    if (vfpu_execute_prefix(opcode))
        return;

    if (vfpu_execute_immediate(opcode))
        return;

    if (vfpu_execute_transfer(opcode, gpr))
        return;

    if (opcode == 0xffff0000 || opcode == 0xffff0320 || opcode == 0xffff040d)
        return;

    if (vfpu_execute_arithmetic(
        opcode,
        major,
        subgroup,
        size,
        destination,
        source,
        target
    ))
        return;

    if (vfpu_execute_unary(opcode, size, destination, source))
        return;

    if (vfpu_execute_conversion(opcode, size, destination, source))
        return;

    if (vfpu_execute_matrix(opcode, size, destination, source, target))
        return;

    if (opcode != last_unknown) {
        PSB_LOG(
            VFPU,
            "vfpu nao entendeu essa porra %08lx em %08lx",
            (unsigned long)opcode,
            (unsigned long)pc
        );
        last_unknown = opcode;
    }
}
