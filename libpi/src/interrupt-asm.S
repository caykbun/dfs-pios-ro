#include "rpi-asm.h"

.align 5
.globl default_interrupt_vector
default_interrupt_vector:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =unhandled_swi
  ldr pc, =unhandled_prefetch_abort
  ldr pc, =unhandled_data_abort
  ldr pc, =unhandled_interrupt
  ldr pc, =unhandled_fiq