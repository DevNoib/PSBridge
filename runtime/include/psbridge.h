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
void *psbridge_video_memory(void);
int psbridge_controller_read(void *output, int count);
int psbridge_utility(u32 nid, void *argument);
u64 psbridge_display(u32 nid, u32 a0, u32 a1, u32 a2, u32 a3);
u64 psbridge_ge(u32 nid, u32 a0, u32 a1, u32 a2, u32 a3);

int psbridge_alloc_partition(int partition, const char *name, int type,u32 size,void *address);

void *psbridge_block_address(int uid);
int psbridge_free_partition(int uid);

int psbridge_create_fpl(const char *name, int partition, u32 attributes, u32 block_size, u32 block_count, void *options);

int psbridge_allocate_fpl(int uid, void **output);
int psbridge_delete_fpl(int uid);

int psbridge_create_thread(const char *name, void *entry, int priority, int stack_size, u32 attributes, void *options);

int psbridge_start_thread(int uid, u32 arg_size, void *arg);
int psbridge_create_lwmutex(void *workarea, u32 attributes, int initial_count);
int psbridge_delete_lwmutex(void *workarea);
int psbridge_lock_lwmutex(void *workarea, int count);
int psbridge_unlock_lwmutex(void *workarea, int count);

int psbridge_file_open(const char *path, int flags, int mode);
int psbridge_file_close(int descriptor);
int psbridge_file_read(int descriptor, void *buffer, u32 size);
int psbridge_file_write(int descriptor, const void *buffer, u32 size);
s64 psbridge_file_seek(int descriptor, u32 low, u32 high, int whence);
int psbridge_file_stat(const char *path, void *output);
int psbridge_file_remove(const char *path);
int psbridge_file_rename(const char *old_path, const char *new_path);

#endif
