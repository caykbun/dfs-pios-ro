/*
 * implement these.  ordered in difficulty.
 */
#include "rpi-asm.h"

@ return the current stack pointer.
MK_FN(pre_get_sp)
    mov r0, sp
    bx lr

MK_FN(switch_to_sys_mode)
    mov r0, lr
    mov r1, sp
    cps #SYS_MODE
    prefetch_flush(r2)
    mov sp, r1
    mov lr, r0
    bx lr

MK_FN(cswitch_from_scheduler)
    sub sp, sp, #64
    stm sp, {r0-r12,lr}
    add sp, sp, #56
    stm sp, {lr}
    add sp, sp, #4
    mrs r2, cpsr
    stm sp, {r2}
    sub sp, sp, #60

    str sp, [r0]
    mov sp, r1

    ldm sp, {r0-r12,lr}
    add sp, sp, #56
    rfe sp!
    asm_not_reached()

MK_FN(cswitch_from_exit)    
    mov sp, r0

    @ load context
    ldm sp, {r0-r12,lr}
    add sp, sp, #56

    rfe sp!
    asm_not_reached()

MK_FN(cswitch_from_exception)
    mov sp, r0

    @ save USER/SYS mode sp
    add sp, sp, #64
    mov r0, #INT_STACK_ADDR
    sub r0, r0, #4
    str sp, [r0]
    ldm r0, {sp}^

    @ load context
    sub sp, sp, #64
    ldm sp, {r0-r12,lr}^
    add sp, sp, #56

    rfe sp!
    asm_not_reached()

@ use this to setup each thread for the first time.
@ setup the stack so that when cswitch runs it will:
@	- load address of <pre_init_trampoline> into LR
@	- <code> into r1, 
@	- <arg> into r0
@ 
MK_FN(pre_init_trampoline)
    mov r0, r4
    mov r1, r5
    blx r1
    SYSTEM_DISABLE_INTERRUPTS
    bl pre_exit


.weak pre_print_regs
pre_print_regs:
    asm_not_reached()

.weak pre_exit
pre_exit:
    asm_not_reached()


.align 5
.globl interrupt_vector_pre_threads
interrupt_vector_pre_threads:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =syscall_interrupt_asm
  ldr pc, =unhandled_prefetch_abort_vm
  ldr pc, =unhandled_data_abort_vm
  ldr pc, =unhandled_interrupt
fast_interrupt_asm:
  @ FIQ and IRQ are disabled when entering FIQ

  sub lr, lr, #4
  
  mov sp, #INT_STACK_ADDR
  sub sp, sp, #4
  stm sp, {lr}

  sub sp, sp, #4
  stm sp, {sp}^
  ldr sp, [sp]

  sub sp, sp, #64
  stm sp, {r0-r12,lr}^

  add sp, sp, #56
  stm sp, {lr}

  add sp, sp, #4
  mrs r1, spsr
  stm sp, {r1}

  sub sp, sp, #60

  mov r1, sp
               
  mov sp, #INT_STACK_ADDR
  sub sp, sp, #8
  str r1, [sp]
  ldm sp, {sp}^
  add sp, sp, #4
  ldr r0, [sp]
  add sp, sp, #4
  bl    fiq_vector           
  asm_not_reached()

syscall_interrupt_asm:
  @ get the user/system mode banked sp
  mov sp, #INT_STACK_ADDR
  sub sp, sp, #4
  stm sp, {sp}^
  ldr sp, [sp]

  sub sp, sp, #64
  stm sp, {r0-r12,lr}^
  ldm sp, {r2-r5}
  add sp, sp, #56
  stm sp, {lr}^
  add sp, sp, #4
  mrs r1, spsr
  stm sp, {r1}
  sub sp, sp, #60

  mov r1, sp

  mov sp, #INT_STACK_ADDR
  sub sp, sp, #4
  str r1, [sp]
  ldm sp, {sp}^

  sub lr, lr, #4
  mov r0, lr
  add sp, sp, #4
  bl  syscall_vector 
  asm_not_reached()

MK_FN(test_prefetch_flush)
  bl print_reg
  prefetch_flush(r0)
  bl print_reg
  bx lr