set pagination off
set confirm off
set tcp connect-timeout 5
file /home/jianglinjing/oslab-lab2/code/bin/page-init/0/kernel
target remote 127.0.0.1:37933
hbreak pmm_init
continue
delete breakpoints
set $pages_pa = (((unsigned long long)&end - 0xffffffff40000000 + 4095) & ~4095)
set $delta = $pages_pa - 0x80000000
set $count = ($delta + (4096 - sizeof(struct Page)) - 1) / (4096 - sizeof(struct Page))
set variable memory_size = $count * 4096
hbreak default_check
continue
printf "NO_FREE_NPAGE=%llu NO_FREE_COUNT=%u\n", npage, free_area.nr_free
if free_area.nr_free != 0
  quit 1
end
if free_area.free_list.next != &free_area.free_list || free_area.free_list.prev != &free_area.free_list
  quit 1
end
set $i = 0
while $i < npage - nbase
  if (pages[$i].flags & 1) == 0
    quit 1
  end
  set $i = $i + 1
end
printf "[GDB page_init] no full free page: PASS (stopped before allocator self-test)\n"
disconnect
