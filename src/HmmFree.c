/*
    File : HmmFree.c
    Desc : Heap Memory Manager free function stub
*/

#define _DEFAULT_SOURCE
#include <unistd.h>
#include "Hmm.h"
#include "hmm_int.h"

void HmmFree(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    block_header_t *block = (block_header_t *)ptr - 1;
    if (block->is_free) {
        return; /* Guard against double-free */
    }

    block->is_free = 1;

    /*  Forward coalescing with physically next block */
    block_header_t *next = hmm_next_phys(block);
    if (next != NULL && next->is_free) {
        hmm_freelist_remove(next);
        block->size += sizeof(block_header_t) + next->size;

        block_header_t *after = hmm_next_phys(block);
        if (after != NULL) {
            after->phys_prev = block;
        } else {
            hmm_last_block = block;
        }
    }

    /*  Backward coalescing with physically previous block */
    block_header_t *prev = block->phys_prev;
    if (prev != NULL && prev->is_free) {
        hmm_freelist_remove(prev);
        prev->size += sizeof(block_header_t) + block->size;

        block_header_t *after = hmm_next_phys(prev);
        if (after != NULL) {
            after->phys_prev = prev;
        } else {
            hmm_last_block = prev;
        }
        block = prev;
    }

    hmm_freelist_insert(block);

    /*  Shrink program break in SBRK_CHUNK_SIZE multiples if top block > threshold */
    if (block == hmm_last_block && block->size >= SHRINK_THRESHOLD) {
        if (block->phys_prev == NULL) {
          /* Entire heap is free?  restore sbrk(0) to the exact initial break */
            size_t total_bytes = (size_t)((char *)hmm_heap_end - (char *)hmm_raw_heap_start);
            if (sbrk(-(intptr_t)total_bytes) != (void *)-1) {
                hmm_freelist_remove(block);
                hmm_last_block     = NULL;
                hmm_heap_start     = NULL;
                hmm_heap_end       = NULL;
                hmm_raw_heap_start = NULL; 
	}
        } else {
            size_t release_bytes = (block->size / SBRK_CHUNK_SIZE) * SBRK_CHUNK_SIZE;
            if (release_bytes > 0 && sbrk(-(intptr_t)release_bytes) != (void *)-1) {
                block->size -= release_bytes;
                hmm_heap_end = (char *)hmm_heap_end - release_bytes;
            }
        }
    }
}
