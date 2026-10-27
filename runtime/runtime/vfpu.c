#include <stdio.h>
#include <stdint.h>
#include <tamtypes.h>

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
        printf(
            "import gozou a pilha anterior %08x %08x %08x agora %08x %08x %08x bagulho %08x era %08x\n",
            last_library,
            last_nid,
            last_callsite,
            library,
            nid,
            callsite,
            pointer,
            psbridge_mixer_pointer
        );
        psbridge_mixer_pointer = pointer;
    }

    last_library = library;
    last_nid = nid;
    last_callsite = callsite;
}

void psbridge_vfpu_dispatch(u32 opcode, u32 pc, u32 *guest_registers)
{
    u32 sp;
    u32 pointer;

    (void)opcode;

    if (pc < 0x0883abc8 || pc > 0x0883e270)
        return;

    sp = guest_registers[29];

    if (sp >= 0x02000000 || sp + 20512 < sp)
        return;

    pointer = *(u32 *)(uintptr_t)(sp + 20508);

    if (!psbridge_mixer_sp) {
        psbridge_mixer_sp = sp;
        psbridge_mixer_pointer = pointer;
        printf("vfpu cheirou a pilha %08x e a mulher dj tava %08x\n", sp, pointer);
        return;
    }

    if (sp != psbridge_mixer_sp || pointer != psbridge_mixer_pointer) {
        printf(
            "vfpu viu a caceta torta pc %08x pilha %08x era %08x mulher %08x era %08x\n",
            pc,
            sp,
            psbridge_mixer_sp,
            pointer,
            psbridge_mixer_pointer
        );
        psbridge_mixer_sp = sp;
        psbridge_mixer_pointer = pointer;
    }
}
