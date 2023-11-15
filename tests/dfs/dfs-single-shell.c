#include "rpi.h"
#include "dfs-master-fat.h"
#include "nrf-test.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"
#include "rpi-interrupts.h"
#include "dfs-client-command.h"

#define MODE 1

#if MODE == 0
#include "dfs-master.h"
static void master_main_start(void *arg) {
    dfs_master_init_t *master_init = arg;
    _thread_safe_printk("start master\n");
    dfs_master_start(master_init);
    _thread_safe_printk("end master\n");
}

#elif MODE == 1
#include "dfs-client.h"




#else 
#include "dfs-chunkserver.h"
#endif

void notmain() {

#if MODE == 0

    dfs_master_t *master = (dfs_master_t*)kmalloc(sizeof(dfs_master_t));
    master->addr = dfs_master_addr;
    master->num_chunkserver = 1;
    dfs_master_init_t *master_init = (dfs_master_init_t*)kmalloc(sizeof(dfs_master_init_t));
    master_init->master = master;
    master_init->config = server_conf(dfs_get_pkt_bytes());
    pre_fork(master_main_start, (void *)master_init, 1, 1);

    pre_thread_start();
    trace("SUCCESS!\n");

#elif MODE == 1

    dfs_client_t *client = (dfs_client_t *)kmalloc(sizeof(dfs_client_t));
    client->addr = dfs_client_addr;
    dfs_client_init_t *client_init = (dfs_client_init_t *)kmalloc(sizeof(dfs_client_init_t));
    client_init->client = client;
    client_init->config = client_conf(dfs_get_pkt_bytes());
    pre_fork(client_shell_start, (void *)client_init, 1, 1);

    pre_thread_start();
    trace("SUCCESS!\n");
#else 

    dfs_chunkserver_t *chunk = dfs_init_chunkserver(server_conf(dfs_get_pkt_bytes()), dfs_chunk_addr1);
    chunk->id = 0; 
    dfs_chunkserver_start(chunk);
    
#endif
}