#ifndef __NRF_ROUTER_H__
#define __NRF_ROUTER_H__

#include "nrf.h"
#include "nrf-hw-support.h"

#define router_ptx_addr 0x7d7d7d
#define node_init_addr 0xd7d7d7

#define nrf_rt_open_window 100
#define nrf_rt_timeout_us_1 1000000
#define nrf_rt_timeout_us_2 10000000
#define nrf_rt_ack_timeout_us 10000000

#define USE_TX_INTERRUPT 0
#define USE_RX_INTERRUPT 1

enum {
    NRF_RT_OPEN = 0x12121212,
    NRF_RT_REG = 0x23232323,
    NRF_RT_ACP = 0x34343434,
    NRF_RT_REJ = 0x45454545,
};

typedef struct {
    uint32_t op_code;
    uint32_t payload;
    uint8_t node_id;
} nrf_rt_pkt;

static uint32_t router_prx_addr[6] = {0xa1a1a1, 0xb2b2b2, 0xc3c3c3, 0xd4d4d4, 0xe5e5e5, 0xf6f6f6};
static uint32_t node_addr[6];

static inline uint32_t nrf_rt_get_pkt_bytes() {
    return sizeof(nrf_rt_pkt);
}

nrf_t *  nrf_init_router_ptx();
nrf_t *  nrf_init_router_prx();
nrf_t* nrf_init_node(nrf_conf_t c);
int nrf_rt_open_registration(nrf_t* ptx, nrf_t* prx);
int nrf_rt_try_register(nrf_t* node, uint32_t chosen_addr);


#endif