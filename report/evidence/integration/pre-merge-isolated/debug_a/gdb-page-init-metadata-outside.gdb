set pagination off
set confirm off
set tcp connect-timeout 5
file ~/oslab-lab2-integ/code/bin/page-init/0/kernel
target remote 127.0.0.1:39927
hbreak pmm_init
continue
delete breakpoints
set variable memory_size = (((unsigned long long)&end - 0xffffffff40000000 + 4095) & ~4095) - 0x80000000 + 4096
hbreak __panic
continue
printf "PANIC_FORMAT=%s\n", fmt
detach
