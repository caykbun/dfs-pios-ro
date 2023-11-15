// engler, cs140e: your code to find the tty-usb device on your laptop.
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "libunix.h"

#define _SVID_SOURCE
#include <dirent.h>
static const char *ttyusb_prefixes[] = {
	"ttyUSB",	// linux
    "ttyACM",   // linux
	"cu.SLAB_USB", // mac os
    "cu.usbserial", // mac os
    "cu.usbserial-0001", // mac os m1
    // if your system uses another name, add it.
	0
};

int versionsort(const struct dirent **a, const struct dirent **b) {
    struct stat statbuf1;
    struct stat statbuf2;
    stat((*a)->d_name, &statbuf1);
    stat((*b)->d_name, &statbuf2);
    return statbuf1.st_mtime < statbuf2.st_mtime;
}


static int filter(const struct dirent *d) {
    // scan through the prefixes, returning 1 when you find a match.
    // 0 if there is no match.
    for (int i = 0; i < 5; ++i) {
        if (strcmp(d->d_name, ttyusb_prefixes[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// find the TTY-usb device (if any) by using <scandir> to search for
// a device with a prefix given by <ttyusb_prefixes> in /dev
// returns:
//  - device name.
// error: panic's if 0 or more than 1 devices.
char *find_ttyusb(void) {
    // use <alphasort> in <scandir>
    // return a malloc'd name so doesn't corrupt.
    struct dirent **namelist;
    int n = scandir("/dev", &namelist, filter, alphasort);
    if (n == -1) {
        perror("scandir");
        exit(EXIT_FAILURE);
    }

    if (n == 0 || n > 1) {
        panic("0 or more than 1 device found.\n");
    }
    char *s = (char *) malloc(strlen(namelist[0]->d_name) + 6);
    strcpy(s, "/dev/");
    return strcat(s, namelist[0]->d_name);
}

// return the most recently mounted ttyusb (the one
// mounted last).  use the modification time 
// returned by state.
char *find_ttyusb_last(void) {
    struct dirent **namelist;
    int n = scandir("/dev", &namelist, filter, versionsort);
    if (n == -1) {
        perror("scandir");
        exit(EXIT_FAILURE);
    }

    if (n == 0) {
        panic("0 device found.\n");
    }
    char *s = (char *) malloc(strlen(namelist[n-1]->d_name) + 6);
    strcpy(s, "/dev/");
    return strcat(s, namelist[n-1]->d_name);
}

// return the oldest mounted ttyusb (the one mounted
// "first") --- use the modification returned by
// stat()
char *find_ttyusb_first(void) {
    struct dirent **namelist;
    int n = scandir("/dev", &namelist, filter, versionsort);
    if (n == -1) {
        perror("scandir");
        exit(EXIT_FAILURE);
    }

    if (n == 0) {
        panic("0 device found.\n");
    }
    char *s = (char *) malloc(strlen(namelist[0]->d_name) + 6);
    strcpy(s, "/dev/");
    return strcat(s, namelist[0]->d_name);
}
