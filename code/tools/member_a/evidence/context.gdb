set pagination off
set confirm off
set print pretty on
set remotetimeout 10
set architecture riscv:rv64
target remote :45553
hbreak *lab3_a_probe_trap
continue
delete breakpoints
echo === BEFORE EXPLICIT 32-BIT EBREAK ===\n
x/i $pc
if !((unsigned long long)$sp % 16 == 0)
  echo FAIL: pre-trap stack alignment\n
  quit 1
end
set $a_before_status = $sstatus
hbreak *__alltraps
continue
delete breakpoints
echo === HARDWARE TRAP ENTRY ===\n
info registers pc sp sstatus sepc scause
set $a_entry_status = $sstatus
set $a_epc = $sepc
set $a_cause = $scause
set $a_old_0 = $zero
set $a_old_1 = $ra
set $a_old_2 = $sp
set $a_old_3 = $gp
set $a_old_4 = $tp
set $a_old_5 = $t0
set $a_old_6 = $t1
set $a_old_7 = $t2
set $a_old_8 = $s0
set $a_old_9 = $s1
set $a_old_10 = $a0
set $a_old_11 = $a1
set $a_old_12 = $a2
set $a_old_13 = $a3
set $a_old_14 = $a4
set $a_old_15 = $a5
set $a_old_16 = $a6
set $a_old_17 = $a7
set $a_old_18 = $s2
set $a_old_19 = $s3
set $a_old_20 = $s4
set $a_old_21 = $s5
set $a_old_22 = $s6
set $a_old_23 = $s7
set $a_old_24 = $s8
set $a_old_25 = $s9
set $a_old_26 = $s10
set $a_old_27 = $s11
set $a_old_28 = $t3
set $a_old_29 = $t4
set $a_old_30 = $t5
set $a_old_31 = $t6
if !($a_cause == 3)
  echo FAIL: expected scause\n
  quit 1
end
hbreak *trap
continue
delete breakpoints
set $a_frame = $a0
echo === C TRAP FRAME ===\n
p/x $a_frame
p sizeof(struct trapframe)
p/x *(struct trapframe *)$a_frame
if !($a_frame == $sp)
  echo FAIL: a0 points to current trapframe\n
  quit 1
end
if !($a_old_2 - $sp == 288)
  echo FAIL: exactly 288 bytes allocated\n
  quit 1
end
if !((unsigned long long)$sp % 16 == 0)
  echo FAIL: C ABI stack alignment\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 0) == $a_old_0)
  echo FAIL: saved GPR slot 0\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 8) == $a_old_1)
  echo FAIL: saved GPR slot 1\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 16) == $a_old_2)
  echo FAIL: saved GPR slot 2\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 24) == $a_old_3)
  echo FAIL: saved GPR slot 3\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 32) == $a_old_4)
  echo FAIL: saved GPR slot 4\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 40) == $a_old_5)
  echo FAIL: saved GPR slot 5\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 48) == $a_old_6)
  echo FAIL: saved GPR slot 6\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 56) == $a_old_7)
  echo FAIL: saved GPR slot 7\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 64) == $a_old_8)
  echo FAIL: saved GPR slot 8\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 72) == $a_old_9)
  echo FAIL: saved GPR slot 9\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 80) == $a_old_10)
  echo FAIL: saved GPR slot 10\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 88) == $a_old_11)
  echo FAIL: saved GPR slot 11\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 96) == $a_old_12)
  echo FAIL: saved GPR slot 12\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 104) == $a_old_13)
  echo FAIL: saved GPR slot 13\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 112) == $a_old_14)
  echo FAIL: saved GPR slot 14\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 120) == $a_old_15)
  echo FAIL: saved GPR slot 15\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 128) == $a_old_16)
  echo FAIL: saved GPR slot 16\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 136) == $a_old_17)
  echo FAIL: saved GPR slot 17\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 144) == $a_old_18)
  echo FAIL: saved GPR slot 18\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 152) == $a_old_19)
  echo FAIL: saved GPR slot 19\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 160) == $a_old_20)
  echo FAIL: saved GPR slot 20\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 168) == $a_old_21)
  echo FAIL: saved GPR slot 21\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 176) == $a_old_22)
  echo FAIL: saved GPR slot 22\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 184) == $a_old_23)
  echo FAIL: saved GPR slot 23\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 192) == $a_old_24)
  echo FAIL: saved GPR slot 24\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 200) == $a_old_25)
  echo FAIL: saved GPR slot 25\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 208) == $a_old_26)
  echo FAIL: saved GPR slot 26\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 216) == $a_old_27)
  echo FAIL: saved GPR slot 27\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 224) == $a_old_28)
  echo FAIL: saved GPR slot 28\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 232) == $a_old_29)
  echo FAIL: saved GPR slot 29\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 240) == $a_old_30)
  echo FAIL: saved GPR slot 30\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 248) == $a_old_31)
  echo FAIL: saved GPR slot 31\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 256) == $a_entry_status)
  echo FAIL: saved CSR slot 32\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 264) == $a_epc)
  echo FAIL: saved CSR slot 33\n
  quit 1
end
if !(*(unsigned long long *)($a_frame + 280) == $a_cause)
  echo FAIL: saved CSR slot 35\n
  quit 1
end
hbreak *__trapret
continue
delete breakpoints
echo === BEFORE RESTORE_ALL ===\n
info registers pc sp sstatus sepc
p/x ((struct trapframe *)$a_frame)->epc
if !(((struct trapframe *)$a_frame)->epc == (unsigned long long)&lab3_a_probe_after)
  echo FAIL: handler return EPC\n
  quit 1
end
echo Poison all 30 writable GPRs except SP before RESTORE_ALL.\n
set $ra = 3735879681
set $gp = 3735879683
set $tp = 3735879684
set $t0 = 3735879685
set $t1 = 3735879686
set $t2 = 3735879687
set $s0 = 3735879688
set $s1 = 3735879689
set $a0 = 3735879690
set $a1 = 3735879691
set $a2 = 3735879692
set $a3 = 3735879693
set $a4 = 3735879694
set $a5 = 3735879695
set $a6 = 3735879696
set $a7 = 3735879697
set $s2 = 3735879698
set $s3 = 3735879699
set $s4 = 3735879700
set $s5 = 3735879701
set $s6 = 3735879702
set $s7 = 3735879703
set $s8 = 3735879704
set $s9 = 3735879705
set $s10 = 3735879706
set $s11 = 3735879707
set $t3 = 3735879708
set $t4 = 3735879709
set $t5 = 3735879710
set $t6 = 3735879711
hbreak *(unsigned long long)&lab3_a_probe_after
continue
delete breakpoints
echo === AFTER SRET ===\n
info registers pc sp sstatus sepc
if !($zero == $a_old_0)
  echo FAIL: restored GPR zero\n
  quit 1
end
if !($ra == $a_old_1)
  echo FAIL: restored GPR ra\n
  quit 1
end
if !($sp == $a_old_2)
  echo FAIL: restored GPR sp\n
  quit 1
end
if !($gp == $a_old_3)
  echo FAIL: restored GPR gp\n
  quit 1
end
if !($tp == $a_old_4)
  echo FAIL: restored GPR tp\n
  quit 1
end
if !($t0 == $a_old_5)
  echo FAIL: restored GPR t0\n
  quit 1
end
if !($t1 == $a_old_6)
  echo FAIL: restored GPR t1\n
  quit 1
end
if !($t2 == $a_old_7)
  echo FAIL: restored GPR t2\n
  quit 1
end
if !($s0 == $a_old_8)
  echo FAIL: restored GPR s0\n
  quit 1
end
if !($s1 == $a_old_9)
  echo FAIL: restored GPR s1\n
  quit 1
end
if !($a0 == $a_old_10)
  echo FAIL: restored GPR a0\n
  quit 1
end
if !($a1 == $a_old_11)
  echo FAIL: restored GPR a1\n
  quit 1
end
if !($a2 == $a_old_12)
  echo FAIL: restored GPR a2\n
  quit 1
end
if !($a3 == $a_old_13)
  echo FAIL: restored GPR a3\n
  quit 1
end
if !($a4 == $a_old_14)
  echo FAIL: restored GPR a4\n
  quit 1
end
if !($a5 == $a_old_15)
  echo FAIL: restored GPR a5\n
  quit 1
end
if !($a6 == $a_old_16)
  echo FAIL: restored GPR a6\n
  quit 1
end
if !($a7 == $a_old_17)
  echo FAIL: restored GPR a7\n
  quit 1
end
if !($s2 == $a_old_18)
  echo FAIL: restored GPR s2\n
  quit 1
end
if !($s3 == $a_old_19)
  echo FAIL: restored GPR s3\n
  quit 1
end
if !($s4 == $a_old_20)
  echo FAIL: restored GPR s4\n
  quit 1
end
if !($s5 == $a_old_21)
  echo FAIL: restored GPR s5\n
  quit 1
end
if !($s6 == $a_old_22)
  echo FAIL: restored GPR s6\n
  quit 1
end
if !($s7 == $a_old_23)
  echo FAIL: restored GPR s7\n
  quit 1
end
if !($s8 == $a_old_24)
  echo FAIL: restored GPR s8\n
  quit 1
end
if !($s9 == $a_old_25)
  echo FAIL: restored GPR s9\n
  quit 1
end
if !($s10 == $a_old_26)
  echo FAIL: restored GPR s10\n
  quit 1
end
if !($s11 == $a_old_27)
  echo FAIL: restored GPR s11\n
  quit 1
end
if !($t3 == $a_old_28)
  echo FAIL: restored GPR t3\n
  quit 1
end
if !($t4 == $a_old_29)
  echo FAIL: restored GPR t4\n
  quit 1
end
if !($t5 == $a_old_30)
  echo FAIL: restored GPR t5\n
  quit 1
end
if !($t6 == $a_old_31)
  echo FAIL: restored GPR t6\n
  quit 1
end
if !(($sstatus & 0x20) == 0)
  echo FAIL: version-specific SPIE after sret\n
  quit 1
end
if !(($sstatus & 0x100) == 0)
  echo FAIL: sret clears SPP\n
  quit 1
end
if !(($sstatus & 2) == ($a_before_status & 2))
  echo FAIL: restored prior SIE\n
  quit 1
end
hbreak *lab3_a_test_done
continue
delete breakpoints
echo GDB_CONTEXT_ALL_PASS\n
detach
quit
