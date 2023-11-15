#include "rpi.h"
#include "rpi-constants.h"
#include "rpi-interrupts.h"
#include "libc/helper-macros.h"
#include "vm/mmu.h"

/***********************************************************************
 * the following code is given
 */

cp_asm(dac, p15, 0, c3, c0, 0)

void cp15_domain_ctrl_wr(uint32_t r);

enum { OneMB = 1024 * 1024 };

// maximum number of 1mb section entries for a 32-bit address space
enum { MAX_SEC_PTE = 4096};

void mmu_reset(void);
void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
void mmu_sync_pte_mods(void);

// given.
int mmu_is_enabled(void) {
    return cp15_ctrl_reg1_rd().MMU_enabled != 0;
}

// given memory allocated at <addr> do page table initialization.
fld_t *mmu_pt_init(void *addr, unsigned nbytes) {
    fld_t *pt = addr;
    unsigned n = 4096 * sizeof *pt;
    demand(mod_pow2_ptr(addr, 14), must be 14-bit aligned!);
    demand(nbytes == n, only handing full allocation);
    memset(pt, 0, n);
    return pt;
}

// allocate page table with <n> entries.
fld_t *mmu_pt_alloc(unsigned n) {
    demand(n == 4096, we only handling a fully-populated page table right now);
    unsigned nbytes = 4096 * sizeof(fld_t);

    // first-level page table is 4096 entries.
    fld_t *pt = kmalloc_aligned(nbytes, 1<<14);

    demand(is_aligned_ptr(pt, 1<<14), must be 14-bit aligned!);
    return mmu_pt_init(pt, nbytes);
}


// disable the mmu by setting control register 1
// to <c:32>.
// 
// we use a C veneer over the assembly (mmu_disable_set_asm)
// so we can easily do assertions: the real work is 
// done by the asm code (you'll write this next time).
void mmu_disable_set(cp15_ctrl_reg1_t c) {
    assert(!c.MMU_enabled);
    assert(!c.C_unified_enable);
    // You'll implement this next lab.
    mmu_disable_set_asm(c);
}

// disable the MMU by flipping the enable bit.   we 
// use a C vener so we can do assertions and then call
// out to assembly to do the real work (you'll write this
// next time).
void mmu_disable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(c.MMU_enabled);
    c.MMU_enabled=0;
    mmu_disable_set(c);
}

// enable the mmu by setting control reg 1 to
// <c>.   we start in C so we can do assertions
// and then call out to the assembly for the 
// real work (you'll write this code next time).
void mmu_enable_set(cp15_ctrl_reg1_t c) {
    assert(c.MMU_enabled);
    // you'll implement this next lab.
    mmu_enable_set_asm(c);
}

// enable mmu by flipping enable bit.
void mmu_enable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(!c.MMU_enabled);
    c.MMU_enabled = 1;
    mmu_enable_set(c);
}

// C end of this: does sanity checking then calls asm.
void set_procid_ttbr0(unsigned pid, unsigned asid, fld_t *pt) {
    assert((pid >> 24) == 0);
    assert(pid > 64);
    assert(asid < 64 && asid);
    // you'll implement this next lab.
    cp15_set_procid_ttbr0(pid << 8 | asid, pt);
}

/**************************************************************************
 * for lab 12: implement the following code.
 */

// you'll need implement this: 
//   iterate over the page table <pt> setting up the mappings from
//   va->pa for <nsec> sections.  use domain id <dom>.
void mmu_map_sections(fld_t *pt, unsigned va, unsigned pa, unsigned nsec, uint32_t dom) {
    assert(dom < 16);
    assert(mod_pow2(va, 20));
    assert(mod_pow2(pa, 20));

    // implement
    for (unsigned i = 0; i < nsec; ++i) {
        mmu_map_section(pt, va, pa, dom);
        va += 1 << 20;
        pa += 1 << 20;
    }

    
}

// lookup va in pt and return the pte entry.
fld_t * mmu_lookup_section(fld_t *pt, unsigned va) {
    assert(mod_pow2(va, 20));
    fld_t *pte = 0;

    // implement
    // pte = staff_mmu_lookup_section(pt,va);
    pte = pt + (va >> 20);
    // pte = (fld_t *) addr;

    // for today: tag should be set.  in the future you'd return 0.
    demand(pte->tag, invalid section);
    return pte;
}

// set the ap bits for va,nsec to <perm>
void mmu_mprotect(fld_t *pt, unsigned va, unsigned nsec, unsigned perm) {
    demand(perm <= 0b11, invalid permission);

    // you need to implement this.
    // trace("%x\n", mmu_lookup_section(pt, va & ~((1 << 20) - 1))->AP);
    // staff_mmu_mprotect(pt,va,nsec, perm);
    // trace("%x\n", mmu_lookup_section(pt, va & ~((1 << 20) - 1))->AP);
    fld_t *pte = mmu_lookup_section(pt, va & ~((1 << 20) - 1));
    for (unsigned i = 0; i < nsec; ++i) {
        pte->AP = perm;
        pte++;
    }

    // must call this routine on each PTE modification (you'll implement
    // next lab).
    mmu_sync_pte_mods();
}

// set so that we use armv6 memory.
// this should be wrapped up neater.  broken down so can replace 
// one by one.
//  1. the fields are defined in vm.h.
//  2. specify armv6 (no subpages).
//  3. check that the coprocessor write succeeded.
void mmu_init(void) { 
    // reset the MMU state: you will implement next lab
    mmu_reset();

    // trivial: RMW the xp bit in control reg 1.
    // leave mmu disabled.
    // todo("read control reg 1, turn on XP bit (non-back compat)");
    struct control_reg1 temp = cp15_ctrl_reg1_rd();
    temp.XP_pt = 1;

    cp15_ctrl_reg1_wr(temp);

    // make sure write succeeded.
    struct control_reg1 c1 = cp15_ctrl_reg1_rd();
    assert(c1.XP_pt);
    assert(!c1.MMU_enabled);
}

// set f->sec_base_addr correctly: called by <mmu_map_section>
static void fld_set_base_addr(fld_t *f, unsigned addr) {
    // 20 b/c we have 1MB sections.
    demand(mod_pow2(addr,20), addr is not aligned!);

    // make sure if you read it back, it's what you set it to.
    f->sec_base_addr = addr >> 20;

    // if the previous code worked, this should always succeed.
    assert((f->sec_base_addr << 20) == addr);
}

// create a mapping for <va> to <pa> in the page table <pt>
// for now: 
//  - assume domain = 0.
//  - global mapping
//  - executable
//  - don't use APX
//  - TEX,A,B: for today mark everything as non-cacheable 6-15/6-16 in armv1176
fld_t * mmu_map_section(fld_t *pt, uint32_t va, uint32_t pa, uint32_t dom) {
    // only 16 domains.
    assert(dom < 16);
    // 20 b/c we have 1MB sections.
    assert(mod_pow2(va, 20));
    assert(mod_pow2(pa, 20));

    // assign pte: call <fld_set_base_addr> to set <sec_base_addr>
    // set each pte entry to: 
    //   1. global
    //   2. AP = full access in user and kernel (B4-9)
    //   3. APX = 0
    //   4. domain to <dom>
    //   5. TEX strongly ordered (B4-12)
    //   6. executable.
    // fld_t *pte = staff_mmu_map_section(pt, va, pa, dom);
    // trace("WARNING %x\n", pte);
    fld_t *pte = pt + (va >> 20);

    fld_set_base_addr(pte, pa);
    pte->nG = 0;
    pte->APX = 0;
    pte->AP = 0b11;
    pte->domain = dom;
    pte->TEX = 0b000;
    pte->XN = 0;
    pte->tag = 0b10;
    pte->B = 0;
    pte->C = 0;
    pte->IMP = 0;
    pte->super = 0;
    pte->_sbz1 = 0;
    pte->S = 0;
    // trace("WARNING %x\n", pte);

    fld_print(pte);
    printk("my.pte@ 0x%x = %b\n", pt, *(unsigned*)pte);
    hash_print("PTE crc:", pte, sizeof *pte);
    return pte;
}

// read and return the domain access control register
uint32_t domain_access_ctrl_get(void) {
    // return 0;
    return dac_get();
    // return staff_domain_access_ctrl_get();
}

// b4-42
// set domain access control register to <r>
// void domain_access_ctrl_set(uint32_t r) {
//     staff_domain_access_ctrl_set(r);
//     assert(domain_access_ctrl_get() == r);
// }

void domain_access_ctrl_set(uint32_t r) {
    // staff_cp15_domain_ctrl_wr(r);
    // dac_set(r);
    cp15_domain_ctrl_wr(r);
    // prefetch_flush();
}