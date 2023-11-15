#ifndef __LOCK_H__
#define __LOCK_H__

#include "rpi.h"

typedef struct lock_wtid {
    volatile uint32_t tid;
} lock_wtid_t;

uint32_t read_lock_tid(lock_wtid_t *lock) {
    return lock->tid;
}

void write_lock_tid(lock_wtid_t *lock, uint32_t new_tid) {
    lock->tid = new_tid;
}

#endif