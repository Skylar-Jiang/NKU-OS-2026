#!/bin/sh
# Member A: reproduce host-GDB (QEMU source) + guest-GDB (uCore) address translation observation.
# Usage (from code/ after `make`): sh ../report/evidence/member-a-qemu-host-gdb/run.sh
set -u
E=$(cd "$(dirname "$0")" && pwd)
QEMU_DBG=${QEMU_DBG:-$HOME/src/qemu-4.1.1/build/riscv64-softmmu/qemu-system-riscv64}
HOST_GDB=${HOST_GDB:-$HOME/opt/gdb-12.1-ubuntu/bin/gdb-host}
GUEST_GDB=${GUEST_GDB:-riscv64-unknown-elf-gdb}
PORT=${PORT:-$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')}
sed "s/__PORT__/$PORT/" "$E/guest.gdb" > "$E/guest.run.gdb"
# Terminal 1+2 equivalent: QEMU runs as a child of the host GDB (no sudo attach needed).
timeout 180 "$HOST_GDB" -batch -nx -x "$E/host.gdb" --args "$QEMU_DBG" -machine virt -m 128M -nographic \
  -bios default -device loader,file=bin/ucore.img,addr=0x80200000 -S -gdb tcp:127.0.0.1:$PORT \
  > "$E/host-gdb.log" 2>&1 &
HPID=$!
i=0; while ! ss -ltn | grep -q "127.0.0.1:$PORT "; do i=$((i+1)); [ $i -gt 600 ] && break; sleep 0.1; done
# Terminal 3 equivalent: guest GDB.
timeout 120 "$GUEST_GDB" -batch -nx -x "$E/guest.run.gdb" > "$E/guest-gdb.log" 2>&1
echo "GUEST_GDB_EXIT=$?" >> "$E/guest-gdb.log"
sleep 3
pkill -f "^$QEMU_DBG .*tcp:127.0.0.1:$PORT" 2>/dev/null
wait $HPID; echo "HOST_GDB_EXIT=$?" >> "$E/host-gdb.log"
