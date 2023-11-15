#include "rpi.h"

typedef enum
{
    AUX_IRQ = 0x20215000,
    AUX_ENABLES = 0x20215004,
    AUX_MU_IO_REG = 0x20215040,
    AUX_MU_IER_REG = 0x20215044,
    AUX_MU_IIR_REG = 0x20215048,
    AUX_MU_LCR_REG = 0x2021504C,
    AUX_MU_LSR_REG = 0x20215054,
    AUX_MU_CNTL_REG = 0x20215060,
    AUX_MU_STAT_REG = 0x20215064,
    AUX_MU_BAUD = 0x20215068,
} uart_func;

void uart_init_int(void);

void uart_reset_int_off();

// -1 if cq is full or error, 0 on success
int uart_putc_int(uint8_t c);

int uart_getc_int();

int uart_has_data_int(void);

void uart_disable_interrupt(uint8_t is_tx);

void uart_enable_interrupt(uint8_t is_tx);

void int_vector_uart(unsigned pc);

int uart_interrupt_is_on();
