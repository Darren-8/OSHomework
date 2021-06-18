#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched.h>

// the list of timer
static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void
sched_class_enqueue(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->enqueue(rq, proc);
    }
}

static inline void
sched_class_dequeue(struct proc_struct *proc) {
    sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *
sched_class_pick_next(void) {
    return sched_class->pick_next(rq);
}

// 减少指定进程的时间片
void
sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
        // 注意，这里虽然传入了参数rq，但是对于RR调度方案，函数设置这个参数只是为了保留，实际中并没有使用，因为减少时间片的进程通常为正在运行的进程，然而这个进程并不在待调度队列中
        sched_class->proc_tick(rq, proc);
    }
    else {
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

void
sched_init(void) {

    // 计时器链表初始化，在RR调度中，此处没有作用
    list_init(&timer_list);

    // 设置调度器
    sched_class = &default_sched_class;

    // 设置待调度队列
    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq); // 初始化调度器

    cprintf("sched class: %s\n", sched_class->name);
}

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                // 放入待调度队列中
                sched_class_enqueue(proc);
            }
        }
        else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

void
schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        // 如果发现进程时间片用完后，并没有执行完成，则重新放入待调度队列
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);
        }
        // 调度运行下一个待调度的进程
        if ((next = sched_class_pick_next()) != NULL) {
            // 从待调度队列中删除这个元素
            sched_class_dequeue(next);
        }
        // 发现已经没有可以被调度的就绪进程，则运行第0号进程
        if (next == NULL) {
            next = idleproc;
        }
        // 记录被调度次数
        next->runs ++;
        // 执行调度
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
