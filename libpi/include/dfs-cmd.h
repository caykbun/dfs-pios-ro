#ifndef __DFS_CMD_H__
#define __DFS_CMD_H__


/*This is the fix addresses for server rx pipes, don't overlap:
master server: 
static uint32_t ports[DFS_MAX_CONN] = {0xa1a1a1, 0xb2b2b2, 0xc3c3c3, 0xd4d4d4}; // use p2 - p5

chunk server1:
static uint32_t ports[DFS_MAX_CONN] = {0xa5a5a5, 0xb6b6b6, 0xc7c7c7, 0xd8d8d8}; // use p2 - p5
*/

#define dfs_master_addr 0x7e7e7e
#define dfs_client_addr 0x7d7d7d
#define dfs_client_addr2 0x7f7f7f
#define dfs_chunk_addr1 0x6a6a6a
#define dfs_chunk_addr2 0x6b6b6b
#define dfs_chunk_addr3 0x6c6c6c

// opcode
enum {
    DFS_LS = 0x0,
    DFS_CD = 0x1,
    DFS_RD = 0x2,
    DFS_WR = 0x3,
    DFS_CR = 0xc,
    DFS_RM = 0xd,

    DFS_END_RD = 0x4,
    DFS_END_WR = 0x5,
    DFS_END_LS = 0xf,

    DFS_RETRY_RD = 0x6,
    DFS_RETRY_WR = 0x7,

    DFS_COMMIT_WR = 0x8,

    DFS_CNCT = 0x9,
    DFS_ACP = 0xa,
    DFS_REJ = 0xb,

    DFS_HB = 0xe,
    DFS_SD = 0xf, // SHUT DOWN SERVER, for test
};

// status
enum {
    STATUS_SUCCESS = 0x0,
    STATUS_FAILED  = 0x1,
};

typedef struct {
    uint32_t addr;
} dfs_conn_pkt;

typedef struct {
    uint32_t hb_id;
    uint32_t addr; // from addr
    uint32_t usage; // in KB
    uint32_t storage; // in KB
} dfs_hb_pkt;

// 20B
// chunk server -> client
typedef struct {
    uint32_t offset;
    char data[16];
} dfs_data;

// 20B
#define MAX_FILE_NAME 12
typedef struct {
    uint32_t chunk_server_addr;
    uint32_t nbytes; // client->master: filename length | master->client: file size
    char file_name[MAX_FILE_NAME];  
} dfs_metadata;

// client query data -> chunk server in an active connection
typedef struct {
    uint32_t offset;
    uint32_t nbytes;
} dfs_data_query;

// 20B
union dfs_active_payload {
    dfs_metadata metadata;
    dfs_data data;
    dfs_data_query dataquery;
};

// 28B
typedef struct {
    uint32_t chunk_id_sqn;
    uint8_t status;
    union dfs_active_payload payload;
} dfs_active_pkt;

// 30B
union dfs_typed_pkt {
    dfs_conn_pkt conn_pkt;
    dfs_active_pkt active_pkt;
    dfs_hb_pkt hb_pkt;
    char placeholder[28];
};

// should be 32 bytes
typedef struct {
    uint8_t op_code;
    uint8_t port_id;
    union dfs_typed_pkt t_pkt;
} dfs_pkt;

static inline uint32_t dfs_get_pkt_bytes() {
    return sizeof(dfs_pkt);
}
#endif