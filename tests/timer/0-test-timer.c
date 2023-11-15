#include "rpi.h"
#include "timer-interrupt.h"
#include "cycle-count.h"
#include "../header/timer.h"

void timer_interrupt_setup(void *int_vec, unsigned interval) {
    uart_init();
	
    int_vec_init(int_vec);

    timer_interrupt_init(interval);

    cpsr_int_enable();
}

void notmain() {
    extern uint32_t interrupt_vector_timer[];
    timer_interrupt_setup((void *)interrupt_vector_timer, 0x40);

    unsigned start = timer_get_usec();

    // Q: what happens if you enable cache?  Why are some parts
    // the same, some change?
    //enable_cache(); 	
    unsigned iter = 0, sum = 0;
#   define N 20
    while(cnt < N) {
        // Q: if you comment this out?  why do #'s change?
        printk("iter=%d: cnt = %d, time between interrupts = %d usec (%x)\n", 
                                    iter,cnt, period,period);
        iter++;
    }

    // overly complicated calculation of sec/ms/usec breakdown
    // make it easier to tell the overhead of different changes.
    // not really specific to interrupt handling.
    unsigned tot = timer_get_usec() - start,
             tot_sec    = tot / (1000*1000),
             tot_ms     = (tot / 1000) % 1000,
             tot_usec   = (tot % 1000);
    printk("-----------------------------------------\n");
    printk("summary:\n");
    printk("\t%d: total iterations\n", iter);
    printk("\t%d: tot interrupts\n", N);
    printk("\t%d: iterations / interrupt\n", iter/N);
    printk("\t%d: average period\n\n", period_sum/(N-1));
    printk("total execution time: %dsec.%dms.%dusec\n", 
                    tot_sec, tot_ms, tot_usec);

    printk("\nmeasuring cost of different operations (pi A+ = 700 cyc / usec)\n");
    cycle_cnt_init();
    TIME_CYC_PRINT("\tread/write barrier", dev_barrier());
    TIME_CYC_PRINT("\tread barrier", dmb());
    TIME_CYC_PRINT("\tsafe timer", timer_get_usec());
    TIME_CYC_PRINT("\tunsafe timer", timer_get_usec_raw());
    // macro expansion messes up the printk
    printk("\t<cycle_cnt_read()>: %d\n", TIME_CYC(cycle_cnt_read()));
    printk("-----------------------------------------\n");

    clean_reboot();
}