// implement:
//  void uart_init(void)
//
//  int uart_can_get8(void);
//  int uart_get8(void);
//
//  int uart_can_put8(void);
//  void uart_put8(uint8_t c);
//
//  int uart_tx_is_empty(void) {
//
// see that hello world works.
//
//
#include "rpi.h"
#include "timer-interrupt.h"
#include "global.h"
#include "preemptive-thread.h"

enum {
    AUXENB = 0x20215004,
    AUX_MU_IO_REG = 0x20215040,
    AUX_MU_IER_REG = (AUX_MU_IO_REG + 0x4),
    AUX_MU_IIR_REG = (AUX_MU_IO_REG + 0x8),
    AUX_MU_LCR_REG = (AUX_MU_IO_REG + 0xC),
    AUX_MU_MCR_REG = (AUX_MU_IO_REG + 0x10),
    AUX_MU_LSR_REG = (AUX_MU_IO_REG + 0x14),
    AUX_MU_CNTL_REG = (AUX_MU_IO_REG + 0x20),
    AUX_MU_STAT_REG = (AUX_MU_IO_REG + 0x24),
    AUX_MU_BAUD = (AUX_MU_IO_REG + 0x28),
};

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    if (!set_uart_ready()) {
        return;
    }
    dev_barrier();
    
    gpio_set_function(GPIO_TX, GPIO_FUNC_ALT5);
    gpio_set_function(GPIO_RX, GPIO_FUNC_ALT5);
    
    dev_barrier();

    uint32_t aux_enabled = GET32(AUXENB);
    if (aux_enabled & 0x1)
        uart_disable();
    PUT32(AUXENB, aux_enabled | 0x1);

    dev_barrier();

    // disable transmitter and receiver
    PUT32(AUX_MU_CNTL_REG, 0x0);
    // p12 disable interrupts
    PUT32(AUX_MU_IER_REG, 0x0);
    // p14 only set bits 1-0 for uart to run in 8-bit mode
    PUT32(AUX_MU_LCR_REG, 0x3);
    // p14 I don't think this is necessary
    PUT32(AUX_MU_MCR_REG, 0x0);
    // p13 bits 2-1 set to clear tx & rx FIFO queue
    PUT32(AUX_MU_IIR_REG, 0x6);
    // p19 bits 15-0 are baud rate
    PUT32(AUX_MU_BAUD, 270);
    // p17 enable transmitter receiver 
    PUT32(AUX_MU_CNTL_REG, 0x3);

    dev_barrier();    
}

// disable the uart.
void uart_disable(void) {
    uart_flush_tx();
    PUT32(AUXENB, GET32(AUXENB) & ~0x1);
}


// returns one byte from the rx queue, if needed
// blocks until there is one.
int uart_get8(void) {
    dev_barrier();
	while (!uart_has_data()) {
        
    }
    // p11 bits 7-0 are received byte
    int x = GET32(AUX_MU_IO_REG) & 0xff;
    PUT32(AUX_MU_IO_REG, (GET32(AUX_MU_IO_REG) & 0xffffff00));
    return x;
}

// 1 = space to put at least one byte, 0 otherwise.
int uart_can_put8(void) {
    // p15 bit 5 is set if the transmit FIFO can accept at least one byte 
    if (GET32(AUX_MU_STAT_REG) & 0x2)
        return 1;
    return 0;
}

// put one byte on the tx qqueue, if needed, blocks
// until TX has space.
// returns < 0 on error.
int uart_put8(uint8_t c) {
    // if (!USE_UART_INTERRUPTS) {
        // timer_interrupt_turnoff();
    // }

    dev_barrier();
    while (!uart_can_put8()) {
        
    }
    // p11 bits 7-0 are byte to transmit
    PUT32(AUX_MU_IO_REG, (GET32(AUX_MU_IO_REG) & 0xffffff00) | c);

    // if (!USE_UART_INTERRUPTS) {
        // timer_interrupt_turnon();
    // }
    return 0;
}

// simple wrapper routines useful later.

// 1 = at least one byte on rx queue, 0 otherwise
int uart_has_data(void) {
    // p15 bit 0 is set if the receive FIFO holds at least 1 symbol
    return GET32(AUX_MU_STAT_REG) & 0x1;
}

// return -1 if no data, otherwise the byte.
int uart_get8_async(void) { 
    if(!uart_has_data())
        return -1;
    return uart_get8();
}

// 1 = tx queue empty, 0 = not empty.
int uart_tx_is_empty(void) {
    // p15, bit 6 is set if the transmit FIFO is empty and the transmitter is idle
    if ((GET32(AUX_MU_LSR_REG) & 0x40) == 0x40) // || (GET32(AUX_MU_STAT_REG) & 0x100) == 0x100)
        return 1;
    return 0;
}

// flush out all bytes in the uart --- we use this when 
// turning it off / on, etc.
void uart_flush_tx(void) {
    while(!uart_tx_is_empty())
        ;
}

void uart_flush_rx(void) {
    while(uart_get8_async() != -1)
        ;
}
