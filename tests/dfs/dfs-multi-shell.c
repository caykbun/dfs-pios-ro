#include "rpi.h"
#include "dfs-master-fat.h"
#include "nrf-test.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"
#include "rpi-interrupts.h"
#include "dfs-client-command.h"

#define MODE 1

#if MODE == 0 // MASTER + CHUNK
#include "dfs-master.h"
#include "dfs-chunkserver.h"

static void dfs_master() {
    dfs_master_t *master = (dfs_master_t*)kmalloc(sizeof(dfs_master_t));
    master->addr = dfs_master_addr;
    master->num_chunkserver = 1;
    dfs_init_master(master, server_conf(dfs_get_pkt_bytes()));
    dfs_master_start(master);	
}

static void dfs_chunkserver(int id, uint32_t addr) {
    dfs_chunkserver_t *chunkserver = (dfs_chunkserver_t*)kmalloc(sizeof(dfs_chunkserver_t));
    chunkserver->addr = addr;
    chunkserver->id = id; 
    dfs_init_chunkserver(chunkserver, client_conf(dfs_get_pkt_bytes()));
    dfs_chunkserver_start(chunkserver);
}

#elif MODE == 1 // CLIENT + CHUNK
#include "dfs-client.h"
#include "dfs-chunkserver.h"

static void dfs_client(void *arg) {
    dfs_client_t *client = (dfs_client_t *)kmalloc(sizeof(dfs_client_t));
    client->addr = dfs_client_addr;
    dfs_client_init_t *client_init = (dfs_client_init_t *)kmalloc(sizeof(dfs_client_init_t));
    client_init->client = client;
    client_init->config = client_conf(dfs_get_pkt_bytes());
    pre_fork(client_shell_start, (void *)client_init, 1, 1);
}

static void dfs_chunkserver(int id, uint32_t addr) {
    dfs_chunkserver_t *chunkserver = (dfs_chunkserver_t*)kmalloc(sizeof(dfs_chunkserver_t));
    chunkserver->addr = addr;
    chunkserver->id = id; 
    dfs_init_chunkserver(chunkserver, client_conf(dfs_get_pkt_bytes()));
    dfs_chunkserver_start(chunkserver);
}

#endif

void notmain() {

#if MODE == 0

    dfs_master();
    dfs_chunkserver(0, dfs_chunk_addr1);

    pre_thread_start();
    trace("SUCCESS!\n");

#elif MODE == 1

    pre_fork(dfs_client, NULL, 1, 1);
    // dfs_chunkserver(0, dfs_chunk_addr1);

    pre_thread_start();
    trace("SUCCESS!\n");
    
#endif
}