#include "psbridge_log.h"
#include "vfpu_internal.h"
#include <stdint.h>
#include <stdio.h>

void *vfpu_checked_pointer(u32 raw, u32 bytes)
{
    u32 a = raw & 0x3fffffff;
    if ((a & 3) || a < 0x1000 || bytes > 0x10000)
        goto bad;

    if (a >= 0x04000000 && a <= 0x04180000 && bytes <= 0x04180000 - a)
        return (u8 *)psbridge_video_memory() + (a - 0x04000000);
    if ((raw & 0xe0000000u) == 0x80000000u ||
        (raw & 0xe0000000u) == 0xa0000000u) {
        if (a < 0x02000000 && bytes <= 0x02000000 - a)
            return (void *)(uintptr_t)raw;
        goto bad;
    }
    if (a >= guest_info.base && a - guest_info.base <= guest_info.memory_size &&
        bytes <= guest_info.memory_size - (a - guest_info.base))
        return psbridge_game_pointer((const void *)(uintptr_t)a);
    if (a >= 0x00100000 && a < 0x02000000 && bytes <= 0x02000000 - a)
        return (void *)(uintptr_t)(a | 0x80000000u);
bad:
    if (vfpu_invalid_memory < 12)
        PRINT_LOG("vfpu tentou gozar em ponteiro furado %08lx tamanho %lu\n",
               (unsigned long)raw, (unsigned long)bytes);
    vfpu_invalid_memory++;
    vfpu_last_invalid_address = raw;
    return NULL;
}
