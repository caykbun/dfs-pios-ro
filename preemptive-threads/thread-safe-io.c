#include "thread-safe-io.h"
#include "preemptive-thread.h"
#include "timer-interrupt.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "lock.h"
#include "uart-interrupt.h"


static lock_wtid_t stdout_lock = {0};
static cq_t putQ;
static cq_t getQ;

const int stdio_is_idle(uint32_t thread_id) {
    uint32_t stdout_tid = read_lock_tid(&stdout_lock);
    if (stdout_tid == 0 || stdout_tid == thread_id) {
        return 1;
    } else {
        return 0;
    }
}

void write_stdio_tid(uint32_t thread_id) {
    write_lock_tid(&stdout_lock, thread_id);
}

const uint32_t read_stdio_tid() {
    return read_lock_tid(&stdout_lock);
}

int _thread_safe_printk(const char *format, ...) {
    // first request to use stdout
    // will be blocked if another thread is using
    make_system_call(REQUEST_STDOUT);

    va_list args;
    int ret;
    va_start(args, format);
       ret = vprintk(format, args);
    va_end(args);

    // then wait for stdout queue to drain
    // will be blocked stdout queue is not empty
    make_system_call(WAIT_STDOUT);
    return ret;
}

int _thread_safe_scank(const char *fmt, ...) {
    make_system_call(REQUEST_STDIN);

    va_list args;

    int ret = 0;
    va_start(args, fmt);
       vscank(fmt, args);
    va_end(args);

    make_system_call(END_STDIN);
    return ret;
}

void stdio_init() {
    cq_init(&putQ, 1);
    cq_init(&getQ, 1);
}

int stdout_queue_is_full() {
    return cq_full(&putQ);
}

int stdout_queue_is_empty() {
    return cq_empty(&putQ);
}

void stdout_queue_push_char(unsigned char c) {
    cq_push(&putQ, c);
}

unsigned char stdout_queue_pop_char() {
    return cq_pop(&putQ);
}

unsigned int get_stdout_queue_length() {
    return cq_nelem(&putQ);
}

int stdin_queue_is_full() {
    return cq_full(&getQ);
}

int stdin_queue_is_empty() {
    return cq_empty(&getQ);
}

void stdin_queue_push_char(unsigned char c) {
    cq_push(&getQ, c);
}

unsigned char stdin_queue_pop_char() {
    return cq_pop(&getQ);
}

unsigned int get_stdin_queue_length() {
    return cq_nelem(&getQ);
}

/* for debug purpose */
void print_stdout_queue() {
    while (!cq_empty(&putQ)) {
        rpi_putchar(cq_pop(&putQ));
    }
}