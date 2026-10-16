# Why is the kern_init code page (VA 0xffffffffc0200000) filled into the TLB again later?
# Stop at every later fill of that page and show what occupied its slot (index 0) before.
set pagination off
set confirm off
set width 0
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
set $n = 0
break cputlb.c:777 if vaddr_page == 0xffffffffc0200000UL && mmu_idx == 1
commands
  silent
  set $n = $n + 1
  printf "[REFILL #%d] page 0xffffffffc0200000 -> index %lu; slot before: addr_read=0x%lx addr_write=0x%lx addr_code=0x%lx; tlb mask=0x%lx (entries=%lu)\n", $n, index, te->addr_read, te->addr_write, te->addr_code, env_tlb(env)->f[mmu_idx].mask, (env_tlb(env)->f[mmu_idx].mask >> 5) + 1
  if $n <= 4
    bt 3
  end
  continue
end
break tlb_flush_by_mmuidx_async_work
commands
  silent
  printf "[FLUSH] tlb_flush_by_mmuidx_async_work\n"
  continue
end
run
