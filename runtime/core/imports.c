#include <debug.h>
#include <delaythread.h>
#include <stdint.h>
#include <string.h>
#include <timer.h>
#include <stdio.h>
#include <kernel.h>

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

#define LIB_DISPLAY 0x1cebc208
#define LIB_SYSMEM 0x1902db0c
#define LIB_IO 0x67e92fd1
#define LIB_KERNEL 0xff65d9d0
#define LIB_AUDIO 0xf41677cc
#define LIB_CTRL 0x6e7a8e3b
#define LIB_UTILITY  0x241c3336
#define LIB_GE 0x412aa1a0
#define LIB_THREADMAN 0x9aacf809
#define LIB_UTILS 0xa8587892
#define LIB_UMD 0x964db0c7
#define LIB_POWER 0x892fba53
#define LIB_IMPOSE 0x8c6c6141
#define LIB_LOADEXEC 0xc469257c
#define LIB_MODULEMGR 0xe2307313
#define LIB_SUSPEND 0x1707c132
#define LIB_SAS 0x1d46fbf4
#define LIB_ATRAC 0x75263ea8

u64 psbridge_dispatch(const PspImport *import, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5)
{
    (void)a5;

    static u32 last_library;
    static u32 last_nid;

    if (import->library != last_library || import->nid != last_nid) {
        printf(
            "passou import %08x %08x\n",
            import->library,
            import->nid
        );

        last_library = import->library;
        last_nid = import->nid;
    }


    if (import->library == LIB_SYSMEM) {
        if (import->nid == 0x13a5abef)
            return 0;

        if (
            import->nid == 0x35669d4c ||
            import->nid == 0xf77d77cb ||
            import->nid == 0x1b4217bc
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

    if (import->library == LIB_IO) {
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

        if (import->nid == 0x54f5fb11) {
            const char *device = psbridge_game_pointer((void *)a0);

            if (
                device &&
                !strcmp(device, "fatms0:") &&
                a1 == 0x2415821 &&
                a2 &&
                a3 >=4
            )
                return 0;

            return -1;
        }

        if (import->nid == 0x779103a0)
            return psbridge_file_rename((const char *)a0, (const char *)a1);
    }

    if (import->library == LIB_KERNEL) {
        if (import->nid == 0x092968f4)
            return 1;

        if (import->nid == 0x5f10d406)
            return 0;

        if (import->nid == 0xd13bde95)
            return 0x10000;

        if (import->nid == 0xbea46419)
            return psbridge_lock_lwmutex((void *)a0, (int)a1);

        if (import->nid == 0x1fc64e09)
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

    if (import->library == LIB_AUDIO) {
        if(import->nid == 0x2d53f36e)
            DelayThread(20000);

        return 0;
    }

    if (import->library == LIB_SAS) {
        if(
            import->nid == 0x42778a9f ||
            import->nid == 0x50a14dfc
        )

        return 0;
    }
       
    
    if (import->library == LIB_ATRAC && import->nid == 0x132f1eca)
        return 0;

    if (import->library == LIB_DISPLAY)
        return psbridge_display(import->nid, a0, a1, a2, a3);

    if (import->library == LIB_CTRL) {
        if (
            import->nid == 0x1f803938 ||
            import->nid == 0x3a622550 ||
            import->nid == 0x60b81f86 ||
            import->nid == 0xc152080a
        )
            return psbridge_controller_read((void *)a0, (int)a1);

        return 0;
    }

    if (import->library == LIB_UTILITY)
        return psbridge_utility(import->nid, (void *)a0);

    if (import->library == LIB_GE)
        return psbridge_ge(import->nid, a0, a1, a2, a3);

    if (import->library == LIB_THREADMAN) {

        if (import->nid == 0x8125221d)
            return psbridge_create_mailbox(
                (const char *)a0,
                a1,
                (void *)a2
            );

        if (import->nid == 0x86255ada)
            return psbridge_delete_mailbox((int)a0);

        if (import->nid == 0xe9b3061e)
            return psbridge_send_mailbox(
                (int) a0,
                (void *)a1
            );

        if (import->nid == 0xf3986382 ||
            import->nid == 0x18260574
        )
            return psbridge_receive_mailbox(
                (int)a0,
                (void *)a1,
                (void *)a2
            );

        if (import->nid == 0xbc80ec7c)
            return psbridge_call_guest((void *)a1, (void *)a2);

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

        if (
            import->nid == 0xceadeb47 ||
            import->nid == 0x68da9e36
        ) {
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

    if (import->library == LIB_LOADEXEC && import->nid == 0x4ac57943)
        return 0;

    if (import->library == LIB_MODULEMGR && import->nid == 0x8f2df740)
        return 0;

    if (import->library == LIB_POWER) {
        if (
            import->nid == 0x04b7766e ||
            import->nid == 0x469989ad
        )
            return 0;
    }

    if (import->library == LIB_IMPOSE)
        return 0;

    if (import->library == LIB_UTILS) {
        if (import->nid == 0x27cc57f0) {
            u32 *output = psbridge_game_pointer((void *)a0);

            if (output)
                *output = 0;

            return 0;
        }

        if(import->nid ==0xb435dec5) {
            FlushCache(0);
            return 0;
        }

        if(import->nid == 0x3ee30821)
            return 0;

        if (import->nid == 0x79d1c3fa)
            return 0;
    }

    if (import->library == LIB_UMD) {
        if (import->nid == 0x46ebb729)
            return psbridge_umd_present();

        if (import->nid == 0x6b4a146c)
            return psbridge_umd_state();

        if (import->nid == 0xc6183d47)
            return psbridge_umd_activate((int)a0, (const char *)a1);

        if (import->nid == 0xe83742ba)
            return psbridge_umd_deactivate((int)a0, (const char *)a1);

        if (
            import->nid == 0x8ef08fce ||
            import->nid == 0x4a9e5e29
        )
            return psbridge_umd_wait(a0);

        if (import->nid == 0xaee7404d)
            return 0;
    }

    if (import->library == LIB_SUSPEND) {
            if (import->nid == 0x3e0271d3)
                return psbridge_volatile_lock((void *)a1, (void *)a2);

            if (import->nid == 0xa569e425)
                return psbridge_volatile_unlock();
    }

    printf(
        "import sem runtime lib 0x%08x nid 0x%08x\n",
        import->library,
        import->nid
    );

    printf(
        "thread %d import %08x %08x\n",
        GetThreadId(),
        import->library,
        import->nid
    );

    return -1;
}
