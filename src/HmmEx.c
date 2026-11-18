/*
 * File : HmmEx.c
 * Desc : Implementations of hmmCalloc, hmmReAlloc, and hmmSize
 */

#include <string.h>
#include "Hmm.h"
#include "hmm_int.h"

size_t hmmSize(void *ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    block_header_t *block = (block_header_t *)ptr - 1;
    return block->size;
}

void *hmmCalloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL; // Prevent integer multiplication overflow 
    }

    size_t total = nmemb * size;
    void *ptr = HmmAlloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *hmmReAlloc(void *ptr, size_t reqSize)
{
    if (ptr == NULL) {
        return HmmAlloc(reqSize);
    }
    if (reqSize == 0) {
        HmmFree(ptr);
        return NULL;
    }

    block_header_t *block = (block_header_t *)ptr - 1;
    size_t aligned_req = ALIGN(reqSize);

    /* if cur. block already fits */
    if (block->size >= aligned_req) {
        return ptr;
    }

    /* Check if merging with an adjacent free block satisfies reqSize */
    block_header_t *next = hmm_next_phys(block);
    if (next != NULL && next->is_free &&
        (block->size + sizeof(block_header_t) + next->size) >= aligned_req) {
        hmm_freelist_remove(next);
        block->size += sizeof(block_header_t) + next->size;

        block_header_t *after = hmm_next_phys(block);
        if (after != NULL) {
            after->phys_prev = block;
        } else {
            hmm_last_block = block;
        }
        return ptr;
    }

    /* Allocate a new block, copy data, and free the old block */
    void *new_ptr = HmmAlloc(reqSize);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_bytes = (block->size < reqSize) ? block->size : reqSize;
    memcpy(new_ptr, ptr, copy_bytes);
    HmmFree(ptr);
    return new_ptr;
}
