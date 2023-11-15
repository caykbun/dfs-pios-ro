#include "rpi.h"
#include "fat32/pi-sd.h"
#include "fat32/fat32.h"

void notmain() {
  uart_init();
  kmalloc_init();
  pi_sd_init();

  printk("Reading the MBR.\n");
  mbr_t *mbr = mbr_read();

  printk("Loading the first partition.\n");
  mbr_partition_ent_t partition;
  memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
  assert(mbr_part_is_fat32(partition.part_type));

  printk("Loading the FAT.\n");
  fat32_fs_t fs = fat32_mk(&partition);

  printk("Loading the root directory.\n");
  pi_dirent_t root = fat32_get_root(&fs);

  printk("Listing files:\n");
  uint32_t n;
  pi_directory_t files = fat32_readdir(&fs, &root);
  printk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    } else {
      printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }

  // char *hello_name = "EMPTY.TXT";
  printk("Creating hello.txt\n");
  char *hello_name = "HELLO.TXT";
  fat32_delete(&fs, &root, hello_name);
  assert(fat32_create(&fs, &root, hello_name, 0));
  char *data = "Hello, World!\n";
  pi_file_t hello = (pi_file_t) {
    .data = data,
    .n_data = strlen(data),
    .n_alloc = strlen(data),
  };

  assert(fat32_write(&fs, &root, hello_name, &hello));
  printk("Check your SD card for a file called 'HELLO.TXT'\n");

  char *folder_name = "FOLDER";
//   fat32_delete(&fs, &root, folder_name);
  pi_dirent_t *folder = fat32_create(&fs, &root, folder_name, 1);
//   fat32_create(&fs, folder, hello_name, 0);
  files = fat32_readdir(&fs, &root);
  printk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    } else {
      printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }
  files = fat32_readdir(&fs, folder);
  printk("Got %d files.\n", files.ndirents);
  for (int i = 0; i < files.ndirents; i++) {
    if (files.dirents[i].is_dir_p) {
      printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
    } else {
      printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
    }
  }


  printk("PASS: %s\n", __FILE__);
  clean_reboot();
}
