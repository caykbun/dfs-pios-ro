#ifndef __DFS_CHUNKSERVER_FAT_H__
#define __DFS_CHUNKSERVER_FAT_H__

#include "rpi.h"
#include "fat32/fat32.h"

typedef struct chunkserver_fat32 {
    fat32_fs_t *fat;
    pi_dirent_t *root;
} chunkserver_fat32_fs_t;

chunkserver_fat32_fs_t chunkserver_fat32_init();

int chunkserver_create_chunk(chunkserver_fat32_fs_t *fs, uint32_t chunk_id);

int chunkserver_append_chunk(chunkserver_fat32_fs_t *fs, uint32_t chunk_id, char *data);

int chunkserver_delete_chunk(chunkserver_fat32_fs_t *fs, uint32_t chunk_id);

pi_file_t *chunkserver_read(chunkserver_fat32_fs_t *fs, uint32_t chunk_id);


#endif