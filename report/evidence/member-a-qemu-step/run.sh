#!/bin/sh
# Usage (from code/ after `make`): sh ../report/evidence/member-a-qemu-step/run.sh fetch|data
#   fetch: --enable-debug QEMU (TCG), instruction-fetch TLB miss/walk/fill/hit
#   data : --enable-debug --enable-tcg-interpreter QEMU (TCI), data store/load TLB miss and hit
set -u
CASE=$1
E=$(cd "$(dirname "$0")" && pwd)
case $CASE in
  fetch) QEMU_DBG=${QEMU_DBG:-$HOME/src/qemu-4.1.1/build-debug/riscv64-softmmu/qemu-system-riscv64} ;;
  data)  QEMU_DBG=${QEMU_DBG:-$HOME/src/qemu-4.1.1/build-debug-tci/riscv64-softmmu/qemu-system-riscv64} ;;
  refill) QEMU_DBG=${QEMU_DBG:-$HOME/src/qemu-4.1.1/build-debug/riscv64-softmmu/qemu-system-riscv64} ;;
  *) echo "usage: $0 fetch|data|refill"; exit 2 ;;
esac
HOST_GDB=${HOST_GDB:-$HOME/opt/gdb-12.1-ubuntu/bin/gdb-host}
GUEST_GDB=${GUEST_GDB:-riscv64-unknown-elf-gdb}
PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
sed "s/__PORT__/$PORT/" "$E/$CASE-guest.gdb" > "$E/.$CASE-guest.run.gdb"
# Terminals 1+2 of the manual: QEMU (with -S and a gdbstub) runs under the host GDB.
( cd "$E" && exec timeout 900 "$HOST_GDB" -batch -nx -x "$E/$CASE-host.gdb" --args "$QEMU_DBG" \
    -machine virt -m 128M -nographic -bios default \
    -device loader,file="$OLDPWD/bin/ucore.img",addr=0x80200000 -S -gdb tcp:127.0.0.1:$PORT ) \
    > "$E/$CASE-host-gdb.log" 2>&1 &
HPID=$!
i=0; while ! ss -ltn | grep -q "127.0.0.1:$PORT "; do i=$((i+1)); [ $i -gt 1200 ] && break; sleep 0.1; done
# Terminal 3 of the manual: guest GDB (riscv64-unknown-elf-gdb) on uCore.
timeout 840 "$GUEST_GDB" -batch -nx -x "$E/.$CASE-guest.run.gdb" > "$E/$CASE-guest-gdb.log" 2>&1
echo "GUEST_GDB_EXIT=$?" >> "$E/$CASE-guest-gdb.log"
sleep 5
pkill -f "^$QEMU_DBG .*tcp:127.0.0.1:$PORT" 2>/dev/null
wait $HPID; echo "HOST_GDB_EXIT=$?" >> "$E/$CASE-host-gdb.log"
rm -f "$E/.$CASE-guest.run.gdb"
