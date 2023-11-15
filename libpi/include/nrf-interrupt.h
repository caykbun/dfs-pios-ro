#ifndef __NRF_INTERRUPT_H__
#define __NRF_INTERRUPT_H__
// aggregate all the test definitions and code.
//  search for "TODO" below for the 6 routines to write.
#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "nrf.h"


void nrf_int_startup();

enum { N = 1024*32 };

void nrf_interrupt_init(unsigned pin);
int nrf_test_pin(unsigned pin);

void set_nrf_left(nrf_t *_server);
void set_nrf_right(nrf_t *_client);

nrf_t *get_nrf_left();
nrf_t *get_nrf_right();



#endif
