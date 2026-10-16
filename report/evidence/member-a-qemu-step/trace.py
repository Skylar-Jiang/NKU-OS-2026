# gdb Python helper: line-by-line `next` trace of QEMU source with changed-variable dump.
# Usage inside host GDB:  trace-fn [MAX_STEPS]
# Steps the selected frame with `next`; stops when that frame returns. If a `next`
# stops at a breakpoint inside a callee, the callee is traced too (deeper indent).
import gdb, os

WATCH = {
    "get_page_addr_code": ["addr", "mmu_idx", "index", "entry->addr_code", "entry->addend",
                           "(entry->addr_code & ~0x7ffUL) == (addr & ~0xfffUL)", "p"],
    "tlb_fill": ["addr", "access_type", "mmu_idx", "ok"],
    "riscv_cpu_tlb_fill": ["address", "access_type", "mmu_idx", "mode", "ret", "pa", "prot"],
    "get_physical_address": ["addr", "access_type", "mmu_idx", "mode", "env->satp", "vm", "base",
                             "levels", "ptidxbits", "ptesize", "va_bits", "mask", "masked_msbs",
                             "ptshift", "i", "idx", "pte_addr", "pte", "ppn", "vpn",
                             "*physical", "*prot"],
    "tlb_set_page_with_attrs": ["vaddr", "paddr", "prot", "mmu_idx", "size", "vaddr_page",
                                "paddr_page", "index", "address", "addend", "tn.addr_read",
                                "tn.addr_write", "tn.addr_code", "tn.addend"],
    "load_helper": ["addr", "mmu_idx", "index", "tlb_addr", "entry->addr_read", "entry->addend",
                    "(tlb_addr & ~0x7ffUL) == (addr & ~0xfffUL)", "haddr", "res"],
    "store_helper": ["addr", "val", "mmu_idx", "index", "tlb_addr", "entry->addr_write",
                     "entry->addend", "(tlb_addr & ~0x7ffUL) == (addr & ~0xfffUL)", "haddr"],
}
# Function parameters are valid on entry; other expressions are recorded silently the first
# time they are seen (uninitialised locals) and printed only when their value changes.
PARAMS = {
    "get_page_addr_code": {"addr"},
    "tlb_fill": {"addr", "access_type", "mmu_idx"},
    "riscv_cpu_tlb_fill": {"address", "access_type", "mmu_idx"},
    "get_physical_address": {"addr", "access_type", "mmu_idx", "env->satp"},
    "tlb_set_page_with_attrs": {"vaddr", "paddr", "prot", "mmu_idx", "size"},
    "load_helper": {"addr"},
    "store_helper": {"addr", "val"},
}
_src = {}

def _line(path, n):
    if path not in _src:
        try:
            _src[path] = open(path).read().split("\n")
        except OSError:
            _src[path] = []
    lines = _src[path]
    return lines[n - 1].strip() if 0 < n <= len(lines) else "?"

def _depth(f):
    d = 0
    while f is not None:
        d += 1
        f = f.older()
    return d

class TraceFn(gdb.Command):
    """trace-fn [MAX]: next line-by-line until the current frame returns."""
    def __init__(self):
        super().__init__("trace-fn", gdb.COMMAND_RUNNING)

    def invoke(self, arg, from_tty):
        limit = int(arg) if arg.strip() else 500
        base = _depth(gdb.selected_frame())
        last = {}
        for n in range(limit):
            f = gdb.selected_frame()
            d = _depth(f)
            sal = f.find_sal()
            name = f.name() or "??"
            if d < base:
                print("[TRACE end] returned to %s %s:%d" % (name,
                      os.path.basename(sal.symtab.filename) if sal.symtab else "?", sal.line))
                return
            fn = sal.symtab.fullname() if sal.symtab else ""
            print("[%03d]%s %s %s:%d  %s" % (n, "  " * (d - base), name,
                  os.path.basename(fn), sal.line, _line(fn, sal.line)))
            for expr in WATCH.get(name, []):
                try:
                    v = gdb.parse_and_eval(expr)
                    s = str(v)
                    if v.type.strip_typedefs().code == gdb.TYPE_CODE_INT:
                        s = "0x%x (%s)" % (int(v) & 0xffffffffffffffff, s)
                except gdb.error:
                    continue
                key = (name, d, expr)
                if key not in last and expr not in PARAMS.get(name, ()):
                    last[key] = s
                    continue
                if last.get(key) != s:
                    last[key] = s
                    print("        %s%s = %s" % ("  " * (d - base), expr, s))
            gdb.execute("next", to_string=True)
        print("[TRACE stop] step limit %d reached" % limit)

TraceFn()
