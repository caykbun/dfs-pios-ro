#include "nrf-router.h"
#include "nrf-test.h"

enum { ntrial = 1000, timeout_usec = 1000000, nbytes = 32 };

#define CODE_START 0x10000000
#define CODE_END   0x11111111

void notmain(void) {
    // WORKER SIDE
    nrf_conf_t server_1_config = server_conf(nrf_rt_get_pkt_bytes());
    nrf_conf_t c_2_config = client_conf(nrf_rt_get_pkt_bytes());
    nrf_t *s1 = nrf_init_node(server_1_config);
    nrf_t *c2 = nrf_init_node(c_2_config);
    trace(" connection open from node=[%x] %x\n", c2->rxaddr[1], c2->config.spi_chip);
    if (nrf_rt_try_register(c2, 0xd8d8d8) == -1) {
        panic("try register failed\n");
    } 
    trace(" connection open from node=[%x] %x\n", s1->rxaddr[1], s1->config.spi_chip);
    if (nrf_rt_try_register(s1, 0xd9d9d9) == -1) {
        panic("try register failed\n");
    } 
    
    // ==============================================================
    // ROUTER SIDE
    
    // nrf_t * router_ptx = nrf_init_router_ptx();
    // nrf_t * router_prx = nrf_init_router_prx();


    // nrf_stat_start(router_ptx);
    // nrf_stat_start(router_prx);

    // if (nrf_rt_open_registration(router_ptx, router_prx) == 0) {
    //     panic("No node registered!\n");
    // }

    // emit all the stats.
    // nrf_stat_print(router_ptx, "server: done with one-way test");
    // nrf_stat_print(router_prx, "client: done with one-way test");

}