#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"

// Print extra tracing info when this is enabled.  You can and should add your
// own.
static int trace_p = 0; 
static int init_p = 0;
static int verify_fat = 0;
static int verify_write = 1;

fat32_boot_sec_t boot_sector;


fat32_fs_t fat32_mk(mbr_partition_ent_t *partition) {
  demand(!init_p, "the fat32 module is already in use\n");
  // TODO: Read the boot sector (of the partition) off the SD card.
  uint32_t partition_start = partition->lba_start;
  boot_sector = *(fat32_boot_sec_t *) pi_sec_read(partition_start, 1);

  // TODO: Verify the boot sector (also called the volume id, `fat32_volume_id_check`)
  // fat32_volume_id_print("LOL", &boot_sector);
  fat32_volume_id_check(&boot_sector);

  // TODO: Read the FS info sector (the sector immediately following the boot
  // sector) and check it (`fat32_fsinfo_check`, `fat32_fsinfo_print`)
  assert(boot_sector.info_sec_num == 1);
  struct fsinfo *fsinfo_sec = (struct fsinfo *) pi_sec_read(partition_start + 1, 1);
  // fat32_fsinfo_print("LOL", fsinfo_sec);
  fat32_fsinfo_check(fsinfo_sec);


  // END OF PART 2
  // The rest of this is for Part 3:
  // TODO: calculate the fat32_fs_t metadata, which we'll need to return.
  unsigned lba_start = partition->lba_start; // from the partition
  unsigned fat_begin_lba = lba_start + boot_sector.reserved_area_nsec; // the start LBA + the number of reserved sectors
  unsigned cluster_begin_lba = fat_begin_lba + boot_sector.nfats * boot_sector.nsec_per_fat; // the beginning of the FAT, plus the combined length of all the FATs
  unsigned sec_per_cluster = boot_sector.sec_per_cluster; // from the boot sector
  unsigned root_first_cluster = boot_sector.first_cluster; // from the boot sector
  unsigned n_entries = boot_sector.nsec_per_fat * boot_sector.bytes_per_sec / 4; // from the boot sector
  // unimplemented();

  /*
   * TODO: Read in the entire fat (one copy: worth reading in the second and
   * comparing).
   *
   * The disk is divided into clusters. The number of sectors per
   * cluster is given in the boot sector byte 13. <sec_per_cluster>
   *
   * The File Allocation Table has one entry per cluster. This entry
   * uses 12, 16 or 28 bits for FAT12, FAT16 and FAT32.
   *
   * Store the FAT in a heap-allocated array.
   */
  uint32_t *fat = pi_sec_read(fat_begin_lba, boot_sector.nsec_per_fat);
  if (verify_fat) {
    uint32_t *fat2 = pi_sec_read(fat_begin_lba + boot_sector.nsec_per_fat, boot_sector.nsec_per_fat);
    for (unsigned i = 0; i < n_entries; ++i) {
      if (*(fat + i) != *(fat2 + i)) {
        panic("%d\n", i);
      }
    }
  }

  // Create the FAT32 FS struct with all the metadata
  fat32_fs_t fs = (fat32_fs_t) {
    .lba_start = lba_start,
      .fat_begin_lba = fat_begin_lba,
      .cluster_begin_lba = cluster_begin_lba,
      .sectors_per_cluster = sec_per_cluster,
      .root_dir_first_cluster = root_first_cluster,
      .fat = fat,
      .n_entries = n_entries,
  };

  if (trace_p) {
    trace("begin lba = %d\n", fs.fat_begin_lba);
    trace("cluster begin lba = %d\n", fs.cluster_begin_lba);
    trace("sectors per cluster = %d\n", fs.sectors_per_cluster);
    trace("root dir first cluster = %d\n", fs.root_dir_first_cluster);
  }

  init_p = 1;
  return fs;
}

// Given cluster_number, get lba.  Helper function.
static uint32_t cluster_to_lba(fat32_fs_t *f, uint32_t cluster_num) {
  assert(cluster_num >= 2);
  // TODO: calculate LBA from cluster number, cluster_begin_lba, and
  // sectors_per_cluster
  unsigned lba = f->cluster_begin_lba + (cluster_num - 2) * f->sectors_per_cluster;
  if (trace_p) trace("cluster %d to lba: %d\n", cluster_num, lba);
  return lba;
}

pi_dirent_t fat32_get_root(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // TODO: return the information corresponding to the root directory (just
  // cluster_id, in this case)  
  return (pi_dirent_t) {
    .name = "",
      .raw_name = "",
      .cluster_id = fs->root_dir_first_cluster, // fix this
      .is_dir_p = 1,
      .nbytes = 0,
  };
}

// Given the starting cluster index, get the length of the chain.  Helper
// function.
static uint32_t get_cluster_chain_length(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // `fat32_fat_entry_type(cluster) == LAST_CLUSTER`.  Count the number of
  // clusters.

  if (start_cluster == 0)
    return 0;

  int count = 0;
  uint32_t cluster = start_cluster;
  while (1) {
    count++;
    cluster = fs->fat[cluster];
    if (fat32_fat_entry_type(cluster) == LAST_CLUSTER)
      break;
  }
  return count;
}

// Given the starting cluster index, read a cluster chain into a contiguous
// buffer.  Assume the provided buffer is large enough for the whole chain.
// Helper function.
static int read_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data) {
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // fat32_fat_entry_type(cluster) == LAST_CLUSTER.  For each cluster, copy it
  // to the buffer (`data`).  Be sure to offset your data pointer by the
  // appropriate amount each time.
  // unimplemented();
  if (start_cluster == 0)
    return 0;

  uint32_t cluster = start_cluster;
  unsigned data_index = 0;
  while (1) {
    uint32_t lba = cluster_to_lba(fs, cluster);
    cluster = fs->fat[cluster];
    uint8_t *cluster_data = pi_sec_read(lba, fs->sectors_per_cluster);
    memcpy(data + data_index, cluster_data, fs->sectors_per_cluster * 512);
    data_index += fs->sectors_per_cluster * 512;
    if (fat32_fat_entry_type(cluster) == LAST_CLUSTER)
      break;
  }
  return data_index;
}

// Converts a fat32 internal dirent into a generic one suitable for use outside
// this driver.
static pi_dirent_t dirent_convert(fat32_dirent_t *d) {
  pi_dirent_t e = {
    .cluster_id = fat32_cluster_id(d),
    .is_dir_p = (d->attr & 0b10000) > 0,
    .nbytes = d->file_nbytes,
  };
  // can compare this name
  memcpy(e.raw_name, d->filename, sizeof d->filename);
  // for printing.
  fat32_dirent_name(d,e.name);
  return e;
}

// Gets all the dirents of a directory which starts at cluster `cluster_start`.
// Return a heap-allocated array of dirents.
static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n) {
  // TODO: figure out the length of the cluster chain (see
  // `get_cluster_chain_length`)
  int length = get_cluster_chain_length(fs, cluster_start);

  // TODO: allocate a buffer large enough to hold the whole directory
  uint8_t *data = kmalloc(fs->sectors_per_cluster * 512 * length);

  // TODO: read in the whole directory (see `read_cluster_chain`)
  unsigned nbytes = read_cluster_chain(fs, cluster_start, data);
  *dir_n = nbytes / 32;

  return (fat32_dirent_t *)data;
}

pi_directory_t fat32_readdir(fat32_fs_t *fs, pi_dirent_t *dirent) {
  demand(init_p, "fat32 not initialized!");
  demand(dirent->is_dir_p, "tried to readdir a file!");
  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, dirent->cluster_id, &n_dirents);

  // TODO: allocate space to store the pi_dirent_t return values
  pi_dirent_t *sub_dirents = kmalloc(sizeof(pi_dirent_t) * n_dirents);

  // TODO: iterate over the directory and create pi_dirent_ts for every valid
  // file.  Don't include empty dirents, LFNs, or Volume IDs.  You can use
  // `dirent_convert`.
  unsigned true_count = 0;
  for (unsigned i = 0; i < n_dirents; ++i) {
    if (dirents[i].filename[0] == 0x0 || dirents[i].filename[0] == 0xE5 || (dirents[i].attr & 0b1111) > 0) { // (dirents[i].attr & 0b1111) == 0b1111 || (dirents[i].attr & 0b1000)) {
      continue;
    }
    
    // for (unsigned j = 0; j < 11; ++j) {
    //   printk("%d ", dirents[i].filename[j]);
    // }
    // printk("\n");
    // fat32_dirent_print("DEBUG", &dirents[i]);
    sub_dirents[true_count++] = dirent_convert(&dirents[i]);
    // printk("%d, %b, %s\n", i, dirents[i].attr, sub_dirents[true_count - 1].name);
  }


  // TODO: create a pi_directory_t using the dirents and the number of valid
  // dirents we found
  return (pi_directory_t) {
    .dirents = sub_dirents,
    .ndirents = true_count,
  };
}


// only works for file, not directory
static int find_dirent_with_name(fat32_dirent_t *dirents, int n, char *filename) {
  // TODO: iterate through the dirents, looking for a file which matches the
  // name; use `fat32_dirent_name` to convert the internal name format to a
  // normal string.
  for (unsigned i = 0; i < n; ++i) {
    if (dirents[i].filename[0] == 0x0 || dirents[i].filename[0] == 0xE5) continue;
    if ((dirents[i].attr & 0b1111) > 0) {
      continue;
    }

    // int has_dot = 0;
    // int idx = 0;
    // while (filename[idx] != '\0') {
    //   if (filename[idx] == '.') {
    //     has_dot = 1;
    //     break;
    //   }
    //   idx++;
    // }

    char tmp[100];
    fat32_dirent_name(&dirents[i], tmp);
    // if (has_dot) {
    //   if (strcmp(tmp, filename) == 0) {
    //     return i;
    //   }
    // } else {
    //   idx = 0;
    //   while (filename[idx] != '\0') {
    //     if (filename[idx] != tmp[idx]) break;
    //     idx++;
    //   }
    //   if (filename[idx] == '\0') {
    //     return i;
    //   }
    // }
    
    
    unsigned idx = 0;
    int has_dot = 0;
    while (filename[idx] != '\0') {
      if (filename[idx] == '.') has_dot = 1;
      if (filename[idx] != tmp[idx]) break;
      idx++;
    }
    if (filename[idx] == '\0') {
      if (has_dot) {
        if (tmp[idx] == ' ' || tmp[idx] == '\0' || idx == 11) {
          // printk("success %s\n", tmp);
          return i;
        }
      } else {
        if (idx >= 10 || (tmp[idx] == '.' && (tmp[idx + 1] == ' ' || tmp[idx + 1] == '\0'))) {
          // printk("success %s\n", tmp);
          return i;
        }
      }
    }
  }
  
  return -1;
}

pi_dirent_t *fat32_stat(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory");

  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  // unimplemented();
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);

  // TODO: Iterate through the directory's entries and find a dirent with the
  // provided name.  Return NULL if no such dirent exists.  You can use
  // `find_dirent_with_name` if you've implemented it.

  // for (unsigned i = 0; i < n_dirents; ++i) {
  //   if (dirents[i].filename[0] == 0x0 || dirents[i].filename[0] == 0xE5) continue;
  //   if ((dirents[i].attr & 0b1111) == 0b1111) {
  //     continue;
  //   }
  //   char tmp[100];
  //   fat32_dirent_name(&dirents[i], tmp);
  //   pi_dirent_t *res = (pi_dirent_t *)kmalloc(sizeof(pi_dirent_t));
  //   if (strcmp(tmp, filename) == 0) {
  //     *res = dirent_convert(&dirents[i]);
  //     return res;
  //   }
  // }
  int dirent_idx = find_dirent_with_name(dirents, n_dirents, filename);

  // no match
  if (dirent_idx != -1) {
    pi_dirent_t *res = kmalloc(sizeof(pi_dirent_t));
    *res = dirent_convert(dirents + dirent_idx);
    return res;
  }


  // TODO: allocate enough space for the dirent, then convert
  // (`dirent_convert`) the fat32 dirent into a Pi dirent.
  return NULL;
}

pi_file_t *fat32_read(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // This should be pretty similar to readdir, but simpler.
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: read the dirents of the provided directory and look for one matching the provided name
  // unimplemented();
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);

  int idx = 0;

  for (unsigned i = 0; i < n_dirents; ++i) {
    if (dirents[i].filename[0] == 0x0 || dirents[i].filename[0] == 0xE5 || (dirents[i].attr & 0b1111) == 0b1111 || (dirents[i].attr & 0b1000)) {
      continue;
    }
    char tmp[100];
    fat32_dirent_name(&dirents[i], tmp);
    pi_dirent_t *res = (pi_dirent_t *)kmalloc(sizeof(pi_dirent_t));
    
    if (strcmp(tmp, filename) == 0) {
      idx = i;
      break;
    }
  }

  // TODO: figure out the length of the cluster chain
  uint32_t cluster_num = fat32_cluster_id(&dirents[idx]);
  int length = get_cluster_chain_length(fs, cluster_num);

  // TODO: allocate a buffer large enough to hold the whole file
  pi_file_t *file = kmalloc(sizeof(pi_file_t));
  uint32_t nbytes_alloc = fs->sectors_per_cluster * 512 * length;
  if (nbytes_alloc == 0) {
    *file = (pi_file_t) {
      .data = NULL,
      .n_data = 0,
      .n_alloc = 0,
    };
    return file;
  }
  uint8_t *file_buffer = kmalloc(nbytes_alloc);

  // TODO: read in the whole file (if it's not empty)
  read_cluster_chain(fs, cluster_num, file_buffer);

  // TODO: fill the pi_file_t
  *file = (pi_file_t) {
    .data = file_buffer,
    .n_data = dirents[idx].file_nbytes,
    .n_alloc = nbytes_alloc,
  };
  return file;
}

/******************************************************************************
 * Everything below here is for writing to the SD card (Part 7/Extension).  If
 * you're working on read-only code, you don't need any of this.
 ******************************************************************************/

static uint32_t find_free_cluster(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: loop through the entries in the FAT until you find a free one
  // (fat32_fat_entry_type == FREE_CLUSTER).  Start from cluster 3.  Panic if
  // there are none left.
  if (start_cluster < 3) start_cluster = 3;
  uint32_t cluster = start_cluster;
  while (cluster < fs->n_entries && fat32_fat_entry_type(fs->fat[cluster]) != FREE_CLUSTER) {
    // if (fat32_fat_entry_type(cluster) != USED_CLUSTER) {
    //   printk("type %d %d %d\n", fs->n_entries, cluster, fat32_fat_entry_type(cluster));
    //   clean_reboot();
    // }
    cluster++; 
  }
  if (cluster < fs->n_entries) {
    if (trace_p) trace("found free cluster from %d\n", cluster);
    return cluster;
  }
  // printk("type %d %d %d\n", fs->n_entries, cluster, fat32_fat_entry_type(cluster));

  if (trace_p) trace("failed to find free cluster from %d\n", start_cluster);
  panic("No more clusters on the disk!\n");
}

static void write_fat_to_disk(fat32_fs_t *fs) {
  // TODO: Write the FAT to disk.  In theory we should update every copy of the
  // FAT, but the first one is probably good enough.  A good OS would warn you
  // if the FATs are out of sync, but most OSes just read the first one without
  // complaining.
  if (trace_p) trace("syncing FAT\n");
  const uint32_t num_secs = fs->n_entries / 128;
  pi_sd_write(fs->fat, fs->fat_begin_lba, num_secs);
}

// Given the starting cluster index, write the data in `data` over the
// pre-existing chain, adding new clusters to the end if necessary.
static uint32_t write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) {
  // Walk the cluster chain in the FAT, writing the in-memory data to the
  // referenced clusters.  If the data is longer than the cluster chain, find
  // new free clusters and add them to the end of the list.
  // Things to consider:
  //  - what if the data is shorter than the cluster chain?
  //  - what if the data is longer than the cluster chain?
  //  - the last cluster needs to be marked LAST_CLUSTER
  //  - when do we want to write the updated FAT to the disk to prevent
  //  corruption?
  //  - what do we do when nbytes is 0?
  //  - what about when we don't have a valid cluster to start with?
  //
  //  This is the main "write" function we'll be using; the other functions
  //  will delegate their writing operations to this.

  // TODO: As long as we have bytes left to write and clusters to write them
  // to, walk the cluster chain writing them out.
  // unimplemented();
  uint32_t cluster_length = get_cluster_chain_length(fs, start_cluster);
  if (trace_p) trace("original cluster length is %d\n", cluster_length);  

  uint32_t cluster = start_cluster;
  unsigned data_index = 0;
  unsigned cluster_count = 0;
  int need_more_clusters = 0;
  while (1) {
    if (cluster == 0) {
      if (nbytes > 0)
        need_more_clusters = 1;
      break;
    }

    cluster_count++;
    uint32_t tmp = fs->fat[cluster];
    // no bytes to write
    if (nbytes <= (cluster_count - 1) * 512 * fs->sectors_per_cluster) {
      // special case when nbytes = 0
      if (fat32_fat_entry_type(tmp) == LAST_CLUSTER) {
        fs->fat[cluster] = 0;
        start_cluster = 0;
        break;
      }
      // reset FAT entry to 0 
      fs->fat[cluster] = 0;
    }
    // write
    else {
      uint32_t lba = cluster_to_lba(fs, cluster);
      pi_sd_write(data + data_index, lba, fs->sectors_per_cluster);
      data_index += 512 * fs->sectors_per_cluster;
      // already at end
      if (fat32_fat_entry_type(tmp) == LAST_CLUSTER) {
        // need more clusters
        if (nbytes > cluster_count * 512 * fs->sectors_per_cluster) {
          need_more_clusters = 1;
        }
        // should exit this loop anyway
        break;
      } else {
        // mark LAST_CLUSTER and continue
        if (nbytes <= cluster_count * 512 * fs->sectors_per_cluster) {
          fs->fat[cluster] = 0xFFFFFFFF;
        }
      }
    }
    cluster = tmp;
  }

  

  // TODO: If we run out of clusters to write to, find free clusters using the
  // FAT and continue writing the bytes out.  Update the FAT to reflect the new
  // cluster.
  // unimplemented();
  if (need_more_clusters) {
    while (1) {
      uint32_t free_cluster = find_free_cluster(fs, 3);
      uint32_t lba = cluster_to_lba(fs, free_cluster);
      if (start_cluster == 0) {
        start_cluster = free_cluster;
      } else {
        fs->fat[cluster] = free_cluster;
      }
      cluster = free_cluster;
      pi_sd_write(data + data_index, lba, fs->sectors_per_cluster);
      data_index += 512 * fs->sectors_per_cluster;
      if (nbytes <= data_index) {
        // mark LAST_CLUSTER
        fs->fat[cluster] = 0xFFFFFFFF;
        break;
      }
    }
  }

  // TODO: If we run out of bytes to write before using all the clusters, mark
  // the final cluster as "LAST_CLUSTER" in the FAT, then free all the clusters
  // later in the chain.
  // unimplemented();

  // TODO: Ensure that the last cluster in the chain is marked "LAST_CLUSTER".
  // The one exception to this is if we're writing 0 bytes in total, in which
  // case we don't want to use any clusters at all.
  // unimplemented();

  // write_fat_to_disk(fs);
  return start_cluster;
}

int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  // TODO: Get the dirents `directory` off the disk, and iterate through them
  // looking for the file.  When you find it, rename it and write it back to
  // the disk (validate the name first).  Return 0 in case of an error, or 1
  // on success.
  // Consider:
  //  - what do you do when there's already a file with the new name?
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("renaming %s to %s\n", oldname, newname);
  if (!fat32_is_valid_name(newname)) return 0;

  // TODO: get the dirents and find the right one
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_idx = find_dirent_with_name(dirents, n_dirents, oldname);

  // no match
  if (dirent_idx == -1)
    return 0;

  int new_dirent_idx = find_dirent_with_name(dirents, n_dirents, newname);
  if (new_dirent_idx != -1) {
    fat32_delete(fs, directory, newname);
  }

  // TODO: update the dirent's name
  fat32_dirent_t *dirent = dirents + dirent_idx;
  fat32_dirent_set_name(dirent, newname);

  fat32_write_dirent(fs, dirent, directory, dirent_idx);

  // TODO: write out the directory, using the existing cluster chain (or
  // appending to the end); implementing `write_cluster_chain` will help
  // unimplemented();
  return 1;

}

void fat32_write_dirent(fat32_fs_t *fs, fat32_dirent_t *dirent, pi_dirent_t *directory, uint32_t idx) {
  uint32_t cluster = directory->cluster_id;
  uint32_t entries_per_cluster = fs->sectors_per_cluster * 16;
  uint32_t cluster_num = idx / entries_per_cluster;
  uint32_t count = 0;
  while (count < cluster_num) {
    ++count;
    cluster = fs->fat[cluster];
  }
  idx = idx % entries_per_cluster;
  uint32_t lba = cluster_to_lba(fs, cluster);
  fat32_dirent_t *data = pi_sec_read(lba, fs->sectors_per_cluster);
  memcpy(data + idx, dirent, sizeof(fat32_dirent_t));
  // printk("%d %d %d\n", entries_per_cluster, idx, cluster);
  // clean_reboot();
  pi_sd_write(data, lba, fs->sectors_per_cluster);
}

// Create a new directory entry for an empty file (or directory).
pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) return NULL;

  // TODO: read the dirents and make sure there isn't already a file with the
  // same name
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_idx = find_dirent_with_name(dirents, n_dirents, filename);

  // file exists
  if (dirent_idx != -1) {
    pi_dirent_t *res = (pi_dirent_t *)kmalloc(sizeof(pi_dirent_t));
    *res = dirent_convert(dirents + dirent_idx);

    return res;
  }
    // return NULL;

  // TODO: look for a free directory entry and use it to store the data for the
  // new file.  If there aren't any free directory entries, either panic or
  // (better) handle it appropriately by extending the directory to a new
  // cluster.
  // When you find one, update it to match the name and attributes
  // specified; set the size and cluster to 0.
  // unimplemented();
  fat32_dirent_t *free_dirent = dirents;
  fat32_dirent_t *new_end_dirent = NULL;
  for (unsigned i = 0; i < n_dirents; ++i) {
    // printk("idx %d %x\n", i, free_dirent->filename[0]);
    // fat32_dirent_print("DEBUG", free_dirent);
    if (free_dirent->filename[0] == 0xE5) {
      break;
    }
    if (free_dirent->filename[0] == 0) {
      if (i < n_dirents - 1) {
        new_end_dirent = free_dirent + 1;
        break;
      } else {
        panic("a whole cluster cannot satisfy your needs???\n");
      }
    }
    free_dirent++;
  }

  fat32_dirent_set_name(free_dirent, filename);
  
  if (is_dir) {
    free_dirent->attr = 0x10;
    free_dirent->file_nbytes = 4096;
    uint32_t cluster = find_free_cluster(fs, 3);
    free_dirent->lo_start = cluster & 0xffff;
    free_dirent->hi_start = (cluster >> 16) & 0xffff;
    fs->fat[cluster] = 0xFFFFFFFF;
    write_fat_to_disk(fs);
    // clean directory
    uint32_t lba = cluster_to_lba(fs, cluster);
    fat32_dirent_t *data = pi_sec_read(lba, fs->sectors_per_cluster);
    // for (unsigned i = 0; i < fs->sectors_per_cluster * 16; ++i) {
    //     data[i].filename[0] = 0x0;
    // }
    memset(data, 0, fs->sectors_per_cluster * 512);
    // data->filename[0] = 0x0;
    pi_sd_write(data, lba, fs->sectors_per_cluster);
  } else {
    free_dirent->attr = 0x20;
    free_dirent->file_nbytes = 0;
    free_dirent->lo_start = 0;
    free_dirent->hi_start = 0;
  }
  

  // fat32_dirent_print("DEBUG", free_dirent);
  // clean_reboot();
  fat32_write_dirent(fs, free_dirent, directory, free_dirent - dirents);
  if (new_end_dirent != NULL) {
    new_end_dirent->filename[0] = 0;
    fat32_write_dirent(fs, new_end_dirent, directory, new_end_dirent - dirents);
  }


  // TODO: write out the updated directory to the disk
  // unimplemented();

  // TODO: convert the dirent to a `pi_dirent_t` and return a (kmalloc'ed)
  // pointer
  // unimplemented();
  pi_dirent_t *res = (pi_dirent_t *)kmalloc(sizeof(pi_dirent_t));
  *res = dirent_convert(free_dirent);
  return res;
}

// Delete a file, including its directory entry.
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("deleting %s\n", filename);
  if (!fat32_is_valid_name(filename)) return 0;
  // TODO: look for a matching directory entry, and set the first byte of the
  // name to 0xE5 to mark it as free
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_idx = find_dirent_with_name(dirents, n_dirents, filename);

  // no match
  if (dirent_idx == -1)
    return 0;

  fat32_dirent_t *dirent = dirents + dirent_idx;
  dirent->filename[0] = 0xE5;
  uint32_t cluster = fat32_cluster_id(dirent);
  
  // TODO: free the clusters referenced by this dirent
  if (cluster != 0) {
    while (1) {
      uint32_t tmp = fs->fat[cluster];
      fs->fat[cluster] = 0;
      if (fat32_fat_entry_type(tmp) == LAST_CLUSTER) {
        break;
      }
      cluster = tmp;
    }
  }

  fat32_write_dirent(fs, dirent, directory, dirent_idx);
  write_fat_to_disk(fs);
  return 1;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("truncating %s\n", filename);

  // TODO: edit the directory entry of the file to list its length as `length` bytes,
  // then modify the cluster chain to either free unused clusters or add new
  // clusters.
  //
  // Consider: what if the file we're truncating has length 0? what if we're
  // truncating to length 0?
  // unimplemented();

  // TODO: write out the directory entry
  if (!fat32_stat(fs, directory, filename))
    return 0;
  // unimplemented();
  pi_file_t *file = fat32_read(fs, directory, filename);
  if (length < file->n_data) {
    file->n_data = length;
    file->n_alloc = length;
  } else if (length > file->n_data) {
    file->n_alloc = length;
  }

  assert(fat32_delete(fs, directory, filename));
  assert(fat32_create(fs, directory, filename, 0));
  assert(fat32_write(fs, directory, filename, file));
  return 1;
}

int fat32_write(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, pi_file_t *file) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: Surprisingly, this one should be rather straightforward now.
  // - load the directory
  // - exit with an error (0) if there's no matching directory entry
  // - update the directory entry with the new size
  // - write out the file as clusters & update the FAT
  // - write out the directory entry
  // Special case: the file is empty to start with, so we need to update the
  // start cluster in the dirent
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_idx = find_dirent_with_name(dirents, n_dirents, filename);

  // no match
  if (dirent_idx == -1)
    return 0;

  // directory record/entry for the target file
  fat32_dirent_t *file_dirent = dirents + dirent_idx;
  uint32_t file_cluster_id = fat32_cluster_id(file_dirent);
  uint32_t start_cluster = write_cluster_chain(fs, file_cluster_id, file->data, file->n_alloc);

  // update directory entry
  file_dirent->lo_start = start_cluster & 0xffff;
  file_dirent->hi_start = (start_cluster >> 16) & 0xffff;
  file_dirent->file_nbytes = file->n_data;

  // write file_dirent
  uint32_t file_dirent_cluster_id = directory->cluster_id;
  uint32_t file_dirent_cluster_num = dirent_idx / (16 * fs->sectors_per_cluster);
  uint32_t count = 0;
  while (count < file_dirent_cluster_num) {
    count++;
    file_dirent_cluster_id = fs->fat[file_dirent_cluster_id];
  }

  uint32_t lba = cluster_to_lba(fs, file_dirent_cluster_id);
  fat32_dirent_t *sec = pi_sec_read(lba, fs->sectors_per_cluster);
  fat32_dirent_t *addr = sec + (dirent_idx % (16 * fs->sectors_per_cluster));
  memcpy(addr, file_dirent, 32);
  pi_sd_write(sec, lba, fs->sectors_per_cluster);

  // update FAT
  write_fat_to_disk(fs);

  if (verify_write) {
    sec = pi_sec_read(lba, fs->sectors_per_cluster);
    addr = sec + (dirent_idx % (16 * fs->sectors_per_cluster));
    demand(addr->access_date == file_dirent->access_date, "wrong directory entry contents written");
    demand(addr->attr == file_dirent->attr, "wrong directory entry contents written");
    demand(addr->file_nbytes == file->n_data, "wrong directory entry contents written");
    demand(addr->lo_start == (start_cluster & 0xffff), "wrong directory entry contents written");
    demand(addr->hi_start == ((start_cluster >> 16) & 0xffff), "wrong directory entry contents written");
    // fat32_dirent_print("Verify", (fat32_dirent_t *) addr);
    uint32_t bytes_per_cluster = fs->sectors_per_cluster * 512;
    uint32_t file_num_clusters = (file->n_alloc + bytes_per_cluster - 1) / bytes_per_cluster;
    if (file->n_alloc > 0) {
      count = 0;
      while (count < file_num_clusters) {
        uint32_t tmp = fs->fat[start_cluster];
        demand(fat32_fat_entry_type(tmp) != FREE_CLUSTER, "FAT incorrect");
        if (count == file_num_clusters - 1) {
          // printk("%x %x\n", tmp, fat32_fat_entry_type(tmp));
          demand(fat32_fat_entry_type(tmp) == LAST_CLUSTER, "FAT incorrect");
        }
        if (fat32_fat_entry_type(tmp) == LAST_CLUSTER) {
          demand(count == file_num_clusters - 1, "FAT incorrect");
        }
        count++;
        start_cluster = tmp;
      }
    } else {
      demand(start_cluster == 0, "FAT incorrect");
    }
  }
  
  return 1;
}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}
