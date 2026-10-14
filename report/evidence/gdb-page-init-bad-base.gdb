set pagination off
set confirm off
set tcp connect-timeout 5
file /home/jianglinjing/oslab-lab2/code/bin/page-init/0/kernel
target remote 127.0.0.1:42223
hbreak pmm_init
continue
delete breakpoints
set variable memory_base = 0
hbreak __panic
continue
printf "PANIC_FORMAT=%s\n", fmt
detach
