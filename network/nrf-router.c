#include "nrf-router.h"
#include "nrf-test.h"
#include "pi-random.h"

// enable crc, enable 2 byte
#   define set_bit(x) (1<<(x))
#   define enable_crc      set_bit(3)
#   define crc_two_byte    set_bit(2)
#   define mask_int         set_bit(6)|set_bit(5)|set_bit(4)
enum {
    // pre-computed: can write into NRF_CONFIG to enable TX.
    tx_config = enable_crc | crc_two_byte | set_bit(PWR_UP)
    #if USE_TX_INTERRUPT == 0
    | set_bit(5) | set_bit(4)
    #endif
    #if USE_RX_INTERRUPT == 0
    | set_bit(6)
    #endif
    ,
    // pre-computed: can write into NRF_CONFIG to enable RX.
    rx_config = tx_config  | set_bit(PRIM_RX)
} ;

// should move to nrf-driver.c
// set a single rx 

// global config
void nrf_rt_init_global(nrf_t* n) {
    nrf_put8(n, NRF_SETUP_AW, nrf_get_addr_nbytes(n)-2);
    nrf_put8(n, NRF_SETUP_RETR, nrf_default_retran_attempts + ((nrf_default_retran_delay / 250 - 1)<<4));
    nrf_put8(n, NRF_FIFO_STATUS, 0b10001);
    nrf_put8(n, NRF_RF_CH, n->config.channel);
    nrf_put8(n, NRF_RF_SETUP, nrf_default_db | nrf_default_data_rate);

    nrf_rx_flush(n);
    nrf_tx_flush(n);

    nrf_set_pwrup_on(n);
    delay_ms(2);
    nrf_put8(n, NRF_CONFIG, rx_config);
    gpio_set_on(n->config.ce_pin);

    delay_ms(2);

    // should be true after setup.
    assert(!nrf_tx_fifo_full(n));
    assert(nrf_tx_fifo_empty(n));
    assert(!nrf_rx_fifo_full(n));
    assert(nrf_rx_fifo_empty(n));
}

// the router side nrf chip used for transmitting data 
nrf_t *  nrf_init_router_ptx() {
    nrf_conf_t c = server_conf(nrf_rt_get_pkt_bytes());
    nrf_t *n = kmalloc(sizeof *n);
    n->config = c;
    nrf_stat_start(n);
    n->spi = pin_init(c.ce_pin, c.spi_chip);

    nrf_set_pwrup_off(n);

    // setup rx pipe 1 for receiving register packets and pipe 0 for auto acking
    nrf_put8(n, NRF_EN_RXADDR, 0b11); 
    nrf_put8(n, NRF_EN_AA, 0b111111);
    nrf_put8(n, NRF_DYNPD, 0b111111);
    nrf_put8(n, NRF_FEATURE, 0b101);
    n->rxaddr[1] = router_ptx_addr;
    nrf_set_addr(n, NRF_RX_ADDR_P1, n->rxaddr[1], nrf_default_addr_nbytes);
    nrf_put8(n, NRF_RX_PW_P1, n->config.nbytes);
    
    nrf_enable_rx(n, 1);    

    nrf_rt_init_global(n);

    return n;
}

// the router side nrf chip used for receiving data 
nrf_t *  nrf_init_router_prx() {
    nrf_conf_t c = client_conf(nrf_rt_get_pkt_bytes());
    nrf_t *n = kmalloc(sizeof *n);
    n->config = c;
    nrf_stat_start(n);
    n->spi = pin_init(c.ce_pin, c.spi_chip);

    nrf_set_pwrup_off(n);

    // setup rx pipes
    for (int i = 0; i < 6; i++) {
        n->rxaddr[i] = router_prx_addr[i];
        nrf_set_addr(n, NRF_RX_ADDR_P0 + i, n->rxaddr[i], nrf_default_addr_nbytes);
        nrf_put8(n, NRF_RX_PW_P0 + i, n->config.nbytes);
    }
    nrf_put8(n, NRF_EN_AA, 0b111111);  // turn on auto acking for all rx pipes
    // TODO: enable required to set registers?
   
    nrf_rt_init_global(n);
    
    return n;
}

/*
try assigning this client to a rx pipe on the router's prx side and return node_id 
for this client if a collision detected or all rx pipes are occupied, return -1
*/ 
static int try_assignment(nrf_t* prx, uint32_t node_chosen_addr) {
    if (prx->n_conns == 6) {
        printk("Assignment slots full\n");
        return -1;
    }

    int first_idle = 0;
    for (int i = 1; i < 6; i++) {
        if (node_addr[i] == node_chosen_addr) {
            // collision detected, disable this rx 
            gpio_set_off(prx->config.ce_pin);
            nrf_disable_rx(prx, i);
            gpio_set_on(prx->config.ce_pin);
            delay_us(130);

            node_addr[i] = 0;
            prx->n_conns--;
            return -1;
        } else if (first_idle == 0) {
            first_idle = i;
        }
    }

    gpio_set_off(prx->config.ce_pin);
    nrf_enable_rx(prx, first_idle);
    gpio_set_on(prx->config.ce_pin);
    delay_us(130);

    node_addr[first_idle] = node_chosen_addr;
    prx->n_conns++;
    return first_idle;
}

void cancel_assignment(nrf_t* prx, uint32_t node_id) {
    if (node_addr[node_id] == 0) return;

    gpio_set_off(prx->config.ce_pin);
    nrf_disable_rx(prx, node_id);
    gpio_set_on(prx->config.ce_pin);
    delay_us(130);

    node_addr[node_id] = 0;
    prx->n_conns--;
}

// only using one nrf chip on nodes for now
nrf_t* nrf_init_node(nrf_conf_t c) {
    nrf_t *n = kmalloc(sizeof *n);
    n->config = c;
    nrf_stat_start(n);
    n->spi = pin_init(c.ce_pin, c.spi_chip);

    nrf_set_pwrup_off(n);

    // setup rx pipe P1 for receiving, P0 for auto acking
    nrf_put8(n, NRF_EN_RXADDR, 0b11); 
    nrf_put8(n, NRF_EN_AA, 0b11);
    n->rxaddr[1] = node_init_addr;
    nrf_set_addr(n, NRF_RX_ADDR_P1, n->rxaddr[1], nrf_default_addr_nbytes);
    nrf_put8(n, NRF_RX_PW_P1, n->config.nbytes);
   
    nrf_rt_init_global(n);
    
    return n;
}

/* Router opens to registration from nodes
1. server -> client: nrf_rt_open, server_init_rxaddr
2. client -> server: 
    - client choose an rx address and reset its p1 to that address
    - client set tx to server_init_rxaddr
    - nrf_rt_reg, client rxaddr (new)
3. server -> client: 
    - check collision
    - If no collision:
        - add to addr-pipe mappings
        - nrf_rt_acp, server rxaddr (for a particular client)  // Do we need the forth handshake?
    - Else:
        - remove both from mappings // TODO: tell the clients first?
        - nrf_rt_rej, 0 
4. client:
    - If nrf_rt_acp:
        - set tx to server rxaddr from packet
    - If nrf_rt_rej:
        - close connection or back to step 1 and retry
*/

// return number of established connections, -1 on error
int nrf_rt_open_registration(nrf_t* ptx, nrf_t* prx) {
    nrf_rt_pkt send_pkt;
    send_pkt.op_code = NRF_RT_OPEN;
    send_pkt.payload = ptx->rxaddr[1];

    // 1. broadcast open connection packets
    for (int i = 0; i < nrf_rt_open_window || prx->n_conns == 6; i++) {
        nrf_send_noack(ptx, node_init_addr, (void*)(&send_pkt), nrf_rt_get_pkt_bytes());
        printk("sending packet %d\n", i);

        char data[nrf_rt_get_pkt_bytes()];
        int ret = nrf_read_exact_timeout(ptx, data, nrf_rt_get_pkt_bytes(), nrf_rt_timeout_us_1);  // use ptx to receive register packets here because 
        printk("received a packet\n");

        if (!(ret == nrf_rt_get_pkt_bytes())) {
            printk("Wrong packet length, received %d bytes\n", ret);
            continue;
        }
        
        // received one packet
        nrf_rt_pkt* recv_pkt = (nrf_rt_pkt*)data; 
        if (recv_pkt->op_code != NRF_RT_REG) {
            printk("Bad Opcode\n");
            continue;
        }

        int node_id = try_assignment(prx, recv_pkt->payload);
        if (node_id >=0) {  
            // successfully assigned a rx pipe to this client -> 3. reply with acp
            nrf_rt_pkt acp_pkt = {NRF_RT_ACP, prx->rxaddr[node_id], node_id};
            ret = nrf_send_ack(ptx, recv_pkt->payload, (void*)(&acp_pkt), nrf_rt_get_pkt_bytes());
            printk("Sent ACP packet to address %x\n", recv_pkt->payload);

            if (ret != nrf_rt_get_pkt_bytes()) {  
                // should cancel assignment
                cancel_assignment(prx, node_id);
            }
        } else {
            // otherwise, 3. reply with rej
            nrf_rt_pkt rej_pkt = {NRF_RT_REJ, 0, 0};
            ret = nrf_send_ack(ptx, recv_pkt->payload, (void*)(&rej_pkt), nrf_rt_get_pkt_bytes()); 
            printk("Sent REJ packet to address %x\n", recv_pkt->payload);
        }
    }

    return prx->n_conns;
}

// Node tries to register for an address from the router
int nrf_rt_try_register(nrf_t* node, uint32_t chosen_addr) {
    char data[nrf_rt_get_pkt_bytes()];
    int ret = nrf_read_exact_timeout(node, data, nrf_rt_get_pkt_bytes(), nrf_rt_timeout_us_2);
    if (ret != nrf_rt_get_pkt_bytes()) return -1;

    nrf_rt_pkt* recv_pkt = (nrf_rt_pkt*) data;
    printk("READ -- ret = (%d) data = %x \n", ret, recv_pkt->op_code);
    if (recv_pkt->op_code == NRF_RT_OPEN) {  // is open packet
        // 2. choose address, reset rx p1 address, and send register packet to the router
        
        // do {
        //     chosen_addr = pi_random();  // the implementation says this is broken...
        // } while (chosen_addr == 0);
        gpio_set_off(node->config.ce_pin);
        nrf_rx_flush(node);
        node->rxaddr[1] = chosen_addr;
        nrf_set_addr(node, NRF_RX_ADDR_P1, node->rxaddr[1], nrf_default_addr_nbytes);
        gpio_set_on(node->config.ce_pin);
        delay_us(130);
        nrf_rx_flush(node);

        nrf_rt_pkt reg_pkt = {NRF_RT_REG, chosen_addr, 0};
        int ret = nrf_send_ack(node, router_ptx_addr, (void*)(&reg_pkt), nrf_rt_get_pkt_bytes());
        printk("Sending %d \n", ret);
        // if (ret != nrf_rt_get_pkt_bytes()) return -1;
    }

    // 4. wait for accept/reject packet
    for (int i = 0; i < 100; i ++) {
        ret = nrf_read_exact_timeout(node, data, nrf_rt_get_pkt_bytes(), nrf_rt_timeout_us_2);


        // if (ret != nrf_rt_get_pkt_bytes()) return -1;
        recv_pkt = (nrf_rt_pkt*) data;
        printk("READ -- ret = (%d) data = %x payload %x \n", ret, recv_pkt->op_code, recv_pkt->payload);
        if (recv_pkt->op_code == NRF_RT_ACP) {
            // set tx, p0 address to prx router address 
                gpio_set_off(node->config.ce_pin);
                nrf_set_addr(node, NRF_TX_ADDR, recv_pkt->payload, nrf_default_addr_nbytes);
                nrf_set_addr(node, NRF_RX_ADDR_P0, recv_pkt->payload, nrf_default_addr_nbytes);
                nrf_put8(node, NRF_EN_RXADDR, 0b11); 
                nrf_put8(node, NRF_EN_AA, 0b11);
                gpio_set_on(node->config.ce_pin);
                delay_us(130);

            return 0;
        }
        if (recv_pkt->op_code != NRF_RT_OPEN) {
            return -1;
        }
    }
    

    
    return -1;
}