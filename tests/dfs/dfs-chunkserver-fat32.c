#include "rpi.h"
#include "dfs-chunk-fat.h"

void notmain() {
    chunkserver_fat32_fs_t chunk_fs = chunkserver_fat32_init();
    fat32_delete(chunk_fs.fat, chunk_fs.root, "1.TXT");
    chunkserver_create_chunk(&chunk_fs, 0x1);
    chunkserver_append_chunk(&chunk_fs, 0x1, "hello world!");
    chunkserver_append_chunk(&chunk_fs, 0x1, "hello world!");
    pi_file_t *file = chunkserver_read(&chunk_fs, 0x1);

    printk("Printing file (%d bytes):\n", file->n_data);
    printk("--------------------\n");
    for (int i = 0; i < file->n_data; i++) {
        printk("%c", file->data[i]);
    }
    printk("\n");
    chunkserver_delete_chunk(&chunk_fs, 0x1);
}