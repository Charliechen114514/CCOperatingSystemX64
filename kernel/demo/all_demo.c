/**
 * @file all_demo.c
 * @brief Central demo controller implementation
 */

#include "all_demo.h"
#include "klogs/kprintf.h"

// Include individual demo headers based on compile-time flags
#ifdef RTC_DEMO_ENABLED
#    include "rtc/rtc_demo.h"
#endif

#ifdef VMM_DEMO_ENABLED
#    include "vmm/vmm_demo.h"
#endif

#ifdef HEAP_DEMO_ENABLED
#    include "heap/heap_demo.h"
#endif

#ifdef COW_DEMO_ENABLED
#    include "cow/cow_demo.h"
#endif

#ifdef MOCK_SYSCALL_DEMO_ENABLED
#    include "mock_syscall/mock_syscall_demo.h"
#endif

#ifdef PROCESS_DEMO_ENABLED
#    include "process_simple/process_demo.h"
#endif

#ifdef USER_DEMO_ENABLED
#    include "user/user_demo.h"
#endif

#ifdef SCHED_DEMO_ENABLED
#    include "sched/sched_demo.h"
#endif

#ifdef ATA_DEMO_ENABLED
#    include "ata/ata_demo.h"
#endif

/**
 * @brief Run all enabled demos
 *
 * This function checks which demos are enabled at compile time and runs them.
 */
void run_possible_demos(void) {
#ifdef CCOS_ENABLE_DEMO
    klog_trace("\n");
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   Running Enabled Demos                ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");

#    ifdef RTC_DEMO_ENABLED
    klog_trace("[Demo Controller] RTC Demo is enabled, running...\n");
    rtc_run_demo();
#    endif

#    ifdef VMM_DEMO_ENABLED
    klog_trace("[Demo Controller] VMM Demo is enabled, running...\n");
    /* Run VMM demo without page fault test by default for safety */
    vmm_run_demo(false); /* Set to true to test page fault handling */
#    endif

#    ifdef HEAP_DEMO_ENABLED
    klog_trace("[Demo Controller] Heap Demo is enabled, running...\n");
    heap_run_demo();
#    endif

#    ifdef COW_DEMO_ENABLED
    klog_trace("[Demo Controller] COW Demo is enabled, running...\n");
    cow_run_demo();
#    endif

#    ifdef MOCK_SYSCALL_DEMO_ENABLED
    klog_trace("[Demo Controller] Mock Syscall Demo is enabled, running...\n");
    mock_syscall_run_demo();
#    endif

#    ifdef PROCESS_DEMO_ENABLED
    klog_trace("[Demo Controller] Process Simple Demo is enabled, running...\n");
    process_run_demo();
#    endif

#    ifdef USER_DEMO_ENABLED
    klog_trace("[Demo Controller] User Mode Demo is enabled, running...\n");
    user_run_demo();
#    endif

#    ifdef SCHED_DEMO_ENABLED
    klog_trace("[Demo Controller] Sched Demo is enabled, running...\n");
    sched_run_rr_demo();
    sched_run_prio_demo();
#    endif

#    ifdef ATA_DEMO_ENABLED
    klog_trace("[Demo Controller] ATA Demo is enabled, running...\n");
    ata_run_demo();
#    endif

    // Add more demos here as they are implemented
    // #ifdef TIMER_DEMO_ENABLED
    //     timer_run_demo();
    // #endif

    // #ifdef KEYBOARD_DEMO_ENABLED
    //     keyboard_run_demo();
    // #endif
#    ifdef CCOS_ENABLE_DEMO
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   Demo Controller Finished             ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");
#    endif
#else
    klog_trace("CCOS_ENABLE_DEMO is marking as disabled, jump the demo shell");
#endif
}
