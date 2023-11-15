#include "dfs-common.h"
#include "global.h"
#include "nrf-hw-support.h"
#include "nrf-router.h"
#include "nrf-interrupt.h"
#include "dfs-master.h"


nrf_t *dfs_nrf_init(nrf_conf_t c, uint32_t rx_addr) {
    if (!set_nrf1_ready()) {
        if (!set_nrf2_ready()) {
            return NULL;
        }
    }
    nrf_t *n = kmalloc(sizeof(nrf_t));
    n->nrf_id = c.id;
    n->config = c;
    nrf_stat_start(n);
    n->spi = pin_init(c.ce_pin, c.spi_chip);

    n->tx_sent = 0;

    nrf_set_pwrup_off(n);

    // setup rx pipe 1 for receiving register packets and pipe 0 for auto acking
    nrf_put8(n, NRF_EN_RXADDR, 0b10); 
    nrf_put8(n, NRF_EN_AA, 0b0);
    // nrf_put8(n, NRF_FEATURE, 0b01); // enable no ack
    n->rxaddr[1] = rx_addr;
    nrf_set_addr(n, NRF_RX_ADDR_P1, n->rxaddr[1], nrf_default_addr_nbytes);
    nrf_set_addr(n, NRF_TX_ADDR, 0, nrf_default_addr_nbytes);
    nrf_put8(n, NRF_RX_PW_P1, n->config.nbytes);
    nrf_put8(n, NRF_RX_PW_P2, 0);
    nrf_put8(n, NRF_RX_PW_P3, 0);
    nrf_put8(n, NRF_RX_PW_P4, 0);
    nrf_put8(n, NRF_RX_PW_P5, 0);

    nrf_put8(n, NRF_SETUP_AW, nrf_get_addr_nbytes(n)-2);
    nrf_put8(n, NRF_SETUP_RETR, nrf_default_retran_attempts + ((nrf_default_retran_delay / 250 - 1)<<4));
    nrf_put8(n, NRF_FIFO_STATUS, 0b10001);
    nrf_put8(n, NRF_RF_CH, n->config.channel);
    nrf_put8(n, NRF_RF_SETUP, nrf_default_db | nrf_default_data_rate);

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
    }

    delay_ms(2);

    // should be true after setup.
    assert(!nrf_tx_fifo_full(n));
    assert(nrf_tx_fifo_empty(n));
    assert(!nrf_rx_fifo_full(n));
    assert(nrf_rx_fifo_empty(n));
    assert(n->nrf_id == 0 || n->nrf_id == 1);

    return n;
}

int dfs_wait_recv(nrf_t *n) {
    char data[dfs_get_pkt_bytes()];
    int ret = nrf_read_exact_timeout(n, data, dfs_get_pkt_bytes(), dfs_master_ack_timeout_us);
    if (ret == -1) {
        trace("Receive time out\n");
        return -1;
    } else if (ret != dfs_get_pkt_bytes()) {
        trace("Received incomplete data\n");
        return -1;
    }
    return 0;
}