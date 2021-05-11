#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include <mmu.h>

// 磁盘一个扇区的大小为512字节
#define SECTSIZE            512

// 一个物理页存储到磁盘上所需要的扇区个数8个
#define PAGE_NSECT          (PGSIZE / SECTSIZE)

// SWAP磁盘的磁盘号，这里是磁盘1
#define SWAP_DEV_NO         1

#endif /* !__KERN_FS_FS_H__ */

