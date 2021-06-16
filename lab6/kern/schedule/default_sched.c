#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

static void
RR_init(struct run_queue *rq) {
    list_init(&(rq->run_list));
    rq->proc_num = 0;
}

// 将待调度（就绪）进程放到待调度队列末尾
static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    // 将新加入的进程放到待调度队列的末尾
    list_add_before(&(rq->run_list), &(proc->run_link));
    // 如果发现进程的时间片为0，表明刚刚执行完一次时间片，但进程尚未结束，将其时间片重置，如果发现进程的时间片超过要求的最大时间片，则将其时间片置成最大时间片。
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    // 标记进程在哪个队列中
    proc->rq = rq;
    // 待调度队列的进程数量加1
    rq->proc_num ++;
}

// 将某个进程从待调度队列中删除
static void
RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));
    rq->proc_num --;
}

// 获得下一个可以被调度运行的进程，此处直接返回了队头元素
static struct proc_struct *
RR_pick_next(struct run_queue *rq) {
    // 获得队头元素
    list_entry_t *le = list_next(&(rq->run_list));
    // 检查队列是否为空
    if (le != &(rq->run_list)) {
        // 不为空则执行返回
        return le2proc(le, run_link);
    }
    return NULL;
}

// 减少进程时间片
static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    // 减少时间片
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    // 发现此进程时间片已经被用完，则准备发起调度
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}

// 默认时间片轮转调度器
struct sched_class default_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};

