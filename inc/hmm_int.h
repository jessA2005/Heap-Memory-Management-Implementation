/*
 * File : hmm_int.h
 * Desc : Int. block metadata, const, and freelist helpers for HMM
 */

#ifndef HMM_INTERNAL_H
#define HMM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* 16-byte alignment */
#define ALIGNMENT         16UL
#define ALIGN(size)       (((size) + (ALIGNMENT - 1UL)) & ~(ALIGNMENT - 1UL))

/* Batch sbrk() calls in 128 KB chunks to minimize syscall overhead */
#define SBRK_CHUNK_SIZE   (128UL * 1024UL)
#define SHRINK_THRESHOLD  (128UL * 1024UL)
#define MIN_SPLIT_PAYLOAD 16UL

/* Padded to 48 bytes (a multiple of 16) so payload pointers stay 16-byte aligned */
typedef struct block_header {
    size_t size;                    // Payload size in bytes (excluding header)
    size_t is_free;                 // 1 = free, 0 = allocated 
    struct block_header *prev;     // Previous node in explicit free list 
    struct block_header *next;      // Next node in explicit free list 
    struct block_header *phys_prev; // Physically adjacent block lower in memory 
    size_t _pad;                    // Explicit padding to 48 bytes 
} block_header_t;

/* Shared heap state def. in HmmAlloc.c */
extern block_header_t *hmm_free_list_head;
extern block_header_t *hmm_last_block;
extern void *hmm_heap_start;
extern void *hmm_heap_end;
extern void *hmm_raw_heap_start;

static inline void hmm_freelist_insert(block_header_t *block)
{
    block->prev = NULL;
    block->next = hmm_free_list_head;
    if (hmm_free_list_head != NULL) {
        hmm_free_list_head->prev = block;
    }
    hmm_free_list_head = block;
}

static inline void hmm_freelist_remove(block_header_t *block)
{
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        hmm_free_list_head = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    block->prev = NULL;
    block->next = NULL;
}

static inline block_header_t *hmm_next_phys(block_header_t *block)
{
    char *next_addr = (char *)(block + 1) + block->size;
    if ((void *)next_addr >= hmm_heap_end) {
        return NULL;
    }
    return (block_header_t *)next_addr;
}

#endif /* HMM_INTERNAL_H */
