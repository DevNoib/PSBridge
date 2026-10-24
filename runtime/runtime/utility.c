#include <stdint.h>

#include "psbridge.h"

#define DIALOG_NONE 0
#define DIALOG_INIT 1
#define DIALOG_VISIBLE 2
#define DIALOG_QUIT 3
#define DIALOG_FINISHED 4

#define SAVEDATA_LOAD_NO_DATA 0x80110307

typedef struct {
    u32 size;
    s32 language;
    s32 button_swap;
    s32 graphics_thread;
    s32 access_thread;
    s32 font_thread;
    s32 sound_thread;
    s32 result;
    s32 reserved[4];
} UtilityDialog;

typedef struct {
    UtilityDialog dialog;
    s32 mode;
} SavedataParams;

typedef struct {
    UtilityDialog dialog;
    s32 unknown;
    s32 mode;
    u32 error;
    char message[512];
    u32 options;
    s32 button;
} MessageParams;

static int savedata_status;
static int message_status;

static int next_status(int *status)
{
    int current = *status;

    if (current == DIALOG_INIT)
        *status = DIALOG_VISIBLE;
    else if (current == DIALOG_FINISHED)
        *status = DIALOG_NONE;

    return current;
}

static int savedata_init(void *address)
{
    SavedataParams *params = psbridge_game_pointer(address);

    if (!params)
        return -1;

    params->dialog.result = 0;

    if (
        params->mode == 0 ||
        params->mode == 2 ||
        params->mode == 4 ||
        params->mode == 15 ||
        params->mode == 16
    )
        params->dialog.result = SAVEDATA_LOAD_NO_DATA;

    savedata_status = DIALOG_INIT;
    return 0;
}

static int message_init(void *address)
{
    MessageParams *params = psbridge_game_pointer(address);

    if (!params)
        return -1;

    params->dialog.result = 0;
    params->button = 1;
    message_status = DIALOG_INIT;
    return 0;
}

int psbridge_utility(u32 nid, void *argument)
{
    if (nid == 0x50c4cd57)
        return savedata_init(argument);

    if (nid == 0xd4b95ffb) {
        if (savedata_status == DIALOG_VISIBLE)
            savedata_status = DIALOG_QUIT;
        return 0;
    }

    if (nid == 0x9790b33c) {
        savedata_status = DIALOG_FINISHED;
        return 0;
    }

    if (nid == 0x8874dbe0)
        return next_status(&savedata_status);

    if (nid == 0x2ad8e239)
        return message_init(argument);

    if (nid == 0x95fc253b) {
        if (message_status == DIALOG_VISIBLE)
            message_status = DIALOG_QUIT;
        return 0;
    }

    if (nid == 0x67af3428) {
        message_status = DIALOG_FINISHED;
        return 0;
    }

    if (nid == 0x9a1c91d7)
        return next_status(&message_status);

    if (nid == 0x4928bd96) {
        message_status = DIALOG_QUIT;
        return 0;
    }

    return 0;
}
