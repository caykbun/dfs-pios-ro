/*
 * first basic test: will succeed even if you have not implemented
 * context switching.
 * 
 *   - gives same result if you hvae pre_fork return immediately or
 *   - enqueue threads on a stack.
 *
 * this test is useful to make sure you have the rough idea of how the
 * threads should work.  you should rerun when you have your full package
 * working to ensure you get the same result.
 */
#include "pre-test-header.h"

static unsigned thread_count, thread_sum;

// trivial first thread: does not block, explicitly calls exit.
static void thread_code(void *arg) {
    unsigned *x = arg;

    // check tid
    pre_thread_t *t = pre_cur_thread();

    for (int i = 0; i < 10; ++i) {
        delay_us(1500);
        int y = (*x) * (*x);
	    _thread_safe_printk("[%d] in thread [%p], tid=%d with x=%d\n", i, t, t->tid, y);
        thread_count ++;
	    thread_sum += *x;
        _thread_safe_printk("THIS IS FOR DEBUGGING PURPOSE\n");
    }
    demand(pre_cur_thread()->tid == *x+1, 
                "expected %d, have %d\n", t->tid,*x+1);

	
    // pre_exit(0);
}

static void empty_thread(void *arg) {
    // while (1);
    delay_ms(100000);
}


void notmain() {
    // test_init();

    // trace("about to test summing of 30 threads\n");

    // change this to increase the number of threads.
	int n = 30;
	thread_sum = thread_count = 0;

    unsigned sum = 0;
	for(int i = 0; i < 30; i++)  {
        int *x = kmalloc(sizeof *x);
        sum += *x = i;

        // as a first step, can have pre_fork run immediately.
		pre_fork(thread_code, x, 1, 1);
    }
    // pre_fork(empty_thread, NULL, 0);

	pre_thread_start();

	// no more threads: check.
	trace("count = %d, sum=%d\n", thread_count, thread_sum);
	assert(thread_count == n * 10);
	assert(thread_sum == sum * 10);
    trace("SUCCESS!\n");
}
