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
#endif
#ifdef RTC_DEMO_ENABLED
    klog_trace("[Demo Controller] RTC Demo is enabled, running...\n");
    rtc_run_demo();
#else
    klog_trace("[Demo Controller] RTC Demo is disabled (use -DENABLE_RTC_DEMO=ON to enable)\n");
#endif

    // Add more demos here as they are implemented
    // #ifdef TIMER_DEMO_ENABLED
    //     timer_run_demo();
    // #endif

    // #ifdef KEYBOARD_DEMO_ENABLED
    //     keyboard_run_demo();
    // #endif
#ifdef CCOS_ENABLE_DEMO
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   Demo Controller Finished             ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");
#endif
}
