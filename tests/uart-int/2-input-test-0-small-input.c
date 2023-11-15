// low level print that makes a bit easier to debug.
#include "rpi.h"
#include "memmap.h"
#include "uart-interrupt.h"
#include "rpi-interrupts.h"

int get8_timeout(unsigned timeout_msec) {
    unsigned n = 1000 * timeout_msec;

    unsigned s = timer_get_usec();
    while((timer_get_usec() - s) < n)
        if(uart_has_data_int())
            return uart_getc_int();

    panic("uart had no data for %dms: did you pipe it into my-install?\n", 
                timeout_msec);
}

// test that we can read a single line of input.
void notmain(void) {
    extern uint32_t interrupt_vector_uart[];
    int_vec_init(interrupt_vector_uart);

    uart_init_int();

    char buf[1024];
    for(int i = 0; i < sizeof buf; i++) {
        buf[i] = get8_timeout(300);
        if(buf[i] == '\n') {
            buf[i] = 0;
            break;
        }
    }
    output("TRACE: received: <%s>\n", buf);
}
