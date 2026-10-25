#include <debug.h>
#include <kernel.h>
#include <tamtypes.h>
#include <string.h>
#include <sifrpc.h>
#include <psbridge.h>

u32 main_kernel_work[0x400] __attribute__((aligned(16)));

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
