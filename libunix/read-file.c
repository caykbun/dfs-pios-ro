#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libunix.h"

// allocate buffer, read entire file into it, return it.   
// buffer is zero padded to a multiple of 4.
//
//  - <size> = exact nbytes of file.
//  - for allocation: round up allocated size to 4-byte multiple, pad
//    buffer with 0s. 
//
// fatal error: open/read of <name> fails.
//   - make sure to check all system calls for errors.
//   - make sure to close the file descriptor (this will
//     matter for later labs).
// 
void *read_file(unsigned *size, const char *name) {
    // How: 
    //    - use stat to get the size of the file.
    //    - round up to a multiple of 4.
    //    - allocate a buffer
    //    - zero pads to a multiple of 4.
    //    - read entire file into buffer.  
    //    - make sure any padding bytes have zeros.
    //    - return it.   
    struct stat statbuf;
    if (stat(name, &statbuf) == -1) 
        return NULL;
    *size = statbuf.st_size;
    int div4 = (statbuf.st_size + 3) / 4;
    void *buf = calloc(div4, 4);
    if (buf == NULL)
        return NULL;
    int fd = open(name, O_RDONLY);
    if (fd == -1)
        return NULL;
    if (read(fd, buf, statbuf.st_size) == -1)
        return NULL;
    if (close(fd) == -1)
        return NULL;
    return buf;
}
