#include <stdint.h>
#include <string.h>

#include "psbridge.h"

typedef struct {
    u32 timestamp;
    u32 buttons;
    u8 lx;
    u8 ly;
    u8 reserved[6];
} PspControllerData;

int psbridge_controller_read(void *output, int count)
{
    PspControllerData *samples = psbridge_game_pointer(output);
    int index;

    if (!samples || count <= 0)
        return 0;

    for (index = 0; index < count; index++) {
        memset(&samples[index], 0, sizeof(samples[index]));
        samples[index].lx = 128;
        samples[index].ly = 128;
    }

    return count;
}
