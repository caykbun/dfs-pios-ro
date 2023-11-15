#include "sys-call.h"


void make_system_call(unsigned sys_call_num, ...) {
    va_list args;
    switch (sys_call_num)
    {
    case REQUEST_STDOUT: {
        sys_call_request_stdout();
        break;
    }
    case WAIT_STDOUT: {
        sys_call_wait_stdout();
        break;
    }
    case REQUEST_STDIN: {
        sys_call_request_stdout();
        break;
    }
    case END_STDIN: {
        sys_call_end_stdin();
        break;
    }
    case NRF_START_SEND: {
        sys_call_nrf_start_send();
        break;
    }
    case NRF_WAIT_SEND: {
        va_start(args, sys_call_num);
        uint32_t nbytes = va_arg(args, uint32_t);
        void *n = va_arg(args, void *);
        sys_call_nrf_wait_send(nbytes, n);
        va_end(args);
        break;
    }
    case NRF_SPI_TRANSFER: {
        va_start(args, sys_call_num);
        void *nrf = va_arg(args, void *);
        uint32_t txaddr = va_arg(args, uint32_t );
        void *msg = va_arg(args, void *);
        unsigned nbytes2 = va_arg(args, unsigned);
        sys_call_spi_transfer(nrf, txaddr, msg, nbytes2);
        va_end(args);
        break;
    }
    default:
        panic("Illegal/Unsupported system call with number: %d", sys_call_num);
        break;
    }
}