#ifndef __FS_APIS_H__
#define __FS_APIS_H__

#include "fat32/fat32.h"

typedef struct fs_chain {
    struct fs_chain *parent;
    pi_dirent_t dir;
} fs_chain_t;

typedef struct fs_mirror {
    char name[16];
    unsigned id;
    pi_dirent_t cwd;
    fs_chain_t *chain;
} fs_mirror_t;


void fs_init();

fs_mirror_t *register_mirror();

int fs_listdir(fs_mirror_t *mirror, char *dirname, char *target);
int fs_changedir(fs_mirror_t *mirror, char *dirname);

int fs_create(fs_mirror_t *mirror, char *dirname, char *filename, int is_dir, int cd);
int fs_delete(fs_mirror_t *mirror, char *dirname, char *filename);
char *fs_read(fs_mirror_t *mirror, char *dirname, char *filename, unsigned *nbytes);

int fs_write(fs_mirror_t *mirror, char *dirname, char *filename, char *data, unsigned nbytes);
int fs_append(fs_mirror_t *mirror, char *dirname, char *filename, char *data);

pi_dirent_t *get_root();
// pi_dirent_t *get_cwd();
fat32_fs_t *get_fs();


#endif