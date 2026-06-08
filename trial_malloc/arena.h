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

typedef struct{
  size_t* position;
  size_t* size;
  void* base_ptr;
  uint8_t counter;
}partial_list;

void  arena_init(arena* cur_arena, const char* path, size_t length);
void* borrow_mem(arena* parent_arena, size_t length, uint8_t addr_offset);
void   rollback(arena* cur_arena, int prev_flg, size_t rollback_len);
void   empty(arena* cur_arena);
void   free_arena(arena* end_arena);
void create_partial_list(arena* cur_arena, partial_list* addr_list, uint8_t limit);
void add_partial_list(arena* cur_arena, partial_list* cur_list, void* ptr, size_t length);
void list_merge(partial_list* cur_list);
void* get_partial_block(partial_list* cur_list, size_t length);
#endif
