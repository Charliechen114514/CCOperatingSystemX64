/**
 * @file kprintf_backends.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Backend registration and selection for kprintf
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "kprintf_backends.h"
#include "kprintf_config.h"

// Backend registry
static struct {
    klog_backend_t type;
    KLogBackendOps ops;
    bool registered;
} g_backend_registry[KPRINTF_MAX_BACKEND_N] = {0};

// Default backend
static klog_backend_t g_default_backend = KLOG_BACKEND_NONE;

// Find backend slot by type
static int find_backend_slot(klog_backend_t backend_type) {
    for (int i = 0; i < KPRINTF_MAX_BACKEND_N; i++) {
        if (g_backend_registry[i].registered && g_backend_registry[i].type == backend_type) {
            return i;
        }
    }
    return -1;
}

// Find empty slot
static int find_empty_slot(void) {
    for (int i = 0; i < KPRINTF_MAX_BACKEND_N; i++) {
        if (!g_backend_registry[i].registered) {
            return i;
        }
    }
    return -1;
}

const KLogBackendOps* klog_get_backend_ops(klog_backend_t backend) {
    int slot = find_backend_slot(backend);
    if (slot >= 0) {
        return &g_backend_registry[slot].ops;
    }
    return NULL;
}

void klog_set_default_backend(klog_backend_t backend) {
    g_default_backend = backend;
}

klog_backend_t klog_get_default_backend(void) {
    return g_default_backend;
}

bool klog_register_backend(klog_backend_t backend_type, const KLogBackendOps* ops) {
    if (ops == NULL) {
        return false;
    }

    // Check if already registered
    int slot = find_backend_slot(backend_type);
    if (slot >= 0) {
        // Update existing
        g_backend_registry[slot].ops = *ops;
        return true;
    }

    // Find empty slot
    slot = find_empty_slot();
    if (slot < 0) {
        return false; // Registry full
    }

    // Register new backend
    g_backend_registry[slot].type = backend_type;
    g_backend_registry[slot].ops = *ops;
    g_backend_registry[slot].registered = true;

    // Set as default if no default is set
    if (g_default_backend == KLOG_BACKEND_NONE) {
        g_default_backend = backend_type;
    }

    return true;
}
