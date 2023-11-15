#include "rpi-asm.h"

MK_FN(sys_call_request_stdout)
    swi 1

MK_FN(sys_call_wait_stdout)
    swi 2

MK_FN(sys_call_scank)
    swi 3

MK_FN(sys_call_end_stdin)
    swi 4

MK_FN(sys_call_nrf_start_send)
    swi 6

MK_FN(sys_call_nrf_wait_send)
    swi 7

MK_FN(sys_call_spi_transfer)
    swi 8