#ifndef __DFS_MASTER_H__
#define __DFS_MASTER_H__

#include "nrf.h"
#include "nrf-hw-support.h"
#include "dfs-cmd.h"
#include "dfs-master-fat.h"

#define dfs_master_timeout_us 1000000
#define dfs_master_ack_timeout_us 10000000
#define dfs_master_lifecycle 10000000000
#define dfs_hb_window 15000000

#define DFS_MAX_CONN 4
#define DFS_MAX_CLIENT 10
#define DFS_HB_MAX_LAG 3  // minimum lag is 1
#define DFS_MAX_CHUNK_SERVER 64

// fix configuration, shoud stored in file system 
// uint32_t ports[DFS_MAX_CONN] = {0xa1a1a1, 0xb2b2b2, 0xc3c3c3, 0xd4d4d4}; // use p2 - p5

static client_dirent_t client_fs[DFS_MAX_CLIENT];
static uint8_t client_num = 0;


// chunkserver status
enum { 
    CHUNK_DOWN = 0,  // init value, don't change this
    CHUNK_OK = 1,
};

typedef struct {
    uint32_t addr;
    uint8_t status;
    uint32_t storage;  // in KB
    uint32_t usage; // in KB
    uint32_t cur_hb_id;
} dfs_chunkserver_status;

typedef struct {
    master_fat32_fs_t *fs;
    uint32_t addr;  // must specify upon creation
    nrf_t *nrf;
    dfs_chunkserver_status* chunkservers; 
    uint32_t num_chunkserver;  // must specify upon creation
    uint32_t num_act_server;
    uint32_t cur_hb_id; 
    // uint32_t ports[DFS_MAX_CONN] = {0xa1a1a1, 0xb2b2b2, 0xc3c3c3, 0xd4d4d4}; // use p2 - p5
    uint32_t client_addrs[DFS_MAX_CONN];
    uint32_t hb_pin;
} dfs_master_t;

typedef struct 
{
    dfs_master_t *master;
    nrf_conf_t config;
} dfs_master_init_t;


int dfs_init_master(dfs_master_t *m, nrf_conf_t c);
int dfs_master_main_start(dfs_master_t *master);
int dfs_master_heartbeat_daemon_start(dfs_master_t *master);
void dfs_master_start(dfs_master_t *master);
#endif