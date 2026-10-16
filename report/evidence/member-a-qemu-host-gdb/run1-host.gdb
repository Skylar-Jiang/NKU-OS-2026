# Host GDB: debugs the QEMU 4.1.1 process itself (x86-64 code of the emulator).
set pagination off
set confirm off
set width 0
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
set $fills = 0
set $walks = 0
set $hits = 0
set $flushes = 0

# 1. TLB fill entry (called only after the software TLB lookup missed).
break riscv_cpu_tlb_fill if address == 0x80000000 || (address & ~0xfffUL) == 0x80200000 || address == 0x80206000 || (address >= 0xffffffffc0000000UL && $fills < 6)
commands
  silent
  set $fills = $fills + 1
  set $env = &((RISCVCPU *)cs)->env
  printf "\n[HOST tlb_fill #%d] VA=0x%lx access_type=%d(0=load,1=store,2=fetch) mmu_idx=%d priv=%ld satp=0x%lx\n", $fills, address, access_type, mmu_idx, $env->priv, $env->satp
  bt 3
  continue
end

# 2. M-mode shortcut: OpenSBI runs in M mode, get_physical_address returns physical=addr.
break get_physical_address if addr == 0x80000000 && mmu_idx == 3
commands
  silent
  printf "[HOST get_physical_address] M-mode VA=0x%lx mmu_idx=%d -> 'mode == PRV_M' branch, physical = addr (no page table)\n", addr, mmu_idx
  continue
end

# 3. Bare mode (S mode, satp.MODE=0): VM_1_10_MBARE case.
break cpu_helper.c:197 if (addr & ~0xfffUL) == 0x80200000 || addr == 0x80206000
commands
  silent
  printf "[HOST get_physical_address] satp.MODE=BARE (satp=0x%lx) VA=0x%lx access_type=%d -> case VM_1_10_MBARE, physical = addr\n", env->satp, addr, access_type
  continue
end

# 4. Sv39 walk: PTE has just been read by ldq_phys().
break cpu_helper.c:254 if addr >= 0xffffffffc0000000UL && $walks < 8
commands
  silent
  set $walks = $walks + 1
  printf "[HOST Sv39 walk #%d] VA=0x%lx access_type=%d satp=0x%lx root=0x%lx level_i=%d ptshift=%d VPN_idx=0x%lx pte_addr=0x%lx PTE=0x%lx\n", $walks, addr, access_type, env->satp, (env->satp & 0xfffffffffffUL) << 12, i, ptshift, idx, pte_addr, pte
  bt 4
  continue
end

# 5. Leaf found: superpage fake-leaf physical address.
break cpu_helper.c:334 if addr >= 0xffffffffc0000000UL && $walks <= 8
commands
  silent
  set $pa_page = (ppn | ((addr >> 12) & ((1UL << ptshift) - 1))) << 12
  printf "[HOST Sv39 leaf] VA=0x%lx PPN=0x%lx ptshift=%d -> PA_page=0x%lx page_offset=0x%lx PA=0x%lx\n", addr, ppn, ptshift, $pa_page, addr & 0xfff, $pa_page | (addr & 0xfff)
  continue
end

# 6. satp write and sfence.vma -> whether QEMU flushes its software TLB.
break write_satp
commands
  silent
  printf "\n[HOST write_satp] old satp=0x%lx new=0x%lx priv=%ld\n", env->satp, val, env->priv
  continue
end
break helper_tlb_flush
commands
  silent
  printf "[HOST helper_tlb_flush] sfence.vma executed, priv=%ld satp=0x%lx\n", env->priv, env->satp
  continue
end
break tlb_flush if $flushes < 6
commands
  silent
  set $flushes = $flushes + 1
  printf "[HOST tlb_flush #%d]\n", $flushes
  bt 2
  continue
end

# 7. Instruction-fetch TLB lookup for the kern_init page (enabled after the miss).
break get_page_addr_code if (addr & ~0xfffUL) == 0xffffffffc0200000UL && $hits < 3
commands
  silent
  set $hits = $hits + 1
  set $cpu = (RISCVCPU *)((char *)env - (long)&((RISCVCPU *)0)->env)
  set $mmu = env->priv
  set $f = $cpu->neg.tlb.f[$mmu]
  set $idx = (addr >> 12) & ($f.mask >> 5)
  set $e = $f.table[$idx]
  printf "[HOST get_page_addr_code #%d] VA=0x%lx mmu_idx=%ld tlb_index=%ld entries=%ld entry.addr_code=0x%lx entry.addend=0x%lx tlb_hit=%d\n", $hits, addr, $mmu, $idx, ($f.mask >> 5) + 1, $e.addr_code, $e.addend, ($e.addr_code & ~0x7ffUL) == (addr & ~0xfffUL)
  printf "  host pointer = VA + addend = 0x%lx ; bytes there:\n", addr + $e.addend
  x/4bx addr + $e.addend
  continue
end
disable 9

# Enable the hit observer once the kern_init page has been filled.
break cpu_helper.c:472 if (address & ~0xfffUL) == 0xffffffffc0200000UL && access_type == 2
commands
  silent
  printf "[HOST tlb_set_page] VA page 0x%lx -> PA page 0x%lx prot=%d (1=R 2=W 4=X) mmu_idx=%d; enabling get_page_addr_code observer\n", address & ~0xfffUL, pa & ~0xfffUL, prot, mmu_idx
  enable 9
  continue
end

run
