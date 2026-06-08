#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include "arena.h"

#define PAGESIZE 4096
#define MEMALIGN(size, align_size) (size + (align_size-1)) & ~(align_size-1)

void arena_init(arena* inital_arena, const char* path,size_t length){

  inital_arena->size = MEMALIGN(length, PAGESIZE);
  inital_arena->offset = 0;
  inital_arena->prev_offset = 0;
  if (path == NULL){
    inital_arena->fd = -1;
    inital_arena->base = mmap(NULL, inital_arena->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, inital_arena->fd, 0);
  }
  else{
    inital_arena->fd = open((char*)path, O_RDWR | O_CREAT, 0644);
    if (inital_arena->fd == -1) { perror("open"); }
    ftruncate(inital_arena->fd,inital_arena->size);
    inital_arena->base = mmap(NULL, inital_arena->size, PROT_READ | PROT_WRITE, MAP_SHARED, inital_arena->fd, 0);
  }
  if (inital_arena->base == MAP_FAILED) { perror("fail"); }
}

void* borrow_mem(arena* parent_arena, size_t length, uint8_t addr_offset){
  
  size_t aligned_length = MEMALIGN(length,8);
  if ((aligned_length != 0) && ((parent_arena->size - parent_arena->offset) >= aligned_length)){
    if (addr_offset == 0){
      parent_arena->prev_offset = parent_arena->offset;
      parent_arena->offset += aligned_length;
      return (void*)((uint8_t*)parent_arena->base + parent_arena->prev_offset);
    }
    else if (addr_offset == 1) {
      parent_arena->size -= aligned_length;
      return (void*) ((uint8_t*)parent_arena->base + parent_arena->size - aligned_length);
    }
  }

  else{
    return NULL;
  }

}

void rollback(arena* cur_arena, int prev_flg, size_t rollback_len){

  if (prev_flg == 1) {

    memset(((uint8_t*)cur_arena->base + cur_arena->prev_offset), 0, (cur_arena->offset - cur_arena->prev_offset));
    cur_arena->offset = cur_arena->prev_offset;
    return;
  }
  size_t aligned_len = MEMALIGN(rollback_len, 8);

  if (aligned_len > cur_arena->offset) { 

    cur_arena->offset = 0;
    empty(cur_arena);
    return; 
  }

  cur_arena->offset -= aligned_len;
  memset(((uint8_t*)cur_arena->base + cur_arena->offset), 0, aligned_len);
}

void empty(arena* cur_arena){
  memset(cur_arena->base, 0, cur_arena->size);
}

void free_arena(arena* end_arena){
  munmap(end_arena->base, end_arena->size);
  if (end_arena->fd != -1) {close(end_arena->fd);}
}

void create_partial_list(arena* cur_arena, partial_list* addr_list, uint8_t limit){
  addr_list->base_ptr = cur_arena->base;
  addr_list->counter = 0;
  addr_list->position = borrow_mem(cur_arena, sizeof(size_t)*limit, 1);
  addr_list->size = borrow_mem(cur_arena, sizeof(size_t)*limit, 1);
  memset(addr_list->position, 0, limit*sizeof(size_t));
  memset(addr_list->size, 0, limit*sizeof(size_t));
}

void add_partial_list(arena* cur_arena, partial_list* cur_list, void* ptr, size_t length){
  size_t aligned_length = MEMALIGN(length, 8);
  memset(ptr, 0, aligned_length);
  cur_list->position[cur_list->counter] = (size_t)((uint8_t*)ptr - (uint8_t*)cur_arena->base);
  cur_list->size[cur_list->counter] = aligned_length;
  cur_list->counter += 1;
}

void list_merge(partial_list* cur_list){
  if (cur_list->size[0] == 0 | cur_list->size[1] == 0){
    printf("nothing to merge");
  }
  else {
    for (int i = 0; i < cur_list->counter; i++){
      int block_end = cur_list->position[i] + cur_list->size[i];

      for (int j = (i+1); j < cur_list->counter; j++){

        if ((block_end) == cur_list->position[j]) {
          cur_list->size[i] += cur_list->size[j];
          cur_list->counter--;

          for (int k = j; k < cur_list->counter-1; k++){
            cur_list->position[k] = cur_list->position[(k+1)];
            cur_list->size[k] = cur_list->size[(k+1)];
          }
          j--;
        }
      }
    }
  }
}

void* get_partial_block(partial_list* cur_list, size_t length){
  size_t aligned_length = MEMALIGN(length, 8);
  for(int i = 0; i < cur_list->counter; i++){
    if (cur_list->size[i] == aligned_length){
      return (void*)((uint8_t*)cur_list->base_ptr + cur_list->position[i]);
    }
  }
  perror("No match found");
  return NULL;
}
