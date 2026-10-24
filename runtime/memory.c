#include <stdint.h>
#include <debug.h>
#include <string.h>

#include <psbridge.h>

#define HEAP_SIZE 0x1400000
#define VIDEO_MEMORY_SIZE 0x200000
#define BLOCK_LIMIT 64
#define FPL_LIMIT 16

typedef struct {
    int used;
    int uid;
    void *address;
    u32 size;
} MemoryBlock;

typedef struct {
    int used;
    int uid;
    void *address;
    u32 block_size;
    u32 block_count;
    u32 used_blocks;
} FixedPool;

static unsigned char game_heap[HEAP_SIZE] __attribute__((aligned(64)));
static unsigned char video_memory[VIDEO_MEMORY_SIZE] __attribute__((aligned(64)));
static u32 heap_position;
static MemoryBlock blocks[BLOCK_LIMIT];
static FixedPool pools[FPL_LIMIT];

void *psbridge_video_memory(void)
{
    return video_memory;
}

void *psbridge_game_pointer(const void *pointer) 
{
    u32 address = (u32) (uintptr_t)pointer;

    if (!address)
        return NULL;

    if (
        address < guest_info.base ||
        address - guest_info.base >= guest_info.memory_size
    )
        return (void *) (uintptr_t) address;

    return (u8 *)guest_info.memory + address - guest_info.base;
    
}

static void *heap_alloc(u32 size)
{
    u32 start = (heap_position + 63) & ~63;

    size = (size + 63) & ~63;

    if (!size || start + size > HEAP_SIZE)
        return NULL;

    heap_position = start + size;
    return game_heap + start;
}

int psbridge_alloc_partition (int partition, const char *name, int type,u32 size,void *address)
{
    int index;
    void *memory;

    (void)partition;
    (void)type;
    (void)address;

    memory = heap_alloc(size);

    if (!memory)
        return -1;

    for (index = 0; index < BLOCK_LIMIT; index++) {
        if (blocks[index].used)
            continue;

        blocks[index].used = 1;
        blocks[index].uid = 0x400 + index;
        blocks[index].address = memory;
        blocks[index].size = size;

        scr_printf(
            "memoria criada uid %d tamanho %u\n",
            blocks[index].uid,
            size
        );

        return blocks[index].uid;
    }

    return -1;
}

void *psbridge_block_address(int uid)
{
    int index;

    for (index = 0; index < BLOCK_LIMIT; index++) {
        if (blocks[index].used && blocks[index].uid == uid)
            return blocks[index].address;
    }

    return NULL;
}

int psbridge_free_partition(int uid)
{
    int index;

    for (index = 0; index < BLOCK_LIMIT; index++) {
        if (blocks[index].used && blocks[index].uid == uid) {
            blocks[index].used = 0;
            return 0;
        }
    }

    return -1;
}

int psbridge_create_fpl(const char *name, int partition, u32 attributes, u32 block_size, u32 block_count, void *options)
{
    int index;
    void *memory;

    (void)name;
    (void)partition;
    (void)attributes;
    (void)options;

    if (!block_size || !block_count || block_count > 32)
        return -1;

    block_size = (block_size + 63) & ~63;
    memory = heap_alloc(block_size * block_count);

    if (!memory)
        return -1;

    for (index = 0; index < FPL_LIMIT; index++) {
        if (pools[index].used)
            continue;

        pools[index].used = 1;
        pools[index].uid = 0x800 + index;
        pools[index].address = memory;
        pools[index].block_size = block_size;
        pools[index].block_count = block_count;
        pools[index].used_blocks = 0;
        return pools[index].uid;
    }
    
    return -1;

}

int psbridge_allocate_fpl(int uid, void **output)
{
    int index;
    void **real_output = psbridge_game_pointer(output);

    if (!real_output) 
        return -1;

    for (index = 0; index < FPL_LIMIT; index++) {
        FixedPool *pool = &pools[index];
        u32 block;

        if (!pool->used || pool->uid != uid)
            continue;

        for (block = 0; block < pool->block_count; block++) {
            if (pool->used_blocks & (1u << block))
                continue;

            pool->used_blocks |= 1u << block;
            *real_output = (u8 *)pool->address + block * pool->block_size;
            return 0;
        }
    }
    return -1;
}


int psbridge_delete_fpl(int uid)
{
    int index;

    for (index = 0; index < FPL_LIMIT; index++) {
        if (pools[index].used && pools[index].uid == uid) {
            memset(&pools[index], 0, sizeof(pools[index]));
            return 0;
        }
    }

    return -1;
}
