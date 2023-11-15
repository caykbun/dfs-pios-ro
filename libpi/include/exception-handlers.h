#ifndef __EXCEPTION_HANDLERS_H__
#define __EXCEPTION_HANDLERS_H__


static void (*timer_handler_ptr)(unsigned, unsigned);
static void (*uart_handler_ptr)(unsigned, unsigned);
static void (*nrf_handler_ptr)(unsigned, unsigned);
static void (*last_handler_ptr)(unsigned, unsigned);

void register_timer_handler(void (*func_ptr)(unsigned, unsigned));
void register_uart_handler(void (*func_ptr)(unsigned, unsigned));
void register_nrf_handler(void (*func_ptr)(unsigned, unsigned));
void register_last_handler(void (*func_ptr)(unsigned, unsigned));

void timer_interrupt_handler(unsigned pc, unsigned sp);
void uart_interrupt_handler(unsigned pc, unsigned sp);
void nrf_interrupt_handler(unsigned pc, unsigned sp);
void last_interrupt_handler(unsigned pc, unsigned sp);

#endif