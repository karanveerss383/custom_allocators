#include "arena.h"
#include "stdio.h"

int main() {
    arena a = arena_init(NULL, 5096);
    int *nums = borrow_mem(&a, sizeof(int) * 10);
    for(int i = 0; i < 16; i++) { nums[i] = i*i; printf("%d\n", nums[i]); }
    rollback(&a, 0, 6);
    printf("\n\n\t%d", nums[4]);
    free_arena(&a);
    return 0;
}
