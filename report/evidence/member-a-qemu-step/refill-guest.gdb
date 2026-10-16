set pagination off
set confirm off
set arch riscv:rv64
file bin/kernel
target remote 127.0.0.1:__PORT__
hbreak slub_init
continue
printf "[GUEST] reached slub_init after pmm_init\n"
delete
detach
