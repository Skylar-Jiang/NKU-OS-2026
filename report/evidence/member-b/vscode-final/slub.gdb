set pagination off
set confirm off
set print pretty off
file bin/kernel
target remote 127.0.0.1:1249
break slub_alloc if size == 1025
continue
info args
finish
print/x $a0
disable 1
break slub_alloc if size == 8
continue
info args
bt 4
finish
print/x $a0
disable 2
break kern/mm/slub_test.c:135
continue
info locals
detach
quit
