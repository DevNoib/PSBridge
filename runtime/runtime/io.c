#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "psbridge.h"

#define PATH_SIZE 512
#define UMD_DESCRIPTOR 0x4000
#define UMD_HANDLES 8

#define UMD_FILES 64

typedef struct {
    int used;
    s64 position;
} UmdHandle;

typedef struct {
    int used;
    char path[PATH_SIZE];
    u64 size;
    u32 first_sector;
    u32 sector_count;
} UmdFile;

static UmdHandle umd_handles[UMD_HANDLES];
static int umd_active = 1;

static UmdFile umd_files[UMD_FILES];
static u32 next_umd_sector;

typedef struct {
    u16 year;
    u16 month;
    u16 day;
    u16 hour;
    u16 minute;
    u16 second;
    u32 microsecond;
} PspDateTime;

typedef struct {
    u32 mode;
    u32 attributes;
    u64 size;
    PspDateTime created;
    PspDateTime accessed;
    PspDateTime modified;
    u32 private_data[6];
} PspFileStat;

static int host_path(const char *path, char *output)
{
    const char *game_path = psbridge_game_pointer(path);
    const char *prefix;
    const char *relative;
    int length;

    if (!game_path)
        return -1;

    if (!strncmp(game_path, "disc0:/sce_lbn", 14))
        return -1;

    if (!strncmp(game_path, "disc0:/", 7)) {
        prefix = "host:";
        relative = game_path + 7;
    } else if (!strncmp(game_path, "ms0:/", 5)) {
        prefix = "host:ms0/";
        relative = game_path + 5;
    } else {
        return -1;
    }

    length = snprintf(output, PATH_SIZE, "%s%s", prefix, relative);
    return length >= 0 && length < PATH_SIZE ? 0 : -1;
}

static int umd_path(const char *path)
{
    const char *game_path = psbridge_game_pointer(path);

    if (!game_path)
        return 0;

    return !strcmp(game_path, "umd0:") || !strcmp(game_path, "umd1:");
}

static UmdHandle *umd_handle(int descriptor)
{
    int index = descriptor - UMD_DESCRIPTOR;

    if (index < 0 || index >= UMD_HANDLES || !umd_handles[index].used)
        return NULL;

    return &umd_handles[index];
}

static int host_file_exists(const char *path)
{
    int descriptor = open(path, O_RDONLY);

    if (descriptor < 0)
        return 0;

    close(descriptor);
    return 1;
}

static UmdFile *register_umd_file(const char *path, u64 size)
{
    u64 sectors;
    int index;

    for (index = 0; index < UMD_FILES; index++) {
        if (umd_files[index].used && !strcmp(umd_files[index].path, path))
            return &umd_files[index];
    }

    sectors = (size + 2047) / 2048;

    if (sectors > 0xffffffff - next_umd_sector) {
        return NULL;
    }

    for (index = 0; index < UMD_FILES; index++) {
        UmdFile *file = &umd_files[index];

        if (file->used)
            continue;

        memset(file, 0, sizeof(*file));

        strncpy(file->path, path, PATH_SIZE - 1);

        file->used = 1;
        file->size = size;
        file->first_sector = next_umd_sector;
        file->sector_count = sectors;

        next_umd_sector += file->sector_count;
        return file;
    }

    return NULL;
}

static UmdFile *find_umd_file(s64 sector)
{
    int index;

    if (sector < 0)
        return NULL;

    for (index = 0; index < UMD_FILES; index++) {
        UmdFile *file = &umd_files[index];
        u64 end;

        if (!file->used)
            continue;

        end = (u64)file->first_sector + file->sector_count;

        if (
            (u64)sector >= file->first_sector &&
            (u64)sector < end
        )
            return file;
    }

    return NULL;
}

int psbridge_umd_present(void)
{
    return host_file_exists("host:UMD_DATA.BIN") &&
        host_file_exists("host:PSP_GAME/PARAM.SFO");
}

u32 psbridge_umd_state(void)
{
    if (!psbridge_umd_present())
        return 0x01;

    return umd_active ? 0x32 : 0x12;
}

int psbridge_umd_activate(int mode, const char *name)
{
    const char *device = psbridge_game_pointer(name);

    if ((mode != 1 && mode != 2) || !device || strcmp(device, "disc0:"))
        return -1;

    if (!psbridge_umd_present())
        return -1;

    umd_active = 1;
    return 0;
}

int psbridge_umd_deactivate(int mode, const char *name)
{
    const char *device = psbridge_game_pointer(name);

    if ((mode != 1 && mode != 2) || !device || strcmp(device, "disc0:"))
        return -1;

    umd_active = 0;
    return 0;
}

int psbridge_umd_wait(u32 state)
{
    return (psbridge_umd_state() & state) ? 0 : -1;
}

static int open_flags(int flags)
{
    int result;

    if ((flags & 3) == 2)
        result = O_WRONLY;
    else if ((flags & 3) == 3)
        result = O_RDWR;
    else
        result = O_RDONLY;

    if (flags & 0x0100)
        result |= O_APPEND;
    if (flags & 0x0200)
        result |= O_CREAT;
    if (flags & 0x0400)
        result |= O_TRUNC;
    if (flags & 0x0800)
        result |= O_EXCL;

    return result;
}

int psbridge_file_open(const char *path, int flags, int mode)
{
    

    char translated[PATH_SIZE];
    int index;

    const char *game_path = psbridge_game_pointer(path);

    printf(
        "io open %s flags %08x\n",
        game_path ? game_path : "invalido",
        flags
    );

    if (umd_path(path)) {
        if ((flags & 3) == 2 || (flags & 3) == 3 || !psbridge_umd_present())
            return -1;

        for (index = 0; index < UMD_HANDLES; index++) {
            if (!umd_handles[index].used) {
                umd_handles[index].used = 1;
                umd_handles[index].position = 0;
                return UMD_DESCRIPTOR + index;
            }
        }

        return -1;
    }

    if (host_path(path, translated) < 0)
        return -1;

    return open(translated, open_flags(flags), mode);
}

int psbridge_file_close(int descriptor)
{
    UmdHandle *handle = umd_handle(descriptor);

    if (handle) {
        memset(handle, 0, sizeof(*handle));
        return 0;
    }

    return close(descriptor);
}

int psbridge_file_read(int descriptor, void *buffer, u32 size)
{
    void *destination = psbridge_game_pointer(buffer);
    UmdHandle *handle = umd_handle(descriptor);

    if (!destination && size)
        return -1;

    if (handle) {
        UmdFile *file = find_umd_file(handle->position);

        u64 sector_offset;
        u64 byte_offset;
        u64 byte_count;
        int source;
        int bytes_read;

        if (!file || size > 0x7fffffff / 2048)
            return -1;

        sector_offset = handle->position - file->first_sector;

        if (size > file->sector_count - sector_offset)
            return -1;

        byte_offset = sector_offset * 2048;
        byte_count = size * 2048;

        if (byte_offset + byte_count > file->size)
            return -1;

        source = open(file->path, O_RDONLY);

        if (source < 0)
            return -1;

        if (lseek(source, byte_offset, SEEK_SET) < 0) {
            close(source);
            return -1;
        }

        bytes_read = read(source, destination, byte_count);
        close(source);

        if (bytes_read != byte_count)
            return -1;

        handle->position += size;
        return size;
        
    }

    return read(descriptor, destination, size);
}

int psbridge_file_write(int descriptor, const void *buffer, u32 size)
{
    const void *source = psbridge_game_pointer(buffer);

    if (umd_handle(descriptor))
        return -1;

    if (!source && size)
        return -1;

    return write(descriptor, source, size);
}

s64 psbridge_file_seek(int descriptor, u32 low, u32 high, int whence)
{
    printf("io seek fd %d high %08x low %08x whence %d\n",
    descriptor,
    high,
    low,
    whence
    );
    s64 offset = (s64)(((u64)high << 32) | low);
    UmdHandle *handle = umd_handle(descriptor);

    if (handle) {
        s64 position;

        if (whence == SEEK_SET)
            position = offset;
        else if (whence == SEEK_CUR)
            position = handle->position + offset;
        else
            return -1;

        if (position < 0)
            return -1;

        handle->position = position;
        return position;
    }

    return lseek(descriptor, offset, whence);
}

int psbridge_file_stat(const char *path, void *output)
{
    char translated[PATH_SIZE];
    struct stat source;
    PspFileStat *destination = psbridge_game_pointer(output);

    const char *game_path = psbridge_game_pointer(path);
    UmdFile *umd_file = NULL;

    if (!destination)
        return -1;

    if (umd_path(path)) {
        if (!psbridge_umd_present())
            return -1;

        memset(destination, 0, sizeof(*destination));
        destination->mode = 0x21ff;
        destination->attributes = 0x20;
        return 0;
    }

    if (host_path(path, translated) < 0)
        return -1;

    if (stat(translated, &source) < 0)
        return -1;

    if (game_path && !strncmp(game_path, "disc0:/", 7))
        umd_file = register_umd_file(translated, source.st_size);

    memset(destination, 0, sizeof(*destination));
    destination->mode = S_ISDIR(source.st_mode) ? 0x11ff : 0x21ff;
    destination->attributes = S_ISDIR(source.st_mode) ? 0x10 : 0x20;
    destination->size = source.st_size;

    if (umd_file)
        destination ->private_data[0] = umd_file->first_sector;

    return 0;
}

int psbridge_file_remove(const char *path)
{
    char translated[PATH_SIZE];

    if (host_path(path, translated) < 0)
        return -1;

    return remove(translated);
}

int psbridge_file_rename(const char *old_path, const char *new_path)
{
    char old_translated[PATH_SIZE];
    char new_translated[PATH_SIZE];

    if (
        host_path(old_path, old_translated) < 0 ||
        host_path(new_path, new_translated) < 0
    )
        return -1;

    return rename(old_translated, new_translated);
}
