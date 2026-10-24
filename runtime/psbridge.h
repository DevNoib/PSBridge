#ifndef PSBRIDGE_H
#define PSBRIDGE_H

#include <tamtypes.h>

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

extern const GuestInfo guest_info;
extern const u32 psp_import_count;

int psbridge_start(void);

void *psbridge_game_pointer(const void *pointer);

int psbridge_alloc_partition(int partition, const char *name, int type,u32 size,void *address);

void *psbridge_block_address(int uid);
int psbridge_free_partition(int uid);

int psbridge_create_fpl(const char *name, int partition, u32 attributes, u32 block_size, u32 block_count, void *options);

int psbridge_allocate_fpl(int uid, void **output);
int psbridge_delete_fpl(int uid);

int psbridge_create_thread(const char *name, void *entry, int priority, int stack_size, u32 attributes, void *options);

int psbridge_start_thread(int uid, u32 arg_size, void *arg);

#endif