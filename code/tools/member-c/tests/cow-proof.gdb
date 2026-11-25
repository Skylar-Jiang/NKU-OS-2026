set pagination off
set confirm off
target remote 127.0.0.1:12446
break cow_proof_before
continue
set $pm=(struct mm_struct *)$a0
set $cm=(struct mm_struct *)$a1
set $pl1=(unsigned long long *)((($pm->pgdir[0] >> 10) << 12)+va_pa_offset)
set $pl0=(unsigned long long *)((($pl1[4] >> 10) << 12)+va_pa_offset)
set $cl1=(unsigned long long *)((($cm->pgdir[0] >> 10) << 12)+va_pa_offset)
set $cl0=(unsigned long long *)((($cl1[4] >> 10) << 12)+va_pa_offset)
set $ppa=($pl0[0] >> 10) << 12
set $cpa=($cl0[0] >> 10) << 12
set logging file evidence/cow-proof-before.txt
set logging overwrite on
set logging redirect off
set logging on
printf "COW PROOF 1: BEFORE CHILD STORE\n"
printf "Scope: isolated C memory-module harness, S-mode store to U/COW page.\n"
printf "Production pmm.c/vmm.c; test allocator, scheduler and Trap entry.\n"
printf "Not the complete integrated user-process regression.\n\n"
printf "Virtual address       = 0x00800000\n"
printf "Parent physical page  = 0x%lx\n", $ppa
printf "Child physical page   = 0x%lx\n", $cpa
printf "Same physical page    = %d\n", $ppa == $cpa
printf "Parent W=%d COW=%d\n", !!($pl0[0]&4), !!($pl0[0]&0x100)
printf "Child  W=%d COW=%d\n", !!($cl0[0]&4), !!($cl0[0]&0x100)
printf "Physical page refs    = %d\n", pages[($ppa>>12)-nbase].ref
printf "Parent byte=0x%02x Child byte=0x%02x\n", *(unsigned char *)($ppa+va_pa_offset), *(unsigned char *)($cpa+va_pa_offset)
set logging off
break trap
continue
set $frame=(struct trapframe *)$a0
set $epc=$frame->epc
tbreak __trapret
continue
set $newpa=($cl0[0] >> 10) << 12
set logging file evidence/cow-proof-after.txt
set logging on
printf "COW PROOF 2: AFTER FAULT REPAIR, BEFORE STORE RETRY\n"
printf "Scope: isolated C module; real QEMU hardware page fault.\n\n"
printf "scause=%lu (15 = store page fault)\n", $frame->cause
printf "Fault VA=0x%lx\n", $frame->tval
printf "Saved EPC=0x%lx Repaired EPC=0x%lx\n", $epc,$frame->epc
printf "EPC unchanged (retry) = %d\n", $epc==$frame->epc
printf "Faulting instruction: "
x/i $epc
printf "Parent physical page  = 0x%lx\n", $ppa
printf "Child physical page   = 0x%lx\n", $newpa
printf "Different pages       = %d\n", $ppa!=$newpa
printf "Parent W=%d COW=%d refs=%d\n",!!($pl0[0]&4),!!($pl0[0]&0x100),pages[($ppa>>12)-nbase].ref
printf "Child  W=%d COW=%d refs=%d\n",!!($cl0[0]&4),!!($cl0[0]&0x100),pages[($newpa>>12)-nbase].ref
printf "Before retry: parent=0x%02x child=0x%02x\n",*(unsigned char *)($ppa+va_pa_offset),*(unsigned char *)($newpa+va_pa_offset)
set logging off
tbreak cow_proof_after
continue
set logging file evidence/cow-proof-isolation.txt
set logging on
printf "COW PROOF 3: STORE RETRIED; PARENT/CHILD DATA ISOLATION\n"
printf "Scope: isolated C memory-module harness; real RISC-V sb instruction.\n\n"
printf "Store target VA       = 0x00800000\n"
printf "Parent physical page  = 0x%lx\n",$ppa
printf "Child physical page   = 0x%lx\n",$newpa
printf "Parent byte           = 0x%02x (original 0x44)\n",*(unsigned char *)($ppa+va_pa_offset)
printf "Child byte            = 0x%02x (stored 0x55)\n",*(unsigned char *)($newpa+va_pa_offset)
printf "Real store faults     = %d\n",faults
printf "Parent unchanged      = %d\n",*(unsigned char *)($ppa+va_pa_offset)==0x44
printf "Child store committed = %d\n",*(unsigned char *)($newpa+va_pa_offset)==0x55
printf "Data isolation        = %d\n",*(unsigned char *)($ppa+va_pa_offset)!=*(unsigned char *)($newpa+va_pa_offset)
set logging off
detach
quit
