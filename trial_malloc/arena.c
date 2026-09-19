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
    *a = (arena){
        .base = base,
        .mapped_size = length,
        .usable_size = length & ~(size_t)7
    };
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
        aligned > a->usable_size - a->offset) {
        return NULL;
    }
    a->prev_offset = a->offset;
    a->offset += aligned;
    return (unsigned char *)a->base + a->prev_offset;
}

void *arena_alloc_back(arena *a, size_t length)
{
    size_t aligned;
    if (!a || !a->base || !align_size(length, &aligned) ||
        aligned > a->usable_size - a->offset) {
        return NULL;
    }
    a->usable_size -= aligned;
    return (unsigned char *)a->base + a->usable_size;
}

bool arena_rollback(arena *a)
{
    if (!a || !a->base || a->list || a->prev_offset == a->offset) {
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
    if (!a || !a->base || a->list || !align_size(length, &aligned) ||
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
    memset(a->base, 0, a->usable_size);
    return true;
}

bool arena_reset(arena *a)
{
    if (!arena_zero(a)) {
        return false;
    }
    a->offset = 0;
    a->prev_offset = 0;
    if (a->list) {
        a->list->counter = 0;
    }
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
    if (a->list) {
        *a->list = (partial_list){0};
    }
    *a = (arena){0};
    return true;
}

bool create_partial_list(arena *a, partial_list *list, size_t limit)
{
    size_t array_bytes;
    _Static_assert(_Alignof(size_t) <= 8, "metadata needs at most 8-byte alignment");
    if (!a || !a->base || a->list || !list || list->owner ||
        limit > SIZE_MAX / sizeof(size_t) ||
        !align_size(limit * sizeof(size_t), &array_bytes) ||
        array_bytes > SIZE_MAX / 2) {
        return false;
    }

    /* Reserve both arrays at once so failure cannot consume half the metadata. */
    void *storage = arena_alloc_back(a, array_bytes * 2);
    if (!storage) {
        return false;
    }
    memset(storage, 0, array_bytes * 2);
    *list = (partial_list){
        .position = storage,
        .size = (size_t *)((unsigned char *)storage + array_bytes),
        .capacity = limit,
        .owner = a
    };
    a->list = list;
    return true;
}

static void remove_range(partial_list *list, size_t index)
{
    for (size_t i = index + 1; i < list->counter; ++i) {
        list->position[i - 1] = list->position[i];
        list->size[i - 1] = list->size[i];
    }
    --list->counter;
}

bool add_partial_list(arena *a, partial_list *list, void *ptr, size_t length)
{
    size_t aligned;
    if (!a || !a->base || !list || a->list != list || list->owner != a ||
        !ptr || !align_size(length, &aligned)) {
        return false;
    }

    /* Integer addresses allow rejecting foreign pointers without undefined
     * pointer subtraction. This assumes the flat address space used by mmap. */
    uintptr_t base = (uintptr_t)a->base;
    uintptr_t address = (uintptr_t)ptr;
    if (address < base || address - base > a->offset) {
        return false;
    }
    size_t start = (size_t)(address - base);
    if (start % 8 != 0 || aligned > a->offset - start) {
        return false;
    }
    size_t end = start + aligned; /* Bounded by offset before addition. */
    size_t i = 0;
    while (i < list->counter && list->position[i] < start) {
        ++i;
    }

    size_t left_end = i ? list->position[i - 1] + list->size[i - 1] : 0;
    if ((i && left_end > start) ||
        (i < list->counter && end > list->position[i])) {
        return false;
    }
    bool left = i && left_end == start;
    bool right = i < list->counter && end == list->position[i];
    if (!left && !right && list->counter >= list->capacity) {
        return false;
    }

    /* A rejected release leaves both the payload and the list unchanged.
     * Neighbor merges need no new slot, even when the list is full. */
    memset(ptr, 0, aligned);
    /* Adjacent ranges end at or before offset, bounding the sums below. */
    if (left && right) {
        list->size[i - 1] += aligned + list->size[i];
        remove_range(list, i);
    } else if (left) {
        list->size[i - 1] += aligned;
    } else if (right) {
        list->position[i] = start;
        list->size[i] += aligned;
    } else {
        for (size_t j = list->counter; j > i; --j) {
            list->position[j] = list->position[j - 1];
            list->size[j] = list->size[j - 1];
        }
        list->position[i] = start;
        list->size[i] = aligned;
        ++list->counter;
    }
    return true;
}

void *get_partial_block(partial_list *list, size_t length)
{
    size_t aligned;
    if (!list || !list->owner || list->owner->list != list ||
        !align_size(length, &aligned)) {
        return NULL;
    }
    size_t candidate = list->counter;
    for (size_t i = 0; i < list->counter; ++i) {
        if (list->size[i] == aligned) {
            void *ptr = (unsigned char *)list->owner->base + list->position[i];
            remove_range(list, i);
            return ptr;
        }
        if (list->size[i] > aligned && candidate == list->counter) {
            candidate = i;
        }
    }
    if (candidate == list->counter) {
        return NULL;
    }
    void *ptr = (unsigned char *)list->owner->base + list->position[candidate];
    list->position[candidate] += aligned;
    list->size[candidate] -= aligned;
    return ptr;
}
