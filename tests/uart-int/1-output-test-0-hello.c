// low level print that makes a bit easier to debug.
#include "uart-interrupt.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"

void int_vec_init(void *v);
int uart_putc_int(uint8_t c);
void uart_init_int(void);
void uart_destroy(void);

static void my_putk(char *s)
{
    for (; *s; s++)
        uart_putc_int(*s);
}

void notmain(void)
{
    // disable system interrupt?
    extern uint32_t interrupt_vector_uart[];
    int_vec_init(interrupt_vector_uart);

    uart_init_int();

    cpsr_int_enable();

    // uart_putc_int('a');
    // my_putk("TRACE:hello world using my_putk\n");
    // my_putk("TRACE:hello world using my_putk\n");
    // my_putk("TRACE:hello world using my_putk\n");
    // my_putk("TRACE:hello world using my_putk\n");
    // my_putk("TRACE:hello world using my_putk\n");
    for (int i = 0; i < 2000; ++i) {
        // my_putk("TRACE:hello world using my_putk\n");
        int bits = 0;
        int j = i;
        while (j > 0) {
            j /= 10;
            bits++;
        }
        for (int k = 0; k < 9 - bits; ++k) {
            uart_putc_int('0');
        }
        printk("%d\n", i);
    }
    // delay_ms(60000);
    // cpsr_int_disable();
    printk("TRACE:hello world using printk &&&&&&&&&&&&&&&&&7\n");
    // uart_disable();
}
