#include <dmaKit.h>
#include <delaythread.h>
#include <gsKit.h>
#include <kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>

#include "psbridge.h"

#define PSP_WIDTH 480
#define PSP_HEIGHT 272
#define PSP_BUFFER_WIDTH 512

static GSGLOBAL *gs;
static GSTEXTURE frame_texture;
static u16 frame_pixels[PSP_BUFFER_WIDTH * PSP_HEIGHT]
    __attribute__((aligned(128)));

static const void *framebuffer;
static int framebuffer_width = PSP_BUFFER_WIDTH;
static int framebuffer_format = 3;
static int graphics_ready;
static u64 vblank_epoch;
static int vblank_ready;
static u32 display_wait_count;
static u32 framebuffer_count;

#define VBLANK_DURATION_US 16683u

static u64 current_time(void)
{
    u32 seconds;
    u32 microseconds;

    TimerBusClock2USec(GetTimerSystemTime(), &seconds, &microseconds);
    return (u64)seconds * 1000000 + microseconds;
}

static u32 current_vcount(void)
{
    u64 now = current_time();

    if (!vblank_ready) {
        vblank_epoch = now;
        vblank_ready = 1;
    }

    return (u32)((now - vblank_epoch) / VBLANK_DURATION_US);
}

static void wait_vblank(u32 cycles, int use_callbacks)
{
    u64 now = current_time();
    u64 current;
    u64 target;

    if (!vblank_ready) {
        vblank_epoch = now;
        vblank_ready = 1;
    }

    if (!cycles)
        cycles = 1;
    if (cycles > 1000)
        cycles = 1000;

    current = (now - vblank_epoch) / VBLANK_DURATION_US;
    target = vblank_epoch + (current + cycles) * VBLANK_DURATION_US;

    while ((now = current_time()) < target) {
        u64 remaining = target - now;

        if (remaining > 3000)
            remaining = 3000;

        if (use_callbacks)
            psbridge_delay_thread((u32)remaining, 1);
        else
            DelayThread((int)remaining);
    }
}

static u16 gs_color(u8 red, u8 green, u8 blue, u8 alpha)
{
    return
        (red >> 3) |
        ((green >> 3) << 5) |
        ((blue >> 3) << 10) |
        ((alpha >= 0x80) << 15);
}

static int start_graphics(void)
{
    u32 texture_size;

    if (graphics_ready)
        return 0;

    gs = gsKit_init_global_custom(256 * 1024, 64 * 1024);

    if (!gs)
        return -1;

    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gs->Mode = GS_MODE_NTSC;
    gs->Interlace = GS_INTERLACED;
    gs->Field = GS_FIELD;
    gs->Width = 640;
    gs->Height = 448;
    gs->PSM = GS_PSM_CT16;
    gs->PSMZ = GS_PSMZ_16S;
    gs->ZBuffering = GS_SETTING_OFF;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_OFF;

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    gsKit_set_test(gs, GS_ATEST_OFF);
    gs->TexturePointer = gs->CurrentPointer;

    memset(&frame_texture, 0, sizeof(frame_texture));
    frame_texture.Width = PSP_BUFFER_WIDTH;
    frame_texture.Height = PSP_HEIGHT;
    frame_texture.PSM = GS_PSM_CT16;
    frame_texture.Filter = GS_FILTER_NEAREST;
    frame_texture.Mem = (u32 *)frame_pixels;

    texture_size = gsKit_texture_size(
        frame_texture.Width,
        frame_texture.Height,
        frame_texture.PSM
    );
    frame_texture.Vram = gsKit_vram_alloc(
        gs,
        texture_size,
        GSKIT_ALLOC_USERBUFFER
    );

    if (frame_texture.Vram == GSKIT_ALLOC_ERROR)
        return -1;

    gsKit_setup_tbw(&frame_texture);
    graphics_ready = 1;
    return 0;
}

static const void *framebuffer_pointer(const void *address)
{
    u32 value = (u32)(uintptr_t)address;
    u32 video = (u32)(uintptr_t)psbridge_video_memory();

    if (value & 0x40000000)
        value &= ~0x40000000;

    if (value >= 0x04000000 && value < 0x04200000)
        return (u8 *)psbridge_video_memory() + value - 0x04000000;

    if (value < 0x00200000)
        return (u8 *)psbridge_video_memory() + value;

    if (value >= video && value < video + 0x00200000)
        return (const void *)(uintptr_t)value;

    return psbridge_game_pointer((const void *)(uintptr_t)value);
}

static u16 convert_pixel(const void *source, u32 index, int format)
{
    u16 pixel16;
    u32 pixel32;
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;

    if (format == 3) {
        pixel32 = ((const u32 *)source)[index];
        return gs_color(
            pixel32,
            pixel32 >> 8,
            pixel32 >> 16,
            pixel32 >> 24
        );
    }

    pixel16 = ((const u16 *)source)[index];

    if (format == 0) {
        red = (pixel16 & 0x1f) << 3;
        green = ((pixel16 >> 5) & 0x3f) << 2;
        blue = ((pixel16 >> 11) & 0x1f) << 3;
        return gs_color(red, green, blue, 0xff);
    }

    if (format == 1)
        return pixel16;

    red = (pixel16 & 0xf) * 17;
    green = ((pixel16 >> 4) & 0xf) * 17;
    blue = ((pixel16 >> 8) & 0xf) * 17;
    alpha = ((pixel16 >> 12) & 0xf) * 17;
    return gs_color(red, green, blue, alpha);
}

static void copy_framebuffer(void)
{
    const void *source = framebuffer_pointer(framebuffer);
    int width = framebuffer_width;
    int x;
    int y;

    memset(frame_pixels, 0, sizeof(frame_pixels));

    if (!source)
        return;

    if (width < PSP_WIDTH || width > 2048)
        width = PSP_BUFFER_WIDTH;

    if (framebuffer_format == 1) {
        for (y = 0; y < PSP_HEIGHT; y++) {
            memcpy(
                frame_pixels + y * PSP_BUFFER_WIDTH,
                (const u16 *)source + y * width,
                PSP_WIDTH * sizeof(u16)
            );
        }

        return;
    }

    for (y = 0; y < PSP_HEIGHT; y++) {
        for (x = 0; x < PSP_WIDTH; x++) {
            u16 pixel = convert_pixel(
                source,
                y * width + x,
                framebuffer_format
            );

            frame_pixels[y * PSP_BUFFER_WIDTH + x] = pixel;
        }
    }
}

static void present(void)
{
    if (start_graphics() < 0)
        return;

    copy_framebuffer();

    FlushCache(0);

    gsKit_texture_send_inline(
        gs,
        (u32 *)frame_pixels,
        frame_texture.Width,
        frame_texture.Height,
        frame_texture.Vram,
        frame_texture.PSM,
        frame_texture.TBW,
        0
    );

    gsKit_clear(gs, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
    gsKit_prim_sprite_texture(
        gs,
        &frame_texture,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        640.0f,
        448.0f,
        PSP_WIDTH,
        PSP_HEIGHT,
        1,
        GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0)
    );

    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
}

u64 psbridge_display(u32 nid, u32 a0, u32 a1, u32 a2, u32 a3)
{
    if (nid == 0x0e20f177) {
        start_graphics();
        return 0;
    }

    if (nid == 0x289d82fe) {
        framebuffer = (const void *)(uintptr_t)a0;
        framebuffer_width = a1;
        framebuffer_format = a2;
        framebuffer_count++;

        if (framebuffer_count <= 8 || !(framebuffer_count % 256))
            printf(
                "[display] buffer n=%u endereco=%08x largura=%u formato=%u sync=%u thread=%d\n",
                framebuffer_count,
                a0,
                a1,
                a2,
                a3,
                GetThreadId()
            );

        (void)a3;
        return 0;
    }

    if (nid == 0x9c6eaad7)
        return current_vcount();

    if (
        nid == 0x984c27e7 ||
        nid == 0x46f186c3 ||
        nid == 0x77ed8b3a
    ) {
        u32 cycles = nid == 0x77ed8b3a ? a0 : 1;

        present();
        wait_vblank(cycles, nid != 0x984c27e7);
        display_wait_count++;

        if (display_wait_count <= 8 || !(display_wait_count % 120))
            printf(
                "[display] vblank n=%u nid=%08x contador=%u buffer=%08x thread=%d\n",
                display_wait_count,
                nid,
                current_vcount(),
                (u32)(uintptr_t)framebuffer,
                GetThreadId()
            );

        return 0;
    }

    return 0;
}
