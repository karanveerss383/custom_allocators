#include "arena.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static bool align_size(size_t length, size_t *aligned)
{
    if (length == 0 || length > SIZE_MAX - 7) {
        return false;
    }
    *aligned = (length + 7) & ~(size_t)7;
    return true;
}

static bool init_mapping(arena *a, const char *path, size_t length)
{
    if (!a || a->base || length == 0 || length > PTRDIFF_MAX) {
        return false;
    }

    int fd = -1;
    if (path) {
        off_t file_length = (off_t)length;
        if (file_length <= 0 || (uintmax_t)file_length != (uintmax_t)length) {
            return false;
        }
        fd = open(path, O_RDWR | O_CREAT, 0644);
        if (fd == -1) {
            return false;
        }
        if (ftruncate(fd, file_length) == -1) {
            close(fd);
            return false;
        }
    }

    void *base = mmap(NULL, length, PROT_READ | PROT_WRITE,
                      path ? MAP_SHARED : MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);
    if (fd != -1) {
        /* No descriptor is retained by the allocator after mapping. */
        int closed = close(fd);
        if (closed == -1) {
            if (base != MAP_FAILED) {
                munmap(base, length);
            }
            return false;
        }
    }
    if (base == MAP_FAILED) {
        return false;
    }
    *a = (arena){.base = base, .mapped_size = length};
    return true;
}

bool arena_init(arena *a, size_t length)
{
    return init_mapping(a, NULL, length);
}

bool arena_init_file(arena *a, const char *path, size_t length)
{
    return path && init_mapping(a, path, length);
}

void *arena_alloc(arena *a, size_t length)
{
    size_t aligned;
    if (!a || !a->base || !align_size(length, &aligned) ||
        aligned > a->mapped_size - a->offset) {
        return NULL;
    }
    a->prev_offset = a->offset;
    a->offset += aligned;
    return (unsigned char *)a->base + a->prev_offset;
}

bool arena_rollback(arena *a)
{
    if (!a || !a->base || a->prev_offset == a->offset) {
        return false;
    }
    memset((unsigned char *)a->base + a->prev_offset, 0,
           a->offset - a->prev_offset);
    a->offset = a->prev_offset;
    return true;
}

bool arena_rewind(arena *a, size_t length)
{
    size_t aligned;
    if (!a || !a->base || !align_size(length, &aligned) ||
        aligned > a->offset) {
        return false;
    }
    a->offset -= aligned;
    a->prev_offset = a->offset;
    memset((unsigned char *)a->base + a->offset, 0, aligned);
    return true;
}

bool arena_zero(arena *a)
{
    if (!a || !a->base) {
        return false;
    }
    memset(a->base, 0, a->mapped_size);
    return true;
}

bool arena_reset(arena *a)
{
    if (!arena_zero(a)) {
        return false;
    }
    a->offset = 0;
    a->prev_offset = 0;
    return true;
}

bool arena_destroy(arena *a)
{
    if (!a) {
        return false;
    }
    if (a->base && munmap(a->base, a->mapped_size) == -1) {
        return false;
    }
    *a = (arena){0};
    return true;
}
