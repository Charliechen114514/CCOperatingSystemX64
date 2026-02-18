/**
 * @file heap_stub.c
 * @brief Host-side stub implementation for kernel heap functions
 *
 * This provides a minimal implementation of kmalloc/kfree for static testing
 * on the host system. Uses the standard malloc/free underneath.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations
void kfree(void* ptr);
extern void* memset(void* s, int c, size_t n);  // Provided by kernel/memory.c

// Simple tracking for allocations (helps detect leaks)
#define MAX_ALLOCATIONS 8192
static struct {
    void* ptr;
    size_t size;
    const char* comment;
} g_alloc_tracker[MAX_ALLOCATIONS];

static size_t g_total_allocations = 0;
static size_t g_current_allocations = 0;
static size_t g_total_bytes_allocated = 0;

void* kmalloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "[heap_stub] kmalloc(%zu) failed\n", size);
        return NULL;
    }

    // Track allocation
    if (g_current_allocations < MAX_ALLOCATIONS) {
        for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
            if (g_alloc_tracker[i].ptr == NULL) {
                g_alloc_tracker[i].ptr = ptr;
                g_alloc_tracker[i].size = size;
                g_alloc_tracker[i].comment = NULL;
                break;
            }
        }
    }

    g_current_allocations++;
    g_total_allocations++;
    g_total_bytes_allocated += size;

    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (ptr == NULL) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    // Find and update tracking
    for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (g_alloc_tracker[i].ptr == ptr) {
            void* new_ptr = realloc(ptr, new_size);
            if (new_ptr == NULL) {
                return NULL;
            }

            g_total_bytes_allocated -= g_alloc_tracker[i].size;
            g_total_bytes_allocated += new_size;
            g_alloc_tracker[i].ptr = new_ptr;
            g_alloc_tracker[i].size = new_size;
            return new_ptr;
        }
    }

    // Not tracked, just do realloc
    return realloc(ptr, new_size);
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    // Find and remove from tracking
    for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (g_alloc_tracker[i].ptr == ptr) {
            g_current_allocations--;
            g_alloc_tracker[i].ptr = NULL;
            g_alloc_tracker[i].size = 0;
            g_alloc_tracker[i].comment = NULL;
            break;
        }
    }

    free(ptr);
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

// Optional: Get heap statistics for testing
size_t heap_get_current_allocations(void) {
    return g_current_allocations;
}

size_t heap_get_total_allocations(void) {
    return g_total_allocations;
}

size_t heap_get_total_bytes(void) {
    return g_total_bytes_allocated;
}

void heap_dump_leaks(void) {
    if (g_current_allocations == 0) {
        printf("[heap_stub] No memory leaks detected\n");
        return;
    }

    printf("[heap_stub] WARNING: %zu potential leaks detected:\n", g_current_allocations);
    for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (g_alloc_tracker[i].ptr != NULL) {
            printf("  - Leak: %p (%zu bytes)\n",
                   g_alloc_tracker[i].ptr,
                   g_alloc_tracker[i].size);
        }
    }
}
