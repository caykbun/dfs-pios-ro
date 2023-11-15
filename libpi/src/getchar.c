#include "rpi.h"

typedef int (*rpi_getchar_t)();

static int default_getchar() { 
    int c = uart_get8(); 
    return c; 
}

rpi_getchar_t rpi_getchar = default_getchar;

rpi_getchar_t rpi_getchar_set(rpi_getchar_t getc) {
    rpi_getchar_t old  = rpi_getchar;
    rpi_getchar = getc;
    return old;
}

void rpi_getchar_reset() {
    rpi_getchar = default_getchar;
}
