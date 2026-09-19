#ifndef TRIAL_MALLOC_ARENA_H
#define TRIAL_MALLOC_ARENA_H

#include <stdbool.h>
#include <stddef.h>

typedef struct partial_list partial_list;

/* Initialize with {0}. Treat fields as read-only; do not copy a live arena. */
typedef struct {
    void *base;
    size_t mapped_size;
    size_t usable_size; /* Back allocations lower this boundary. */
    size_t offset;
    size_t prev_offset;
    partial_list *list;
} arena;

/* Initialization rejects a live arena. Failure leaves an empty arena unchanged. */
bool arena_init(arena *a, size_t length);
/* Creates/resizes path; the shared mapping survives closing its descriptor. */
bool arena_init_file(arena *a, const char *path, size_t length);
void *arena_alloc(arena *a, size_t length);
void *arena_alloc_back(arena *a, size_t length);
/* Rewinds zero discarded bytes. Rollback is one-step, not an undo stack.
 * Both are rejected while a partial list is attached, even if empty. */
bool arena_rollback(arena *a);
bool arena_rewind(arena *a, size_t length);
/* Zero preserves offsets; reset discards front allocations and clears the list.
 * Both preserve back reservations, including metadata and its capacity. */
bool arena_zero(arena *a);
bool arena_reset(arena *a);
/* Repeated destruction is safe. On munmap failure the arena remains live. */
bool arena_destroy(arena *a);

/* External descriptor: initialize with {0}; keep it alive until arena_destroy.
 * The arrays live in the arena. Entries are sorted, disjoint and non-adjacent.
 * Do not modify fields or copy a live descriptor. Only one list per arena. */
struct partial_list {
    size_t *position;
    size_t *size;
    size_t counter;
    size_t capacity;
    arena *owner;
};

bool create_partial_list(arena *a, partial_list *list, size_t limit);
/* Release an owned, complete front allocation using its original byte length.
 * Bounds/overlap are checked; allocation ownership cannot be inferred. */
bool add_partial_list(arena *a, partial_list *list, void *ptr, size_t length);
/* Exact fit first, then first larger range. No bump-allocation fallback. */
void *get_partial_block(partial_list *list, size_t length);

#endif
