#include "dfs-client.h"

typedef struct {
    dfs_connection_t *master_conn;
    dfs_connection_t *chunk_conn;
} dfs_client_conns_t;


void client_shell_start(void *arg);

int client_parse(char *input, dfs_client_conns_t *client_conns);