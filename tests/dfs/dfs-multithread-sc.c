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
#include "dfs-client-command.h"


static unsigned thread_count, thread_sum;

#define HAS_CHUNKSERVER 0

static void dfs_master() {
    dfs_master_t *master = (dfs_master_t*)kmalloc(sizeof(dfs_master_t));
    master->addr = dfs_master_addr + 2;
    master->num_chunkserver = 1;
    dfs_init_master(master, server_conf(dfs_get_pkt_bytes()));
    dfs_master_start(master);
}

void dfs_client(void *arg) {
    dfs_connection_t conn = *(dfs_connection_t *)arg;
    _thread_safe_printk("start master loop\n");
    
    if (dfs_client_try_connect(&conn) == -1) {
        panic("");
    }

    dfs_client_listdir(&conn, ".");

    // 1. Create
    trace("creating folder ... \n");
    dfs_client_create(&conn, "TEMP", 4);

    trace("changing directory ... \n");
    dfs_client_change_directory(&conn, "TEMP");

    dfs_client_listdir(&conn, ".");

    // trace("cd to TEMP ... \n");
    // if (dfs_client_change_directory(&conn, "TEMP", 4) > 0) {
    //     trace("cd success \n");
    // } else {
    //     panic("cd failed\n");
    // }

    trace("creating file ... \n");
    dfs_client_create(&conn, "TEMP.TXT", 8);

    dfs_client_listdir(&conn, ".");

    // 2. Write
    // trace("writing to file ... \n");
    // dfs_pkt *wr_pkt = dfs_client_write_chunk_meta(&conn, "TEMP.TXT", 8, 0);
    // if (!wr_pkt) {
    //     panic("write meta failed \n");
    // }

    dfs_client_shutdown_server(&conn);
    trace("shutdown !!! \n");
    _thread_safe_printk("end master loop\n");
}

void dfs_client_start(dfs_connection_t *conn) {
    // dfs_client_t *client = (dfs_client_t *)kmalloc(sizeof(dfs_client_t));
    // client->addr = client_addr;
    // dfs_init_client(client, client_conf(dfs_get_pkt_bytes()));    
    // dfs_connection_t conn;
    // conn.client = client;
    // conn.server_addr = 0x7e7e7e + 2;
    // dfs_init_master(master_init->master, master_init->config);
    pre_fork(dfs_client, conn, 1, 1);
    // pre_fork(master_heartbeat, (void *)(master), 1, 1);
}

static void dfs_client_shell() {
    dfs_client_t *client = (dfs_client_t *)kmalloc(sizeof(dfs_client_t));
    client->addr = dfs_client_addr;
    dfs_client_init_t *client_init = (dfs_client_init_t *)kmalloc(sizeof(dfs_client_init_t));
    client_init->client = client;
    client_init->config = client_conf(dfs_get_pkt_bytes());
    pre_fork(client_shell_start, (void *)client_init, 1, 1);
}



void notmain() {
	// pre_fork(dfs_master, NULL, 1, 1);
    char target[100];
    // memset(target, 0, 100);
    // sprintk(target, "test %s %b %d %x", "hello", 2, 3, 16);
    // printk("%s", target);
    // dfs_master();
    // dfs_client_t *client = (dfs_client_t *)kmalloc(sizeof(dfs_client_t));
    // client->addr = client_addr;
    // dfs_init_client(client, client_conf(dfs_get_pkt_bytes()));    
    // dfs_connection_t conn;
    // conn.client = client;
    // conn.server_addr = 0x7e7e7e + 2;
    // pre_fork(dfs_client, &conn, 1, 1);
    dfs_client_shell();

	pre_thread_start();

    trace("SUCCESS!\n");
}
