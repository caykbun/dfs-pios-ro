#include "rpi.h"
#include "rpi-constants.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "libc/helper-macros.h"
#include "circular.h"
#include "asm-helpers.h"
#include "libc/bit-support.h"
#include "exception-handlers.h"


void int_vector(unsigned pc) {
    clean_reboot();
}

void prefetch_abort_vector(unsigned lr) {
    clean_reboot();
}

void data_abort_vector(unsigned lr) {
    clean_reboot();
}

void fiq_vector(unsigned pc, unsigned sp) {
    nrf_interrupt_handler(pc, sp);
    timer_interrupt_handler(pc, sp);
    uart_interrupt_handler(pc, sp);
    last_interrupt_handler(pc, sp);
}


void register_timer_handler(void (*func_ptr)(unsigned, unsigned)) {
    timer_handler_ptr = func_ptr;
}

void register_uart_handler(void (*func_ptr)(unsigned, unsigned)) {
    uart_handler_ptr = func_ptr;
}

void register_nrf_handler(void (*func_ptr)(unsigned, unsigned)) {
    nrf_handler_ptr = func_ptr;
}

void register_last_handler(void (*func_ptr)(unsigned, unsigned)) {
    last_handler_ptr = func_ptr;
}

void timer_interrupt_handler(unsigned pc, unsigned sp) {
    if (timer_handler_ptr == NULL)
        return;
    (*timer_handler_ptr)(pc, sp);
}

void uart_interrupt_handler(unsigned pc, unsigned sp) {
    if (uart_handler_ptr == NULL)
        return;
    (*uart_handler_ptr)(pc, sp);
}

void nrf_interrupt_handler(unsigned pc, unsigned sp) {
    if (nrf_handler_ptr == NULL)
        return;
    (*nrf_handler_ptr)(pc, sp);
}

void last_interrupt_handler(unsigned pc, unsigned sp) {
    if (last_handler_ptr == NULL)
        return;
    (*last_handler_ptr)(pc, sp);
}
