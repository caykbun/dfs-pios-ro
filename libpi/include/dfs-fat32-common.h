#ifndef __DFS_FAT32_COMMON_H__
#define __DFS_FAT32_COMMON_H__

#include "rpi.h"

#define CHUNK_SIZE 1024
#define CHUNK_SERVER_PKT_SIZE 16

static inline char *int_to_filename(uint32_t n, int is_dir) {
    char *res = kmalloc(sizeof(char) * 32);
    res[0] = '0';
    char map[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint8_t i = 0;
    while (n > 0) {
        res[i] = map[n%10];
        n /= 10;
        i++;
    }
    if (is_dir) {
        res[i] = '\0';
    } else {
        res[i] = '.';
        res[i+1] = 'T';
        res[i+2] = 'X';
        res[i+3] = 'T';
        res[i+4] = '\0';
    }
    return res;
}

#endif