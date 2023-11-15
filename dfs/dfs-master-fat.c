#include "rpi.h"
#include "fs-apis.h"
#include "fat32/pi-sd.h"
#include "fat32/fat32.h"
#include "dfs-master-fat.h"
#include "dfs-fat32-common.h"


static fs_mirror_t *master_mirror;

master_fat32_fs_t master_fat32_init() {
    uart_init();
    _kmalloc_init();
    fs_init();

    master_fat32_fs_t master_fs = (master_fat32_fs_t) {
        .fat = get_fs(),
        .root = get_root(),
    };

    master_mirror = register_mirror("master");

    return master_fs;
}

client_dirent_t master_fat32_client_init(uint32_t client_id) {
    // pi_dirent_t *cur_dir = fat32_create(fs, root, int_to_filename(client_id, 1), 1);
    fs_create(master_mirror, "~", int_to_filename(client_id, 1), 1, 1);
    // master_mirror->cwd = *cur_dir;
    return (client_dirent_t) {
        .client_id = client_id,
        .cur_dir = master_mirror->cwd,
        .root = master_mirror->cwd,
        .chunk_id = 1,
    };
}

int master_fat32_listdir(char *dirname, char *target) {
    return fs_listdir(master_mirror, dirname, target);
}

pi_directory_t master_fat32_readdir(master_fat32_fs_t *fs, client_dirent_t *client) {
    return fat32_readdir(fs->fat, &(client->cur_dir));
}

int master_fat32_changedir(char *dirname) {
    return fs_changedir(master_mirror, dirname);
}

uint32_t master_fat32_get_chunk_num(char *filename) {
    unsigned nbytes;
    chunk_map_t *maps = (chunk_map_t *)fs_read(master_mirror, ".", filename, &nbytes);
    return nbytes / sizeof(chunk_map_t);
}

chunk_map_t master_fat32_get_chunks(char *filename, uint32_t sqn) {
    unsigned nbytes;
    chunk_map_t *maps = (chunk_map_t *)fs_read(master_mirror, ".", filename, &nbytes);
    return maps[sqn];
}

int master_fat32_create(char *filename, int is_dir) {
    return fs_create(master_mirror, ".", filename, is_dir, /* whether cd int created dir*/0);
}

int master_fat32_delete(char *filename) {
    return fs_delete(master_mirror, ".", filename);
}

void master_fat32_add_chunk_server(char *filename, uint32_t chunk_id, uint32_t sqn, uint32_t chunk_server_addr) {
    unsigned nbytes;
    chunk_map_t *maps = (chunk_map_t *)fs_read(master_mirror, ".", filename, &nbytes);

    chunk_map_t new_map;
    uint32_t chunk_num = (nbytes) / (sizeof(chunk_map_t));
    if (chunk_num <= sqn) {
        new_map = (chunk_map_t) {
            .chunk_id = chunk_id,
            .chunk_server_addrs = {chunk_server_addr, 0, 0},
            .num_replica = 1,
        };
        chunk_num ++;
    } else {
        chunk_map_t old_map = maps[sqn];
        old_map.chunk_server_addrs[old_map.num_replica] = chunk_server_addr;

        new_map = (chunk_map_t) {
            .chunk_id = chunk_id,
            .chunk_server_addrs = {old_map.chunk_server_addrs[0], old_map.chunk_server_addrs[1], old_map.chunk_server_addrs[2]},
            .num_replica = old_map.num_replica + 1,
        };
    }

    unsigned new_nbytes = sizeof(chunk_map_t) * chunk_num;
    chunk_map_t *data = kmalloc(new_nbytes);
    if (chunk_num > 1) {
        memcpy(data, maps, sizeof(chunk_map_t) * (chunk_num-1));
        memcpy(data + (chunk_num - 1), &new_map, sizeof(chunk_map_t));
    } else {
        memcpy(data, &new_map, sizeof(chunk_map_t));
    }
    fs_write(master_mirror, ".", filename, (char *)data, new_nbytes);
}