#include <stdint.h>
#include <string.h>
#include <libpad.h>
#include <loadfile.h>

#include "psbridge.h"

typedef struct {
    u32 timestamp;
    u32 buttons;
    u8 lx;
    u8 ly;
    u8 reserved[6];
} PspControllerData;

static unsigned char pad_buffer[256] __attribute__((aligned(64)));
static int controller_ready;

static int start_controller(void)
{
    if (controller_ready)
        return 0;

    if (SifLoadModule("rom0:SIO2MAN", 0, NULL) < 0)
        return -1;

    if (SifLoadModule("rom0:PADMAN", 0, NULL) < 0)
        return -1;

    if (!padInit(0))
        return -1;

    if (!padPortOpen(0, 0, pad_buffer))
        return -1;

    controller_ready = 1;
    return 0;
}

static void read_controller(PspControllerData *sample)
{
    struct padButtonStatus pad;
    u32 pressed;
    int state;

    memset(sample, 0, sizeof(*sample));
    sample->lx = 128;
    sample->ly = 128;

    state = padGetState(0, 0);

    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1)
        return;

    if (!padRead(0, 0, &pad))
        return;

    pressed = 0xffff ^ pad.btns;

    sample->buttons = pressed & 0xf0f9;

    if (pressed & (PAD_L1 | PAD_L2))
        sample->buttons |= 0x100;

    if (pressed & (PAD_R1 | PAD_R2))
        sample->buttons |= 0x200;

    sample->lx = pad.ljoy_h;
    sample->ly = pad.ljoy_v;
    
}

int psbridge_controller_read(void *output, int count)
{
    PspControllerData *samples = psbridge_game_pointer(output);
    PspControllerData current;
    int index;

    if (start_controller() < 0)
        return 0;

    if (!samples || count <= 0)
        return 0;

    read_controller(&current);

    for (index = 0; index < count; index++) {
        samples[index] = current;
    }

    return count;
}
