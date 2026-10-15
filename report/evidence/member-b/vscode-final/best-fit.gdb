set pagination off
set confirm off
set print pretty off
file bin/kernel
target remote 127.0.0.1:1249
break best_fit_alloc_pages
continue
info args
bt 6
list best_fit_alloc_pages
disable 1
detach
quit
