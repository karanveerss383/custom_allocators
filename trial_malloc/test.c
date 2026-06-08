#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "arena.h"

void main() {
    // arena_init anonymous
    arena a1;
    arena_init(&a1, NULL, 4096);

    // arena_init file-backed
    arena a2;
    arena_init(&a2, "/tmp/test_arena.bin", 8192);

    // borrow_mem front
    void* p1 = borrow_mem(&a1, 64, 0);
    memset(p1, 0xAA, 64);

    // borrow_mem back
    void* p2 = borrow_mem(&a1, 64, 1);
    memset(p2, 0xBB, 64);

    // borrow_mem multiple sequential
    void* p3 = borrow_mem(&a1, 32, 0);
    void* p4 = borrow_mem(&a1, 32, 0);
    void* p5 = borrow_mem(&a1, 32, 0);
    memset(p3, 0x11, 32);
    memset(p4, 0x22, 32);
    memset(p5, 0x33, 32);

    // borrow_mem overflow returns NULL
    void* pnull = borrow_mem(&a1, 99999, 0);
    printf("overflow borrow (expect NULL): %s\n", pnull == NULL ? "NULL" : "NOT NULL");

    // rollback by prev_offset
    arena a3;
    arena_init(&a3, NULL, 4096);
    void* r1 = borrow_mem(&a3, 64, 0);
    memset(r1, 0xFF, 64);
    void* r2 = borrow_mem(&a3, 128, 0);
    memset(r2, 0xFF, 128);
    rollback(&a3, 1, 0);
    printf("rollback prev_flg=1 offset (expect 64): %zu\n", a3.offset);

    // rollback by length
    void* r3 = borrow_mem(&a3, 128, 0);
    memset(r3, 0xFF, 128);
    rollback(&a3, 0, 128);
    printf("rollback by len offset (expect 64): %zu\n", a3.offset);

    // rollback overflow clamps to zero
    rollback(&a3, 0, 99999);
    printf("rollback overflow offset (expect 0): %zu\n", a3.offset);

    // empty
    arena a4;
    arena_init(&a4, NULL, 4096);
    void* e1 = borrow_mem(&a4, 256, 0);
    memset(e1, 0xBE, 256);
    empty(&a4);
    uint8_t first = *((uint8_t*)a4.base);
    printf("empty first byte (expect 0): %d\n", first);

    // create_partial_list
    arena a5;
    arena_init(&a5, NULL, 4096);
    partial_list pl;
    create_partial_list(&a5, &pl, 10);
    printf("partial_list counter after create (expect 0): %d\n", pl.counter);

    // add_partial_list
    void* b1 = borrow_mem(&a5, 64, 0);
    void* b2 = borrow_mem(&a5, 64, 0);
    void* b3 = borrow_mem(&a5, 64, 0);
    memset(b1, 0xFF, 64);
    memset(b2, 0xFF, 64);
    memset(b3, 0xFF, 64);
    add_partial_list(&a5, &pl, b1, 64);
    add_partial_list(&a5, &pl, b2, 64);
    add_partial_list(&a5, &pl, b3, 64);
    printf("partial_list counter after 3 adds (expect 3): %d\n", pl.counter);

    // list_merge
    list_merge(&pl);
    printf("partial_list counter after merge (expect 1): %d\n", pl.counter);

    // get_partial_block
    arena a6;
    arena_init(&a6, NULL, 4096);
    partial_list pl2;
    create_partial_list(&a6, &pl2, 10);
    void* c1 = borrow_mem(&a6, 64, 0);
    void* c2 = borrow_mem(&a6, 128, 0);
    add_partial_list(&a6, &pl2, c1, 64);
    add_partial_list(&a6, &pl2, c2, 128);
    void* found1 = get_partial_block(&pl2, 64);
    void* found2 = get_partial_block(&pl2, 128);
    printf("get_partial_block 64  (expect match): %s\n", found1 == c1 ? "match" : "no match");
    printf("get_partial_block 128 (expect match): %s\n", found2 == c2 ? "match" : "no match");

    // free_arena
    free_arena(&a1);
    free_arena(&a2);
    free_arena(&a3);
    free_arena(&a4);
    free_arena(&a5);
    free_arena(&a6);
    printf("free_arena: done\n");
}
