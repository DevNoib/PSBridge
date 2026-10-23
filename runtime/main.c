#include <debug.h>
#include <kernel.h>
#include <tamtypes.h>
#include <string.h>
#include <sifrpc.h>

typedef struct {
    void *memory;
    u32 base;
    u32 memory_size;
    u32 file_size;
    u32 gp;
} GuestInfo;

typedef struct {
    u32 nid;
    const char *library;
    u32 stub;
    const void *wrapper;
} PspImport;

extern const u32 psp_import_count;
extern const GuestInfo guest_info;

extern int psbridge_start(void);

int psbridge_dispatch(const PspImport *import) 
{
    scr_printf("import sem runtime %s 0x%08x\n", import->library, import->nid);
    return -1;
}

int main(void) 
{
    sceSifInitRpc(0);
    init_scr();

    scr_printf("PSBridge levantou a frica\n");
    scr_printf("memoria do jogo %u bytes\n", guest_info.memory_size);
    scr_printf("imports na fila do sus %u\n", psp_import_count);

    memset(
        (u8 *)guest_info.memory + guest_info.file_size,
        0,
        guest_info.memory_size - guest_info.file_size
    );

    FlushCache(0);
    FlushCache(2);

    scr_printf("pulando pro jogo\n");
    int result = psbridge_start();
    scr_printf("o jogo voltou com %d\n", result);

    SleepThread();
    return result;
}
