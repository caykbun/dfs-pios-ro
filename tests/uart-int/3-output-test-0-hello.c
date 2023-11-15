// low level print that makes a bit easier to debug.
#include "rpi.h"
#include "uart-interrupt.h"
#include "rpi-interrupts.h"

static void my_putk(char *s) {
    for(; *s; s++)
        uart_putc_int(*s);
}

void notmain(void) {
    // initialize.
    extern uint32_t interrupt_vector_uart[];
    int_vec_init(interrupt_vector_uart);

    uart_init_int();
    my_putk("TRACE:hello world using my_putk\n");
    printk("TRACE:hello world using printk\n");
}
