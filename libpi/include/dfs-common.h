#ifndef __DFS_COMMON_H__
#define __DFS_COMMON_H__

#include "nrf.h"
#include "nrf-hw-support.h"
#include "pi-random.h"
#include "nrf-router.h"
#include "thread-safe-io.h"

#define NRF_LEFT 0
#define NRF_RIGHT 1

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

nrf_t *dfs_nrf_init(nrf_conf_t c, uint32_t rx_addr);
int dfs_wait_recv(nrf_t *n);


// HEARTBEAT LIGHT
static void hb_light_blink(uint32_t hb_led) {
    dev_barrier();
    gpio_set_output(hb_led);
    gpio_set_on(hb_led);
    delay_ms(1000);
    gpio_set_off(hb_led);
    dev_barrier();
}

// Tracing utility
#define dfs_trace_p 0  // global trace  
#define dfs_log_p 0 // switch log or debug, log=1

// when dfs_trace_p is off, can turn individual ones on
#define dfs_master_p 1
#define dfs_chunk_p 1
#define dfs_client_p 0
#define dfs_hb_p 1

#define dfs_master_trace(args...) do { \
    if (dfs_master_p == 0 && dfs_trace_p == 0) { \
        break; \
    } \
    if (dfs_log_p == 1) { \
         _thread_safe_printk("DFS_MASTER_LOG: "); _thread_safe_printk(args); \
         break; \
    } \
    _thread_safe_printk("DFS_MASTER_DEBUG: %s:", __FUNCTION__); _thread_safe_printk(args);  \
} while(0) 


#define dfs_chunk_trace(args...) do { \
    if (dfs_chunk_p == 0 && dfs_trace_p == 0) { \
        break; \
    } \
    if (dfs_log_p == 1) { \
         _thread_safe_printk("DFS_CHUNK_LOG: "); _thread_safe_printk(args); \
         break; \
    } \
    _thread_safe_printk("DFS_CHUNK_DEBUG: %s:", __FUNCTION__); _thread_safe_printk(args); \
} while(0)


#define dfs_client_trace(args...) do { \
    if (dfs_client_p == 0 && dfs_trace_p == 0) { \
        break; \
    } \
    if (dfs_log_p == 1) { \
         _thread_safe_printk("DFS_CLIENT_LOG: "); _thread_safe_printk(args); \
         break; \
    } \
    _thread_safe_printk("DFS_CLIENT_DEBUG: %s:", __FUNCTION__); _thread_safe_printk(args); \
} while(0)

#define dfs_hb_trace(args...) do { \
    if (dfs_hb_p == 0 && dfs_trace_p == 0) { \
        break; \
    } \
    if (dfs_log_p == 1) { \
         _thread_safe_printk("DFS_HB_LOG: "); _thread_safe_printk(args); \
         break; \
    } \
    _thread_safe_printk("DFS_HB_DEBUG: %s:", __FUNCTION__); _thread_safe_printk(args); \
} while(0)

void spi_transfer_syscall_handler(unsigned pc, unsigned sp, nrf_t *n, uint32_t txaddr, void *msg, unsigned nbytes);

#endif