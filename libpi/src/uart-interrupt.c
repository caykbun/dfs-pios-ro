#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "libc/helper-macros.h"
#include "uart-interrupt.h"
#include "timer-interrupt.h"
#include "thread-safe-io.h"
#include "global.h"
#include "preemptive-thread.h"

#define IRQ_AUX 29

static int interrupt_on;

// 0 for tx, 1 for rx
void uart_disable_interrupt(uint8_t is_tx)
{
    PUT32(AUX_MU_IER_REG, GET32(AUX_MU_IER_REG) & ~(1 << is_tx));
}

void uart_enable_interrupt(uint8_t is_tx)
{
    PUT32(AUX_MU_IER_REG, GET32(AUX_MU_IER_REG) | (1 << is_tx));
}

static int uart_int_putchar(int c) {
    uart_putc_int(c);
    return c;
}

static int uart_int_getchar() {
    int c;
    while ((c = uart_getc_int()) == -1) {
        make_system_call(REQUEST_STDIN);
    }
    return c;
}

void uart_reset_int_off() {
    uart_disable_interrupt(0);
    uart_disable_interrupt(1);
    uart_disable();
    reset_uart_ready();
    rpi_putchar_reset();
    interrupt_on = 0;
    uart_init();
} 

void uart_init_int(void) {
    if (!set_uart_ready() && interrupt_on == 1) {
        return;
    }
    interrupt_on = 1;
    // force printk/scank to use our interrupt uart API
    if (USE_OUT_INTERRUPTS) {
        rpi_putchar_set(uart_int_putchar);
    }
    rpi_getchar_set(uart_int_getchar);

    // initialize queue
    stdio_init();

    dev_barrier();

    // set pin
    gpio_set_function(GPIO_TX, GPIO_FUNC_ALT5);
    gpio_set_function(GPIO_RX, GPIO_FUNC_ALT5);

    dev_barrier();

    // enable aux IRQ
    PUT32(Enable_IRQs_1, 1 << IRQ_AUX);

    dev_barrier();

    // enable mini uart
    unsigned old = GET32(AUX_ENABLES);
    unsigned mask = 0x1;
    PUT32(AUX_ENABLES, old | mask);

    dev_barrier();

    // disable tx/rx so don't receive garbage
    PUT32(AUX_MU_CNTL_REG, 0);

    // clear fifo
    PUT32(AUX_MU_IIR_REG, 0b110);

    // set baud rate
    PUT32(AUX_MU_BAUD, 270);

    // set uart to 8 bit mode
    PUT32(AUX_MU_LCR_REG, 0b11);

    // enable mini uart interrupt
    // 3:2 errata?
    if (USE_OUT_INTERRUPTS) {
        PUT32(AUX_MU_IER_REG, 0b1100);
    } else {
        PUT32(AUX_MU_IER_REG, 0b1110); // don't enable receive before 
    }

    // enable tx/rx
    PUT32(AUX_MU_CNTL_REG, 0b11);

    dev_barrier();
}

// -1 if cq is full or error, 0 on success
int uart_putc_int(uint8_t c)
{   
    dev_barrier();
    uart_disable_interrupt(1);

    if (stdout_queue_is_full()) {
        uart_enable_interrupt(1);
        dev_barrier();
        return -1;
    }

    stdout_queue_push_char(c);
    uart_enable_interrupt(1);
    dev_barrier();
    return 0;
}

int uart_getc_int()
{
    dev_barrier();
    uart_disable_interrupt(0);
    if (stdin_queue_is_empty())
    {
        uart_enable_interrupt(0);
        dev_barrier();
        return -1;
    }
    int res = stdin_queue_pop_char();
    uart_enable_interrupt(0);
    dev_barrier();
    return res;
}

int uart_has_data_int(void)
{
    dev_barrier();
    uart_disable_interrupt(0);
    int res = stdin_queue_is_empty() ? 0 : 1;
    uart_enable_interrupt(0);
    dev_barrier();
    return res;
}

void int_vector_uart(unsigned pc) {
    dev_barrier();

    // not an aux uart interrupt
    if (GET32(AUX_MU_IIR_REG) & 1) {
        dev_barrier();
        return;
    }


    // if it is tx interrupt: non-full tx fifo
    if (GET32(AUX_MU_IIR_REG) & 2)
    {
        // drain the queue until fifo is full
        while (uart_can_put8())
        {
            if (stdout_queue_is_empty())
            {
                // disable tx interrupt
                uart_disable_interrupt(1);
                break;
            }
            unsigned char c = stdout_queue_pop_char();
            uart_put8(c);
        }
    }
    // if it is rx interrupt: non-empty rx fifo
    else if (GET32(AUX_MU_IIR_REG) & 4)
    {
        // fill the queue until fifo is empty
        while (uart_has_data())
        {
            if (stdin_queue_is_full())
            {
                // drop data!
                break;
            }
            stdin_queue_push_char((unsigned char)uart_get8());
        }
    }

    // need to clear interrupt pending bit?
    dev_barrier();

}

int uart_interrupt_is_on() {
    return interrupt_on;
}
