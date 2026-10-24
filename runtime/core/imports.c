#include <debug.h>
#include <delaythread.h>
#include <stdint.h>
#include <string.h>
#include <timer.h>

#include <psbridge.h>

static int next_callback_uid = 0x1800;
static int next_event_uid = 0x2000;

static u64 system_time(void)
{
    u32 seconds;
    u32 microseconds;

    TimerBusClock2USec(
        GetTimerSystemTime(),
        &seconds,
        &microseconds
    );

    return (u64)seconds * 1000000 + microseconds;
}

u64 psbridge_dispatch(const PspImport *import, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5)
{
    (void)a5;

    if (!strcmp(import->library, "SysMemUserForUser")) {
        if (
            import->nid == 0x35669d4c ||
            import->nid == 0xf77d77cb
        )
            return 0;

        if (import->nid == 0x237dbd4f) {
            return psbridge_alloc_partition(
                (int)a0,
                (const char *)a1,
                (int)a2,
                a3,
                (void*) a4
            );
        }

        if (import->nid == 0x9d9a5ba1)
            return (u32) (uintptr_t)psbridge_block_address((int)a0);

        if (import-> nid == 0xb6d61d02)
            return psbridge_free_partition((int)a0);
    }

    if (!strcmp(import->library, "IoFileMgrForUser")) {
        if (import->nid == 0x109f50bc)
            return psbridge_file_open((const char *)a0, (int)a1, (int)a2);

        if (import->nid == 0x810c4bc3)
            return psbridge_file_close((int)a0);

        if (import->nid == 0x6a638d83)
            return psbridge_file_read((int)a0, (void *)a1, a2);

        if (import->nid == 0x42ec03ac)
            return psbridge_file_write((int)a0, (const void *)a1, a2);

        if (import->nid == 0x27eb27b8)
            return psbridge_file_seek((int)a0, a2, a3, (int)a4);

        if (import->nid == 0xace946e8)
            return psbridge_file_stat((const char *)a0, (void *)a1);

        if (import->nid == 0xf27a9c51)
            return psbridge_file_remove((const char *)a0);

        if (import->nid == 0x779103a0)
            return psbridge_file_rename((const char *)a0, (const char *)a1);
    }

    if (!strcmp(import->library, "Kernel_Library")) {
        if (import->nid == 0x092968f4)
            return 1;

        if (import->nid == 0x5f10d406)
            return 0;

        if (import->nid == 0xbea46419)
            return psbridge_lock_lwmutex((void *)a0, (int)a1);

        if (import->nid == 0x15b6446b)
            return psbridge_unlock_lwmutex((void *)a0, (int)a1);

        if (import->nid == 0x1839852a) {
            void *destination = psbridge_game_pointer((void *)a0);
            const void *source = psbridge_game_pointer((void *)a1);

            if ((!destination || !source) && a2)
                return 0;

            memcpy(destination, source, a2);
            return a0;
        }

        if (import->nid == 0xa089eca4) {
            void *destination = psbridge_game_pointer((void *)a0);

            if (!destination && a2)
                return 0;

            memset(destination, (int)a1, a2);
            return a0;
        }
    }

    if (!strcmp(import->library, "sceAudio"))
        return 0;

    if (!strcmp(import->library, "sceDisplay"))
        return psbridge_display(import->nid, a0, a1, a2, a3);

    if (!strcmp(import->library, "sceCtrl")) {
        if (
            import->nid == 0x1f803938 ||
            import->nid == 0x3a622550 ||
            import->nid == 0x60b81f86 ||
            import->nid == 0xc152080a
        )
            return psbridge_controller_read((void *)a0, (int)a1);

        return 0;
    }

    if (!strcmp(import->library, "sceUtility"))
        return psbridge_utility(import->nid, (void *)a0);

    if (!strcmp(import->library, "sceGe_user"))
        return psbridge_ge(import->nid, a0, a1, a2, a3);

    if (!strcmp(import->library, "ThreadManForUser")) {
        if (import->nid == 0xe81caf8f)
            return next_callback_uid++;

        if (import->nid == 0x55c20a00)
            return next_event_uid++;

        if (
            import->nid == 0x1fb15a32 ||
            import->nid == 0x402fcf22 ||
            import->nid == 0x812346e4 ||
            import->nid == 0xef9e4c70
        )
            return 0;

        if (import->nid == 0xceadeb47) {
            DelayThread((int)a0);
            return 0;
        }

        if (import->nid == 0x369ed59d)
            return (u32)system_time();

        if (import->nid == 0x19cff145)
            return psbridge_create_lwmutex((void *)a0, a2, (int)a3);

        if (import->nid == 0x60107536)
            return psbridge_delete_lwmutex((void *)a0);

        if (import->nid == 0xc07bb470)
            return psbridge_create_fpl(
                (const char *)a0,
                (int)a1,
                a2,
                a3,
                a4,
                (void *)a5
            );

        if (import->nid == 0x623ae665)
            return psbridge_allocate_fpl((int)a0, (void**)a1);

        if (import->nid == 0xed1410e0)
            return psbridge_delete_fpl((int)a0);

        if (import->nid == 0x446d8de6) {
            return psbridge_create_thread(
                (const char *)a0,
                (void *)a1,
                (int)a2,
                (int)a3,
                a4,
                (void *) a5
            );
        }

        if (import->nid == 0xf475845d)
            return psbridge_start_thread((int)a0, a1, (void *)a2);

        if (import->nid == 0x82bc5777)
            return system_time();
    }

    if (!strcmp(import->library, "LoadExecForUser") && import->nid == 0x4ac57943)
        return 0;

    if (!strcmp(import->library, "scePower") && import->nid == 0x04b7766e)
        return 0;

    if (!strcmp(import->library, "UtilsForUser")) {
        if (import->nid == 0x27cc57f0) {
            u32 *output = psbridge_game_pointer((void *)a0);

            if (output)
                *output = 0;

            return 0;
        }

        if (import->nid == 0x79d1c3fa)
            return 0;
    }

    if (!strcmp(import->library, "sceUmdUser")) {
        if (import->nid == 0x46ebb729)
            return 1;

        if (import->nid == 0xaee7404d || import->nid == 0xc6183d47)
            return 0;
    }

    scr_printf(
        "import sem runtime %s 0x%08x\n",
        import->library,
        import->nid
    );
    return -1;
}
