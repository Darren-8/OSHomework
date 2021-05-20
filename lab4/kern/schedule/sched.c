#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

void
schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    // 关闭中断
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        // 检查正在运行的进程是否是第0号进程
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        // 从进程列表中寻找到就绪的可以运行的进程
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        // 如果没有找到可以运行的进程，就运行第0号进程
        if (next == NULL || next->state != PROC_RUNNABLE) {
            next = idleproc;
        }
        // 运行次数统计
        next->runs ++;

        // 进行进程调度
        if (next != current) {
            proc_run(next);
        }
    }
    // 恢复中断
    local_intr_restore(intr_flag);
}

