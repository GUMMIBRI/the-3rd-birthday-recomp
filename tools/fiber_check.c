/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "platform/fiber.h"
#include <stdio.h>
static void *root;
static int stage;
#ifdef SR_FIBER_SYSV
static unsigned worker_mxcsr;
static unsigned short worker_cw;
#endif
static void SR_FIBER_ENTRY worker(void *arg) {
    int local = *(int *)arg;
#ifdef SR_FIBER_SYSV
    unsigned mxcsr;
    unsigned short cw;
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    worker_mxcsr = (mxcsr & ~0x6000u) | 0x2000u;
    worker_cw = (unsigned short)((cw & ~0x0c00u) | 0x0400u);
    __asm__ volatile("ldmxcsr %0" : : "m"(worker_mxcsr));
    __asm__ volatile("fldcw %0" : : "m"(worker_cw));
#endif
    stage = local;
    sr_fiber_switch(root);
#ifdef SR_FIBER_SYSV
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    if ((mxcsr & 0x6000u) != (worker_mxcsr & 0x6000u) ||
        (cw & 0x0c00u) != (worker_cw & 0x0c00u)) abort();
#endif
    /* Verify that a suspended native stack and its local value survive. */
    stage = local + 1;
    sr_fiber_switch(root);
    abort();
}
int main(void) {
    int seed = 41;
    void *child;
#ifdef SR_FIBER_SYSV
    unsigned initial_mxcsr, current_mxcsr;
    unsigned short initial_cw, current_cw;
    __asm__ volatile("stmxcsr %0" : "=m"(initial_mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(initial_cw));
#endif
    root = sr_fiber_attach();
    if (!root) return 1;
    child = sr_fiber_create(256 * 1024, 1024 * 1024, worker, &seed);
    if (!child) return 2;
    sr_fiber_switch(child);
    if (stage != 41) return 3;
#ifdef SR_FIBER_SYSV
    __asm__ volatile("stmxcsr %0" : "=m"(current_mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(current_cw));
    if ((current_mxcsr & 0x6000u) != (initial_mxcsr & 0x6000u) ||
        (current_cw & 0x0c00u) != (initial_cw & 0x0c00u)) return 5;
#endif
    sr_fiber_switch(child);
    if (stage != 42) return 4;
    sr_fiber_delete(child);
#ifdef SR_FIBER_SYSV
    sr_fiber_detach();
#endif
    puts("fiber round-trip and stack retention passed");
    return 0;
}
