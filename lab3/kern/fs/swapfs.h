#ifndef __KERN_FS_SWAPFS_H__
#define __KERN_FS_SWAPFS_H__

#include <memlayout.h>
#include <swap.h>

//初始化磁盘
void swapfs_init(void);

//从磁盘上读内存页
int swapfs_read(swap_entry_t entry, struct Page *page);

//把内存页写到磁盘上
int swapfs_write(swap_entry_t entry, struct Page *page);

#endif /* !__KERN_FS_SWAPFS_H__ */

