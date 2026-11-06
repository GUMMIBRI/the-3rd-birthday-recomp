/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "platform/fiber.h"
#include <stdio.h>
static void *root;
static int stage;
static void SR_FIBER_ENTRY worker(void *arg) {
    int local = *(int *)arg;
    stage = local;
    sr_fiber_switch(root);
    /* Verify that a suspended native stack and its local value survive. */
    stage = local + 1;
    sr_fiber_switch(root);
    abort();
}
int main(void) {
    int seed = 41;
    void *child;
    root = sr_fiber_attach();
    if (!root) return 1;
    child = sr_fiber_create(256 * 1024, 1024 * 1024, worker, &seed);
    if (!child) return 2;
    sr_fiber_switch(child);
    if (stage != 41) return 3;
    sr_fiber_switch(child);
    if (stage != 42) return 4;
    sr_fiber_delete(child);
    puts("fiber round-trip and stack retention passed");
    return 0;
}
