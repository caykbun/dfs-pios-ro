#include "nrf-test.h"

enum { ntrial = 1000, timeout_usec = 1000000, nbytes = 32 };

// max message size.
typedef struct {
    uint32_t data[NRF_PKT_MAX/4];
} data_t;
_Static_assert(sizeof(data_t) == NRF_PKT_MAX, "invalid size");

#define CODE_START 0x10000000
#define CODE_END   0x11111111

static inline int send32_ack(nrf_t *nic, uint32_t txaddr, uint32_t x) {
    assert(nbytes>=4);
    data_t d = {0};
    d.data[0] = x;
    return nrf_send_ack(nic, txaddr, &d, nbytes) == nbytes;
}

static inline int recv32(nrf_t *nic, uint32_t *out) {
    assert(nbytes>=4);
    data_t d;
    int ret = nrf_read_exact_timeout(nic, &d, nbytes, timeout_usec);
    assert(ret<= nbytes);
    if(ret == nbytes)
        memcpy(out, &d, 4);
    return ret;
}

static void
one_way_ack_exec(nrf_t *server, nrf_t *client, int verbose_p) {
    uint32_t msg;
    int ret;
    uint32_t addr[10];
    uint32_t *code = &addr[0];
    int len = 0;
    while (1)
    {
        ret = recv32(client, &msg);
        trace("ret=%d, got %x\n", ret, msg);
        if (ret < 0) {
            trace("receive failed\n");
        } else {
            if (msg == CODE_START) {
                trace("Start receiving code!!!!\n");
            }
            if (msg == CODE_END) {
                trace("Received all code -- start executing!!!!\n");
                volatile uint32_t p = (0x200000);
                *(code + len) = 0;
                void (*foo)(unsigned, unsigned) = (void (*)())(code);
                foo(p, 10);
                trace("GET32 :%d \n", GET32(p));
                return;
            }
            *(code + len) = msg;
            len++;
        }
    }
}

static void
one_way_ack_put(nrf_t *server, nrf_t *client, int verbose_p, uint32_t *code, unsigned len) {
    unsigned client_addr = client->rxaddr;
    unsigned ntimeout = 0, npackets = 0;

    if(verbose_p)
        trace("sent code_start ack'd packets\n");

    if(!send32_ack(server, client_addr, CODE_START))
        panic("send failed\n");

    for (unsigned i = 0; i < len; i++) {
        if(verbose_p)
            trace("sent code %x ack'd packets\n", (uint32_t)(*(code + i)));

        if(!send32_ack(server, client_addr, (uint32_t)(*(code + i))))
            panic("send failed\n");
    }

    if(verbose_p)
        trace("sent code_end ack'd packets\n");

    if(!send32_ack(server, client_addr, CODE_END))
        panic("send failed\n");
}

void notmain(void) {
    nrf_t *s = server_mk_ack(server_addr, nbytes);
    nrf_t *c = client_mk_ack(client_addr, nbytes);

    nrf_stat_start(s);
    nrf_stat_start(c);

    // uint32_t code[2];
    // code[0] = 0xe5801000;
    // code[1] = 0xe12fff1e;

    // run test.
    //one_way_ack_put(s, c, 1, &code[0], 2);
    one_way_ack_exec(s, c, 0);

}