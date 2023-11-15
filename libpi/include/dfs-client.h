#ifndef _DFS_CLIENT_H_
#define _DFS_CLIENT_H_

#include "nrf.h"
#include "nrf-hw-support.h"
#include "dfs-cmd.h"
#include "dfs-fat32-common.h"

#define dfs_client_connect_timeout_us 20000000
#define dfs_client_ack_timeout_us 10000000

typedef struct {
    nrf_t* nrf;
    uint32_t addr;
} dfs_client_t;

typedef struct {
    uint32_t port_id;
    uint32_t server_addr;
    dfs_client_t* client;
} dfs_connection_t;

typedef struct {
    dfs_client_t *client;
    nrf_conf_t config;
} dfs_client_init_t;


int dfs_init_client(dfs_client_t * client, nrf_conf_t c);

int dfs_client_try_connect(dfs_connection_t *conn);

int dfs_client_shutdown_server(dfs_connection_t *conn);

uint32_t dfs_client_create(dfs_connection_t *conn, const char* filename, uint32_t name_len);

uint32_t dfs_client_delete(dfs_connection_t *conn, const char* filename, uint32_t name_len);

int dfs_client_listdir(dfs_connection_t *conn, const char* dirname);

int dfs_client_change_directory(dfs_connection_t *conn, const char *dirname);

dfs_pkt* dfs_client_read_chunk_meta(dfs_connection_t *conn, const char* filename, uint32_t name_len, uint32_t chunk_sqn);
int dfs_client_query_chunk_data(dfs_connection_t *conn, uint32_t chunk_id, char* chunk_data);

dfs_pkt* dfs_client_write_chunk_meta(dfs_connection_t *conn, const char *filename, uint32_t name_len, uint32_t chunk_sqn);
int dfs_client_write_chunk_data(dfs_connection_t *conn, uint32_t chunk_id, char *chunk_data, uint32_t nbytes);

int dfs_send_cmd(nrf_t *server, dfs_pkt *cmd);

int dfs_client_read_cmd(nrf_t *server, char *filename);

#endif