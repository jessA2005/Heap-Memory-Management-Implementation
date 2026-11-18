/*
 * File : HmmAlloc.c
 * Desc : Heap Memory Manager allocation implementation using sbrk()
 */

#define _DEFAULT_SOURCE
#include <unistd.h>
#include "Hmm.h"
#include "hmm_int.h"

block_header_t *hmm_free_list_head = NULL;
block_header_t *hmm_last_block     = NULL;
void *hmm_heap_start               = NULL;
void *hmm_heap_end                 = NULL;
void *hmm_raw_heap_start 	   = NULL;


static int init_heap_alignment(void)
{
    void *brk = sbrk(0);
    
    if (brk == (void *)-1) {
        return -1;
    }

  hmm_raw_heap_start = brk;  

    uintptr_t misalign = (uintptr_t)brk % ALIGNMENT;
    if (misalign != 0) {
        size_t pad = ALIGNMENT - misalign;
        if (sbrk((intptr_t)pad) == (void *)-1) {
            return -1;
        }
        brk = (char *)brk + pad; 
	//NOTE: could've just done sbrk(0) again but this wont minimize sbrk calls
    }

    hmm_heap_start = brk;
    hmm_heap_end   = brk;
    return 0;
}

static block_header_t *find_fit(size_t req_size)
{
    block_header_t *curr = hmm_free_list_head;
    while (curr != NULL) {
        if (curr->size >= req_size) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static void split_block(block_header_t *block, size_t req_size)
{
    if (block->size < req_size + sizeof(block_header_t) + MIN_SPLIT_PAYLOAD) {
        return;
    }

    size_t rem_size = block->size - req_size - sizeof(block_header_t);
    block_header_t *rem = (block_header_t *)((char *)(block + 1) + req_size);

    rem->size      = rem_size;
    rem->is_free   = 1;
    rem->prev      = NULL;
    rem->next      = NULL;
    rem->phys_prev = block;
    rem->_pad      = 0; //still unsure whether to leave it or not :/

    block->size = req_size;

    block_header_t *after = hmm_next_phys(rem);
    if (after != NULL) {
        after->phys_prev = rem;
    } else {
        hmm_last_block = rem;
    }

    hmm_freelist_insert(rem);
}

static block_header_t *expand_heap(size_t req_size)
{
    if (hmm_heap_start == NULL && init_heap_alignment() != 0) {
        return NULL;
    }

    /* If the top-most block is already free, extend it instead of creating a new header */
    if (hmm_last_block != NULL && hmm_last_block->is_free) {
        size_t deficit = req_size - hmm_last_block->size;
        size_t expand_bytes = ((deficit + SBRK_CHUNK_SIZE - 1UL) / SBRK_CHUNK_SIZE) * SBRK_CHUNK_SIZE;

        if (sbrk((intptr_t)expand_bytes) == (void *)-1) {
            return NULL;
        }

        hmm_heap_end = (char *)hmm_heap_end + expand_bytes;
        hmm_last_block->size += expand_bytes;
        return hmm_last_block;
    }

    size_t total_needed = req_size + sizeof(block_header_t);
    size_t expand_bytes = ((total_needed + SBRK_CHUNK_SIZE - 1UL) / SBRK_CHUNK_SIZE) * SBRK_CHUNK_SIZE;

    void *mem = sbrk((intptr_t)expand_bytes);
    if (mem == (void *)-1) {
        return NULL;
    }

    hmm_heap_end = (char *)hmm_heap_end + expand_bytes;

    block_header_t *new_block = (block_header_t *)mem;
    new_block->size      = expand_bytes - sizeof(block_header_t);
    new_block->is_free   = 1;
    new_block->prev      = NULL;
    new_block->next      = NULL;
    new_block->phys_prev = hmm_last_block;
    new_block->_pad      = 0; //unnecessary but feels clean

    hmm_last_block = new_block;
    hmm_freelist_insert(new_block);
    return new_block;
}

void *HmmAlloc(size_t size)
{
    /* Return a minimum-aligned valid block for size 0 (safe for LD_PRELOAD callers) */
    size_t req_size = (size == 0) ? ALIGNMENT : ALIGN(size);

    block_header_t *block = find_fit(req_size);
    if (block == NULL) {
        block = expand_heap(req_size);
        if (block == NULL) {
            return NULL;
        }
    }

    hmm_freelist_remove(block);
    block->is_free = 0;
    split_block(block, req_size);

    return (void *)(block + 1);
}
