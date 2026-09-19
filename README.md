# C arena allocators

An educational systems-programming project exploring virtual memory, manual
allocation, 8-byte alignment, front allocation, rollback and zeroing. Two separate
implementations show the progression from a bump allocator to an arena with
limited individual block reuse. Neither is a general replacement for `malloc`.

## Two versions

[`arena/`](arena/) is the basic version: one fixed `mmap` mapping, a front offset,
and a previous offset for one-step rollback. It has no individual release or back
allocation API.

```text
Basic arena
base                                                mapped_size
 ↓                                                       ↓
┌──────────────────────────┬──────────────────────────────┐
│ used front allocations → │ unused memory                │
└──────────────────────────┴──────────────────────────────┘
                           offset
```

[`trial_malloc/`](trial_malloc/) adds back allocation and a bounded partial list.
Two arrays store each free range's byte offset and aligned length. Released ranges
are kept sorted and coalesced automatically. Reuse checks exact matches first,
then splits the first larger range. Searches and insertion are linear in the
number of tracked ranges.

```text
Hybrid arena
base                                                mapped_size
 ↓                                                       ↓
┌─────────────────────┬──────────────────┬────────────────┐
│ front allocations → │ unused memory    │ ← back reserves│
│ [live][free][live]   │                  │ metadata, etc. │
└─────────────────────┴──────────────────┴────────────────┘
                      offset             usable_size
```

The hybrid invariant is `offset <= usable_size <= mapped_size`. Back allocations
lower `usable_size`; they never change the length used by `munmap`.

The caller explicitly selects the maximum number of tracked free ranges:

```c
create_partial_list(&a, &free_list, 4096);
```

This reserves two arrays of **4096 `size_t` entries**, with at most seven padding
bytes per array (64 KiB total on a system with 8-byte `size_t`). Capacity is never
computed from the number of possible 8-byte fragments. A release needing a new
slot fails when `counter >= capacity`; a release that merges with an existing
neighbor can still succeed at capacity. Failed releases leave the payload live
and unchanged.

## Basic usage

```c
#include "arena.h"

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
    /* Use values. */
    return arena_destroy(&a) ? 0 : 1;
}
```

## Partial reuse usage

```c
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
if (!block || !add_partial_list(&a, &free_list, block, 128)) {
    arena_destroy(&a);
    return 1;
}
void *reused = get_partial_block(&free_list, 48); /* Leaves an 80-byte free range. */
if (!reused) {
    reused = arena_alloc(&a, 48);               /* Explicit bump fallback. */
}
if (!reused) {
    arena_destroy(&a);
    return 1;
}
/* Use reused; block was invalidated by its release. */
return arena_destroy(&a) ? 0 : 1;

