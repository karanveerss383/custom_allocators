#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "arena.h"
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

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

void* borrow_mem(arena* parent_arena, size_t length){
  
  size_t aligned_length = MEMALIGN(length,8);
  if ((aligned_length != 0) && ((parent_arena->size - parent_arena->offset) >= aligned_length)){
    parent_arena->prev_offset = parent_arena->offset;
    parent_arena->offset += aligned_length;
    return (void*)((uint8_t*)parent_arena->base + parent_arena->prev_offset);
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

