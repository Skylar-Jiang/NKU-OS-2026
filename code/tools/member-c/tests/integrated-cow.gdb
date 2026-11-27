set pagination off
set confirm off
set print pretty off
target remote 127.0.0.1:12447
break *cow_handle_store_fault if $a1 == 0x801000
continue
set $tf=current->tf
set $epc=$tf->epc
set $parent=current->parent
set $child=current
set $ppid=$parent->pid
set $cpid=$child->pid
set $pl1=(unsigned long long *)((($parent->mm->pgdir[0]>>10)<<12)+va_pa_offset)
set $pl0=(unsigned long long *)((($pl1[4]>>10)<<12)+va_pa_offset)
set $cl1=(unsigned long long *)((($child->mm->pgdir[0]>>10)<<12)+va_pa_offset)
set $cl0=(unsigned long long *)((($cl1[4]>>10)<<12)+va_pa_offset)
set $ppa=($pl0[1]>>10)<<12
set $cpa=($cl0[1]>>10)<<12
shell printf "\033[2J\033[H"
printf "=== LAB5 INTEGRATED USER COW: 1 / BEFORE WRITE ===\n\n"
printf "Real kernel + user/cowproof.c; no allocator or Trap stubs.\n"
printf "Parent PID=%d; Child PID=%d\n",$parent->pid,$child->pid
printf "Fault VA=0x%lx; scause=%lu\n",$tf->tval,$tf->cause
printf "SPP=%d (0 confirms user-mode fault)\n",!!($tf->status&0x100)
printf "Parent PA=0x%lx; Child PA=0x%lx\n",$ppa,$cpa
printf "Shared physical page=%d; refs=%d\n",$ppa==$cpa,pages[($ppa>>12)-nbase].ref
printf "Parent: W=%d COW=%d\n",!!($pl0[1]&4),!!($pl0[1]&0x100)
printf "Child : W=%d COW=%d\n",!!($cl0[1]&4),!!($cl0[1]&0x100)
printf "Parent byte=0x%02x; Child byte=0x%02x\n",*(unsigned char*)($ppa+va_pa_offset),*(unsigned char*)($cpa+va_pa_offset)
printf "\nGDB stopped before COW fault repair.\n"
shell if [ "${COW_NATIVE_PAUSE:-0}" = 1 ]; then touch tools/member-c/build/native-cow/stage1.ready; while [ ! -f tools/member-c/build/native-cow/stage1.next ]; do sleep 0.2; done; fi
tbreak __trapret
continue
set $npa=($cl0[1]>>10)<<12
shell printf "\033[2J\033[H"
printf "=== LAB5 INTEGRATED USER COW: 2 / AFTER REPAIR ===\n\n"
printf "Parent PID=%d; Child PID=%d\n",$parent->pid,$child->pid
printf "Parent PA=0x%lx; Child PA=0x%lx\n",$ppa,$npa
printf "Different physical pages=%d\n",$ppa!=$npa
printf "Parent: W=%d COW=%d refs=%d\n",!!($pl0[1]&4),!!($pl0[1]&0x100),pages[($ppa>>12)-nbase].ref
printf "Child : W=%d COW=%d refs=%d\n",!!($cl0[1]&4),!!($cl0[1]&0x100),pages[($npa>>12)-nbase].ref
printf "EPC before=0x%lx; EPC after=0x%lx\n",$epc,$tf->epc
printf "EPC unchanged=%d (retry original store)\n",$epc==$tf->epc
x/i $epc
printf "Parent byte=0x%02x; Child byte=0x%02x\n",*(unsigned char*)($ppa+va_pa_offset),*(unsigned char*)($npa+va_pa_offset)
printf "\nGDB stopped before sret; original store has not retried yet.\n"
shell if [ "${COW_NATIVE_PAUSE:-0}" = 1 ]; then touch tools/member-c/build/native-cow/stage2.ready; while [ ! -f tools/member-c/build/native-cow/stage2.next ]; do sleep 0.2; done; fi
set $len=2
if (*(unsigned short*)$epc & 3)==3
 set $len=4
end
tbreak *($epc+$len)
continue
shell printf "\033[2J\033[H"
printf "=== LAB5 INTEGRATED USER COW: 3 / DATA ISOLATION ===\n\n"
printf "User store has actually executed after sret.\n"
printf "Parent PID=%d; Child PID=%d\n",$ppid,$cpid
printf "Parent PA=0x%lx; Child PA=0x%lx\n",$ppa,$npa
printf "Parent physical byte (original 0x44):\n"
eval "monitor xp /1bx 0x%lx",$ppa
printf "Child physical byte (written 0x55):\n"
eval "monitor xp /1bx 0x%lx",$npa
printf "Current child user VA value=0x%02x\n",*(unsigned char*)0x801000
printf "Current PC=0x%lx (original store has completed)\n",$pc
printf "\nCompare physical reads: parent 0x44; child 0x55.\n"
printf "Actual user-mode fork / COW / Trap / sret path.\n"
shell if [ "${COW_NATIVE_PAUSE:-0}" = 1 ]; then touch tools/member-c/build/native-cow/stage3.ready; while [ ! -f tools/member-c/build/native-cow/stage3.next ]; do sleep 0.2; done; fi
detach
quit
