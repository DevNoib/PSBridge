#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "psbridge.h"

#define PATH_SIZE 512

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

    if (host_path(path, translated) < 0)
        return -1;

    return open(translated, open_flags(flags), mode);
}

int psbridge_file_close(int descriptor)
{
    return close(descriptor);
}

int psbridge_file_read(int descriptor, void *buffer, u32 size)
{
    void *destination = psbridge_game_pointer(buffer);

    if (!destination && size)
        return -1;

    return read(descriptor, destination, size);
}

int psbridge_file_write(int descriptor, const void *buffer, u32 size)
{
    const void *source = psbridge_game_pointer(buffer);

    if (!source && size)
        return -1;

    return write(descriptor, source, size);
}

s64 psbridge_file_seek(int descriptor, u32 low, u32 high, int whence)
{
    s64 offset = (s64)(((u64)high << 32) | low);
    return lseek(descriptor, offset, whence);
}

int psbridge_file_stat(const char *path, void *output)
{
    char translated[PATH_SIZE];
    struct stat source;
    PspFileStat *destination = psbridge_game_pointer(output);

    if (!destination || host_path(path, translated) < 0)
        return -1;

    if (stat(translated, &source) < 0)
        return -1;

    memset(destination, 0, sizeof(*destination));
    destination->mode = S_ISDIR(source.st_mode) ? 0x11ff : 0x21ff;
    destination->attributes = S_ISDIR(source.st_mode) ? 0x10 : 0x20;
    destination->size = source.st_size;
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
