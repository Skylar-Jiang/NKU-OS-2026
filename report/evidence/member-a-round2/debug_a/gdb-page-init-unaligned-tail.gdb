set pagination off
set confirm off
set tcp connect-timeout 5
file ~/oslab-lab2/code/bin/page-init/0/kernel
target remote 127.0.0.1:39327
hbreak pmm_init
continue
delete breakpoints
set variable memory_size = 0x4000000 - 17
hbreak page_init_test
continue
printf "UNALIGNED_SIZE=0x%llx NPAGE=%llu\n", memory_size, npage
if npage != (0x80000000 + 0x4000000 - 17) / 4096
  quit 1
end
printf "[GDB page_init] incomplete tail page excluded: PASS\n"
detach
