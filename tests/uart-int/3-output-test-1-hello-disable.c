// low level print that makes a bit easier to debug.
#include "rpi.h"
#include "uart-interrupt.h"
#include "rpi-interrupts.h"
#include "rpi-interrupts.h"

static void my_putk(char *s) {
    for(; *s; s++)
        uart_putc_int(*s);
}

void notmain(void) {
    uart_disable();

    // initialize.
    extern uint32_t interrupt_vector_uart[];
    int_vec_init(interrupt_vector_uart);
    
    for(unsigned i = 0; i < 100; i++) {
        uart_init_int();
        printk("TRACE:hello world %d\n", i);
        uart_reset_int_off();
    }
    uart_init_int();
}
