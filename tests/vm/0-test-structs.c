#include "rpi.h"
#include "vm/mmu.h"

void notmain() {
    output("about to check structure offsets\n");
    check_vm_structs();
    output("SUCCESS: offsets passed\n");
}
