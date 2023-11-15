#ifndef __SHELL_TEST_INIT_H__
#define __SHELL_TEST_INIT_H__

#include "rpi.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"
#include "timer-interrupt.h"

static void inline test_init(void) {
    unsigned oneMB = 1024*1024;
    kmalloc_init_set_start((void*)oneMB, oneMB);
}

#endif
