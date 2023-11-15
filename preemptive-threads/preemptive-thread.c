#include "rpi.h"
#include "preemptive-thread.h"
#include "timer-interrupt.h"
#include "rpi-inline-asm.h"
#include "thread-safe-io.h"
#include "uart-interrupt.h"
#include "circular.h"
#include "fat32/fat32.h"
#include "exception-handlers.h"
#include "cpsr-util.h"
#include "sys-call.h"
#include "dfs-common.h"


// tracing code.  set <trace_p>=0 to stop tracing
enum { trace_p = 0};
#define th_trace(args...) do {                          \
    if(trace_p) {                                       \
        trace(args);                                   \
    }                                                   \
} while(0)

#define E pre_thread_t
#include "libc/Q.h"

// currently only have a single run queue and a free queue.
// the run queue is FIFO.
static Q_t runq, freeq, blockq;
static pre_thread_t *cur_thread;        // current running thread.
static pre_thread_t *scheduler_thread;  // first scheduler thread.
static pre_thread_t *wait_stdout_thread = NULL;
static pre_thread_t *wait_stdin_thread = NULL;
static unsigned tid = 1;

// total number of thread blocks we have allocated.
static unsigned nalloced = 0;

// keep a cache of freed thread blocks.  call kmalloc if run out.
static pre_thread_t *th_alloc(void) {
    pre_thread_t *t = Q_pop(&freeq);

    if(!t) {
        t = kmalloc_aligned(sizeof *t, 8);
        nalloced++;
    }
#   define is_aligned(_p,_n) (((unsigned)(_p))%(_n) == 0)
    demand(is_aligned(&t->stack[0],8), stack must be 8-byte aligned!);
    t->tid = tid++;
    return t;
}

static void th_free(pre_thread_t *th) {
    // push on the front in case helps with caching.
    Q_push(&freeq, th);
}

// stack offset
enum {
    R0_OFFSET = 0,
    R4_OFFSET = 4,
    R5_OFFSET,
    R6_OFFSET,
    R7_OFFSET,
    R8_OFFSET,
    R9_OFFSET,
    R10_OFFSET,
    R11_OFFSET,
    R12_OFFSET,
    LR_OFFSET = 13,
    PC_OFFSET = 14,
    CPSR_OFFSET = 15,
};

static fat32_fs_t fs;

/* DON'T USE THESE FOR NOW */
void log_init() {
   _kmalloc_init();
    pi_sd_init();
    mbr_t *mbr = mbr_read();
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));
    fs = fat32_mk(&partition);
   
}

/* DON'T USE THESE FOR NOW */
void write_log() {
    pi_dirent_t root = fat32_get_root(&fs);
    fat32_delete(&fs, &root, "DEBUG.TXT");
    assert(fat32_create(&fs, &root, "DEBUG.TXT", 0));
    char *data = "Hello, World!\n";
    pi_file_t log = (pi_file_t) {
        .data = data,
        .n_data = strlen(data),
        .n_alloc = strlen(data),
    };
    assert(fat32_write(&fs, &root, "DEBUG.TXT", &log));
}

void stdout_queue_to_txfifo(void *arg) {
    while (uart_can_put8()) {
        if (stdout_queue_is_empty()) {
            // disable tx interrupt
            uart_disable_interrupt(1);
            break;
        }
        unsigned char c = stdout_queue_pop_char();
        uart_put8(c);
    }
    write_stdio_tid(0);
    // fork another if not drained!
    if (!stdout_queue_is_empty()) {
        pre_thread_t *th_printer = pre_fork(&stdout_queue_to_txfifo, NULL, 0, 1);
        th_printer->block_reason = STDIO;
        write_stdio_tid(th_printer->tid);
    }
    
}

void rxfifo_to_stdin_queue(void *arg) {
    while (uart_has_data()) {
        if (stdin_queue_is_full()) {
            // drop data!
            break;
        }
        unsigned char c = uart_get8();
        stdin_queue_push_char(c);
    }
}

void int_vector_last(unsigned pc, unsigned sp) {
    cswitch_from_exception((uint32_t *)sp);
}

void int_vector_uart_pre_threads(unsigned pc, unsigned sp) {
    dev_barrier();

    // not an aux uart interrupt
    if (GET32(AUX_MU_IIR_REG) & 1) {
        return;
    }

    // if it is tx interrupt: non-full tx fifo
    if (GET32(AUX_MU_IIR_REG) & 2) {
        // drain the queue until fifo is full
        while (uart_can_put8()) {
            if (stdout_queue_is_empty()) {
                // disable tx interrupt
                uart_disable_interrupt(1);
                break;
            }
            unsigned char c = stdout_queue_pop_char();
            uart_put8(c);
        }
    }
    // if it is rx interrupt: non-empty rx fifo
    else if (GET32(AUX_MU_IIR_REG) & 4) {
        // fill the queue until fifo is empty
        while (uart_has_data()) {
            if (stdin_queue_is_full()) {
                // drop data!
                break;
            }
            unsigned char c = uart_get8();
            stdin_queue_push_char(c);
        }
    }

    dev_barrier();

    cswitch_from_exception((uint32_t *)sp);
}

int syscall_vector(unsigned pc, unsigned sp, void *arg1, void *arg2, void *arg3, void *arg4) {
    uint32_t inst, sys_num, mode;

    inst = *(uint32_t *)pc;
    sys_num = inst - 0xef000000;
    mode = spsr_get() & 0b11111;

    // put spsr mode in <mode>
    if(mode != USER_MODE && mode != SYS_MODE)
        panic("mode = %b: expected %b\n", mode, USER_MODE);
    else
        th_trace("success: spsr is at user/sys level\n");

    switch(sys_num) {
    case REQUEST_STDOUT:
        th_trace("syscall: requesting stdout\n");
        if (stdio_is_idle(cur_thread->tid)) {
            write_stdio_tid(cur_thread->tid);
            cur_thread->block_reason = STDIO;
            cswitch_from_exception((uint32_t *)sp);
        } else {
            cur_thread->saved_sp = (uint32_t *)sp;
            cur_thread->block_reason = STDIO;
            Q_append(&blockq, cur_thread);
            cur_thread = scheduler_thread;
            cswitch_from_exception(scheduler_thread->saved_sp);
        }
    case REQUEST_STDIN:
        th_trace("syscall: requesting stdin\n");
        if (stdio_is_idle(cur_thread->tid)) {
            // flush and enable rx interrupt?
            uart_flush_rx();
            uart_enable_interrupt(0);
            write_stdio_tid(cur_thread->tid);
            cur_thread->block_reason = STDIO;
            // don't return to thread -- wake it up until there is input
            cur_thread->saved_sp = (uint32_t *)sp;
            wait_stdin_thread = cur_thread;
            cur_thread = scheduler_thread;
            cswitch_from_exception(scheduler_thread->saved_sp);
        } else {
            cur_thread->saved_sp = (uint32_t *)sp;
            cur_thread->block_reason = STDIO;
            Q_append(&blockq, cur_thread);
            cur_thread = scheduler_thread;
            cswitch_from_exception(scheduler_thread->saved_sp);
        }
    case WAIT_STDOUT: 
        th_trace("syscall: waiting stdout\n");
        uart_enable_interrupt(1);
        if (stdout_queue_is_empty()) {
            write_stdio_tid(0);
            cur_thread->block_reason = NONE;
            cswitch_from_exception((uint32_t *)sp);
        } else {
            // create a non-interruptable printer thread
            pre_thread_t *th_printer = pre_fork(&stdout_queue_to_txfifo, NULL, 0, 1);
            th_printer->block_reason = STDIO;
            write_stdio_tid(th_printer->tid);
            cur_thread->saved_sp = (uint32_t *)sp;
            wait_stdout_thread = cur_thread;
            cur_thread = scheduler_thread;
            cswitch_from_exception(scheduler_thread->saved_sp);
        }
    case END_STDIN:
        th_trace("syscall: ended stdout\n");
        // DRAIN stdin queue?
        // disable uart rx interrupt
        uart_disable_interrupt(0);
        write_stdio_tid(0);
        wait_stdout_thread->block_reason = NONE;
        cswitch_from_exception((uint32_t *)sp);
    case NRF_SPI_TRANSFER:
        spi_transfer_syscall_handler(pc, sp, arg1, (uint32_t)arg2, arg3, (unsigned)arg4);
        cswitch_from_exception((uint32_t *)sp);
    default: 
        if (uart_interrupt_is_on()) {
            uart_reset_int_off();
        }
        trace("Illegal system call = %d!\n", sys_num);
        clean_reboot();
    }
}

void int_vector_pre_threads(unsigned pc, unsigned sp) {
    dev_barrier();
    unsigned pending = GET32(IRQ_basic_pending);

    // not a timer interrupt
    if((pending & RPI_BASIC_ARM_TIMER_IRQ) == 0)
        return;

    PUT32(arm_timer_IRQClear, 1);
    dev_barrier();

    th_trace("IN TIMER INTERRUPT saved lr is %x %d\n", ((uint32_t *)sp)[8], ((uint32_t *)sp)[8]);
    th_trace("IN TIMER INTERRUPT pc is %x %d\n", pc, pc);

    Q_append(&runq, cur_thread);
    cur_thread->saved_sp = (uint32_t *)sp;
    cur_thread = scheduler_thread;
    cswitch_from_exception(scheduler_thread->saved_sp);
}

// return pointer to the current thread.  
pre_thread_t *pre_cur_thread(void) {
    return cur_thread;
}

// create a new thread.
pre_thread_t *pre_fork(void (*code)(void *arg), void *arg, int enable_interrupt, int sys_mode) {
    
    pre_thread_t *t = th_alloc();

    // write this so that it calls code,arg.
    void pre_init_trampoline(void (*c)(void *a), void *a);

    /*
     * must do the "brain surgery" (k.thompson) to set up the stack
     * so that when we context switch into it, the code will be
     * able to call code(arg).
     *
     *  1. write the stack pointer with the right value.
     *  2. store arg and code into two of the saved registers.
     *  3. store the address of pre_init_trampoline into the lr
     *     position so context switching will jump there.
     */

    t->arg = arg;
    t->fn = code;
    t->block_reason = NONE;
    t->saved_sp = &t->stack[THREAD_MAXSTACK - 17];
    t->saved_sp[R4_OFFSET] = (uint32_t)(arg);
    t->saved_sp[R5_OFFSET] = (uint32_t)(code);
    t->saved_sp[LR_OFFSET] = (uint32_t)(&pre_init_trampoline);
    t->saved_sp[PC_OFFSET] = (uint32_t)(&pre_init_trampoline);

    // previous
    // t->saved_sp[CPSR_OFFSET] = ((uint32_t)cpsr_get()) & ~(1 << 7); // enable interrupts

    unsigned mode = sys_mode == 1 ? 0b11111 : 0b10000;

    // current. because we don't want our stdout_queue_to_txfifo thread to be interrupted
    if (enable_interrupt)
        t->saved_sp[CPSR_OFFSET] = 0x60000100 | mode; // may be hacky
    else
        t->saved_sp[CPSR_OFFSET] = 0x60000180 | mode; // may be hacky

    th_trace("pre_fork: tid=%d, code=[%p], arg=[%x], saved_sp=[%p]\n",
            t->tid, code, arg, t->saved_sp);

    Q_append(&runq, t);
    return t;
}

// exit current thread.
//   - if no more threads, switch to the scheduler.
//   - otherwise context switch to the new thread.
//     make sure to set cur_thread correctly!
void pre_exit(int exitcode) {
    th_trace("Switching after exiting from %d\n", cur_thread->tid);
    th_free(cur_thread);
    cur_thread = scheduler_thread;
    /* DON'T CREATE USER THREADS FOR NOW. STILL BUGGY */
    if ((cpsr_get() & 0b11111) == USER_MODE) {
        cpsr_set(mode_set(cpsr_get(), SYS_MODE));
    }
    cswitch_from_exit(scheduler_thread->saved_sp);
}

/*
 * starts the thread system.  
 * note: our caller is not a thread!  so you have to 
 * create a fake thread (assign it to scheduler_thread)
 * so that context switching works correctly.   your code
 * should work even if the runq is empty.
 */
void pre_thread_start(void) {
    // should not do it here if we are using real cspr for thread init
    switch_to_sys_mode();

    // set interrupt vector base
    extern uint32_t interrupt_vector_pre_threads[];
    int_vec_init((void *)interrupt_vector_pre_threads);

    // initialize timer interrupt
    timer_interrupt_init(0x100);
    
    // register interrupt handlers
    register_timer_handler(int_vector_pre_threads);
    register_uart_handler(int_vector_uart_pre_threads);
    register_last_handler(int_vector_last);

    // initialize uart
    if (USE_UART_INTERRUPTS) {
        uart_init_int();
        // printk("use uart ints\n");
    } else {
        uart_init();
    }

    // initialize kmalloc
    _kmalloc_init();

    // TODO: (optional) log writing
    // log_init();

    /* Note that global interrupt is not enabled but timer is */
    // cpsr_int_disable();
    // timer_interrupt_turnoff();

    // make sure it is system mode and global interrupt is not enabled
    assert ((cpsr_get() & 0b11111) == SYS_MODE);
    assert ((cpsr_get() & (1 << 7)) != 0);

    th_trace("starting threads!\n");

    // no other runnable thread: return.
    if(Q_empty(&runq))
        goto end;

    // setup scheduler thread block.
    if(!scheduler_thread) {
        scheduler_thread = th_alloc();
        cur_thread = scheduler_thread;
    }

    /* shall not end loop if there exists runnable/blocked */
    while (!Q_empty(&runq) || !Q_empty(&blockq) || wait_stdout_thread != NULL || wait_stdin_thread != NULL) {
        if (!stdin_queue_is_empty() && wait_stdin_thread != NULL) {
            Q_append(&runq, wait_stdin_thread);
            wait_stdin_thread = NULL;
        }

        // dirty logic, may improve in the future
        if (stdout_queue_is_empty() && wait_stdout_thread != NULL) {
            write_stdio_tid(0);
            wait_stdout_thread->block_reason = NONE;
            Q_append(&runq, wait_stdout_thread);
            wait_stdout_thread = NULL;
        }

        if (stdio_is_idle(0)) {
            while (!Q_empty(&blockq)) {
                if (Q_start(&blockq)->block_reason == STDIO) {
                    pre_thread_t *th = Q_pop(&blockq);
                    Q_append(&runq, th);
                } else {
                    break;
                }
            }
        }

        while (!Q_empty(&runq)) {
            pre_thread_t *next_thread = Q_pop(&runq);
            th_trace("check tid %d STDIO state %d\n ", next_thread->tid, read_stdio_tid());
            if (!stdio_is_idle(next_thread->tid) && next_thread->block_reason != NONE) {
                th_trace("fail tid %d\n", next_thread->tid);
                Q_append(&blockq, next_thread);
                continue;
            }
            if (next_thread->block_reason == STDIO) {
                write_stdio_tid(next_thread->tid);
                th_trace("BLOCK but idle\n");
            } else {
                th_trace("what state %d\n", next_thread->block_reason);
            }
            th_trace("success tid %d output state %d\n ", next_thread->tid, read_stdio_tid());
            uint32_t **old_sp = &(scheduler_thread->saved_sp);
            cur_thread = next_thread;
            th_trace("switching from scheduler to tid=%d, next sp=%x\n", cur_thread->tid, cur_thread->saved_sp);
            
            cswitch_from_scheduler(old_sp, next_thread->saved_sp);
            break;
        }
    }

    // otherwise reboot will hang...
    uart_reset_int_off();

end:
    th_trace("done with all threads, returning\n");
}

int get_current_thread_id() {
    return cur_thread->tid;
}

int get_scheduler_thread_id() {
    return scheduler_thread->tid;
}

// helper routine: can call from assembly with r0=sp and it
// will print the stack out.  it then exits.
// call this if you can't figure out what is going on in your
// assembly.
void pre_print_regs(uint32_t *sp) {
#if 1
    // use this to check that your offsets are correct.
    printk("cur-thread=%d\n", cur_thread->tid);
    printk("sp=%p\n", sp);

    // stack pointer better be between these.
    printk("stack=%p\n", &cur_thread->stack[THREAD_MAXSTACK]);
    if (cur_thread->tid != scheduler_thread->tid) {
        assert(sp < &cur_thread->stack[THREAD_MAXSTACK]);
        assert(sp >= &cur_thread->stack[0]);
    }
    for(unsigned i = 0; i < 16; i++) {
        unsigned r = i >= 13 ? i + 1 : i;
        printk("sp[%d]=r%d=%x\n", i, r, sp[i]);
    }
    // clean_reboot();
#endif
}

void print_cpsr() {
    printk("cpsr is %b\n", cpsr_get() & 0b11111);
    clean_reboot();
}

void print_reg (uint32_t reg) {
    trace("print reg %x %d\n", reg);
}

void clear_cur_block_status() {
    cur_thread->block_reason = NONE;
}

void pre_debug_print(uint32_t *sp) {
    if (uart_interrupt_is_on()) {
        // usually if there is a bug with uart interrupt on
        // should turn it off otherwise print won't work
        uart_reset_int_off();
    }
    if (wait_stdout_thread != NULL) {
        trace("Wait stdout thread block reason: %d; tid: %d\n", wait_stdout_thread->block_reason, wait_stdout_thread->tid);
    }
    trace("Block queue length: %d; runnable queue length: %d\n", blockq.cnt, runq.cnt);
    if (stdout_queue_is_empty()) {
        trace("stdout queue is empty\n");
    } else {
        trace("stdout queue is not empty\n");
    }
    trace("Current mode is: %b\n", cpsr_get() & 0b11111);
    if (cpsr_get() & (1 << 7)) {
        trace("Global interrupt is off\n");
    } else {
        trace("Global interrupt is on\n");
    }
    trace("Current stdio thread id: %d\n", read_stdio_tid());
    trace("Traversing runnable queue and printing out stack:\n");
    while (!Q_empty(&runq)) {
        pre_thread_t *th = Q_pop(&runq);
        trace("tid: %d; block reason: %d\n", th->tid, th->block_reason);
        pre_print_regs((uint32_t *)th->saved_sp);
    }
    if (sp != NULL) {
        pre_print_regs(sp);
    }
    clean_reboot();
}

