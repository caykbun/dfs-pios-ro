#include "vm/vm-ident.h"
#include "libc/bit-support.h"

cp_asm(wfar, p14, 0, c0, c6, 0)
cp_asm(ifar, p15, 0, c6, c0, 2)
cp_asm(ifsr, p15, 0, c5, c0, 1)
cp_asm(dfsr, p15, 0, c5, c0, 0)
cp_asm(far, p15, 0, c6, c0, 0)

volatile struct proc_state proc;

// this is called by reboot: we turn off mmu so that things work.
void reboot_callout(void) {
    if(mmu_is_enabled())
        mmu_disable();
}

void prefetch_abort_vector_vm(unsigned lr) {
    // unimplemented();
    clean_reboot();
}

void data_abort_vector_vm(unsigned lr) {
    // clean_reboot();
    unsigned dfsr = dfsr_get();
    unsigned far = far_get();
    printk("pc: %x\n", lr);
    printk("virtual address accessed: %x\n", far);
    uint32_t base = far - (far % OneMB);
    
    if (bits_get(dfsr, 0, 3) == 0b0101) {
        printk("Fault caused by Translation Section Fault\nExtending stack...\n");
        mmu_map_section(proc.pt, base, base, proc.dom_id);
    } else if (bits_get(dfsr, 0, 3) == 0b1101) {
        printk("Fault caused by Permission Section Fault\nExtending stack...\n");
        mmu_mark_sec_rw(proc.pt, base, 1);
        mmu_sync_pte_mods();
    }
    proc.fault_count++;
}
