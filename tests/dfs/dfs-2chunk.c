/*
 * first basic test: will succeed even if you have not implemented
 * context switching.
 * 
 *   - gives same result if you hvae pre_fork return immediately or
 *   - enqueue threads on a stack.
 *
 * this test is useful to make sure you have the rough idea of how the
 * threads should work.  you should rerun when you have your full package
 * working to ensure you get the same result.
 */
#include "rpi.h"
#include "preemptive-thread.h"
#include "thread-safe-io.h"
#include "rpi-interrupts.h"
#include "dfs-master-fat.h"
#include "nrf-test.h"
#include "nrf-interrupt.h"
#include "dfs-master.h"
#include "dfs-client.h"
#include "dfs-chunkserver.h"

static unsigned thread_count, thread_sum;

// static void dfs_master() {
//     dfs_master_t *master = (dfs_master_t*)kmalloc(sizeof(dfs_master_t));
//     master->addr = dfs_master_addr;
//     master->num_chunkserver = 3;
//     dfs_init_master(master, server_conf(dfs_get_pkt_bytes()));
//     dfs_master_start(master);	
// }

static void dfs_chunkserver1() {
    dfs_chunkserver_t *chunkserver = (dfs_chunkserver_t*)kmalloc(sizeof(dfs_chunkserver_t));
    chunkserver->addr = dfs_chunk_addr2;
    chunkserver->id = 1; 
    chunkserver->hb_pin = 17;
    dfs_init_chunkserver(chunkserver, server_conf(dfs_get_pkt_bytes()));
    dfs_chunkserver_start(chunkserver);
}

static void dfs_chunkserver2() {
    dfs_chunkserver_t *chunkserver = (dfs_chunkserver_t*)kmalloc(sizeof(dfs_chunkserver_t));
    chunkserver->addr = dfs_chunk_addr3;
    chunkserver->id = 2; 
    chunkserver->hb_pin = 19;
    dfs_init_chunkserver(chunkserver, client_conf(dfs_get_pkt_bytes()));
    dfs_chunkserver_start(chunkserver);
}


void notmain() {
	dfs_chunkserver1();
    dfs_chunkserver2();

	pre_thread_start();

    trace("SUCCESS!\n");
}
