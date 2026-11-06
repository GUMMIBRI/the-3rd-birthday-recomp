/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original x86-64 SysV cooperative contexts, not an emulator CPU backend.
 * Same host thread only; no C++ unwinding across switched stacks.
 */
#include "fiber.h"
#include <stdint.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    uintptr_t rsp, rbx, rbp, r12, r13, r14, r15;
    uint32_t mxcsr;
    uint16_t x87cw;
} SrSysvContext;
_Static_assert(offsetof(SrSysvContext, mxcsr) == 56, "assembly context layout");
_Static_assert(offsetof(SrSysvContext, x87cw) == 60, "assembly FP layout");
typedef struct SrSysvFiber {
    SrSysvContext registers;
    void *mapping;
    size_t mapping_size;
    size_t stack_size;
    void (*entry)(void *);
    void *arg;
} SrSysvFiber;
extern void sr_sysv_swap(SrSysvContext *, const SrSysvContext *);
static SrSysvFiber *current, *root;
static pthread_t owner;
static unsigned children;
/* Conservative protection budget, NOT a proven PS5 commitment policy.
 * Treat every writable page as potentially resident; refuse rather than OOM.
 * Actual game thread demand may exceed this and needs a verified backing policy.
 */
#ifndef SR_SYSV_STACK_BUDGET
#define SR_SYSV_STACK_BUDGET (512u * 1024u * 1024u)
#endif
static size_t protected_stack_bytes;
static void require_owner(void) {
    if (!root || !pthread_equal(owner, pthread_self())) abort();
}
static void fiber_returned(void) { abort(); }
static void fiber_enter(void) {
    SrSysvFiber *self = current;
    self->entry(self->arg);
    fiber_returned();
}
void *sr_fiber_attach(void) {
    if (root) return NULL;
    root = calloc(1, sizeof(*root));
    if (!root) return NULL;
    owner = pthread_self();
    current = root;
    return root;
}
void *sr_fiber_create(size_t commit, size_t reserve, void (*entry)(void *), void *arg) {
    SrSysvFiber *fiber;
    uintptr_t top;
    size_t page, usable;
    long page_result;
    (void)commit; /* No Windows incremental-commit equivalence is claimed. */
    require_owner();
    if (!entry || reserve < 256 * 1024 || reserve > 64 * 1024 * 1024 || children >= 128)
        return NULL;
    page_result = sysconf(_SC_PAGESIZE);
    if (page_result <= 0) return NULL;
    page = (size_t)page_result;
    if (page > 1024 * 1024 || (page & (page - 1))) return NULL;
    usable = (reserve + page - 1) & ~(page - 1);
    if (usable > SR_SYSV_STACK_BUDGET ||
        protected_stack_bytes > SR_SYSV_STACK_BUDGET - usable) return NULL;
    fiber = calloc(1, sizeof(*fiber));
    if (!fiber) return NULL;
    fiber->mapping_size = usable + 2 * page;
    fiber->stack_size = usable;
    fiber->mapping = mmap(NULL, fiber->mapping_size, PROT_NONE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (fiber->mapping == MAP_FAILED) { free(fiber); return NULL; }
    if (mprotect((char *)fiber->mapping + page, usable, PROT_READ | PROT_WRITE) != 0) {
        munmap(fiber->mapping, fiber->mapping_size); free(fiber); return NULL;
    }
    top = ((uintptr_t)fiber->mapping + page + usable) & ~(uintptr_t)15;
    /* ret consumes entry; at C entry RSP % 16 == 8, with a real return trap. */
    top -= sizeof(uintptr_t); *(uintptr_t *)top = (uintptr_t)fiber_returned;
    top -= sizeof(uintptr_t); *(uintptr_t *)top = (uintptr_t)fiber_enter;
    fiber->registers.rsp = top;
    __asm__ volatile("stmxcsr %0" : "=m"(fiber->registers.mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(fiber->registers.x87cw));
    fiber->entry = entry;
    fiber->arg = arg;
    ++children;
    protected_stack_bytes += usable;
    return fiber;
}
void sr_fiber_switch(void *target) {
    SrSysvFiber *previous = current, *next = target;
    require_owner();
    if (!next) abort();
    if (previous == next) return;
    current = next;
    sr_sysv_swap(&previous->registers, &next->registers);
    current = previous;
}
void sr_fiber_delete(void *target) {
    SrSysvFiber *fiber = target;
    require_owner();
    if (!fiber) return;
    if (fiber == current || fiber == root) abort();
    protected_stack_bytes -= fiber->stack_size;
    if (munmap(fiber->mapping, fiber->mapping_size) != 0) abort();
    free(fiber);
    --children;
}
void sr_fiber_detach(void) {
    require_owner();
    if (current != root || children) abort();
    free(root);
    root = current = NULL;
}
