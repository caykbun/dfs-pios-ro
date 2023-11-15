// engler, cs140 put your gpio-int implementations in here.
#include "rpi.h"
#include "nrf-interrupt.h"
#include "nrf-hw-support.h"
#include "nrf-router.h"
#include "exception-handlers.h"
#include "dfs-common.h"
#include "preemptive-thread.h"

enum { trace_p = 0};
#define nrf_trace(args...) do {                          \
    if(trace_p) {                                       \
        trace(args);                                   \
    }                                                   \
} while(0)

enum {
    IRQ_BASIC_PENDING = 0x2000B200,
    IRQ_PENDING_1  = (IRQ_BASIC_PENDING + 0x4),
    IRQ_PENDING_2  = (IRQ_BASIC_PENDING + 0x8),
    IRQ_ENABLE_1 = (IRQ_BASIC_PENDING + 0x10),
    IRQ_ENABLE_2 = (IRQ_BASIC_PENDING + 0x14)
};

enum {
    GPIO_BASE = 0x20200000,
    GPIO_SET0 = (GPIO_BASE + 0x1C),
    GPIO_SET1 = (GPIO_BASE + 0x20),
    GPIO_CLR0 = (GPIO_BASE + 0x28),
    GPIO_CLR1 = (GPIO_BASE + 0x2C),
    GPIO_LEV0 = (GPIO_BASE + 0x34),
    GPIO_LEV1 = (GPIO_BASE + 0x38),
    GPIO_EDS0 = (GPIO_BASE + 0x40),
    GPIO_EDS1 = (GPIO_BASE + 0x44),
    GPIO_REN0 = (GPIO_BASE + 0x4C),
    GPIO_REN1 = (GPIO_BASE + 0x50),
    GPIO_FEN0 = (GPIO_BASE + 0x58),
    GPIO_FEN1 = (GPIO_BASE + 0x5C),
    GPIO_HEN0 = (GPIO_BASE + 0x64),
    GPIO_HEN1 = (GPIO_BASE + 0x68),
    GPIO_LEN0 = (GPIO_BASE + 0x70),
    GPIO_LEN1 = (GPIO_BASE + 0x74)
};


static nrf_t *nrf_left = NULL;
static nrf_t *nrf_right = NULL;

void set_nrf_left(nrf_t *_left) {
    nrf_left = _left;
}
void set_nrf_right(nrf_t *_right) {
    nrf_right = _right;
}

nrf_t *get_nrf_left() {
    return nrf_left;
}
nrf_t *get_nrf_right() {
    return nrf_right;
}

void nrf_handle_intr(nrf_t *n) {
    nrf_trace("NRF %d has interrupt!\n", n->config.id);

    if (nrf_has_max_rt_intr(n)) {
        nrf_trace("NRF %d has max rt interrupt!\n", n->config.id);
        nrf_rt_intr_clr(n);
    }

    if (nrf_has_rx_intr(n)) {
        assert(USE_RX_INTERRUPT);
        nrf_trace("NRF %d has rx interrupt!\n", n->config.id);
        uint8_t bytes[n->config.nbytes];
        nrf_trace("NRF %d rx fifo empty: %d!\n", n->config.id, nrf_rx_fifo_empty(n));
        while (!nrf_rx_fifo_empty(n)) {
            assert(nrf_rx_get_pipeid(n) == 1);
            uint8_t status = nrf_getn(n, NRF_R_RX_PAYLOAD, bytes, n->config.nbytes);
            cq_push_n(&n->recvq, bytes, n->config.nbytes);
            // printk("recved: ");
            // for (unsigned i = 0; i < n->config.nbytes; ++i) {
                // printk("%c", bytes[i]);
            // }
            // printk("\n");
            nrf_rx_intr_clr(n);                
        }
        nrf_trace("NRF %d rx fifo received %d bytes!\n", n->config.id, cq_nelem(&n->recvq));
    }

    if (nrf_has_tx_intr(n)) {
        nrf_trace("NRF %d has tx interrupt!\n", n->config.id);
        assert(USE_TX_INTERRUPT);
        assert(nrf_tx_fifo_empty(n)); // is this sound?
        nrf_tx_intr_clr(n);
       
        n->tx_sent += n->config.nbytes;
        // nrf_put8(n, NRF_CONFIG, rx_config);
        nrf_set_pwrup_off(n);
        gpio_set_off(n->config.ce_pin);
        nrf_set_pwrup_on(n);
        delay_us(500);
        // gpio_set_off(n->config.ce_pin);
        nrf_put8(n, NRF_CONFIG, rx_config);
        gpio_set_on(n->config.ce_pin);
        delay_us(500);
        nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    }

    nrf_trace("NRF %d leave interrupt!\n", n->config.id);
}

void int_vector_nrf(unsigned pc, unsigned sp) {
    dev_barrier();
    // n_interrupt++;

    // nrf_trace("pc: %x! tid: %d!\n", pc, get_current_thread_id());

    nrf_t *n = get_nrf_left();
    if (n != NULL && nrf_test_pin(n->config.int_pin)) {
        nrf_handle_intr(n);
    } 
    n = get_nrf_right();
    if (n != NULL && nrf_test_pin(n->config.int_pin)) {
        nrf_handle_intr(n);
    }

    dev_barrier();
}

#include "vector-base.h"

// void * int_vec_reset(void *vec) {
//     return vector_base_reset(vec);
// }

// void int_vec_init(void *v) {
//     // turn off system interrupts
//     cpsr_int_disable();

//     //  BCM2835 manual, section 7.5 , 112
//     dev_barrier();
//     PUT32(Disable_IRQs_1, 0xffffffff);
//     PUT32(Disable_IRQs_2, 0xffffffff);
//     dev_barrier();

//     vector_base_set(v);
// }

// initialize all the interrupt stuff.  client passes in the 
// gpio int routine <fn>
//
// make sure you understand how this works.
void nrf_int_startup() {
    // output("\tImportant: must loop back (attach a jumper to) pins 20 & 21\n");
    // output("\tImportant: must loop back (attach a jumper to) pins 20 & 21\n");
    // output("\tImportant: must loop back (attach a jumper to) pins 20 & 21\n");

    // initialize.
    // extern uint32_t interrupt_vec[];
    // int_vec_init(interrupt_vec);

    // in case there was an event queued up.
    // gpio_event_clear(in_pin);

    // start global interrupts.
    // cpsr_int_enable();
}


void nrf_interrupt_init(unsigned pin) {
    register_nrf_handler(int_vector_nrf);

    gpio_set_input(pin);
    gpio_write(pin, 1);
    if (pin >= 32) {
        return;
    } 
    dev_barrier();
    PUT32(GPIO_FEN0, GET32(GPIO_FEN0) | (1 << pin));
    dev_barrier();
    PUT32(IRQ_ENABLE_2, 0x20000);
    dev_barrier();
}


int nrf_test_pin(unsigned pin) {
    if (gpio_event_detected(pin)) {
        if (gpio_read(pin) == 0) {
            gpio_event_clear(pin);
            // n_rising++;
            return 1;
        }
        // gpio_event_clear(in_pin);
    }
    return 0;
}

// returns 1 if there is currently a GPIO_INT0 interrupt, 
// 0 otherwise.
//
// note: we can only get interrupts for <GPIO_INT0> since the
// (the other pins are inaccessible for external devices).
int gpio_has_interrupt(void) {
    // todo("implement: is there a GPIO_INT0 interrupt?\n");
    uint32_t has_interrupt = (GET32(IRQ_PENDING_2) >> 17) & 1;
    return DEV_VAL32(has_interrupt);
}

// p97 set to detect rising edge (0->1) on <pin>.
// as the broadcom doc states, it  detects by sampling based on the clock.
// it looks for "011" (low, hi, hi) to suppress noise.  i.e., its triggered only
// *after* a 1 reading has been sampled twice, so there will be delay.
// if you want lower latency, you should us async rising edge (p99)
void gpio_int_rising_edge(unsigned pin) {
    if (pin >= 32) {
        return;
    } 
    dev_barrier();
    PUT32(GPIO_REN0, GET32(GPIO_REN0) | (1 << pin));
    dev_barrier();
    PUT32(IRQ_ENABLE_2, 0x20000);
    dev_barrier();
}

// p98: detect falling edge (1->0).  sampled using the system clock.  
// similarly to rising edge detection, it suppresses noise by looking for
// "100" --- i.e., is triggered after two readings of "0" and so the 
// interrupt is delayed two clock cycles.   if you want  lower latency,
// you should use async falling edge. (p99)
void gpio_int_falling_edge(unsigned pin) {
    if (pin >= 32) {
        return;
    } 
    dev_barrier();
    PUT32(GPIO_FEN0, GET32(GPIO_FEN0) | (1 << pin));
    dev_barrier();
    PUT32(IRQ_ENABLE_2, 0x20000);
    dev_barrier();
}

// p96: a 1<<pin is set in EVENT_DETECT if <pin> triggered an interrupt.
// if you configure multiple events to lead to interrupts, you will have to 
// read the pin to determine which caused it.
int gpio_event_detected(unsigned pin) {
    if (pin >= 32) {
        return 0;
    } 
    dev_barrier();
    uint32_t status;
    status = (GET32(GPIO_EDS0) >> pin) & 1;
    dev_barrier();
    return DEV_VAL32(status);
}

// p96: have to write a 1 to the pin to clear the event.
void gpio_event_clear(unsigned pin) {
    if (pin >= 32) {
        return;
    } 
    dev_barrier();
    PUT32(GPIO_EDS0, 1 << pin);
    dev_barrier();
}
