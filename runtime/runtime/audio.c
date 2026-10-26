#include <delaythread.h>
#include <stdint.h>
#include <string.h>

#include <psbridge.h>

#define LIB_AUDIO 0xf41677cc
#define LIB_SAS 0x1d46fbf4
#define LIB_ATRAC 0x75263ea8
#define ATRAC_LIMIT 4
#define ATRAC_SAMPLES 2048

typedef struct {
    int used;
    int initialized;
    u32 source_size;
    u32 output_channels;
} AtracContext;

static AtracContext atrac_ids[ATRAC_LIMIT];
static u32 audio2_samples = 1024;
static int audio2_reserved;

static void *audio_pointer(u32 address, u32 size)
{
    u32 offset;

    if (!address || !size || address + size < address)
        return NULL;

    if (address >= guest_info.base && address - guest_info.base < guest_info.memory_size) {
        offset = address - guest_info.base;
        if (size > guest_info.memory_size - offset)
            return NULL;
    } else if (address < 0x1000 || address >= 0x02000000 || size > 0x02000000 - address) {
        return NULL;
    }

    return psbridge_game_pointer((const void *)(uintptr_t)address);
}

static u64 atrac_call(u32 nid, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4)
{
    AtracContext *id;
    u32 *consumed;
    u32 *produced;
    void *pcm;
    u32 pcm_bytes;
    int index;

    if (nid == 0x132f1eca)
        return 0;

    if (nid == 0x780f88d1) {
        for (index = 0; index < ATRAC_LIMIT; index++) {
            if (!atrac_ids[index].used) {
                memset(&atrac_ids[index], 0, sizeof(atrac_ids[index]));
                atrac_ids[index].used = 1;
                return index;
            }
        }

        return (u32)-1;
    }

    if (a0 >= ATRAC_LIMIT || !atrac_ids[a0].used)
        return (u32)-1;

    id = &atrac_ids[a0];

    if (nid == 0x61eb33f5) {
        memset(id, 0, sizeof(*id));
        return 0;
    }

    if (nid == 0x1575d64b) {
        u32 *params = audio_pointer(a1, 12);

        if (!params)
            return (u32)-1;

        id->output_channels = params[1];
        id->source_size = params[2];

        if ((id->output_channels != 1 && id->output_channels != 2) ||
            !id->source_size || id->source_size > 8192)
            return (u32)-1;

        id->initialized = 1;
        return 0;
    }

    if (nid != 0x0c116e1b || !id->initialized)
        return (u32)-1;

    pcm_bytes = ATRAC_SAMPLES * id->output_channels * 2;
    consumed = audio_pointer(a2, sizeof(u32));
    pcm = audio_pointer(a3, pcm_bytes);
    produced = audio_pointer(a4, sizeof(u32));

    if (!consumed || !pcm || !produced || !audio_pointer(a1, id->source_size))
        return (u32)-1;

    memset(pcm, 0, pcm_bytes);
    *consumed = id->source_size;
    *produced = pcm_bytes;
    DelayThread(1000);
    return 0;
}

static u64 audio2_call(u32 nid, u32 a0)
{
    if (nid == 0x01562ba3) {
        if (a0 >= 17 && a0 <= 4111)
            audio2_samples = a0;
        audio2_reserved = 1;
        return 0;
    }

    if (nid == 0x63f2889c) {
        if (a0 >= 17 && a0 <= 4111)
            audio2_samples = a0;
        return 0;
    }

    if (nid == 0x43196845) {
        audio2_reserved = 0;
        return 0;
    }

    if (nid == 0x2d53f36e) {
        u32 wait = 20000;

        if (audio2_reserved)
            wait = (u32)(((u64)audio2_samples * 1000000u + 44099u) / 44100u);

        DelayThread(wait);
    }

    return 0;
}

static u64 sas_call(u32 nid, u32 a1)
{
    if (nid == 0x07f58c24) {
        u32 *heights = psbridge_game_pointer((void *)(uintptr_t)a1);

        if (!heights)
            return (u32)-1;

        memset(heights, 0, 32 * sizeof(*heights));
        return 0;
    }

    if (nid == 0x68a46b95)
        return 0xffffffff;

    return 0;
}

u64 psbridge_audio(u32 library, u32 nid, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4)
{
    if (library == LIB_AUDIO)
        return audio2_call(nid, a0);

    if (library == LIB_SAS)
        return sas_call(nid, a1);

    if (library == LIB_ATRAC)
        return atrac_call(nid, a0, a1, a2, a3, a4);

    return (u32)-1;
}
