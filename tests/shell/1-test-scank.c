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

#include "shell-test-header.h"

// trivial first thread: does not block, explicitly calls exit.
static void thread_code(void *arg) {
    // timer_interrupt_turnoff();
    // test_prefetch_flush(100);
    char input[1000];
    // for (int i = 0; i < 100; ++i) {
    //     printk("%d ", (int) input[i]);
    // }
    int val1 = 100, val2 = 200, val3 = 300;
    _thread_safe_printk("Start typing...\n");
    // scank("don't need to match %d %x %s %d", &val1, &val2, input, &val3);
    // printk("%s %d %d %d\n", input, val1, val2, val3);
    // for (int i = 0; i < 100; ++i) {
    //     printk("%d ", (int) input[i]);
    // }

    _thread_safe_scank("%s", input);
    _thread_safe_printk("\n");
    _thread_safe_printk("%s\n%x\n", input, *(uint32_t*)arg);
    // for (int i = 0; i < 100; ++i) {
    //     printk("%d ", (int) input[i]);
    // }
    delay_ms(2000);

    _thread_safe_scank("%s", input);
    _thread_safe_printk("\n");
    _thread_safe_printk("%s\n%x\n", input, *(uint32_t*)arg);
    // for (int i = 0; i < 100; ++i) {
    //     printk("%d ", (int) input[i]);
    // }
}

void notmain() {
    // test_init()
    int x = 0xdeadbeef, y = 0xbeefdead, z = 0x140e140e;
    pre_fork(thread_code, &x, 1, 1);
    pre_fork(thread_code, &y, 1, 1);
    pre_fork(thread_code, &z, 1, 1);
    // pre_fork(thread_code, 0, 1, 1);
	pre_thread_start();

	// no more threads: check.
    trace("SUCCESS!\n");
}
