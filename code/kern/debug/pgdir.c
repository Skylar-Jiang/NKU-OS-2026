#include <pmm.h>
#include <riscv.h>
#include <stdio.h>
/* LAB6 integration: migrate our own Lab5 diagnostic without altering A PMM. */
/* LAB5 2411788: print the active Sv39 address space, not merely boot_pgdir.
 * Stop at a leaf at ANY level: a 1 GiB/2 MiB leaf is not a table pointer.
 * This diagnostic allocates no pages and never changes PTEs or references. */
static void print_sv39_table(const pte_t *table, int level, uintptr_t prefix)
{
    for (size_t i = 0; i < NPTEENTRY; i++) {
        pte_t entry = table[i];
        if (!(entry & PTE_V)) continue;
        uintptr_t va = prefix | ((uintptr_t)i << (PGSHIFT + 9 * level));
        uintptr_t pa = PTE_ADDR(entry);
        if (entry & (PTE_R | PTE_W | PTE_X)) {
            uintptr_t size = (uintptr_t)1 << (PGSHIFT + 9 * level);
            /* Sv39 bits 63:39 must repeat bit 38. Use start + size rather
             * than an exclusive end, which wraps at the top of VA space. */
            uintptr_t canonical = va;
            if (va & ((uintptr_t)1 << 38)) canonical |= ~(((uintptr_t)1 << 39) - 1);
            cprintf("L%d VA=%016lx PA=%016lx size=%lx %c%c%c%c%c%c%c RSW=%x%s\n",
                    level, (unsigned long)canonical, (unsigned long)pa,
                    (unsigned long)size,
                    entry & PTE_R ? 'r' : '-', entry & PTE_W ? 'w' : '-',
                    entry & PTE_X ? 'x' : '-', entry & PTE_U ? 'u' : '-',
                    entry & PTE_G ? 'g' : '-', entry & PTE_A ? 'a' : '-',
                    entry & PTE_D ? 'd' : '-', (unsigned int)((entry >> 8) & 3),
                    ((entry & PTE_W) && !(entry & PTE_R)) || (pa & (size - 1))
                        ? " INVALID leaf" : "");
        } else if (level == 0 || (entry & (PTE_U | PTE_A | PTE_D)) ||
                   PPN(pa) < nbase || PPN(pa) >= npage) {
            cprintf("L%d INVALID table PTE=%016lx\n", level, (unsigned long)entry);
        } else {
            print_sv39_table((const pte_t *)KADDR(pa), level - 1, va);
        }
    }
}

void print_pgdir(void)
{
    uintptr_t satp = read_csr(satp);
    unsigned int mode = satp >> 60;
    uintptr_t root_pa = (satp & SATP64_PPN) << PGSHIFT;
    cprintf("page table: satp=%016lx root=%016lx mode=%u\n",
            (unsigned long)satp, (unsigned long)root_pa, mode);
    if (mode != SATP_MODE_SV39) {
        cprintf("print_pgdir: requires Sv39 (Bare/other modes not walked)\n");
        return;
    }
    if (PPN(root_pa) < nbase || PPN(root_pa) >= npage) {
        cprintf("print_pgdir: root outside managed RAM\n");
        return;
    }
    print_sv39_table((const pte_t *)KADDR(root_pa), 2, 0);
}

