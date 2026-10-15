set pagination off
set confirm off
file bin/kernel
target remote 127.0.0.1:33893
define freelist
  set $le = free_area.free_list.next
  set $n = 0
  set $t = 0
  while $le != &free_area.free_list
    set $p = (struct Page *)((char *)$le - (unsigned long)&((struct Page *)0)->page_link)
    printf "  block head=%p idx=%ld property=%u flags=0x%lx\n", $p, $p - pages, $p->property, $p->flags
    set $n = $n + 1
    set $t = $t + $p->property
    set $le = $le->next
  end
  printf "  BLOCKS=%d SUM_PROPERTY=%d NR_FREE=%u\n", $n, $t, free_area.nr_free
end
hbreak slub_init
continue
printf "[after pmm_init / before SLUB]\n"
freelist
set $before = free_area.nr_free
delete breakpoints
hbreak init.c:46
continue
printf "[after slub_test, kernel idle loop]\n"
freelist
if $n != 1 || $t != free_area.nr_free || free_area.nr_free != $before
  printf "[GDB standard] free list restore: FAIL\n"
  quit 1
end
printf "[GDB standard] Best-Fit free list single block and nr_free restored after SLUB: PASS\n"
detach
