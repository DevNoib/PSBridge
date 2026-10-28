#include "vfpu_internal.h"
#include <stdint.h>

void *vfpu_checked_pointer(u32 raw, u32 bytes)
{
    u32 a = raw & 0x3fffffff;
    if ((a & 3) || !a || bytes > 0x10000)
        goto bad;

    if (a < 0x02000000 && a + bytes >= a && a + bytes <= 0x02000000)
        return (void *)(uintptr_t)a;
    if (a >= 0x04000000 && a <= 0x04180000 && bytes <= 0x04180000 - a)
        return (u8 *)psbridge_video_memory() + (a - 0x04000000);
    if (a >= guest_info.base && a - guest_info.base <= guest_info.memory_size &&
        bytes <= guest_info.memory_size - (a - guest_info.base))
        return psbridge_game_pointer((const void *)(uintptr_t)a);
bad:
    vfpu_invalid_memory++;
    vfpu_last_invalid_address = raw;
    return NULL;
}
