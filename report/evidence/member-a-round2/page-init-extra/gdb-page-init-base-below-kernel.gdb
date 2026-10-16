set pagination off
set confirm off
set tcp connect-timeout 5
file ~/oslab-lab2/code/bin/page-init/0/kernel
target remote 127.0.0.1:37339
hbreak pmm_init
continue
delete breakpoints
set variable memory_base = 0x80100000
set variable memory_size = 0x7f00000
hbreak page_init_test
continue
printf "BASE=0x%llx NPAGE=%llu NBASE=%llu PAGES=%p FREE=%u\n", memory_base, npage, nbase, pages, free_area.nr_free
set $i = 0
while $i < 256
  if (pages[$i].flags & 1) == 0
    printf "LOW PAGE %d NOT RESERVED\n", $i
    quit 1
  end
  set $i = $i + 1
end
printf "[GDB page_init] base 0x80100000: pages[0..255] (PA 0x80000000-0x800fffff) Reserved: PASS\n"
delete breakpoints
hbreak slub_init
continue
detach
