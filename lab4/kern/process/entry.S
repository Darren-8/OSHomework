.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)

    # 此处ebx存放着init_main的入口地址
    pushl %edx              # push arg
    call *%ebx              # call fn

    # 执行do_exit打印一句话所有程序结束
    pushl %eax              # save the return value of fn(arg)
    call do_exit            # call do_exit to terminate current thread