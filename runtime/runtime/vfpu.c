#include "psbridge.h"
#include "psbridge_log.h"
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

    if (pointer != psbridge_mixer_pointer)
    {
        PSB_LOG(VFPU,
                "import gozou a pilha anterior %08lx %08lx %08lx agora %08lx %08lx "
                "%08lx bagulho %08lx era %08lx",
                (unsigned long)last_library, (unsigned long)last_nid, (unsigned long)last_callsite,
                (unsigned long)library, (unsigned long)nid, (unsigned long)callsite,
                (unsigned long)pointer, (unsigned long)psbridge_mixer_pointer);
        psbridge_mixer_pointer = pointer;
    }

    last_library = library;
    last_nid = nid;
    last_callsite = callsite;
}
