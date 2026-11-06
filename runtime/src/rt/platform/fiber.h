/* SPDX-License-Identifier: GPL-2.0-or-later
 * Cooperative host contexts. Keep this header in one translation unit.
 * The POSIX backend is a porting reference, not a verified PS5 ABI backend.
 */
#ifndef SR_PLATFORM_FIBER_H
#define SR_PLATFORM_FIBER_H
#include <stddef.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#define SR_FIBER_ENTRY CALLBACK
static inline void *sr_fiber_attach(void) { return ConvertThreadToFiber(NULL); }
static inline void *sr_fiber_create(size_t commit, size_t reserve,
        void (CALLBACK *entry)(void *), void *arg) {
    return CreateFiberEx(commit, reserve, 0, entry, arg);
}
static inline void sr_fiber_switch(void *fiber) { SwitchToFiber(fiber); }
static inline void sr_fiber_delete(void *fiber) { DeleteFiber(fiber); }

#elif defined(SR_FIBER_UCONTEXT)
#include <ucontext.h>
#define SR_FIBER_ENTRY
typedef struct SrHostFiber {
    ucontext_t context;
    void *stack;
    void (*entry)(void *);
    void *arg;
} SrHostFiber;
/* All contexts belong to one host thread, matching the existing scheduler. */
static _Thread_local SrHostFiber *sr_current_fiber;
static void sr_fiber_trampoline(void) {
    SrHostFiber *self = sr_current_fiber;
    self->entry(self->arg);
    /* Scheduler thread entries yield forever; returning is a contract error. */
    abort();
}
static inline void *sr_fiber_attach(void) {
    SrHostFiber *root;
    if (sr_current_fiber) return NULL;
    root = (SrHostFiber *)calloc(1, sizeof(*root));
    if (!root) return NULL;
    if (getcontext(&root->context) != 0) { free(root); return NULL; }
    sr_current_fiber = root;
    return root;
}
static inline void *sr_fiber_create(size_t commit, size_t reserve,
        void (*entry)(void *), void *arg) {
    SrHostFiber *fiber;
    (void)commit; /* POSIX stacks are allocated eagerly; no Windows reserve semantics. */
    if (!sr_current_fiber || !entry || reserve == 0) return NULL;
    fiber = (SrHostFiber *)calloc(1, sizeof(*fiber));
    if (!fiber) return NULL;
    fiber->stack = malloc(reserve);
    if (!fiber->stack || getcontext(&fiber->context) != 0) {
        free(fiber->stack); free(fiber); return NULL;
    }
    fiber->entry = entry;
    fiber->arg = arg;
    fiber->context.uc_stack.ss_sp = fiber->stack;
    fiber->context.uc_stack.ss_size = reserve;
    fiber->context.uc_stack.ss_flags = 0;
    fiber->context.uc_link = NULL;
    /* No pointer arguments through makecontext's int-sized varargs. */
    makecontext(&fiber->context, sr_fiber_trampoline, 0);
    return fiber;
}
static inline void sr_fiber_switch(void *target) {
    SrHostFiber *previous = sr_current_fiber;
    SrHostFiber *next = (SrHostFiber *)target;
    if (!previous || !next) abort();
    if (previous == next) return;
    sr_current_fiber = next;
    if (swapcontext(&previous->context, &next->context) != 0) abort();
    sr_current_fiber = previous;
}
static inline void sr_fiber_delete(void *target) {
    SrHostFiber *fiber = (SrHostFiber *)target;
    if (!fiber) return;
    if (fiber == sr_current_fiber) abort();
    free(fiber->stack);
    free(fiber);
}
#else
#error "Select a verified host fiber backend; PS5 support is not yet qualified"
#endif
#endif
