/*
 * first basic test: will succeed even if you have not implemented context switching
 * as long as you:
 *  - simply have pre_exit(0) and pre_yield() return rather than context switch to 
 *    another thread.
 *  - [even more basic if you have issues: have pre_fork simply run immediately]
 * 
 * this test is useful to make sure you have the rough idea of how the threads 
 * should work.  you should rerun when you have your full package working to 
 * ensure you get the same result.
 */

#include "pre-test-header.h"

static unsigned thread_count, thread_sum;

// trivial first thread: does not block, explicitly calls exit.
static void thread_code(void *arg) {
    unsigned *x = arg;

    // check tid
    unsigned tid = pre_cur_thread()->tid;
	// trace("in thread tid=%d, with x=%d\n", tid, *x);
    debug("in thread tid=%d, with x=%d\n", tid, *x);
    demand(pre_cur_thread()->tid == *x+1, 
                "expected %d, have %d\n", tid,*x+1);

    // check yield.
    // pre_yield();
	thread_count ++;
    // pre_yield();
	thread_sum += *x;
    // pre_yield();
    // check exit
    // pre_exit(0);
}

void notmain() {
    // test_init();

    // change this to increase the number of threads.
	int n = 30;
    trace("about to test summing of n=%d threads\n", n);

	thread_sum = thread_count = 0;

    unsigned sum = 0;
	for(int i = 0; i < n; i++)  {
        int *x = kmalloc(sizeof *x);
        sum += *x = i;
		pre_fork(thread_code, x, 1, 1);
    }
	pre_thread_start();

	// no more threads: check.
	trace("count = %d, sum=%d\n", thread_count, thread_sum);
	assert(thread_count == n);
	assert(thread_sum == sum);
    trace("SUCCESS!\n");
}
