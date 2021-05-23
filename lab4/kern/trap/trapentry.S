#include <memlayout.h>

# vectors.S sends all traps here.
.text
.globl __alltraps
__alltraps:
    # push registers to build a trap frame
    # therefore make the stack look like a struct trapframe
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    # load GD_KDATA into %ds and %es to set up data segments for kernel
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    # push %esp to pass a pointer to the trapframe as an argument to trap()
    pushl %esp

    # call trap(tf), where tf=%esp
    call trap

    # pop the pushed stack pointer
    popl %esp

    # return falls through to trapret...
.globl __trapret
__trapret:
    # restore registers from stack
    # 恢复所有通用寄存器
    popal

    # 设置段寄存器
    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    # 跳过tf_err的读取
    addl $0x8, %esp
    # 按照tf_eip的地址进行返回
    iret

# 这里是我们创建的内核进程的入口位置，他将会控制eip跳转到kernel_thread_entry的入口地址
.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    # 跳转到当前进程的进程trapframe，forkret将当前进程的进程trapframe的地址作为参数传入，此处加4是为了跳过返回地址
    movl 4(%esp), %esp
    jmp __trapret
