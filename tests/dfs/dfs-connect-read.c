#include "rpi.h"
#include "dfs-master-fat.h"
#include "nrf-test.h"
#include "nrf-interrupt.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"
#include "rpi-interrupts.h"

#define MODE 1

#if MODE == 0
#include "dfs-master.h"
#elif MODE == 1
#include "dfs-client.h"

void dfs_client(void *arg) {
    dfs_connection_t conn = *(dfs_connection_t *)arg;
    _thread_safe_printk("start master loop\n");
    
    if (dfs_client_try_connect(&conn) == -1) {
        panic("");
    }

    // 1. Create

    // trace("cd to TEMP ... \n");
    // if (dfs_client_change_directory(&conn, "TEMP", 4) > 0) {
    //     trace("cd success \n");
    // } else {
    //     panic("cd failed\n");
    // }

    trace("creating file ... \n");
    dfs_client_create(&conn, "TEST.TXT", 8);

    // 2. Write
    trace("writing to file ... \n");
    dfs_pkt *wr_pkt = dfs_client_write_chunk_meta(&conn, "TEST.TXT", 8, 0);
    if (!wr_pkt) {
        panic("write meta failed \n");
    }

    dfs_connection_t conn_chunk;
    conn_chunk.client = conn.client;
    conn_chunk.server_addr = wr_pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr;

    trace("trying to connect chunk server addr %x\n", wr_pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr);
    if (dfs_client_try_connect(&conn_chunk) == -1) {
        panic("");
    }
    uint32_t chunk_id = wr_pkt->t_pkt.active_pkt.chunk_id_sqn;
    char *chunk_data = "This is a chunk data, it is very very long...";
    if (dfs_client_write_chunk_data(&conn_chunk, chunk_id, chunk_data, strlen(chunk_data)) == -1) {
        panic("write to chunk failed");
    }

    // 3. Read
    trace("reading from file ... \n");
    dfs_pkt* pkt = dfs_client_read_chunk_meta(&conn, "TEST.TXT", 8, 0);
    if (!pkt) {
        panic("");
    };
    conn_chunk.server_addr = pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr;
    trace("trying to connect chunk server addr %x chunk id %d\n", pkt->t_pkt.active_pkt.payload.metadata.chunk_server_addr, chunk_id);
    if (dfs_client_try_connect(&conn_chunk) == -1) {
        panic("");
    }

    char data[CHUNK_SIZE];
    if (dfs_client_query_chunk_data(&conn_chunk, chunk_id, data) == -1) {
        panic("");
    }
    printk("%s", data);
}
#else 
#include "dfs-chunkserver.h"
#endif

void notmain() {

#if MODE == 0
    dfs_master_t *master = (dfs_master_t*)kmalloc(sizeof(dfs_master_t));
    master->addr = dfs_master_addr;
    master->num_chunkserver = 1;
    dfs_init_master(master, server_conf(dfs_get_pkt_bytes()));
    dfs_master_main_start(master);

#elif MODE == 1

    dfs_client_t *client = (dfs_client_t *)kmalloc(sizeof(dfs_client_t));
    client->addr = client_addr;
    dfs_init_client(client, client_conf(dfs_get_pkt_bytes()));    
    dfs_connection_t conn;
    conn.client = client;
    conn.server_addr = 0x7e7e7e;
    pre_fork(dfs_client, &conn, 1, 1);

	pre_thread_start();
#else 

    dfs_chunkserver_t *chunk = dfs_init_chunkserver(server_conf(dfs_get_pkt_bytes()), dfs_chunk_addr1);
    chunk->id = 0; 
    dfs_chunkserver_start(chunk);
    
#endif
}