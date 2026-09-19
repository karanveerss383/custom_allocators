#include "arena.h"

#include <stdio.h>

int main(void)
{
    arena a = {0};
    if (!arena_init(&a, 64 * 1024 * 1024)) {
        return 1;
    }
    int *values = arena_alloc(&a, 1000 * sizeof(*values));
    if (!values) {
        arena_destroy(&a);
        return 1;
    }
    for (int i = 0; i < 1000; ++i) {
        values[i] = i;
    }
    printf("Basic arena: %d values, last = %d\n", 1000, values[999]);
    return arena_destroy(&a) ? 0 : 1;
}
