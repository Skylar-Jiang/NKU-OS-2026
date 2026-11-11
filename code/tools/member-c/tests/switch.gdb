set pagination off
set confirm off
target remote 127.0.0.1:12444
break switch_to
continue
printf "C switch entry: S-mode harness, a0/from, a1/to, current pid\n"
p/x $a0
p/x $a1
p current->pid
p/x $satp
p/x $sstatus
p/x ((struct context *)$a1)->ra
p/x ((struct context *)$a1)->sp
set $from = $a0
set $to = $a1
set $oldsp = $sp
set $oldra = $ra
si 14
printf "Saved old SP and RA must match entry\n"
p ((struct context *)$from)->sp == $oldsp
p ((struct context *)$from)->ra == $oldra
si 14
printf "Restored target SP and RA must match target context\n"
p $sp == ((struct context *)$to)->sp
p $ra == ((struct context *)$to)->ra
detach
quit
