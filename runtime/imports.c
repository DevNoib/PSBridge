#include <debug.h>
#include <stdint.h>
#include <string.h>

#include <psbridge.h>

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

    if (!strcmp(import->library, "ThreadManForUser")) {
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
    }

    scr_printf(
        "import sem runtime %s 0x%08x\n",
        import->library,
        import->nid
    );

    return -1;
}
