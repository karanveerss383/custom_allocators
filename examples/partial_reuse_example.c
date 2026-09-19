#include "arena.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    arena a = {0};
    partial_list free_list = {0};
    if (!arena_init(&a, 64 * 1024 * 1024)) {
        return 1;
    }
    if (!create_partial_list(&a, &free_list, 4096)) {
        arena_destroy(&a);
        return 1;
    }
    void *block = arena_alloc(&a, 128);
    if (!block) {
        arena_destroy(&a);
        return 1;
    }
    memset(block, 0xAA, 128);
    if (!add_partial_list(&a, &free_list, block, 128)) {
        arena_destroy(&a);
        return 1;
    }
    /* Reuse is explicit; callers can fall back to arena_alloc on a miss. */
    void *reused = get_partial_block(&free_list, 48);
    if (!reused) {
        arena_destroy(&a);
        return 1;
    }
    printf("Hybrid arena: reused 48 bytes, %zu bytes remain in the free range\n",
           free_list.size[0]);
    return arena_destroy(&a) ? 0 : 1;
}
