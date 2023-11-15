// engler,cs140e: trivial non-pre-emptive threads package.
#ifndef __PRE_THREAD_H__
#define __PRE_THREAD_H__

/*
 * trivial thread descriptor:
 *   - reg_save_area: space for all the registers (including
 *     the spsr).
 *   - <next>: pointer to the next thread in the queue that
 *     this thread is on.
 *  - <tid> unique thread id.
 *  - <stack>: fixed size stack: must be 8-byte aligned.  
 *
 * the big simplication: rather than save registers on the 
 * stack we stick them in a single fixed-size location at
 * offset in the thread structure.  
 *  - offset 0 means it's hard to mess up offset calcs.
 *  - doing in one place rather than making space on the 
 *    stack makes it much easier to think about.
 *  - the downside is that you can't do nested, pre-emptive
 *    context switches.  since this is not pre-emptive, we
 *    defer until later.
 *
 * changes:
 *  - dynamically sized stack.
 *  - save registers on stack.
 *  - add condition variables or watch.
 *  - some notion of real-time.
 *  - a private thread heap.
 *  - add error checking: thread runs too long, blows out its 
 *    stack.  
 */

// you should define these; also rename to something better.
#define REG_SP_OFF 36/4
#define REG_LR_OFF 40/4

#define THREAD_MAXSTACK (1024 * 8/4)

#define USE_UART_INTERRUPTS 1
#define USE_OUT_INTERRUPTS 0

enum {
    NONE = 0,
    STDIO = 1
};

typedef struct pre_thread {
    uint32_t *saved_sp; // address that stores sp
	struct pre_thread *next; // not used
	uint32_t tid; // thread id

    void (*fn)(void *arg);
    void *arg; // this can serve as private data, may use a pointer to multiple arguments
    
    const char *annot; // what is this

    // threads waiting on the current one to exit.
    // struct pre_thread *waiters; // hmmm sounds useful but maybe next time

    uint32_t block_reason; // hacky way of doing thread-safe IO
    uint32_t placeholder; // used to exist for alignment

	uint32_t stack[THREAD_MAXSTACK];
} pre_thread_t;
_Static_assert(offsetof(pre_thread_t, stack) % 8 == 0, 
                            "must be 8 byte aligned");

// statically check that the register save area is at offset 0.
_Static_assert(offsetof(pre_thread_t, saved_sp) == 0, 
                "stack save area must be at offset 0");

// main routines.
int get_current_thread_id();

int get_scheduler_thread_id();

// starts the thread system: only returns when there are
// no more runnable threads. 
void pre_thread_start(void);

// get the pointer to the current thread.  
pre_thread_t *pre_cur_thread(void);


// create a new thread that takes a single argument.
typedef void (*pre_code_t)(void *);

pre_thread_t *pre_fork(pre_code_t code, void *arg, int enable_interrupt, int sys_mode);

// exit current thread: switch to the next runnable
// thread, or exit the threads package.
void pre_exit(int exitcode);

/***************************************************************
 * internal routines: we put them here so you don't have to look
 * for the prototype.
 */

// internal routine: 
//  - save the current register values into <old_save_area>
//  - load the values in <new_save_area> into the registers
//  reutrn to the caller (which will now be different!)
void cswitch_from_scheduler(uint32_t **old_sp_save, const uint32_t *new_sp);

void cswitch_from_exit(const uint32_t *new_sp);

void cswitch_from_exception(const uint32_t *new_sp);

// returns the stack pointer (used for checking).
const uint8_t *pre_get_sp(void);

// check that: the current thread's sp is within its stack.
void pre_stack_check(void);

// do some internal consistency checks --- used for testing.
void pre_internal_check(void);

#if 0
void pre_wait(pre_cond_t *c, lock_t *l);

// assume non-preemptive: if you share with interrupt
// will have to modify.
void pre_lock(lock_t *l);
void pre_unlock(lock_t *l);

static inline void lock(lock_t *l) {
    while(get32(l)) != 0)
        pre_yield();
}
static inline void unlock(lock_t *l) {
    *l = 0;
}
#endif

// pre_thread helpers
static inline void *pre_arg_get(pre_thread_t *t) {
    return t->arg;
}
static inline void pre_arg_put(pre_thread_t *t, void *arg) {
    t->arg = arg;
}
static inline unsigned pre_tid(void) {
    pre_thread_t *t = pre_cur_thread();
    if(!t)
        panic("pre_threads not running\n");
    return t->tid;
}

void pre_print_regs(uint32_t *sp);

void print_cpsr();

void clear_cur_block_status();

void test_prefetch_flush(uint32_t val);

void pre_debug_print(uint32_t *sp);

#endif
