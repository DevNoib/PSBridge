#include "psbridge_log.h"
#include <debug.h>
#include <stdint.h>
#include <string.h>
#include <timer.h>
#include <stdio.h>
#include <kernel.h>

#include <psbridge.h>

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
#define LIB_STDIO 0x402828c6
#define LIB_MPEG 0xe03657a3

static u64 psbridge_video_skip(u32 nid, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5)
{
    static u32 seen[40];
    static u32 calls[40];
    static int count;
    int i;
    for (i = 0; i < count; i++) {
        if (seen[i] == nid)
            break;
    }
    if (i == count && count < 40) {
        seen[count++] = nid;
        PRINT_LOG("video foi de arrasta nid %08lx\n", (unsigned long)nid);
    }
    if (i < count && ++calls[i] % 20000u == 0)
        PRINT_LOG("video ainda tentando nid %08lx chamadas %lu\n",
            (unsigned long)nid, (unsigned long)calls[i]);

    switch (nid) {
        case 0x37295ed8: {
            u32 *ring = psbridge_game_pointer((void *)(uintptr_t)a0);
            if (!ring || a1 > 8192u || a1 * 2152u > a3)
                return (u32)-1;
            memset(ring, 0, 48);
            ring[0] = a1;
            ring[4] = 2048;
            ring[5] = a2;
            ring[6] = a4;
            ring[7] = a5;
            ring[8] = a2 + a1 * 2048u;
            return 0;
        }
        case 0x682a619b:
        case 0x874624d6:
        case 0x606a4649:
        case 0x13407f13:
        case 0x591a4aa2:
        case 0x707b7629:
        case 0xceb870b1:
        case 0x4571cc64:
        case 0x740fccd1:
        case 0x0e3c2e9d:
            return 0;
        case 0xc132e22f:
            return 0x10000;
        case 0xd8c5f121:
            return 0;
        case 0xd7a29f46:
            return a0 > 0 && a0 < 0x8000 ? a0 * 2152u : 0;
        case 0x21ff80e4:
        case 0x611e9e11: {
            const u8 *header = psbridge_game_pointer((void *)(uintptr_t)(nid == 0x21ff80e4 ? a1 : a0));
            u32 *result = psbridge_game_pointer((void *)(uintptr_t)(nid == 0x21ff80e4 ? a2 : a1));
            u32 offset;
            u32 stream_size;
            if (!header || !result || memcmp(header, "PSMF", 4))
                return (u32)-1;
            offset = ((u32)header[8] << 24) | ((u32)header[9] << 16) |
                     ((u32)header[10] << 8) | header[11];
            stream_size = ((u32)header[12] << 24) | ((u32)header[13] << 16) |
                          ((u32)header[14] << 8) | header[15];
            if (!offset || (offset & 2047u))
                return (u32)-1;
            *result = nid == 0x21ff80e4 ? offset : stream_size;
            return 0;
        }
        case 0x42560f23:
            return (a1 & 0xffu) + 1;
        case 0xa780cf7e:
            return 1;
        case 0xb5f6dc87: {
            u32 *ring = psbridge_game_pointer((void *)(uintptr_t)a0);
            if (!ring || !ring[0] || ring[0] > 8192u)
                return (u32)-1;
            return ring[0];
        }
        case 0xb240a59e: {
            static u32 puts;
            u32 *ring = psbridge_game_pointer((void *)(uintptr_t)a0);
            int result;
            u32 call = ++puts;
            if (call <= 12 || (call & 127u) == 0)
                PRINT_LOG("video mamando pacotes tentativa %u pediu %u disponivel %u callback %08x\n",
                    call, a1, a2, ring ? ring[6] : 0);
            if (!ring || !ring[0] || ring[0] > 8192u || !ring[6])
                return (u32)-1;
            if (a1 > ring[0])
                a1 = ring[0];
            if (a1 > a2)
                a1 = a2;
            if (!a1)
                return 0;
            result = psbridge_call_callback((void *)(uintptr_t)ring[6], ring[5], a1, ring[7]);
            if (call <= 12 || (call & 127u) == 0 || result < 0) {
                PRINT_LOG("video callback gozou tentativa %u pacotes %d\n", call, result);
            } else if (result == 0) {
                static u32 secos;
                if (++secos <= 12)
                    PRINT_LOG("video secou os pacotes na tentativa %u\n", call);
            }
            if (result < 0)
                return result;
            if ((u32)result > a1)
                result = (int)a1;
            return result;
        }
        case 0xe1ce83a7:
        case 0xfe246728:
            return 0x80618001u;
        case 0x800c44df:
            return 0x80618001u;
        case 0x167afd9e: {
            u32 *au = psbridge_game_pointer((void *)(uintptr_t)a2);
            if (au)
                memset(au, 0, 24);
            return 0;
        }
        case 0xf8dcb679: {
            u32 *es = psbridge_game_pointer((void *)(uintptr_t)a1);
            u32 *decoded = psbridge_game_pointer((void *)(uintptr_t)a2);
            if (es)
                *es = 2112;
            if (decoded)
                *decoded = 8192;
            return 0;
        }
        default:
            return 0x80618001u;
    }
}

void psbridge_mixer_probe(u32 library, u32 nid, u32 callsite);

u64 psbridge_dispatch(const PspImport *import, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 callsite)
{
    psbridge_mixer_probe(import->library, import->nid, callsite);

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
        if (import->nid == 0x55f4717d)
            return psbridge_file_chdir((const char *)(uintptr_t)a0);
        if (import->nid == 0xb29ddf9c)
            return psbridge_file_dopen((const char *)(uintptr_t)a0);
        if (import->nid == 0xe3eb004c)
            return psbridge_file_dread((int)a0, (void *)(uintptr_t)a1);
        if (import->nid == 0xeb092469)
            return psbridge_file_dclose((int)a0);
        if (import->nid == 0x109f50bc)
            return psbridge_file_open((const char *)a0, (int)a1, (int)a2);

        if (import->nid == 0x89aa9906)
            return psbridge_file_open_async((const char *)(uintptr_t)a0, (int)a1, (int)a2);
        if (import->nid == 0x35dbd746 || import->nid == 0xe23eec33 ||
            import->nid == 0x3251ea56 || import->nid == 0xcb05f8d6)
            return psbridge_file_async_wait((int)a0, (void *)(uintptr_t)a1,
                import->nid == 0x3251ea56 || import->nid == 0xcb05f8d6);
        if (import->nid == 0xa0b5a7c2)
            return psbridge_file_read_async((int)a0, (void *)(uintptr_t)a1, a2);
        if (import->nid == 0x0facab19)
            return psbridge_file_write_async((int)a0, (const void *)(uintptr_t)a1, a2);
        if (import->nid == 0xff5940b6)
            return psbridge_file_close_async((int)a0);

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
                (!strcmp(device, "umd0:") || !strcmp(device, "umd1:")) &&
                a1 == 0x01f300a7 &&
                a2 &&
                a3 >= sizeof(u32)
            ) {
                const void *cache_request = psbridge_game_pointer(
                    (void *)(uintptr_t)a2
                );

                if (cache_request)
                    return 0;
            }

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

        if (import->nid == 0x63632449)
            return psbridge_file_ioctl(
                (int)a0,
                a1,
                (void *)(uintptr_t)a2,
                a3,
                (void *)(uintptr_t)a4,
                a5
            );
    }

    if (import->library == LIB_KERNEL) {
        if (import->nid == 0x293b45b8)
            return psbridge_psp_thread_uid();

        if (import->nid == 0x092968f4)
            return 1;

        if (import->nid == 0x5f10d406)
            return 0;

        if (import->nid == 0xd13bde95)
            return 0x10000;

        if (import->nid == 0xbea46419 || import->nid == 0x1fc64e09)
            return psbridge_lock_lwmutex_ex(
                (void *)a0,
                (int)a1,
                (void *)(uintptr_t)a2,
                import->nid == 0x1fc64e09,
                0
            );

        if (import->nid == 0xdc692ee3)
            return psbridge_lock_lwmutex_ex((void *)a0, (int)a1, NULL, 0, 1);

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

    if (import->library == LIB_MPEG)
        return psbridge_video_skip(import->nid, a0, a1, a2, a3, a4, a5);

    if (import->library == LIB_AUDIO ||
        import->library == LIB_SAS ||
        import->library == LIB_ATRAC)
        return psbridge_audio(import->library, import->nid, a0, a1, a2, a3, a4);

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
        if (import->nid == 0xd6da4ba1)
            return psbridge_create_semaphore((const char *)(uintptr_t)a0, a1, (int)a2, (int)a3);
        if (import->nid == 0x28b6489c)
            return psbridge_delete_semaphore((int)a0);
        if (import->nid == 0x3f53e640)
            return psbridge_signal_semaphore((int)a0, (int)a1);
        if (import->nid == 0x4e3a1105)
            return psbridge_wait_semaphore((int)a0, (int)a1, (void *)(uintptr_t)a2);
        if (import->nid == 0x9ace131e)
            return SleepThread();
        if (import->nid == 0xd59ead2f)
            return psbridge_wakeup_guest_thread((int)a0);
        if (import->nid == 0x9fa03cd3)
            return psbridge_delete_guest_thread((int)a0);

        if (import->nid == 0x56c039b5)
            return psbridge_create_vpl((const char *)(uintptr_t)a0, (int)a1, a2, a3, (void *)(uintptr_t)a4);

        if (import->nid == 0xbed27435 || import->nid == 0xaf36d708)
            return psbridge_allocate_vpl((int)a0, a1, (void *)(uintptr_t)a2, import->nid == 0xaf36d708);

        if (import->nid == 0xb736e9ff)
            return psbridge_free_vpl((int)a0, (void *)(uintptr_t)a1);

        if (import->nid == 0x89b3d48c)
            return psbridge_delete_vpl((int)a0);

        if (import->nid == 0xa9c2cb9a)
            return psbridge_refer_mutex_status((int)a0, (void *)(uintptr_t)a1);

        if (import->nid == 0xb7d098c6)
            return psbridge_create_mutex((const char *)(uintptr_t)a0, a1, (int)a2, (void *)(uintptr_t)a3);

        if (import->nid == 0xb011b11f || import->nid == 0x5bf4dd27 || import->nid == 0x0ddcd2c9)
            return psbridge_lock_mutex((int)a0, (int)a1, (void *)(uintptr_t)a2, import->nid == 0x0ddcd2c9);

        if (import->nid == 0x6b30100f)
            return psbridge_unlock_mutex((int)a0, (int)a1);

        if (import->nid == 0xf8170fbe)
            return psbridge_delete_mutex((int)a0);

        if (import->nid == 0x71bc9871)
            return psbridge_change_thread_priority((int)a0, (int)a1);

        if (import->nid == 0xaa73c935) {
            psbridge_exit_guest_thread((int)a0);
            return 0;
        }


        if (import->nid == 0xea748e31) {
            return 0;
        }

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
            return psbridge_create_callback(a1, a2);

        if (import->nid == 0xedba5844)
            return psbridge_delete_callback((int)a0);

        if (import->nid == 0xc11ba8c4)
            return psbridge_notify_callback((int)a0, a1);

        if (import->nid == 0xba4051d6)
            return psbridge_cancel_callback((int)a0);

        if (import->nid == 0x2a3d44ff)
            return psbridge_callback_count((int)a0);

        if (import->nid == 0x349d6d6c)
            return psbridge_check_callbacks();

        if (import->nid == 0x55c20a00)
            return psbridge_create_event(a1, a2);

        if (import->nid == 0xef9e4c70)
            return psbridge_delete_event((int)a0);

        if (import->nid == 0x1fb15a32)
            return psbridge_set_event((int)a0, a1);

        if (import->nid == 0x812346e4)
            return psbridge_clear_event((int)a0, a1);

        if (import->nid == 0x402fcf22 || import->nid == 0x328c546a)
            return psbridge_wait_event(
                (int)a0,
                a1,
                a2,
                (void *)(uintptr_t)a3,
                (void *)(uintptr_t)a4,
                import->nid == 0x328c546a,
                0
            );

        if (import->nid == 0x30fd48f0)
            return psbridge_wait_event(
                (int)a0,
                a1,
                a2,
                (void *)(uintptr_t)a3,
                NULL,
                0,
                1
            );

        if (
            import->nid == 0xceadeb47 ||
            import->nid == 0x68da9e36
        )
            return psbridge_delay_thread(a0, import->nid == 0x68da9e36);

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

    if (import->library == LIB_MODULEMGR) {
        if (import->nid == 0xf0a26395)
            return 1;

        if (import->nid == 0xd8b73127) {
            u32 address = a0 & 0x3fffffff;
            if (address >= guest_info.base &&
                address - guest_info.base < guest_info.memory_size)
                return 1;
            return -1;
        }

        if (import->nid == 0x8f2df740)
            return 0;
    }

    if (import->library == LIB_POWER) {
        if (
            import->nid == 0x04b7766e ||
            import->nid == 0x469989ad
        )
            return 0;

        if (import->nid == 0xa85880d0)
            return 1;
    }

    if (import->library == LIB_IMPOSE)
        return 0;

    if (import->library == LIB_UTILS) {
        if (import->nid == 0x71ec4271) {
            u32 *timeval = a0 ? psbridge_game_pointer((void *)(uintptr_t)a0) : NULL;
            u32 *timezone = a1 ? psbridge_game_pointer((void *)(uintptr_t)a1) : NULL;
            u64 clock = system_time();
            if (a0 && !timeval)
                return -1;
            if (timeval) {
                timeval[0] = (u32)(clock / 1000000) + 946684800u;
                timeval[1] = (u32)(clock % 1000000);
            }
            if (timezone) {
                timezone[0] = 0;
                timezone[1] = 0;
            }
            return 0;
        }

        if (import->nid == 0x91e4f6a7)
            return (u32)system_time();
        if (import->nid == 0x27cc57f0) {
            u32 now = (u32)(system_time() / 1000000) + 946684800u;
            if (a0) {
                u32 *output = psbridge_game_pointer((void *)(uintptr_t)a0);
                if (!output)
                    return -1;
                *output = now;
            }
            return now;
        }

        if(import->nid ==0xb435dec5) {
            FlushCache(0);
            return 0;
        }

        if(import->nid == 0x3ee30821)
            return 0;

        if (import->nid == 0x79d1c3fa)
            return 0;

        if (
            import->nid == 0xc2df770e ||
            import->nid == 0x920f104a
        )
            return 0;
    }

    if (import->library == LIB_STDIO) {
        if (import->nid == 0x172d316e)
            return 0;
        if (import->nid == 0xa6bab2e9)
            return 1;
        if (import->nid == 0xf78ba90a)
            return 2;
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

        if (import->nid == 0x8ef08fce)
            return psbridge_umd_wait(a0);

        if (import->nid == 0x4a9e5e29)
            return psbridge_umd_wait_cb(a0, a1);

        if (import->nid == 0xaee7404d)
            return psbridge_umd_register_callback((int)a0);

        if (import->nid == 0xbd2bde07)
            return psbridge_umd_unregister_callback((int)a0);
    }

    if (import->library == LIB_SUSPEND) {
            if (import->nid == 0x090ccb3f)
                return 0;
            if (import->nid == 0x3e0271d3)
                return psbridge_volatile_lock((void *)a1, (void *)a2);

            if (import->nid == 0xa569e425)
                return psbridge_volatile_unlock();
    }

    {
        static u32 unknown[32][2];
        static int unknown_count;
        int i;
        for (i = 0; i < unknown_count; i++) {
            if (unknown[i][0] == import->library && unknown[i][1] == import->nid)
                break;
        }
        if (i == unknown_count && unknown_count < 32) {
            unknown[unknown_count][0] = import->library;
            unknown[unknown_count][1] = import->nid;
            unknown_count++;
            PRINT_LOG("import fantasma brotou biblioteca %08lx nid %08lx chamada %08lx\n",
                (unsigned long)import->library, (unsigned long)import->nid,
                (unsigned long)callsite);
        }
    }
    return -1;
}
