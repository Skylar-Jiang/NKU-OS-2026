/* Isolated harness. Real pmm/vmm/proc implementations; test page/heap
 * allocators. Uses integrated alloc_proc when available, otherwise a stub.
 * This is NOT the complete integrated user-process regression. */
#include <pmm.h>
#include <vmm.h>
#include <proc.h>
#include <string.h>
#include <error.h>
#include <riscv.h>
#include <kmalloc.h>
#include <assert.h>
#include <stdio.h>
#include "../../../kern/process/proc.c"
#define COUNT 8192
static struct Page descriptors[COUNT];
static unsigned char busy[COUNT];
static unsigned char heap[256][512] __attribute__((aligned(16)));
static unsigned char heap_used[256];
static int page_budget=-1, heap_budget=-1, live_pages,live_heap,passed;
static pde_t root[512] __attribute__((aligned(4096)));
static struct proc_struct parent;
static struct mm_struct *fault_mm;
static int faults;
/* Stable debugger checkpoints; empty barriers do not change test state. */
void __attribute__((noinline)) cow_proof_before(struct mm_struct *p, struct mm_struct *c) {
    asm volatile("" : : "r"(p), "r"(c) : "memory");
}
void __attribute__((noinline)) cow_proof_after(struct mm_struct *p, struct mm_struct *c) {
    asm volatile("" : : "r"(p), "r"(c) : "memory");
}
static char pgdir_output[8192];
static size_t pgdir_used;
static void capture_char(int c, void *unused) {
    if (pgdir_used + 1 < sizeof(pgdir_output)) {
        pgdir_output[pgdir_used++] = c;
        pgdir_output[pgdir_used] = 0;
    }
}
int cprintf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    size_t before = pgdir_used;
    vprintfmt(capture_char, NULL, fmt, ap); va_end(ap);
    return pgdir_used - before;
}
static int captured(const char *s) {
    for (size_t i = 0; i < pgdir_used; i++)
        if (!strncmp(pgdir_output+i, s, strlen(s))) return 1;
    return 0;
}
static void say(const char *s) {
    volatile char *uart=(void *)(PHYSICAL_MEMORY_OFFSET+0x10000000UL);
    while(*s) *uart=*s++;
}
static void done(int ok) __attribute__((noreturn));
static void done(int ok) {
    *(volatile unsigned *)(PHYSICAL_MEMORY_OFFSET+0x100000UL)=ok?0x5555:0x3333;
    for(;;) {}
}
void __panic(const char *f,int line,const char *fmt,...) {say("FAIL assert ");say(f);say("\n");done(0);}
void __warn(const char *f,int line,const char *fmt,...) {say("WARN\n");}
void schedule(void) {panic("unexpected scheduler in isolated test");}
void wakeup_proc(struct proc_struct *p) {p->state=PROC_RUNNABLE;p->wait_state=0;}
void forkrets(struct trapframe *tf) {panic("B/A first-entry chain is outside this harness");}
static void check(int ok,const char *name) {if(!ok){say("FAIL ");say(name);say("\n");done(0);}passed++;say("PASS ");say(name);say("\n");}
static struct Page *test_alloc(size_t n) {
    if(page_budget==0)return NULL;
    if(page_budget>0)page_budget--;
    for(size_t i=4096;i+n<=COUNT;i++) {
        size_t j=0;while(j<n&&!busy[i+j])j++;
        if(j!=n){i+=j;continue;}
        for(j=0;j<n;j++){busy[i+j]=1;set_page_ref(&descriptors[i+j],0);}
        live_pages+=n;return &descriptors[i];
    }return NULL;
}
static void test_free(struct Page *p,size_t n) {
    size_t k=p-descriptors;assert(k>=4096&&k+n<=COUNT);
    for(size_t j=0;j<n;j++){assert(busy[k+j]);busy[k+j]=0;set_page_ref(p+j,0);}live_pages-=n;
}
static size_t test_nr(void){return 4096-live_pages;}
static const struct pmm_manager manager={.alloc_pages=test_alloc,.free_pages=test_free,.nr_free_pages=test_nr};
void *kmalloc(size_t n) {
    assert(n<=512);if(heap_budget==0)return NULL;if(heap_budget>0)heap_budget--;
    for(int i=0;i<256;i++)if(!heap_used[i]){heap_used[i]=1;live_heap++;return (void *)((uintptr_t)heap[i]+PHYSICAL_MEMORY_OFFSET);}return NULL;
}
void kfree(void *p) {
    uintptr_t low=(uintptr_t)p-PHYSICAL_MEMORY_OFFSET;
    size_t i=(low-(uintptr_t)heap)/512;assert(i<256&&heap_used[i]);heap_used[i]=0;live_heap--;
}
#ifndef MEMBER_C_PRODUCTION_ALLOC_PROC
static struct proc_struct *alloc_proc(void) {
    struct proc_struct *p=kmalloc(sizeof(*p));if(!p)return NULL;
    memset(p,0,sizeof(*p));p->pid=-1;p->pgdir=boot_pgdir_pa;
    list_init(&p->list_link);list_init(&p->hash_link);return p;
}
#endif
static struct mm_struct *newmm(void) {
    struct mm_struct *mm=mm_create();assert(mm&&setup_pgdir(mm)==0);return mm;
}
static struct Page *addpage(struct mm_struct *mm,uintptr_t va,int writable,int value) {
    assert(mm_map(mm,va,PGSIZE,VM_READ|(writable?VM_WRITE:0),NULL)==0);
    struct Page *p=alloc_page();assert(p);memset(page2kva(p),value,PGSIZE);
    assert(page_insert(mm->pgdir,p,va,PTE_U|PTE_R|(writable?PTE_W:0))==0);return p;
}
static void destroy(struct mm_struct *mm) {assert(mm_count(mm)==0);exit_mmap(mm);put_pgdir(mm);mm_destroy(mm);}
static pte_t *pte(struct mm_struct *mm,uintptr_t va){pte_t *p=get_pte(mm->pgdir,va,0);assert(p);return p;}
static int byte(struct mm_struct *mm,uintptr_t va){return *(unsigned char *)page2kva(pte2page(*pte(mm,va)));}
void trap(struct trapframe *tf) {
    assert(tf->cause==15&&fault_mm);faults++;
    assert(cow_handle_store_fault(fault_mm,tf->tval)==0);
    /* EPC unchanged: restart the faulting store. */
}
static void free_child(struct proc_struct *p) {
    unhash_proc(p);remove_links(p);
    if(p->mm && mm_count_dec(p->mm)==0)destroy(p->mm);
    put_kstack(p);kfree(p);
}
void test_main(void) {
    pages=descriptors;npage=(0x82000000UL>>12);va_pa_offset=PHYSICAL_MEMORY_OFFSET;pmm_manager=&manager;
    boot_pgdir_pa=(uintptr_t)root;boot_pgdir_va=(void *)((uintptr_t)root+PHYSICAL_MEMORY_OFFSET);
    current=idleproc=&parent;parent.pid=0;parent.pgdir=boot_pgdir_pa;parent.state=PROC_RUNNABLE;
    list_init(&proc_list);for(int i=0;i<HASH_LIST_SIZE;i++)list_init(hash_list+i);
    extern void __alltraps(void);write_csr(stvec,(uintptr_t)__alltraps);set_csr(sstatus,SSTATUS_SUM);
    uintptr_t va=UTEXT;
    int before=live_pages;
    /* Synthetic active root: exercise all Sv39 leaf levels, including a
     * canonical high VA. Keep the real root's RAM/MMIO mappings for execution. */
    struct mm_struct *printed = newmm();
    struct Page *printed_page = addpage(printed, va, 1, 0x12);
    *get_pte(printed->pgdir, va, 0) = (*get_pte(printed->pgdir, va, 0) & ~PTE_W) | PTE_COW;
    pte_t *l1 = KADDR(PTE_ADDR(printed->pgdir[0]));
    l1[10] = pte_create(0x82000000UL >> 12, PTE_R);
    pte_t printed_saved = *get_pte(printed->pgdir, va, 0);
    int print_pages = live_pages, print_ref = page_ref(printed_page);
    write_csr(satp, ((uintptr_t)8 << 60) | (PADDR(printed->pgdir) >> 12));
    asm volatile("sfence.vma":::"memory");
    pgdir_used = 0; pgdir_output[0] = 0; print_pgdir();
    check(captured("L0 VA=0000000000800000") && captured("COW=1"), "print active user root and COW leaf");
    check(captured("size=200000") && captured("L1 VA=0000000001400000"), "print 2 MiB leaf without walking as table");
    check(captured("L2 VA=ffffffffc0000000") && captured("size=40000000"), "print canonical high 1 GiB leaf");
    check(live_pages == print_pages && page_ref(printed_page) == print_ref &&
          *get_pte(printed->pgdir, va, 0) == printed_saved, "page table printer is read only");
    write_csr(satp, ((uintptr_t)8 << 60) | (boot_pgdir_pa >> 12));
    asm volatile("sfence.vma":::"memory");
    l1[10] = 0; destroy(printed);
    check(live_pages == before && live_heap == 0, "printer fixture cleanup");
    struct mm_struct *a=newmm(),*b=newmm();struct Page *original=addpage(a,va,1,0x31);
    check(copy_range(b->pgdir,a->pgdir,va,va+PGSIZE,0)==0,"eager range copy");
    check(pte2page(*pte(b,va))!=original&&byte(b,va)==0x31,"eager copy has distinct frame and equal data");
    *(char *)page2kva(pte2page(*pte(b,va)))=0x22;
    check(byte(a,va)==0x31,"eager child write isolation");
    unmap_range(b->pgdir,va,va+PGSIZE);exit_range(b->pgdir,va,va+PGSIZE);destroy(b);destroy(a);
    check(live_pages==before&&live_heap==0,"eager cleanup returns all resources");
#if !LAB5_COW
    a=newmm();original=addpage(a,va,1,0x18);parent.mm=a;set_mm_count(a,1);
    struct trapframe eager_tf={0};eager_tf.gpr.sp=0x7ffff000;eager_tf.epc=va;
    int eager_pid=do_fork(0,eager_tf.gpr.sp,&eager_tf);
    struct proc_struct *eager_child=find_proc(eager_pid);
    check(eager_child&&eager_child->mm!=a,"eager do_fork creates separate mm");
    check(pte2page(*pte(eager_child->mm,va))!=original&&(*pte(eager_child->mm,va)&PTE_W),"eager do_fork copies writable frame");
    check(eager_child->tf->gpr.a0==0,"eager child return zero");
    free_child(eager_child);set_mm_count(a,0);parent.mm=NULL;destroy(a);
    check(live_pages==before&&live_heap==0&&nr_process==0,"eager fork cleanup");
    say("ALL MEMBER C EAGER TESTS PASS\n");done(1);
#else
    a=newmm();b=newmm();original=addpage(a,va,1,0x41);
    check(copy_range(b->pgdir,a->pgdir,va,va+PGSIZE,1)==0,"COW range copy");
    check(pte2page(*pte(b,va))==original&&page_ref(original)==2,"fork shares frame with two references");
    check((*pte(a,va)&PTE_COW)&&!(*pte(a,va)&PTE_W)&&(*pte(b,va)&PTE_COW),"both PTEs write protected");
    /* Give child a real VMA because raw copy_range only copies PTEs. */
    assert(mm_map(b,va,PGSIZE,VM_READ|VM_WRITE,NULL)==0);
    page_budget=0;pte_t saved=*pte(b,va);
    check(cow_handle_store_fault(b,va)==-E_NO_MEM&&*pte(b,va)==saved&&page_ref(original)==2,"COW OOM preserves mapping and refs");page_budget=-1;
    check(do_cow_fault(b,va)==0,"shared COW write allocates private page");
    check(pte2page(*pte(b,va))!=original&&page_ref(original)==1&&byte(b,va)==0x41,"COW copy content and refs");
    *(char *)page2kva(pte2page(*pte(b,va)))=0x42;
    check(byte(a,va)==0x41,"COW child data isolated");
    int count=live_pages;
    check(do_cow_fault(a,va)==0&&live_pages==count&&(*pte(a,va)&PTE_W),"single reference restores W without allocation");
    check(cow_handle_store_fault(a,va)==-E_INVAL,"non-COW fault rejected");destroy(b);destroy(a);
    check(live_pages==before&&live_heap==0,"COW cleanup no leaks");
    a=newmm();b=newmm();addpage(a,va,1,0x33);assert(dup_mmap(b,a)==0);
    check(do_cow_fault(a,va)==0,"parent first write breaks shared COW");
    *(char *)page2kva(pte2page(*pte(a,va)))=0x34;
    check(byte(b,va)==0x33&&byte(a,va)==0x34,"parent first write leaves child data unchanged");
    check(do_cow_fault(a,va+PGSIZE)==-E_INVAL,"fault outside VMA rejected");
    check(!user_mem_check(a,(uintptr_t)-4,16,1),"overflowing user range rejected");destroy(b);destroy(a);
#ifndef TEST_COPYOUT_MUTANT
    a=newmm();b=newmm();addpage(a,va,1,0x35);addpage(a,va+PGSIZE,1,0x36);assert(dup_mmap(b,a)==0);
    page_budget=1;
    check(!copy_to_user(b,(void *)(va+PGSIZE-1),"XY",2),"cross-page copyout OOM returns failure");page_budget=-1;
    check(((unsigned char *)page2kva(pte2page(*pte(b,va))))[PGSIZE-1]==0x35&&byte(b,va+PGSIZE)==0x36,"copyout preflight failure does not partially write payload");
    check(copy_to_user(b,(void *)(va+PGSIZE-1),"XY",2),"cross-page copyout succeeds after retry");
    check(((char *)page2kva(pte2page(*pte(b,va))))[PGSIZE-1]=='X'&&byte(b,va+PGSIZE)=='Y'&&byte(a,va)==0x35&&byte(a,va+PGSIZE)==0x36,"cross-page child payload and parent isolation");destroy(b);destroy(a);
    check(live_pages==before&&live_heap==0,"cross-page copyout cleanup");
#endif
    a=newmm();b=newmm();original=addpage(a,va,0,0x51);
    check(dup_mmap(b,a)==0,"duplicate readonly VMA");
    check(!(*pte(b,va)&PTE_COW)&&!(*pte(b,va)&PTE_W),"true readonly never becomes COW writable");
    check(do_cow_fault(b,va)==-E_INVAL&&!copy_to_user(b,(void *)va,"X",1),"readonly writes denied including kernel copyout");destroy(b);destroy(a);
    a=newmm();b=newmm();struct mm_struct *d=newmm();original=addpage(a,va,1,0x61);
    assert(dup_mmap(b,a)==0&&dup_mmap(d,b)==0);
    check(page_ref(original)==3,"repeated fork three-way sharing");
    check(copy_to_user(b,(void *)va,"Z",1)&&byte(a,va)==0x61&&byte(b,va)=='Z'&&byte(d,va)==0x61,"copy_to_user breaks COW through physical alias");
    destroy(b);check(page_ref(original)==2,"child teardown keeps other mappings alive");destroy(d);
    check(page_ref(original)==1&&do_cow_fault(a,va)==0,"last owner remains usable after other exits");destroy(a);
    a=newmm();b=newmm();addpage(a,va,1,0x71);addpage(a,va+PTSIZE,1,0x72);assert(dup_mmap(b,a)==0);
    destroy(b);check(byte(a,va)==0x71&&byte(a,va+PTSIZE)==0x72,"sibling page table teardown preserves parent");destroy(a);
    check(live_pages==before&&live_heap==0,"multi-VMA cleanup no leaks");
    a=newmm();b=newmm();addpage(a,va,1,0x11);
    count=live_pages;page_budget=1;
    check(copy_range(b->pgdir,a->pgdir,va,va+PGSIZE,1)==-E_NO_MEM&&live_pages==count,"intermediate page-table allocation rollback");page_budget=-1;destroy(b);destroy(a);
    a=newmm();addpage(a,va,1,0x21);addpage(a,va+PGSIZE,1,0x22);parent.mm=a;set_mm_count(a,1);
    struct trapframe tf={0};tf.gpr.sp=0x7ffff000;tf.epc=va;
    int pid=do_fork(0,tf.gpr.sp,&tf);struct proc_struct *child=find_proc(pid);
    check(pid>0&&child&&child->mm!=a&&child->parent==current,"do_fork independent mm and parent registration");
    check(child->tf->gpr.a0==0&&child->tf->gpr.sp==tf.gpr.sp&&child->tf->epc==va,"child return zero and user EPC/SP preserved");
    check(child->context.ra==(uintptr_t)forkret&&child->context.sp==(uintptr_t)child->tf,"fork first-entry context prepared");
    check(parent.cptr==child&&nr_process==1,"set_links registers exactly once");free_child(child);
    pid=do_fork(CLONE_VM,tf.gpr.sp,&tf);child=find_proc(pid);
    check(child&&child->mm==a&&mm_count(a)==2,"CLONE_VM shares mm rather than COW metadata");free_child(child);
    check(mm_count(a)==1&&nr_process==0&&parent.cptr==NULL,"shared mm child cleanup restores parent state");
    count=live_pages;int hs=live_heap;heap_budget=0;
    check(do_fork(0,0,&tf)==-E_NO_MEM&&live_pages==count&&live_heap==hs,"PCB failure rollback");heap_budget=-1;
    page_budget=0;
    check(do_fork(0,0,&tf)==-E_NO_MEM&&live_heap==hs&&nr_process==0,"kernel stack failure rollback");page_budget=-1;
    page_budget=1;
    check(do_fork(0,0,&tf)==-E_NO_MEM&&live_pages==count&&live_heap==hs,"root page allocation failure rollback");page_budget=-1;
    heap_budget=1;
    check(do_fork(0,0,&tf)==-E_NO_MEM&&live_pages==count&&live_heap==hs,"mm structure allocation failure rollback");heap_budget=-1;
    heap_budget=2;
    check(do_fork(0,0,&tf)==-E_NO_MEM&&live_pages==count&&live_heap==hs,"VMA allocation failure rollback");heap_budget=-1;
    heap_budget=3;
    check(do_fork(0,0,&tf)==-E_NO_MEM&&live_pages==count&&live_heap==hs&&page_ref(pte2page(*pte(a,va)))==1,"partial COW duplication failure releases child refs");heap_budget=-1;
    check(do_cow_fault(a,va)==0&&(*pte(a,va)&PTE_W),"parent survives failed fork with lazy COW permission restore");
    set_mm_count(a,0);parent.mm=NULL;destroy(a);
    check(live_pages==before&&live_heap==0,"fork cleanup all resources returned");
    nr_process=MAX_PROCESS;
    check(do_fork(0,0,&tf)==-E_NO_FREE_PROC&&live_pages==before&&live_heap==0,"process limit fails without allocation");nr_process=0;
    tf.status=SSTATUS_SPP;
    pid=do_fork(0,0,&tf);child=find_proc(pid);
    check(child&&child->mm==NULL&&child->tf->gpr.sp==(uintptr_t)child->tf,"kernel thread path keeps kernel stack when stack argument zero");free_child(child);
    check(live_pages==before&&live_heap==0,"kernel thread creation cleanup");
    /* Real supervisor store fault to a U/COW page; production Trap owned by A. */
    a=newmm();b=newmm();addpage(a,va,1,0x44);assert(dup_mmap(b,a)==0);
    fault_mm=b;write_csr(satp,((uintptr_t)8<<60)|(PADDR(b->pgdir)>>12));asm volatile("sfence.vma":::"memory");
    cow_proof_before(a,b);
    *(volatile char *)va=0x55;
    cow_proof_after(a,b);
    check(faults==1&&*(volatile unsigned char *)va==0x55&&byte(a,va)==0x44,"real store fault resolves and retries unchanged EPC");
    write_csr(satp,((uintptr_t)8<<60)|(boot_pgdir_pa>>12));asm volatile("sfence.vma":::"memory");fault_mm=NULL;destroy(b);destroy(a);
    check(live_pages==before&&live_heap==0,"real fault cleanup no leaks");
    say("ALL MEMBER C ISOLATED TESTS PASS\n");done(1);
#endif
}
void test_boot(void) {
    root[2]=0x200000cf;root[511]=0x200000cf;root[509]=0xcf;
    write_csr(pmpaddr0,(uintptr_t)-1);write_csr(pmpcfg0,0x0f);
    write_csr(medeleg,(uintptr_t)1<<15);
    write_csr(satp,((uintptr_t)8<<60)|((uintptr_t)root>>12));asm volatile("sfence.vma":::"memory");
    write_csr(mstatus,(read_csr(mstatus)&~(uintptr_t)0x1800)|0x800);
    write_csr(mepc,(uintptr_t)test_main);asm volatile("mret");
}
