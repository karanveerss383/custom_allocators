#ifndef BASIC_ARENA_H
#define BASIC_ARENA_H

#include <stdbool.h>
#include <stddef.h>

/* Initialize with {0}. Treat fields as read-only; do not copy a live arena. */
typedef struct {
    void *base;
    size_t mapped_size;
    size_t offset;
    size_t prev_offset;
} arena;

/* Initialization rejects a live arena. Failure leaves an empty arena unchanged. */
bool arena_init(arena *a, size_t length);
/* Creates/resizes path; the shared mapping survives closing its descriptor. */
bool arena_init_file(arena *a, const char *path, size_t length);
void *arena_alloc(arena *a, size_t length);
/* Rewinds zero the discarded bytes. Rollback is one-step, not an undo stack. */
bool arena_rollback(arena *a);
bool arena_rewind(arena *a, size_t length);
/* Zero preserves offsets; reset zeros and discards all front allocations. */
bool arena_zero(arena *a);
bool arena_reset(arena *a);
/* Repeated destruction is safe. On munmap failure the arena remains live. */
bool arena_destroy(arena *a);

#endif
