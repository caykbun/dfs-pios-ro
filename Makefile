BOOTLOADER = my-install

# PROGS += $(wildcard tests/vm/*.c)
# PROGS += $(wildcard tests/threads/[4-7]-*.c)
# PROGS += $(wildcard tests/uart-int/[13]-*.c)
# PROGS += $(wildcard tests/timer/*.c)
# PROGS += $(wildcard tests/network/*.c)
# PROGS += $(wildcard tests/fat32/[0-2]-*.c)
# PROGS += $(wildcard tests/preemptive-threads/[7]-*.c)
# PROGS += $(wildcard tests/shell/[1]-*.c)
# PROGS += $(wildcard tests/uart-int/[1]-*.c)
# PROGS += $(wildcard tests/dfs/dfs-master-thread*.c)
# PROGS += $(wildcard tests/dfs/dfs-heartbeat*.c)

# client
PROGS := $(wildcard tests/dfs/dfs-multithread-sc*.c)

# 2chunks
PROGS := $(wildcard tests/dfs/dfs-2chunk*.c)

# master&chunk
PROGS := $(wildcard tests/dfs/dfs-master-chunk*.c)



COMMON_SRC += $(wildcard tests/src/*.[cS])
COMMON_SRC += $(wildcard vm/*.[cS])
COMMON_SRC += $(wildcard threads/*.[cS])
COMMON_SRC += $(wildcard network/*.[cS])
COMMON_SRC += $(wildcard fat32/*.[cS])
COMMON_SRC += $(wildcard preemptive-threads/*.[cS])
COMMON_SRC += $(wildcard dfs/*.[cS])


COMMON_SRC += external-code/unicode-utf8.c external-code/emmc.c external-code/mbox.c 


LIBS += $(CS140E_PROJECT_PATH)/libgcc.a

CFLAGS_EXTRA  = -Iexternal-code

# <staff-mmu-asm.o> this will be removed next lab
# STAFF_OBJS += staff-mmu-asm.o  
STAFF_OBJS += $(CS140E_PROJECT_PATH)/libpi/staff-objs/interrupts-asm.o
STAFF_OBJS += $(CS140E_PROJECT_PATH)/libpi/staff-objs/interrupts-vec-asm.o
STAFF_OBJS += $(CS140E_PROJECT_PATH)/libpi/staff-objs/kmalloc.o
STAFF_OBJS += $(CS140E_PROJECT_PATH)/libpi/staff-objs/new-spi.o

# set RUN = 1 if you want the code to automatically run after building.
RUN = 1

# uart-input-test:
# 	make -C tests/uart-int input-test

GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|TRACE:'
include $(CS140E_PROJECT_PATH)/libpi/mk/Makefile.template

