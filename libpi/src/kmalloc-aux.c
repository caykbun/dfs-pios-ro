#include "rpi.h"
#include "global.h"


void _kmalloc_init(void) {
    if (!set_kmalloc_ready()) {
        return;
    }
    unsigned MB = 1024*1024;
    kmalloc_init_set_start((void*)MB, 64*MB);
}