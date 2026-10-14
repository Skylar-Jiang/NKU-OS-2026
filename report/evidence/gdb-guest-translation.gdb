set pagination off
set confirm off
set tcp connect-timeout 5
file /home/jianglinjing/oslab-lab2/code/bin/page-init/0/kernel
target remote 127.0.0.1:53459
hbreak page_init_test
continue
printf "GUEST_PC=0x%llx SATP=0x%llx OFFSET=0x%llx\n", $pc, $satp, va_pa_offset
set $guest_pc = (unsigned long long)$pc
set $guest_satp = (unsigned long long)$satp
set $root_pa = ($satp & 0xfffffffffff) << 12
set $pte = *(unsigned long long *)((char *)&boot_page_table_sv39 + 511 * 8)
set $translated = (($pte >> 10) << 12) + ($guest_pc & 0x3fffffff)
printf "ROOT_PA=0x%llx VPN2=%llu LEAF_PTE=0x%llx\n", $root_pa, (($guest_pc >> 30) & 511), $pte
printf "TRANSLATED_PA=0x%llx OFFSET_PA=0x%llx\n", $translated, $guest_pc - va_pa_offset
if $translated != $guest_pc - va_pa_offset
  quit 1
end
if ($guest_satp >> 60) != 8 || ($pte & 0xf) != 0xf
  quit 1
end
x/4i $pc
x/8bx $pc
eval "monitor xp /8bx 0x%llx", $translated
printf "PAGE_COUNT=%llu NBASE=%llu FREE=%u\n", npage, nbase, free_area.nr_free
printf "[GDB guest] Sv39 level-2 leaf translation: PASS\n"
detach
