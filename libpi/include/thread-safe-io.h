#ifndef __THREAD_SAFE_IO_H__
#define __THREAD_SAFE_IO_H__

#include "rpi.h"
#include "circular.h"
#include "sys-call.h"


const int stdio_is_idle(uint32_t thread_id);

void write_stdio_tid(uint32_t num);

const uint32_t read_stdio_tid();

int _thread_safe_printk(const char *format, ...);

int _thread_safe_scank(const char *format, ...);

void stdio_init();

int stdout_queue_is_full();

int stdout_queue_is_empty();

void stdout_queue_push_char(unsigned char c);

unsigned char stdout_queue_pop_char();

unsigned int get_stdout_queue_length();

int stdin_queue_is_full();

int stdin_queue_is_empty();

void stdin_queue_push_char(unsigned char c);

unsigned char stdin_queue_pop_char();

unsigned int get_stdin_queue_length();

void print_stdout_queue();

#endif