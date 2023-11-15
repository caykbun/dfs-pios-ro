#include "global.h"


static int KMALLOC_READY = 0;
static int UART_READY = 0;
static int FAT32_READY = 0;
static int NRF1_READY = 0;
static int NRF2_READY = 0;
static int TIMER_READY = 0;
static int INT_VECTOR_READY = 0;

int set_kmalloc_ready() {
    if (KMALLOC_READY)
        return 0;
    KMALLOC_READY = 1;
    return 1;
}

int set_uart_ready() {
    if (UART_READY)
        return 0;
    UART_READY = 1;
    return 1;
}

int reset_uart_ready() {
    UART_READY = 0;
    return 1;
}

int set_fat32_ready() {
    if (FAT32_READY)
        return 0;
    FAT32_READY = 1;
    return 1;
}

int set_nrf1_ready() {
    if (NRF1_READY)
        return 0;
    NRF1_READY = 1;
    return 1;
}

int set_nrf2_ready() {
    if (NRF2_READY)
        return 0;
    NRF2_READY = 1;
    return 1;
}

int set_timer_ready() {
    if (TIMER_READY)
        return 0;
    TIMER_READY = 1;
    return 1;
}

int set_int_vector_ready() {
    if (INT_VECTOR_READY)
        return 0;
    INT_VECTOR_READY = 1;
    return 1;
}