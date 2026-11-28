/* b_kcheck - in-kernel PCB initialization check (member B, Lab6).
 * run.sh appends this file to a TEMPORARY copy of kern/process/proc.c, so it
 * can call the static alloc_proc(); the repository proc.c is never changed.
 * Called once at the end of proc_init(). */
static void
b_kcheck_alloc_proc(void)
{
    struct proc_struct *p = alloc_proc();
    assert(p != NULL);
    int ok = p->state == PROC_UNINIT && p->pid == -1 && p->runs == 0 &&
             p->kstack == 0 && p->need_resched == 0 && p->parent == NULL &&
             p->mm == NULL && p->tf == NULL && p->pgdir == boot_pgdir_pa &&
             p->flags == 0 && p->exit_code == 0 && p->wait_state == 0 &&
             p->cptr == NULL && p->yptr == NULL && p->optr == NULL &&
             list_empty(&(p->list_link)) && list_empty(&(p->hash_link));
    cprintf("b_kcheck: pre-Lab6 PCB fields %s\n", ok ? "ok" : "FAIL");
    int ok6 = p->rq == NULL && list_empty(&(p->run_link)) &&
              p->time_slice == 0 && p->lab6_run_pool.left == NULL &&
              p->lab6_run_pool.right == NULL && p->lab6_run_pool.parent == NULL &&
              p->lab6_stride == 0 && p->lab6_priority == 1;
    cprintf("b_kcheck: six Lab6 scheduling fields %s\n", ok6 ? "ok" : "FAIL");
    int okc = p->cfs_vruntime == 0 && p->cfs_sum_exec == 0 &&
              p->cfs_slice_exec == 0 && p->cfs_flags == 0;
    cprintf("b_kcheck: CFS fields %s\n", okc ? "ok" : "FAIL");
    kfree(p);
    cprintf("b_kcheck: idleproc pid %d, initproc pid %d, current %s\n",
            idleproc->pid, initproc->pid, current == idleproc ? "idle" : "?");
}
