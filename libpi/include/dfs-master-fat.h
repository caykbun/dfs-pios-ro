#ifndef __DFS_MASTER_FAT_H__
#define __DFS_MASTER_FAT_H__

#include "rpi.h"
#include "fat32/fat32.h"


typedef struct master_fat32 {
    fat32_fs_t *fat;
    pi_dirent_t *root;
} master_fat32_fs_t;

typedef struct client_dirent {
    uint32_t client_id;
    pi_dirent_t cur_dir;
    pi_dirent_t root;
    uint32_t chunk_id;
} client_dirent_t;

typedef struct replicas {
    uint32_t chunk_server_addrs[3];
    uint32_t chunk_id;
    uint8_t num_replica;
} chunk_map_t;

master_fat32_fs_t master_fat32_init();

client_dirent_t master_fat32_client_init(uint32_t client_id);

int master_fat32_listdir(char *dirname, char *target);

pi_directory_t master_fat32_readdir(master_fat32_fs_t *fs, client_dirent_t *client);

int master_fat32_changedir(char *dirname);

uint32_t master_fat32_get_chunk_num(char *filename);

chunk_map_t master_fat32_get_chunks(char *filename, uint32_t sqn);

int master_fat32_create(char *filename, int is_dir);
int master_fat32_delete(char *filename);

void master_fat32_add_chunk_server(char *filename, uint32_t chunk_id, uint32_t sqn, uint32_t chunk_server_addr);

#endif