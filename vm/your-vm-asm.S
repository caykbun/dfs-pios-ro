@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ armv6-coprocessor-asm.h
@   - has many macros for the privileged instructions.
@   - many page numbers 
@
@ make sure you look at B2 as well as 

@ just read the cache type: use C code to figure out if it is unified
@ b6-13, b6-14
MK_FN(get_cache_type)
    CACHE_TYPE_RD(r0)
    bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 1:  write to the domain register.
@    easy sequencing rule: look at 2.7.6
@
@ depending on your code, you might name this
@ something else (or inline into the c code):
@  void domain_access_ctrl_set(uint32_t dom_reg);
MK_FN(cp15_domain_ctrl_wr)
    @ trivial: must flush after change to cp15 reg
    DOMAIN_CTRL_WR(r0)
    mov r1, #0
    PREFETCH_FLUSH(r1)
    bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 2: 
@ void mmu_reset(void)
@   - called on boot-up.
@   - clear the dcache, icache, tlbs: make sure you obey 
@     any rules from B2
MK_FN(mmu_reset)
    @ no order constraint between invalidation of caches and tlbs
    @ avoid garbage in cache and TLB
    mov r0, #0
    @B2-22
    @ invalidate TLB since TLB will be used later 
    INV_TLB(r0)
    @ invalidate cache since MMU will be on for the first time
    INV_ALL_CACHES(r0)
    @ DSB ensures invalidation completes
    DSB(r0)
    @ make sure visible
    PREFETCH_FLUSH(r0)
    bx lr



@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 3
@ void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
@
@ given the control register value <c>, write it 
@ and do any needed sync
@
@ this is a pretty tricky routine.  make sure you handle the 
@ icache and dcache.   when the mmu is off, these mappings will
@ change.  also, dcache does not work with MMU off.
@
@ make sure you look at arm1176, 6-9.
@
MK_FN(mmu_disable_set_asm)

    @ note: this piece of code is mapped into the sample place in both
    @ MMU-enabled and MMU-disabled so BTB and Prefetch entries work identically
    @ to both MMU-enable and disable.   Without this requirement I believe is
    @ very hard to get right (perhaps impossible?) since we cannot flush
    @ the BTB and prefetch and switch MMU on/off atomically.  thus, would
    @ flush, but instructions could get fetched before we could disable,
    @ defeating the purpose.
    mov r1, #0

    INV_TLB(r1)
    DSB(r1)
    PREFETCH_FLUSH(r1)

    @ clean cache since MMU will be disabled
    CLEAN_INV_DCACHE(r1)
    DSB(r1)
    PREFETCH_FLUSH(r1)

    @ Invalidate ICache
    INV_ICACHE(r1)
    DSB(r1)
    PREFETCH_FLUSH(r1)

    @ write to ctrl reg 
    CONTROL_REG1_WR(r0)
    PREFETCH_FLUSH(r1)

    @ B2-24
    @ flush since MMU is off now
    FLUSH_BTB(r1)
    PREFETCH_FLUSH(r1)

    bx lr

@ void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
@  called by <mmu.c:mmu_enable()>
@
@ make sure you look at arm1176, 6-9.
@
MK_FN(mmu_enable_set_asm)
    mov r1, #0
    @ invalidate ICache  / (disable ICache also)
    INV_ICACHE(r1)
    DSB(r1)
    PREFETCH_FLUSH(r1)
    
    CONTROL_REG1_RD(r2)
    bic r2, r2, #0x1000
    CONTROL_REG1_WR(r2)
    PREFETCH_FLUSH(r1)

    @ turn MMU on
    CONTROL_REG1_WR(r0)
    PREFETCH_FLUSH(r1)
    
    FLUSH_BTB(r1)
    PREFETCH_FLUSH(r1)

    bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 4
@
@ void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
@     sequence from b2-25 + rules from 3-129
@
@ this is the fanciest one.
MK_FN(cp15_set_procid_ttbr0)
    mov r2, #0

    @ reset ASID to 0, need DSB since PTE changed
    DSB(r2)
    ASID_SET(r2)
    PREFETCH_FLUSH(r2)

    @ set TTBRs
    TTBR0_SET(r1)
    PREFETCH_FLUSH(r2)
    TTBR1_SET(r2)
    PREFETCH_FLUSH(r2)
    TTBR_BASE_CTRL_WR(r2)
    PREFETCH_FLUSH(r2)

    @ B4 - set ASID back
    DSB(r2)
    ASID_SET(r0)
    PREFETCH_FLUSH(r2)

    @ flush BTB since TTBR changed
    FLUSH_BTB(r2)
    PREFETCH_FLUSH(r2)

    bx lr

MK_FN(mmu_sync_pte_mods)
    mov r0, #0
    INV_TLB(r0)
    DSB(r0)
    PREFETCH_FLUSH(r0)

    @ clean data cache
    CLEAN_INV_DCACHE(r0)
    DSB(r0)
    PREFETCH_FLUSH(r0)

    @ change BTB since PTE changed
    FLUSH_BTB(r0)
    PREFETCH_FLUSH(r0)
    bx lr
    


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ you need this for pinned-vm.c
@
@ this just turns the MMU on for the first time.
@ r0 = asid
@ r1 = pointer to empty page table
@
@ is a combination/simplification of mmu_init() and 
@ mmu_enable_set_asm().
@
@ also could just call the above from C code! :)
MK_FN(mmu_on_first_time)
    DSB(r2)
    mov r2, #0
    ASID_SET(r2)
    PREFETCH_FLUSH(r2)
    TTBR0_SET(r1)
    PREFETCH_FLUSH(r2)
    mov r2, #0
    TTBR1_SET(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)
    ASID_SET(r0)

    INV_DCACHE(r2)
    INV_ICACHE(r2)
    INV_DTLB(r2)
    INV_ITLB(r2)
    FLUSH_BTB(r2)
    DSB(r2)

    CONTROL_REG1_RD(r0)
    ORR r0, r0, #1
    CONTROL_REG1_WR(r0)
    PREFETCH_FLUSH(r1)
    bx lr




.global cp15_ctrl_reg1_rd
cp15_ctrl_reg1_rd:
    CONTROL_REG1_RD(r0)
    bx lr

.global cp15_ctrl_reg1_rd_u32
cp15_ctrl_reg1_rd_u32:
    CONTROL_REG1_RD(r0)
    bx lr

.global cp15_ctrl_reg1_wr
cp15_ctrl_reg1_wr:
    CONTROL_REG1_WR(r0)
    mov r1, #0
    PREFETCH_FLUSH(r1)
    bx lr


MK_FN(test)
    BIC r0, r0, #0x1000
    bx lr