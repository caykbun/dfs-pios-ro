#include "rpi-asm.h"

.align 5
.globl interrupt_vector_uart
interrupt_vector_uart:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =unhandled_swi
  ldr pc, =unhandled_prefetch_abort
  ldr pc, =unhandled_data_abort
  ldr pc, =unhandled_interrupt
  ldr pc, =unhandled_fiq_uart

.align 5
.globl interrupt_vector_vm
interrupt_vector_vm:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =unhandled_swi
  ldr pc, =unhandled_prefetch_abort_vm
  ldr pc, =unhandled_data_abort_vm
  ldr pc, =unhandled_interrupt
  ldr pc, =unhandled_fiq

.align 5
.globl interrupt_vector_timer
interrupt_vector_timer:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =unhandled_swi
  ldr pc, =unhandled_prefetch_abort_vm
  ldr pc, =unhandled_data_abort_vm
  ldr pc, =unhandled_interrupt
  ldr pc, =unhandled_fiq_timer

MK_TRAMPOLINE(unhandled_fiq_uart,   4, int_vector_uart)
MK_TRAMPOLINE(unhandled_fiq_timer,   4, int_vector_timer)
MK_TRAMPOLINE(unhandled_prefetch_abort_vm,   4, prefetch_abort_vector_vm)
MK_TRAMPOLINE(unhandled_data_abort_vm,       8, data_abort_vector_vm)