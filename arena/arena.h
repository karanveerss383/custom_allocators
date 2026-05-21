#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>  // size_t

typedef struct {
    void*  base;
    size_t size;
    size_t offset;
    size_t prev_offset;
    int    fd;
} arena;

void  arena_init(arena* cur_arena, const char* path, size_t length);
void*  borrow_mem(arena* parent_arena, size_t length);
void   rollback(arena* cur_arena, int prev_flg, size_t rollback_len);
void   empty(arena* cur_arena);
void   free_arena(arena* end_arena);

#endif
