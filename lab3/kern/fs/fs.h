#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include <mmu.h>

// ����һ�������Ĵ�СΪ512�ֽ�
#define SECTSIZE            512

// һ������ҳ�洢������������Ҫ����������8��
#define PAGE_NSECT          (PGSIZE / SECTSIZE)

// SWAP���̵Ĵ��̺ţ������Ǵ���1
#define SWAP_DEV_NO         1

#endif /* !__KERN_FS_FS_H__ */

