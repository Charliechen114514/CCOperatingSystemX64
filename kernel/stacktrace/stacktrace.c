/**
 * @file stacktrace.c
 * @brief Stack trace dump implementation
 * @date 2026-02-17
 */

#include "stacktrace.h"
#include "klogs/kprintf.h"

// Stack bounds for validation
#define STACK_BASE    0x80000UL    // Approximate stack top (from kernel_entry.asm)
#define STACK_SIZE    0x10000UL    // 64KB stack size

/**
 * @brief Get current stack pointer
 *
 * @return current RSP value
 */
static inline uintptr_t get_rsp(void) {
    uintptr_t rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    return rsp;
}

/**
 * @brief Get current frame pointer
 *
 * @return current RBP value
 */
static inline uintptr_t get_rbp(void) {
    uintptr_t rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    return rbp;
}

/**
 * @brief Validate if a pointer looks like a valid stack frame
 *
 * @param ptr The pointer to validate
 * @return true if the pointer is likely a valid frame pointer
 */
static bool is_valid_frame(uintptr_t ptr) {
    // Check alignment (frames should be 8-byte aligned on x86_64)
    if (ptr & 0x7)
        return false;

    // Check if within reasonable stack bounds
    // Stack grows downward, so valid addresses are below STACK_BASE
    if (ptr > STACK_BASE)
        return false;

    // Check if not too far down (reasonable lower bound)
    if (ptr < (STACK_BASE - STACK_SIZE))
        return false;

    return true;
}

void dump_stack(int max_frames) {
    // Use default if 0 specified
    if (max_frames <= 0)
        max_frames = DUMP_STACK_DEFAULT_FRAMES;

    // Cap at maximum to prevent infinite loops
    if (max_frames > DUMP_STACK_MAX_FRAMES)
        max_frames = DUMP_STACK_MAX_FRAMES;

    uintptr_t rbp = get_rbp();
    uintptr_t rsp = get_rsp();

    kprintf(KLOG_BACKEND_SERIAL, "\n--- Stack Trace ---\n");
    kprintf(KLOG_BACKEND_SERIAL, "RSP: 0x%016lx\n", rsp);
    kprintf(KLOG_BACKEND_SERIAL, "RBP: 0x%016lx\n\n", rbp);

    kprintf(KLOG_BACKEND_SERIAL, "  #     RIP                RBP\n");
    kprintf(KLOG_BACKEND_SERIAL, "--------------------------------\n");

    int frame = 0;

    while (rbp != 0 && frame < max_frames) {
        // Validate frame pointer
        if (!is_valid_frame(rbp)) {
            kprintf(KLOG_BACKEND_SERIAL, "  #%2d    <invalid frame: 0x%016lx>\n", frame, rbp);
            break;
        }

        // Get return address (saved RIP is at RBP + 8)
        // We need to be careful with memory access
        uintptr_t* frame_ptr = (uintptr_t*)rbp;
        uintptr_t prev_rbp = frame_ptr[0];      // Saved RBP
        uintptr_t rip = frame_ptr[1];           // Saved return address

        // Print the frame
        if (rip != 0) {
            kprintf(KLOG_BACKEND_SERIAL, "  #%2d    0x%016lx    0x%016lx\n", frame, rip, rbp);
        } else {
            kprintf(KLOG_BACKEND_SERIAL, "  #%2d    <no return address>\n", frame);
        }

        // Move to previous frame
        rbp = prev_rbp;
        frame++;

        // Check for potential loop (same frame repeating)
        if (rbp == get_rbp()) {
            kprintf(KLOG_BACKEND_SERIAL, "  --- Detected frame loop, stopping ---\n");
            break;
        }
    }

    if (frame >= max_frames && rbp != 0) {
        kprintf(KLOG_BACKEND_SERIAL, "  ... (truncated, max frames reached)\n");
    }

    kprintf(KLOG_BACKEND_SERIAL, "------------------\n\n");
}

void dump_stack_full(void) {
    dump_stack(DUMP_STACK_DEFAULT_FRAMES);
}
