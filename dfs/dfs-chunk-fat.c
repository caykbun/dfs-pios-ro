#include "rpi.h"
#include "fs-apis.h"
#include "fat32/pi-sd.h"
#include "fat32/fat32.h"
#include "dfs-chunk-fat.h"
#include "dfs-fat32-common.h"


static fs_mirror_t *chunkserver_mirror;


chunkserver_fat32_fs_t chunkserver_fat32_init() {
    uart_init();
    _kmalloc_init();
    fs_init();

    chunkserver_fat32_fs_t chunkserver_fs = (chunkserver_fat32_fs_t) {
        .fat = get_fs(),
        .root = get_root(),
    };

    chunkserver_mirror = register_mirror("chunk");

    return chunkserver_fs;
}

int chunkserver_create_chunk(chunkserver_fat32_fs_t *fs, uint32_t chunk_id) {
    char *filename = int_to_filename(chunk_id, 0);
    trace("creating file : %s at root directory of chunk server\n", filename);
    return fs_create(chunkserver_mirror, "~", filename, 0, 0);
}

int chunkserver_append_chunk(chunkserver_fat32_fs_t *fs, uint32_t chunk_id, char *data) {
    char *filename = int_to_filename(chunk_id, 0);
    trace("writing to file : %s \n", filename);

    // pi_file_t *file = fat32_read(fs->fat, fs->root, filename);

    // char *new_data;

    // if (file->n_data > 0) {
    //     new_data = kmalloc(file->n_data + strlen(data)+1);
    //     memcpy(new_data, file->data, file->n_data);
    //     memcpy(new_data + file->n_data, data, strlen(data));
    // } else {
    //     new_data = data;
    // }

    // pi_file_t content = (pi_file_t) {
    //     .data = new_data,
    //     .n_data = strlen(new_data),
    //     .n_alloc = strlen(new_data),
    // };

    // if (fat32_write(fs->fat, fs->root, filename, &content)) {
    return fs_append(chunkserver_mirror, "~", filename, data);
}

int chunkserver_delete_chunk(chunkserver_fat32_fs_t *fs, uint32_t chunk_id) {
    char *filename = int_to_filename(chunk_id, 0);
    trace("deleting file : %s \n", filename);
    return fs_delete(chunkserver_mirror, "~", filename);
}

pi_file_t *chunkserver_read(chunkserver_fat32_fs_t *fs, uint32_t chunk_id) {
    char *filename = int_to_filename(chunk_id, 0);
    trace("reading file : %s \n", filename);

    // pi_file_t *file = fat32_read(fs->fat, fs->root, filename);
    unsigned nbytes;
    void *data = fs_read(chunkserver_mirror, "~", filename, &nbytes);
    pi_file_t *content = kmalloc(sizeof(pi_file_t));
    *content = (pi_file_t) {
        .data = data,
        .n_data = nbytes,
        .n_alloc = nbytes,
    };
    return content;
}
