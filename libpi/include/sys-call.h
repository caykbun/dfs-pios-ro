#ifndef __SYS_CALL_H__
#define __SYS_CALL_H__

#include "rpi.h"

enum {
    REQUEST_STDOUT = 1,
    WAIT_STDOUT = 2,
    REQUEST_STDIN = 3,
    END_STDIN = 4,
    FS_WRITE = 5,
    NRF_START_SEND = 6,
    NRF_WAIT_SEND = 7,
    NRF_SPI_TRANSFER = 8
};

void make_system_call(unsigned sys_call_num, ...);

void sys_call_request_stdout();
void sys_call_wait_stdout();
void sys_call_scank();
void sys_call_end_stdin();
void sys_call_fs_write();
void sys_call_nrf_start_send();
void sys_call_nrf_wait_send(uint32_t nbytes, void *n);
void sys_call_spi_transfer(void *n, uint32_t txaddr, void *msg, unsigned nbytes);

#endif