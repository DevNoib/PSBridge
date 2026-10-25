#include <stdint.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>

#include <psbridge.h>

#define HEAP_SIZE 0x13e0000
#define VIDEO_MEMORY_SIZE 0x200000
#define BLOCK_LIMIT 64
#define FPL_LIMIT 16
#define HEAP_ALLOCATION_LIMIT 96

typedef struct {
    int used;
    u32 start;
    u32 size;
} HeapAllocation;

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

static unsigned char game_heap[HEAP_SIZE] __attribute__((aligned(4096)));
static unsigned char video_memory[VIDEO_MEMORY_SIZE] __attribute__((aligned(64)));
static HeapAllocation heap_allocations[HEAP_ALLOCATION_LIMIT];
static MemoryBlock blocks[BLOCK_LIMIT];
static FixedPool pools[FPL_LIMIT];

static void * volatile_memory;
static int volatile_locked;

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

static int heap_slot(void)
{
    int index;

    for (index = 0; index < HEAP_ALLOCATION_LIMIT; index++) {
        if (!heap_allocations[index].used)
            return index;
    }

    return -1;
}

static u32 align_up(u32 value, u32 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static u32 align_down(u32 value, u32 alignment)
{
    return value & ~(alignment - 1);
}

static int heap_overlaps(u32 start, u32 size, u32 *boundary, int high)
{
    int index;
    u32 end = start + size;

    for (index = 0; index < HEAP_ALLOCATION_LIMIT; index++) {
        HeapAllocation *allocation = &heap_allocations[index];
        u32 allocation_end;

        if (!allocation->used)
            continue;

        allocation_end = allocation->start + allocation->size;

        if (start >= allocation_end || end <= allocation->start)
            continue;

        *boundary = high ? allocation->start : allocation_end;
        return 1;
    }

    return 0;
}

static void *heap_take(u32 start, u32 size, int slot)
{
    heap_allocations[slot].used = 1;
    heap_allocations[slot].start = start;
    heap_allocations[slot].size = size;
    return game_heap + start;
}

static void *heap_alloc(u32 size, u32 alignment, int high)
{
    u32 position;
    u32 boundary;
    int slot;

    if (alignment < 64)
        alignment = 64;

    if ((alignment & (alignment - 1)) || !size || size > HEAP_SIZE)
        return NULL;

    if (size > HEAP_SIZE - 63)
        return NULL;

    size = align_up(size, 64);
    slot = heap_slot();

    if (slot < 0)
        return NULL;

    if (!high) {
        position = 0;

        while (position <= HEAP_SIZE - size) {
            position = align_up(position, alignment);

            if (position > HEAP_SIZE - size)
                break;

            if (!heap_overlaps(position, size, &boundary, 0))
                return heap_take(position, size, slot);

            position = boundary;
        }

        return NULL;
    }

    position = align_down(HEAP_SIZE - size, alignment);

    for (;;) {
        if (!heap_overlaps(position, size, &boundary, 1))
            return heap_take(position, size, slot);

        if (boundary < size)
            break;

        position = align_down(boundary - size, alignment);
    }

    return NULL;
}

static void *heap_alloc_at(void *requested, u32 size)
{
    uintptr_t address = (uintptr_t)requested;
    uintptr_t base = (uintptr_t)game_heap;
    u32 start;
    u32 boundary;
    int slot;

    if (address < base || address >= base + HEAP_SIZE || !size)
        return NULL;

    start = (u32)(address - base);

    if (size > HEAP_SIZE - start)
        return NULL;

    size = align_up(size, 64);
    slot = heap_slot();

    if (slot < 0 || heap_overlaps(start, size, &boundary, 0))
        return NULL;

    return heap_take(start, size, slot);
}

static int heap_free(void *pointer)
{
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t base = (uintptr_t)game_heap;
    int index;

    if (address < base || address >= base + HEAP_SIZE)
        return -1;

    for (index = 0; index < HEAP_ALLOCATION_LIMIT; index++) {
        if (
            heap_allocations[index].used &&
            heap_allocations[index].start == (u32)(address - base)
        ) {
            memset(&heap_allocations[index], 0, sizeof(heap_allocations[index]));
            return 0;
        }
    }

    return -1;
}

int psbridge_volatile_lock(void *address, void *size)
{
    u32 *output_address = psbridge_game_pointer(address);
    u32 *output_size = psbridge_game_pointer(size);

    if (!output_address || !output_size || volatile_locked)
        return -1;

    if (!volatile_memory)
        volatile_memory = heap_alloc(0x400000, 64, 0);

    if (!volatile_memory)
        return -1;

    volatile_locked = 1;
    *output_address = (u32) (uintptr_t)volatile_memory;
    *output_size = 0x400000;

    return 0;

}

int psbridge_volatile_unlock(void)
{
    if (!volatile_locked)
        return -1;

    volatile_locked = 0;
    return 0;
}

int psbridge_alloc_partition(int partition, const char *name, int type, u32 size, void *address)
{
    int index;
    void *memory;
    u32 alignment = 64;

    (void)partition;

    for (index = 0; index < BLOCK_LIMIT; index++) {
        if (!blocks[index].used)
            break;
    }

    if (index == BLOCK_LIMIT)
        return -1;

    if (type == 2) {
        memory = heap_alloc_at(address, size);
    } else {
        if (type == 3 || type == 4)
            alignment = (u32)(uintptr_t)address;

        if (type < 0 || type > 4)
            return -1;

        memory = heap_alloc(size, alignment, type == 1 || type == 4);
    }

    if (!memory)
        return -1;

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
            if (heap_free(blocks[index].address) < 0)
                return -1;

            memset(&blocks[index], 0, sizeof(blocks[index]));
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

    for (index = 0; index < FPL_LIMIT; index++) {
        if (!pools[index].used)
            break;
    }

    if (index == FPL_LIMIT)
        return -1;

    block_size = (block_size + 63) & ~63;
    memory = heap_alloc(block_size * block_count, 64, 0);

    if (!memory)
        return -1;

    pools[index].used = 1;
    pools[index].uid = 0x800 + index;
    pools[index].address = memory;
    pools[index].block_size = block_size;
    pools[index].block_count = block_count;
    pools[index].used_blocks = 0;
    return pools[index].uid;

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
            if (heap_free(pools[index].address) < 0)
                return -1;

            memset(&pools[index], 0, sizeof(pools[index]));
            return 0;
        }
    }

    return -1;
}
