#include "rpi.h"
#include "dfs-master-fat.h"

void notmain() {
    master_fat32_fs_t master_fs = master_fat32_init();
    // client_dirent_t cli = master_fat32_client_init(&master_fs, 1);

    // char* f = "TEMP.TXT";

    // master_fat32_delete(".", f);
    // master_fat32_create(f, 0);

    // master_fat32_listdir("1.   ");
    
    // uint32_t chunk_num = master_fat32_get_chunk_num(f);
    // trace("chunk num = %d \n", chunk_num);

    // master_fat32_add_chunk_server(f, 1, 0, 0xe5e5e5);
    // master_fat32_add_chunk_server(f, 1, 0, 0xf5f5f5);
    // // trace("Add two replicas for Chunk Id %d: %x %x \n", 1, 0xe5e5e5, 0xf5f5f5);

    // chunk_map_t map = master_fat32_get_chunks(f, 0);
    // trace("get chunk id %d with addr 1 %x addr 2 %x addr 3 %x num reaplicas: %d\n", map.chunk_id, map.chunk_server_addrs[0], map.chunk_server_addrs[1], map.chunk_server_addrs[2], map.num_replica);

    // chunk_num = master_fat32_get_chunk_num(f);
    // trace("chunk num = %d \n", chunk_num);

    // master_fat32_add_chunk_server(f, 2, 1, 0xe5e5e5);
    // // master_fat32_add_chunk_server(f, 2, 1, 0xf5f5f5);
    // // trace("Add two replicas for Chunk Id %d: %x %x \n", 2, 0xe5e5e5, 0xf5f5f5);

    // chunk_num = master_fat32_get_chunk_num(f);
    // trace("chunk num = %d \n", chunk_num);

    // map = master_fat32_get_chunks(f, 0);
    // trace("get chunk id %d with addr 1 %x addr 2 %x addr 3 %x num reaplicas: %d\n", map.chunk_id, map.chunk_server_addrs[0], map.chunk_server_addrs[1], map.chunk_server_addrs[2], map.num_replica);

    // map = master_fat32_get_chunks(f, 1);
    // trace("get chunk id %d with addr 1 %x addr 2 %x addr 3 %x num reaplicas: %d\n", map.chunk_id, map.chunk_server_addrs[0], map.chunk_server_addrs[1], map.chunk_server_addrs[2], map.num_replica);

    // master_fat32_listdir(".");
}
