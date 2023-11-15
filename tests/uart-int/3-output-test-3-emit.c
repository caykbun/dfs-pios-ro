// low level print that makes a bit easier to debug.
#include "rpi.h"
#include "uart-interrupt.h"
#include "rpi-interrupts.h"

void uart_destroy(void) {
    // hack to make sure aux is off.
    dev_barrier();
    PUT32(0x20215004, 1);
    dev_barrier();
    dev_barrier();
    gpio_set_function(GPIO_TX, 0);
    gpio_set_function(GPIO_RX, 0);
    dev_barrier();
    uint32_t addr = 0x20215040;
    for(unsigned off = 0; off < 11; off++)
        PUT32(addr+off*4, 0);
    dev_barrier();
    PUT32(0x20215004, 0);
    dev_barrier();
}

void notmain(void) {
    // initialize.
    extern uint32_t interrupt_vector_uart[];
    int_vec_init(interrupt_vector_uart);
    
    uart_destroy();
    uart_init_int();

    // make sure the queues are working.
    
    for(unsigned i = 0; i < 64; i++) {
        printk("TRACE:");
        for(unsigned l = 0; l < 26; l++)  {
            uart_putc_int(l + 'a');
            uart_putc_int(l + 'A');
        }

        uart_putc_int('\n');
    }
    // oh: wow: nasty bug: if you don't flush tx, this breaks.
    // uart_init();
}
