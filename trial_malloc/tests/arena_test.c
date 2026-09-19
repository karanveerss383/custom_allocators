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

static void back_and_capacity(void)
{
    arena a = {0};
    partial_list list = {0};
    assert(arena_init(&a, 65));
    unsigned char *front = arena_alloc(&a, 8);
    unsigned char *back = arena_alloc_back(&a, 9);
    assert(back == front + 48);
    assert((uintptr_t)back % 8 == 0);
    assert(a.mapped_size == 65 && a.usable_size == 48);
    memset(back, 0xCC, 16);
    assert(!arena_alloc_back(&a, 0));
    assert(!arena_alloc_back(&a, SIZE_MAX));
    assert(!arena_alloc_back(&a, 41));
    assert(arena_alloc(&a, 40) == front + 8);
    assert(!arena_alloc_back(&a, 1) && !arena_alloc(&a, 1));
    assert(arena_reset(&a));
    for (size_t i = 0; i < 16; ++i) {
        assert(back[i] == 0xCC);
    }
    assert(a.usable_size == 48);
    assert(arena_destroy(&a));

    assert(arena_init(&a, 1024));
    size_t available = a.usable_size;
    assert(!create_partial_list(&a, &list, 0));
    assert(!create_partial_list(&a, &list, SIZE_MAX));
    assert(!create_partial_list(&a, &list, SIZE_MAX / sizeof(size_t)));
    assert(!create_partial_list(&a, &list, 1024));
    assert(a.usable_size == available && !a.list && !list.owner);
    assert(create_partial_list(&a, &list, 2));
    assert(list.capacity == 2 && list.counter == 0);
    assert(a.usable_size == available - 4 * sizeof(size_t));
    assert(!create_partial_list(&a, &list, 2));
    partial_list other = {0};
    assert(!create_partial_list(&a, &other, 2));
    unsigned char *blocks[5];
    for (size_t i = 0; i < 5; ++i) {
        blocks[i] = arena_alloc(&a, 16);
        assert(blocks[i]);
        memset(blocks[i], 0xAB, 16);
    }
    assert(add_partial_list(&a, &list, blocks[0], 16));
    assert(add_partial_list(&a, &list, blocks[2], 16));
    assert(list.counter == list.capacity);
    assert(!add_partial_list(&a, &list, blocks[4], 16));
    assert(blocks[4][0] == 0xAB && list.counter == 2);
    /* Bridging two neighbors succeeds at capacity and frees one slot. */
    assert(add_partial_list(&a, &list, blocks[1], 16));
    assert(list.counter == 1 && list.position[0] == 0 && list.size[0] == 48);
    assert(add_partial_list(&a, &list, blocks[4], 16));
    assert(add_partial_list(&a, &list, blocks[3], 16));
    assert(list.counter == 1 && list.size[0] == 80);
    assert(!arena_rollback(&a) && !arena_rewind(&a, 16));
    assert(arena_zero(&a));
    assert(list.counter == 1 && list.size[0] == 80);
    assert(arena_reset(&a));
    assert(list.counter == 0 && list.capacity == 2);
    assert(!get_partial_block(&list, 16));
    assert(!arena_rollback(&a) && !arena_rewind(&a, 16));
    assert(arena_alloc(&a, 80) == blocks[0]);
    assert(arena_destroy(&a));
    assert(!list.owner && !list.position && !list.size && !list.capacity);
    assert(!get_partial_block(&list, 16));
    assert(arena_destroy(&a));

    assert(arena_init(&a, 128));
    assert(create_partial_list(&a, &list, 1));
    void *p = arena_alloc(&a, 8);
    assert(p && add_partial_list(&a, &list, p, 8));
    assert(list.counter == 1);
    assert(get_partial_block(&list, 8) == p);
    assert(list.counter == 0 && !get_partial_block(&list, 8));
    assert(arena_destroy(&a));
}

static void exact_and_split(void)
{
    arena a = {0};
    partial_list list = {0};
    assert(arena_init(&a, 1024));
    assert(create_partial_list(&a, &list, 8));
    unsigned char *large = arena_alloc(&a, 128);
    unsigned char *guard = arena_alloc(&a, 8);
    unsigned char *exact = arena_alloc(&a, 48);
    assert(large && guard && exact);
    memset(guard, 0xCC, 8);
    assert(add_partial_list(&a, &list, large, 128));
    assert(add_partial_list(&a, &list, exact, 48));
    assert(get_partial_block(&list, 47) == exact); /* Exact fit has priority. */
    assert(list.counter == 1);
    memset(exact, 0xEE, 48);
    assert(get_partial_block(&list, 48) == large);
    assert(list.position[0] == 48 && list.size[0] == 80);
    memset(large, 0xAA, 48);
    assert(get_partial_block(&list, 80) == large + 48);
    assert(list.counter == 0 && !get_partial_block(&list, 8));
    for (size_t i = 0; i < 48; ++i) {
        assert(exact[i] == 0xEE && large[i] == 0xAA);
    }
    for (size_t i = 0; i < 8; ++i) {
        assert(guard[i] == 0xCC);
    }
    assert(!get_partial_block(&list, 0));
    assert(!get_partial_block(&list, SIZE_MAX));
    assert(arena_destroy(&a));
}

static void merge_orders(void)
{
    const size_t orders[][5] = {
        {0, 1, 2, 3, 4}, {4, 3, 2, 1, 0}, {0, 4, 2, 1, 3}
    };
    for (size_t order = 0; order < 3; ++order) {
        arena a = {0};
        partial_list list = {0};
        assert(arena_init(&a, 1024));
        assert(create_partial_list(&a, &list, 5));
        void *blocks[5];
        for (size_t i = 0; i < 5; ++i) {
            blocks[i] = arena_alloc(&a, (i + 1) * 8);
            assert(blocks[i]);
        }
        for (size_t i = 0; i < 5; ++i) {
            size_t index = orders[order][i];
            assert(add_partial_list(&a, &list, blocks[index], (index + 1) * 8));
        }
        assert(list.counter == 1 && list.position[0] == 0 && list.size[0] == 120);
        assert(get_partial_block(&list, 120) == blocks[0]);
        assert(!get_partial_block(&list, 8));
        assert(arena_destroy(&a));
    }
}

static void invalid_releases(void)
{
    arena a = {0}, foreign = {0};
    partial_list list = {0}, wrong = {0};
    assert(arena_init(&a, 1024) && arena_init(&foreign, 1024));
    assert(create_partial_list(&a, &list, 4));
    unsigned char *p = arena_alloc(&a, 32);
    assert(p);
    memset(p, 0xAB, 32);
    int stack_value = 0;
    assert(!add_partial_list(&a, &list, &stack_value, 8));
    assert(!add_partial_list(&a, &list, foreign.base, 8));
    assert(!add_partial_list(&foreign, &list, p, 8));
    assert(!add_partial_list(&a, &wrong, p, 8));
    assert(!add_partial_list(&a, &list, NULL, 8));
    assert(!add_partial_list(&a, &list, p, 0));
    assert(!add_partial_list(&a, &list, p, SIZE_MAX));
    assert(!add_partial_list(&a, &list, p, SIZE_MAX - 7));
    assert(!add_partial_list(&a, &list, p + 1, 8));
    assert(!add_partial_list(&a, &list, p + 32, 8));
    assert(!add_partial_list(&a, &list, p + 24, 16));
    assert(!add_partial_list(&a, &list, list.position, 8));
    assert(!add_partial_list(&a, &list, p + a.mapped_size, 8));
    assert(!add_partial_list(&a, &list, (void *)((uintptr_t)p - 8), 8));
    assert(p[0] == 0xAB && list.counter == 0);
    assert(add_partial_list(&a, &list, p, 32));
    assert(!add_partial_list(&a, &list, p, 32));
    assert(!add_partial_list(&a, &list, p + 8, 8));
    assert(list.counter == 1 && list.size[0] == 32);
    for (size_t i = 0; i < 32; ++i) {
        assert(p[i] == 0);
    }
    assert(arena_destroy(&foreign) && arena_destroy(&a));
}

static void many_blocks(void)
{
    enum { COUNT = 600 };
    arena a = {0};
    partial_list list = {0};
    assert(arena_init(&a, 128 * 1024));
    assert(create_partial_list(&a, &list, 4096));
    assert(list.capacity == 4096);
    assert(a.mapped_size - a.usable_size == 8192 * sizeof(size_t));
    unsigned char *blocks[COUNT];
    for (size_t i = 0; i < COUNT; ++i) {
        blocks[i] = arena_alloc(&a, 8);
        assert(blocks[i]);
        memset(blocks[i], (int)(i % 251 + 1), 8);
    }
    for (size_t i = 0; i < COUNT; i += 2) {
        assert(add_partial_list(&a, &list, blocks[i], 8));
    }
    assert(list.counter == COUNT / 2); /* Also catches the old uint8_t counter. */
    for (size_t i = 0; i < COUNT; i += 2) {
        assert(get_partial_block(&list, 8) == blocks[i]);
        memset(blocks[i], 0xAA, 8);
    }
    assert(!get_partial_block(&list, 8) && list.counter == 0);
    for (size_t i = 1; i < COUNT; i += 2) {
        for (size_t j = 0; j < 8; ++j) {
            assert(blocks[i][j] == (unsigned char)(i % 251 + 1));
        }
    }
    for (size_t i = COUNT; i > 0; --i) {
        assert(add_partial_list(&a, &list, blocks[i - 1], 8));
    }
    assert(list.counter == 1 && list.size[0] == COUNT * 8);
    assert(get_partial_block(&list, COUNT * 8) == blocks[0]);
    assert(!get_partial_block(&list, 8));
    assert(arena_destroy(&a));
}

static void mixed_lifetimes(void)
{
    enum { SLOTS = 128 };
    struct {
        unsigned char *ptr;
        size_t length;
        unsigned char value;
    } live[SLOTS] = {0};
    arena a = {0};
    partial_list list = {0};
    assert(arena_init(&a, 64 * 1024));
    assert(create_partial_list(&a, &list, 32));
    uint32_t random = 42;
    for (size_t step = 0; step < 5000; ++step) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        size_t slot = (random >> 16) % SLOTS;
        if (live[slot].ptr) {
            if (add_partial_list(&a, &list, live[slot].ptr, live[slot].length)) {
                live[slot].ptr = NULL;
            }
        } else {
            size_t length = (random >> 24) % 128 + 1;
            unsigned char *ptr = get_partial_block(&list, length);
            if (!ptr) {
                ptr = arena_alloc(&a, length);
            }
            if (ptr) {
                size_t aligned = (length + 7) & ~(size_t)7;
                size_t start = (size_t)(ptr - (unsigned char *)a.base);
                assert((uintptr_t)ptr % 8 == 0);
                assert(start <= a.offset && aligned <= a.offset - start);
                for (size_t i = 0; i < SLOTS; ++i) {
                    if (live[i].ptr) {
                        size_t other = (size_t)(live[i].ptr - (unsigned char *)a.base);
                        size_t other_size = (live[i].length + 7) & ~(size_t)7;
                        assert(start + aligned <= other || other + other_size <= start);
                    }
                }
                live[slot].ptr = ptr;
                live[slot].length = length;
                live[slot].value = (unsigned char)(slot + 1);
                memset(ptr, live[slot].value, length);
            }
        }
        /* An independent set of live payloads detects aliases and releases
         * that damage another allocation, including failed releases. */
        for (size_t i = 0; i < SLOTS; ++i) {
            if (live[i].ptr) {
                for (size_t j = 0; j < live[i].length; ++j) {
                    assert(live[i].ptr[j] == live[i].value);
                }
            }
        }
        assert(list.counter <= list.capacity);
        for (size_t i = 0; i < list.counter; ++i) {
            assert(list.position[i] % 8 == 0 && list.size[i] % 8 == 0);
            assert(list.size[i] > 0 && list.position[i] <= a.offset);
            assert(list.size[i] <= a.offset - list.position[i]);
            if (i) {
                assert(list.position[i - 1] + list.size[i - 1] < list.position[i]);
            }
        }
    }
    assert(arena_destroy(&a));
}

int main(void)
{
    lifecycle_and_bounds();
    rewind_and_zero();
    file_mapping();
    back_and_capacity();
    exact_and_split();
    merge_orders();
    invalid_releases();
    many_blocks();
    mixed_lifetimes();
    puts("hybrid tests passed");
    return 0;
}
