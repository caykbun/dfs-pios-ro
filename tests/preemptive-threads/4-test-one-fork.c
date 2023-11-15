// run a single thread: does a single context switch from 
// scheduler thread into this thread.
#include "pre-test-header.h"

static void thread_code(void *arg) {
    unsigned *x = arg;

    // check tid
    pre_thread_t *t = pre_cur_thread();

    // actually i think the thread pointer should be same?
	trace("in thread [%p], tid=%d with x=%d\n", t, t->tid, *x);
    assert(t->tid == 1 && *x == 0xdeadbeef);

    trace("SUCCESS: got to the first thread: rebooting\n");
    clean_reboot();
}

void notmain() {
    test_init();
    trace("about to fork and run one thread\n");

    unsigned x = 0xdeadbeef;
	pre_fork(thread_code, &x, 1, 1);
	pre_thread_start();
    not_reached();
}
