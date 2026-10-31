#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>

#include "psbridge.h"
#include "psbridge_log.h"

typedef struct {
    u32 timestamp;
    u32 buttons;
    u8 lx;
    u8 ly;
    u8 reserved[6];
} PspControllerData;

#ifndef PSBRIDGE_HEADLESS
static unsigned char pad_buffer[256] __attribute__((aligned(64)));
static unsigned char controller_stack[8192] __attribute__((aligned(16)));
static volatile int controller_state;

static void controller_setup_thread(void *arg)
{
    int sio;
    int padman;
    int ready;
    (void)arg;

    sio = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    padman = SifLoadModule("rom0:PADMAN", 0, NULL);
    PRINT_LOG("controle botando o pad pra trabalhar sio %d pad %d\n", sio, padman);

    ready = padInit(0);
    if (ready > 0)
        ready = padPortOpen(0, 0, pad_buffer);

    if (ready > 0) {
        controller_state = 2;
        PRINT_LOG("controle acordou e ta no grau\n");
    } else {
        controller_state = -1;
        PRINT_LOG("controle broxou na inicializacao %d\n", ready);
    }

    ExitDeleteThread();
}

static void start_controller(void)
{
    ee_thread_t config;
    int interrupt_state;
    int thread_id;
    int start = 0;

    interrupt_state = DIntr();
    if (controller_state == 0) {
        controller_state = 1;
        start = 1;
    }
    if (interrupt_state != 0)
        EIntr();

    if (!start)
        return;

    memset(&config, 0, sizeof(config));
    config.func = controller_setup_thread;
    config.stack = controller_stack;
    config.stack_size = sizeof(controller_stack);
    config.gp_reg = &_gp;
    config.initial_priority = 35;

    thread_id = CreateThread(&config);
    if (thread_id < 0) {
        controller_state = -1;
        PRINT_LOG("controle peidou criando thread %d\n", thread_id);
        return;
    }

    if (StartThread(thread_id, NULL) < 0) {
        DeleteThread(thread_id);
        controller_state = -1;
        PRINT_LOG("controle peidou abrindo thread\n");
    }
}

static void read_controller(PspControllerData *sample)
{
    struct padButtonStatus pad;
    u32 pressed;
    int state;

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
#endif

int psbridge_controller_read(void *output, int count)
{
    PspControllerData *samples = psbridge_game_pointer(output);
    PspControllerData current;
    int index;

    if (!samples || count <= 0)
        return 0;

    memset(&current, 0, sizeof(current));
    current.lx = 128;
    current.ly = 128;

#ifndef PSBRIDGE_HEADLESS
    start_controller();
    if (controller_state == 2)
        read_controller(&current);
#endif

    for (index = 0; index < count; index++)
        samples[index] = current;

    return count;
}
