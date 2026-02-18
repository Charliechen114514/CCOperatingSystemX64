/* ==============================================================================
 * CCOS User Library - Standard Library Implementation
 * ==============================================================================
 *
 * Implementation of memory allocation and utility functions.
 *
 * ==============================================================================
 */

#include "stdlib.h"
#include "unistd.h"

/* ============================================================================
 * Simple Heap Implementation
 * ============================================================================ */

typedef struct heap_block {
    size_t size;                 /* Size of the block (including header) */
    bool used;                   /* Whether this block is in use */
    struct heap_block* next;     /* Next block in the free list */
} heap_block_t;

/* Heap start and current break */
static void* heap_start = NULL;
static heap_block_t* heap_free_list = NULL;

/**
 * @brief Initialize the heap
 */
static void heap_init(void) {
    if (heap_start == NULL) {
        /* Get initial break */
        heap_start = sbrk(0);
        void* new_brk = sbrk(16 * 1024);  /* Allocate 16KB initial heap */

        if (new_brk == (void*)-1) {
            return;  /* Failed */
        }

        /* Create first free block */
        heap_free_list = (heap_block_t*)heap_start;
        heap_free_list->size = 16 * 1024;
        heap_free_list->used = false;
        heap_free_list->next = NULL;
    }
}

/**
 * @brief Find a free block of at least the given size
 */
static heap_block_t* heap_find_free(size_t size) {
    heap_block_t* block = heap_free_list;

    while (block != NULL) {
        if (!block->used && block->size >= size) {
            return block;
        }
        block = block->next;
    }

    return NULL;
}

/**
 * @brief Split a block if it's large enough
 */
static void heap_split_block(heap_block_t* block, size_t size) {
    /* Check if we can split this block */
    size_t remaining = block->size - size;

    if (remaining >= sizeof(heap_block_t) + 16) {
        /* Create a new block for the remaining space */
        heap_block_t* new_block = (heap_block_t*)((char*)block + size);
        new_block->size = remaining;
        new_block->used = false;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    /* Align size to 16 bytes */
    size = (size + 15) & ~15;

    /* Add header size */
    size_t total_size = size + sizeof(heap_block_t);

    heap_init();

    /* Find a free block */
    heap_block_t* block = heap_find_free(total_size);

    if (block == NULL) {
        /* Need to expand the heap */
        void* current_brk = sbrk(0);
        size_t alloc_size = total_size;

        /* Allocate at least 4KB */
        if (alloc_size < 4096) {
            alloc_size = 4096;
        }

        /* Align to page size */
        alloc_size = (alloc_size + 4095) & ~4095;

        void* new_brk = sbrk(alloc_size);
        if (new_brk == (void*)-1) {
            return NULL;  /* Failed to expand heap */
        }

        /* Create a new block */
        block = (heap_block_t*)current_brk;
        block->size = alloc_size;
        block->used = false;
        block->next = NULL;

        /* Add to free list */
        if (heap_free_list == NULL) {
            heap_free_list = block;
        } else {
            /* Find the last block */
            heap_block_t* last = heap_free_list;
            while (last->next != NULL) {
                last = last->next;
            }
            last->next = block;
        }
    }

    /* Mark block as used */
    block->used = true;

    /* Split if necessary */
    heap_split_block(block, total_size);

    /* Return pointer to data area */
    return (void*)((char*)block + sizeof(heap_block_t));
}

void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    /* Get the block header */
    heap_block_t* block = (heap_block_t*)((char*)ptr - sizeof(heap_block_t));

    /* Mark as free */
    block->used = false;

    /* TODO: Coalesce adjacent free blocks */
}

void* realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    /* Get the old block */
    heap_block_t* old_block = (heap_block_t*)((char*)ptr - sizeof(heap_block_t));
    size_t old_size = old_block->size - sizeof(heap_block_t);

    if (size <= old_size) {
        /* Shinking - just return the old pointer */
        return ptr;
    }

    /* Growing - allocate new block and copy */
    void* new_ptr = malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    /* Copy old data */
    size_t copy_size = old_size;
    if (size < copy_size) {
        copy_size = size;
    }

    char* src = (char*)ptr;
    char* dst = (char*)new_ptr;
    for (size_t i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }

    free(ptr);
    return new_ptr;
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;

    /* Check for overflow */
    if (nmemb != 0 && total / nmemb != size) {
        return NULL;
    }

    void* ptr = malloc(total);
    if (ptr == NULL) {
        return NULL;
    }

    /* Zero out the memory */
    char* p = (char*)ptr;
    for (size_t i = 0; i < total; i++) {
        p[i] = 0;
    }

    return ptr;
}

/* ============================================================================
 * Process Control
 * ============================================================================ */

void abort(void) {
    /* TODO: Raise SIGABRT */
    exit(134);  /* 128 + 6 = SIGABRT */
    __builtin_unreachable();
}
