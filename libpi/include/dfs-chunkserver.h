#ifndef __DFS_CHUNKSERVER_H__
#define __DFS_CHUNKSERVER_H__

#include "nrf.h"
#include "nrf-hw-support.h"
#include "dfs-cmd.h"
#include "dfs-chunk-fat.h"
#include "dfs-fat32-common.h"

#define dfs_chunk_timeout_us 1000000
#define dfs_chunk_ack_timeout_us 10000000
#define dfs_chunk_lifecycle 10000000000
// #define dfs_chunk_lifecycle 3

#define DFS_MAX_CONN 4
// static uint32_t ports[DFS_MAX_CONN] = {0xa5a5a5, 0xb6b6b6, 0xc7c7c7, 0xd8d8d8}; // use p2 - p5

typedef struct {
    uint32_t addr;
    uint32_t id;
    chunkserver_fat32_fs_t *fs;
    nrf_t *nrf;
    uint32_t client_addrs[DFS_MAX_CONN];
    uint32_t hb_pin;
} dfs_chunkserver_t;

typedef struct 
{
    dfs_chunkserver_t *chunkserver;
    nrf_conf_t config;
} dfs_chunkserver_init_t;

int dfs_init_chunkserver(dfs_chunkserver_t *cserver, nrf_conf_t c);

/* initilize the distributed file system and starts receiving*/
void dfs_chunkserver_start(dfs_chunkserver_t *chunk_init);

#endif