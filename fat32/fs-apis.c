#include "rpi.h"
#include "fs-apis.h"
#include "sys-call.h"
#include "global.h"
#include "thread-safe-io.h"


enum { trace_p = 1};
#define fs_trace(args...) do {                          \
    if(trace_p) {                                       \
        _thread_safe_printk(args);                                   \
    }                                                   \
} while(0)

static pi_dirent_t *root; // root directory
// static pi_dirent_t *cwd;  // current working directory
static fat32_fs_t *fs; 

static unsigned mirror_id = 1;


void fs_init() {
    if (!set_fat32_ready()) {
        return;
    }
    pi_sd_init();

    mbr_t *mbr = mbr_read();
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
     
    fs = kmalloc(sizeof(fat32_fs_t));
    *fs = fat32_mk(&partition);
    root = kmalloc(sizeof(pi_dirent_t));
    *root = fat32_get_root(fs);
    // cwd = root;
}

pi_dirent_t dirname_to_directory(fs_mirror_t *mirror, char *dirname, int cd, int *res) {
    pi_dirent_t directory = mirror->cwd;
    // strip /
    if (strlen(dirname) == 0) {
        *res = 1;
        return directory;
    }
    if (dirname[strlen(dirname) - 1] == '/') {
        dirname[strlen(dirname) - 1] = '\0';
        // printk("here0\n");
    }
    if (strcmp(dirname, "~") == 0) {
        directory = *root;
        if (cd) {
            while (mirror->chain->parent) {
                mirror->chain = mirror->chain->parent;
            }
            mirror->cwd = *root;
        }
        *res = 1;
        // printk("here1\n");
    } else if (strcmp(dirname, ".") == 0) {
        // TODO
        // printk("%s\n", directory.name);
        // printk("here2\n");
        *res = 1;
    } else if (strcmp(dirname, "..") == 0) {
        // TODO
        // printk("here3\n");
        if (mirror->chain->parent != NULL) {
            directory = mirror->chain->parent->dir;
            if (cd) {
                if (mirror->chain->parent) {
                    mirror->chain = mirror->chain->parent;
                    mirror->cwd = mirror->chain->dir;
                }
            }
            *res = 1;
        } else {
            *res = 0;
        }
    } else {
        // directory = fat32_readdir()
        // pi_directory_t subdirs = fat32_readdir(fs, &directory);
        // for (int i = 0; i < subdirs.ndirents; i++) {
        //     printk("subdir %s\n", subdirs.dirents[i].name);
        //     if (subdirs.dirents[i].is_dir_p) {
                
        //         if (strcmp(subdirs.dirents[i].name, dirname) == 0) {
        //             directory = subdirs.dirents[i];
        //             printk("%s\n", subdirs.dirents[i].name);
        //             break;
        //         }
        //     }
        // }
        pi_dirent_t *sub = fat32_stat(fs, &directory, dirname);
        if (sub != NULL) {
            directory = *sub;
            if (cd) {
                fs_chain_t *new_head = kmalloc(sizeof(fs_chain_t));
                new_head->parent = mirror->chain;
                new_head->dir = directory;
                mirror->chain = new_head;
                mirror->cwd = directory;
            }
            *res = 1;
        } else {
            *res = 0;
        }
    }
    return directory;
}

int fs_changedir(fs_mirror_t *mirror, char *dirname) {
    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 1, &res);
    return res;
}

int fs_listdir(fs_mirror_t *mirror, char *dirname, char *target) {
    fs_trace("fs_listdir is called\n");
    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 0, &res);


    pi_directory_t files = fat32_readdir(fs, &directory);
    // printk("Got %d files.\n", files.ndirents);
    // for (int i = 0; i < files.ndirents; i++) {
    //     if (files.dirents[i].is_dir_p) {
    //         printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    //     } else {
    //         printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    //     }
    // }
    for (int i = 0; i < files.ndirents; i++) {
        if (files.dirents[i].is_dir_p) {
            sprintk(target + strlen(target), "\tD: %s\n", files.dirents[i].name);
            // printk("\tD: %s\n", files.dirents[i].name);
        } else {
            sprintk(target + strlen(target), "\tF: %s (%d bytes)\n", files.dirents[i].name, files.dirents[i].nbytes);
            // printk("\tF: %s (%d bytes)\n", files.dirents[i].name, files.dirents[i].nbytes);
        }
    }
    // fs_trace("%s\n", target);
    
    return res;
}

int fs_create(fs_mirror_t *mirror, char *dirname, char *filename, int is_dir, int cd) {
    fs_trace("fs_create is called\n");
    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 0, &res);
    if (res == 0)
        return 0;
    pi_dirent_t *dirent = fat32_create(fs, &directory, filename, is_dir);
    if (cd && is_dir && dirent) {
        // printk("creating\n");
        fs_chain_t *head = mirror->chain;
        fs_chain_t *new_head = kmalloc(sizeof(fs_chain_t));
        new_head->parent = head;
        new_head->dir = *dirent;
        mirror->chain = new_head;
        mirror->cwd = *dirent;
    }
    if (dirent == NULL)
        return 0;
    return 1;
}

int fs_delete(fs_mirror_t *mirror, char *dirname, char *filename) {
    fs_trace("fs_delete is called\n");
    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 0, &res);
    if (res == 0)
        return 0;
    return fat32_delete(fs, &directory, filename);
}

char *fs_read(fs_mirror_t *mirror, char *dirname, char *filename, unsigned *nbytes) {
    fs_trace("fs_read is called\n");
    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 0, &res);
    if (res == 0) {
        *nbytes = 0;
        return NULL;
    }
    pi_file_t *file = fat32_read(fs, &directory, filename);
    *nbytes = file->n_data; // the user don't care about size allocated
    return file->data;
}

int fs_write(fs_mirror_t *mirror, char *dirname, char *filename, char *data, unsigned nbytes) {
    fs_trace("fs_write is called\n");
    // TODO
    // make_system_call(FS_WRITE);

    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 0, &res);
    if (res == 0)
        return 0;
   
    pi_file_t file = (pi_file_t) {
        .data = data,
        .n_data = nbytes,
        .n_alloc = nbytes,
    };
    return fat32_write(fs, &directory, filename, &file);
}

int fs_append(fs_mirror_t *mirror, char *dirname, char *filename, char *data) {
    // TODO
    // make_system_call(FS_WRITE);
    int res;
    pi_dirent_t directory = dirname_to_directory(mirror, dirname, 0, &res);
    if (res == 0)
        return 0;
    pi_file_t *file = fat32_read(fs, &directory, filename);

    char *new_data;

    if (file->n_data > 0) {
        new_data = kmalloc(file->n_data + strlen(data) + 1);
        memset(new_data, 0, file->n_data + strlen(data));
        memcpy(new_data, file->data, file->n_data);
        memcpy(new_data + file->n_data, data, strlen(data));
    } else {
        new_data = data;
    }
   
    pi_file_t new_file = (pi_file_t) {
        .data = new_data,
        .n_data = strlen(new_data),
        .n_alloc = strlen(new_data),
    };
    return fat32_write(fs, &directory, filename, &new_file);
}

fs_mirror_t *register_mirror(char *name) {
    fs_mirror_t *mirror = kmalloc(sizeof(fs_mirror_t));
    fs_chain_t *new_head = kmalloc(sizeof(fs_chain_t));
    mirror->chain = new_head;
    new_head->parent = NULL;
    new_head->dir = *root;
    mirror->cwd = *root;
    mirror->id = mirror_id++;
    strcpy(mirror->name, name);
    return mirror;
}

pi_dirent_t *get_root() {
    return root;
}

// pi_dirent_t *get_cwd() {
//     return cwd;
// }

fat32_fs_t *get_fs() {
    return fs;
}