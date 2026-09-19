#include "arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void lifecycle_and_bounds(void)
{
    arena a = {0};
    assert(!arena_init(NULL, 64));
    assert(!arena_init(&a, 0));
    assert(!arena_init(&a, SIZE_MAX));
    assert(!a.base);
    assert(arena_destroy(&a));
    assert(arena_init(&a, 64));
    void *base = a.base;
    assert(!arena_init(&a, 128));
    assert(a.base == base && a.mapped_size == 64);
    assert(!arena_alloc(&a, 0));
    assert(!arena_alloc(&a, SIZE_MAX));
    assert(!arena_alloc(&a, SIZE_MAX - 7));
    assert(!arena_alloc(&a, 65));
    assert(a.offset == 0);

    unsigned char *first = arena_alloc(&a, 1);
    unsigned char *second = arena_alloc(&a, 9);
    assert(first == base && second == first + 8);
    assert((uintptr_t)first % 8 == 0 && (uintptr_t)second % 8 == 0);
    assert(a.offset == 24);
    memset(first, 0xAA, 8);
    memset(second, 0xBB, 16);
    assert(arena_alloc(&a, 40) == first + 24);
    assert(!arena_alloc(&a, 1));
    assert(a.offset == 64);
    assert(arena_destroy(&a));
    assert(!a.base && a.offset == 0 && a.mapped_size == 0);
    assert(arena_destroy(&a));
    assert(!arena_alloc(&a, 8));
    assert(!arena_zero(&a) && !arena_reset(&a));
    assert(!arena_rewind(&a, 8) && !arena_rollback(&a));
    assert(arena_init(&a, 17));
    assert(arena_alloc(&a, 16));
    assert(!arena_alloc(&a, 1));
    assert(arena_destroy(&a));
    assert(arena_init(&a, 1));
    assert(!arena_alloc(&a, 1));
    assert(arena_destroy(&a));
}

static void rewind_and_zero(void)
{
    arena a = {0};
    assert(arena_init(&a, 64));
    assert(!arena_rollback(&a));
    unsigned char *first = arena_alloc(&a, 16);
    unsigned char *second = arena_alloc(&a, 16);
    memset(first, 0xAA, 16);
    memset(second, 0xBB, 16);
    assert(arena_rollback(&a));
    assert(a.offset == 16);
    for (size_t i = 0; i < 16; ++i) {
        assert(first[i] == 0xAA && second[i] == 0);
    }
    assert(!arena_rollback(&a));
    assert(arena_alloc(&a, 16) == second);
    assert(!arena_rewind(&a, 33));
    assert(!arena_rewind(&a, SIZE_MAX));
    assert(!arena_rewind(&a, 0));
    assert(a.offset == 32);
    assert(arena_rewind(&a, 17));
    assert(a.offset == 8 && a.prev_offset == 8);
    assert(!arena_rollback(&a));
    assert(arena_zero(&a));
    assert(a.offset == 8);
    for (size_t i = 0; i < 64; ++i) {
        assert(((unsigned char *)a.base)[i] == 0);
    }
    assert(arena_reset(&a));
    assert(a.offset == 0 && a.prev_offset == 0);
    assert(arena_alloc(&a, 64) == first);
    assert(arena_reset(&a));
    assert(arena_destroy(&a));
}

static void file_mapping(void)
{
    arena a = {0};
    assert(!arena_init_file(&a, NULL, 8193));
    assert(!arena_init_file(&a, "build/missing-directory/arena", 8193));
    assert(!arena_init_file(&a, "build", 8193));
    assert(!a.base);
    char path[] = "build/arena-file-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(arena_init_file(&a, path, 8193));
    assert(a.mapped_size == 8193);
    unsigned char *data = arena_alloc(&a, 8);
    assert(data);
    memcpy(data, "arena!", 7);
    assert(arena_destroy(&a));
    FILE *file = fopen(path, "rb");
    assert(file);
    unsigned char bytes[7];
    assert(fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    assert(memcmp(bytes, "arena!", 7) == 0);
    assert(fseek(file, 0, SEEK_END) == 0);
    assert(ftell(file) == 8193);
    assert(fclose(file) == 0);
    assert(unlink(path) == 0);
    assert(arena_destroy(&a));
}

int main(void)
{
    lifecycle_and_bounds();
    rewind_and_zero();
    file_mapping();
    puts("basic tests passed");
    return 0;
}
