#include "nrf.h"
#include "nrf-hw-support.h"
#include "nrf-router.h"
#include "dfs-common.h"
#include "nrf-interrupt.h"
#include "sys-call.h"

nrf_t * nrf_init(nrf_conf_t c, uint32_t rxaddr, unsigned acked_p) {
    nrf_t *n = kmalloc(sizeof *n);
    n->config = c;
    nrf_stat_start(n);
    n->spi = pin_init(c.ce_pin, c.spi_chip);
    n->rxaddr[1] = rxaddr;

    nrf_set_pwrup_off(n);

    nrf_put8(n, NRF_EN_RXADDR, 0);

    if (acked_p) {
        nrf_put8(n, NRF_EN_AA, 0b11);
        nrf_put8(n, NRF_EN_RXADDR, 0b11);
    } else {
        nrf_put8(n, NRF_EN_AA, 0b0);
        nrf_put8(n, NRF_EN_RXADDR, 0b10);
    }

    nrf_put8(n, NRF_RX_PW_P1, c.nbytes);
    nrf_set_addr(n, NRF_RX_ADDR_P1, rxaddr, nrf_default_addr_nbytes);
    nrf_set_addr(n, NRF_TX_ADDR, 0, nrf_default_addr_nbytes);
    nrf_put8(n, NRF_RX_PW_P2, 0);
    nrf_put8(n, NRF_RX_PW_P3, 0);
    nrf_put8(n, NRF_RX_PW_P4, 0);
    nrf_put8(n, NRF_RX_PW_P5, 0);

    nrf_put8(n, NRF_SETUP_AW, nrf_get_addr_nbytes(n)-2);
    nrf_put8(n, NRF_SETUP_RETR, nrf_default_retran_attempts + ((nrf_default_retran_delay / 250 - 1)<<4));
    nrf_put8(n, NRF_FIFO_STATUS, 0b10001);
    nrf_put8(n, NRF_RF_CH, c.channel);
    nrf_put8(n, NRF_RF_SETUP, nrf_default_db | nrf_default_data_rate);

    // nrf_rx_flush(n);
    // nrf_tx_flush(n);

    nrf_set_pwrup_on(n);
    delay_ms(2);
    nrf_put8(n, NRF_CONFIG, rx_config);
    gpio_set_on(n->config.ce_pin);

    delay_ms(2);
        
    nrf_rx_flush(n);
    nrf_tx_flush(n);

    if (USE_TX_INTERRUPT) {
        // assert(tx_config == (enable_crc | crc_two_byte | set_bit(PWR_UP)));
        // must clear first!!!!
        nrf_tx_intr_clr(n);
        nrf_rt_intr_clr(n);
    }
    if (USE_RX_INTERRUPT) {
        nrf_rx_intr_clr(n);
    }
    if (USE_RX_INTERRUPT || USE_TX_INTERRUPT) {
        nrf_interrupt_init(c.int_pin);
        printk("interrupt set\n");
    }

    // should be true after setup.
    if(acked_p) {
        nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
        nrf_opt_assert(n, nrf_pipe_is_enabled(n, 0));
        nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
        nrf_opt_assert(n, nrf_pipe_is_acked(n, 0));
        nrf_opt_assert(n, nrf_pipe_is_acked(n, 1));
        nrf_opt_assert(n, nrf_tx_fifo_empty(n));
    } else {
        nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
        nrf_opt_assert(n, !nrf_pipe_is_enabled(n, 0));
        nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
        nrf_opt_assert(n, !nrf_pipe_is_acked(n, 1));
        nrf_opt_assert(n, nrf_tx_fifo_empty(n));
    }
    assert(!nrf_tx_fifo_full(n));
    assert(nrf_tx_fifo_empty(n));
    assert(!nrf_rx_fifo_full(n));
    assert(nrf_rx_fifo_empty(n));
    return n;
}

int nrf_tx_send_ack(nrf_t *n, uint32_t txaddr, 
    const void *msg, unsigned nbytes) {
    if (USE_TX_INTERRUPT) {
        return nrf_tx_send_ack_int(n, txaddr, msg, nbytes);
    }

    int res = 0;

    // default config for acked state.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
    nrf_opt_assert(n, nrf_pipe_is_acked(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_acked(n, 1));
    nrf_opt_assert(n, nrf_tx_fifo_empty(n));

    if (!USE_RX_INTERRUPT) {
        // if interrupts not enabled: make sure we check for packets.
        while(nrf_get_pkts(n))
            ;
    }

    nrf_tx_flush(n);
    nrf_tx_flush(n);

    // TODO: you would implement the rest of the code at this point.
    // int res = staff_nrf_tx_send_ack(n, txaddr, msg, nbytes);
    // set to tx mode
    gpio_set_off(n->config.ce_pin);
    nrf_put8(n, NRF_CONFIG, tx_config);
    nrf_set_addr(n, NRF_TX_ADDR, txaddr, nrf_default_addr_nbytes);
    nrf_set_addr(n, NRF_RX_ADDR_P0, txaddr, nrf_default_addr_nbytes);
    gpio_set_on(n->config.ce_pin);
    delay_us(500);

    // set addr

    if (!nrf_putn(n, NRF_W_TX_PAYLOAD, msg, nbytes)) return -1;

    // gpio_set_off(n->config.ce_pin);
    // gpio_set_on(n->config.ce_pin);


     while (!nrf_tx_fifo_empty(n)) {
        if (nrf_has_max_rt_intr(n)) {
            nrf_tx_intr_clr(n);
            nrf_rt_intr_clr(n);
            nrf_dump("max inter", n);
            panic("max intr\n");
        }

        if (nrf_rx_fifo_full(n)) {  // why?
            nrf_tx_intr_clr(n);
            panic("rx fifo full \n");
        }

        if (nrf_has_tx_intr(n)) {
            nrf_tx_intr_clr(n);
            res = nbytes;
        }
    }

    // uint8_t cnt = nrf_get8(n, NRF_OBSERVE_TX);
    // n->tot_retrans  += bits_get(cnt,0,3);

    // tx interrupt better be cleared.
    if (nrf_has_tx_intr(n)) {
        nrf_tx_intr_clr(n);
        res = nbytes;
    }

    gpio_set_off(n->config.ce_pin);
    nrf_put8(n, NRF_CONFIG, rx_config);
    gpio_set_on(n->config.ce_pin);
    delay_us(500);
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return res;
}

int nrf_tx_send_ack_int(nrf_t *n, uint32_t txaddr, const void *msg, unsigned nbytes) {
    int res = 0;

    // default config for acked state.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
    nrf_opt_assert(n, nrf_pipe_is_acked(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_acked(n, 1));
    nrf_opt_assert(n, nrf_tx_fifo_empty(n));

    if (!USE_RX_INTERRUPT) {
        // if interrupts not enabled: make sure we check for packets.
        while(nrf_get_pkts(n))
            ;
    }

    // nrf_tx_flush(n);
    // nrf_tx_flush(n);

    // TODO: you would implement the rest of the code at this point.
    // int res = staff_nrf_tx_send_ack(n, txaddr, msg, nbytes);
    // set to tx mode
    cpsr_int_disable();
    gpio_set_off(n->config.ce_pin);
    nrf_put8(n, NRF_CONFIG, tx_config);
    if (!nrf_putn(n, NRF_W_TX_PAYLOAD, msg, nbytes)) return -1;
    nrf_set_addr(n, NRF_TX_ADDR, txaddr, nrf_default_addr_nbytes);
    nrf_set_addr(n, NRF_RX_ADDR_P0, txaddr, nrf_default_addr_nbytes);
    gpio_set_on(n->config.ce_pin);
    delay_us(500);

    // set addr

    
    cpsr_int_enable();

    // make_system_call(NRF_START_SEND, n, tx)

    // gpio_set_off(n->config.ce_pin);
    // gpio_set_on(n->config.ce_pin);


    //  while (!nrf_tx_fifo_empty(n)) {
    //     if (nrf_has_max_rt_intr(n)) {
    //         nrf_tx_intr_clr(n);
    //         nrf_rt_intr_clr(n);
    //         nrf_dump("max inter", n);
    //         panic("max intr\n");
    //     }

    //     if (nrf_rx_fifo_full(n)) {  // why?
    //         nrf_tx_intr_clr(n);
    //         panic("rx fifo full \n");
    //     }

    //     // if (nrf_has_tx_intr(n)) {
    //     //     nrf_tx_intr_clr(n);
    //     //     res = nbytes;
    //     // }
    // }

    // uint8_t cnt = nrf_get8(n, NRF_OBSERVE_TX);
    // n->tot_retrans  += bits_get(cnt,0,3);

    // tx interrupt better be cleared.
    // if (nrf_has_tx_intr(n)) {
    //     nrf_tx_intr_clr(n);
    //     res = nbytes;
    // }
    while (n->tx_sent != nbytes)
        ;
    n->tx_sent = 0;
    res = nbytes;

    return res;
}

void spi_transfer_syscall_handler(unsigned pc, unsigned sp, nrf_t *n, uint32_t txaddr, void *msg, unsigned nbytes) {
    msg = ((void **)sp)[2];
    nbytes = ((unsigned *)sp)[3];
    gpio_set_off(n->config.ce_pin);
    nrf_put8(n, NRF_CONFIG, tx_config);
    gpio_set_on(n->config.ce_pin);
    delay_ms(2);

    nrf_set_addr(n, NRF_TX_ADDR, txaddr, nrf_default_addr_nbytes);

    nrf_putn(n, W_TX_PAYLOAD_NO_ACK, msg, nbytes);

    gpio_set_off(n->config.ce_pin);
    gpio_set_on(n->config.ce_pin);

    while (!nrf_tx_fifo_empty(n));

    nrf_tx_intr_clr(n);

    // tx interrupt better be cleared.
    nrf_opt_assert(n, !nrf_has_tx_intr(n));

    nrf_set_pwrup_off(n);
    gpio_set_off(n->config.ce_pin);
    nrf_set_pwrup_on(n);
    delay_us(500);
    nrf_put8(n, NRF_CONFIG, rx_config);
    gpio_set_on(n->config.ce_pin);
    delay_us(500);
}

int nrf_tx_send_noack(nrf_t *n, uint32_t txaddr, 
    const void *msg, unsigned nbytes) {
    if (USE_TX_INTERRUPT) {
        return nrf_tx_send_noack_int(n, txaddr, msg, nbytes);
    }

    // default state for no-ack config.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    // nrf_opt_assert(n, !nrf_pipe_is_enabled(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
    // nrf_opt_assert(n, !nrf_pipe_is_acked(n, 1));
    nrf_opt_assert(n, nrf_tx_fifo_empty(n));

    if (!USE_RX_INTERRUPT) {
        // if interrupts not enabled: make sure we check for packets.
        while(nrf_get_pkts(n))
            ;
    }

    // TODO: you would implement the code here.
    // int res = staff_nrf_tx_send_noack(n, txaddr, msg, nbytes);
    make_system_call(NRF_SPI_TRANSFER, n, txaddr, msg, nbytes);
   

    // better be back in rx mode.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return nbytes;
}

int nrf_tx_send_noack_int(nrf_t *n, uint32_t txaddr, const void *msg, unsigned nbytes) {
    // default state for no-ack config.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    // nrf_opt_assert(n, !nrf_pipe_is_enabled(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
    // nrf_opt_assert(n, !nrf_pipe_is_acked(n, 1));
    nrf_opt_assert(n, nrf_tx_fifo_empty(n));

    if (!USE_RX_INTERRUPT) {
        // if interrupts not enabled: make sure we check for packets.
        while(nrf_get_pkts(n))
            ;
    }

    cpsr_int_disable();
    gpio_set_off(n->config.ce_pin);
    nrf_put8(n, NRF_CONFIG, tx_config);
    gpio_set_on(n->config.ce_pin);
    delay_ms(2);

    nrf_set_addr(n, NRF_TX_ADDR, txaddr, nrf_default_addr_nbytes);

    if (!nrf_putn(n, W_TX_PAYLOAD_NO_ACK, msg, nbytes)) return -1;
    gpio_set_off(n->config.ce_pin);
    gpio_set_on(n->config.ce_pin);
    cpsr_int_enable();

    while (n->tx_sent != nbytes)
        ;
    n->tx_sent = 0;
    // res = nbytes;

    nrf_opt_assert(n, !nrf_has_tx_intr(n));
    

    // better be back in rx mode.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return 1;
}


int nrf_get_pkts(nrf_t *n) {
    if (USE_RX_INTERRUPT) {
        return nrf_get_pkts_int(n);
    }
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);

    // TODO:
    // data sheet gives the sequence to follow to get packets.
    // p63: 
    //    1. read packet through spi.
    //    2. clear IRQ.
    //    3. read fifo status to see if more packets: 
    //       if so, repeat from (1) --- we need to do this now in case
    //       a packet arrives b/n (1) and (2)
    // done when: nrf_rx_fifo_empty(n)
    // int res = staff_nrf_get_pkts(n);
    int cnt = 0;
    while (!nrf_rx_fifo_empty(n))
    {
        unsigned pipeid = nrf_rx_get_pipeid(n);
        // assert(pipeid == 1);
        uint8_t msg[n->config.nbytes];
        uint8_t status = nrf_getn(n, NRF_R_RX_PAYLOAD, &msg, n->config.nbytes);
        if (!cq_push_n(&n->recvq, &msg, n->config.nbytes)) return -1;
        n->tot_sent_msgs ++;
        n->tot_sent_bytes += n->config.nbytes;
        cnt ++;
        nrf_rx_intr_clr(n);
    }

    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return cnt;
}

int nrf_get_pkts_int(nrf_t *n) {
    return 0;
}
