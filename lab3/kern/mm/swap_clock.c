#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <list.h>
/* *
 * PTE的组成是20位基址和12位标志位，定义在mmu.h，其中有用的标志位有
 * 
 * PTE_P 0x001 在内存中/不在内存中
 * PTE_A 0x020 访问位，被读过/未被读过
 * PTE_D 0x040 修改位，被写过/未被写过
 * GET_DIRTY_FLAG和GET_ACCESSED_FLAG用于取得相应位的值
 * CLEAR_ACCESSED_FLAG用于清除访问位
*/
#define GET_LIST_ENTRY_PTE(pgdir, le)  (get_pte((pgdir), le2page((le), pra_page_link)->pra_vaddr, 0))
#define GET_DIRTY_FLAG(pgdir, le)      (*GET_LIST_ENTRY_PTE((pgdir), (le)) & PTE_D)
#define GET_ACCESSED_FLAG(pgdir, le)   (*GET_LIST_ENTRY_PTE((pgdir), (le)) & PTE_A)
#define CLEAR_ACCESSED_FLAG(pgdir, le) do {\
    struct Page *page = le2page((le), pra_page_link);\
    pte_t *ptep = get_pte((pgdir), page->pra_vaddr, 0);\
    *ptep = *ptep & ~PTE_A;\
    tlb_invalidate((pgdir), page->pra_vaddr);\
} while (0)

static int
_clock_init_mm(struct mm_struct *mm)
{     
    mm->sm_priv = NULL;
    return 0;
}

/* *
 * 冷启动的时候直接插入
 * 链表满了之后，调用了一次缺页异常之后，说明物理内存分配完毕
 * 此时禁用该函数
*/
static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    // 头结点和待插入节点
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
    assert(entry != NULL);

    // 因为是循环链表，直接插在头结点之前
    if (head == NULL) {
        list_init(entry);
        mm->sm_priv = entry;
    } else {
        list_add_before(head, entry);
    }
    return 0;
}

/* *
 * 状态转移关系参考教材中关于增强的时钟置换算法的描述
 * 共有四种状态，对应被clock指针扫到时的三种情况
 * 
 * A=0, D=0: 该页可被替换，clock指针跳到链表下一项
 * A=0, D=1: 调用 swapfs_write 函数，将该页面写入交换分区，之后将D修改为0，clock指针跳到链表下一项
 * A=1, D=X: 将A修改为0，clock指针跳到链表下一项
*/
static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    assert(head != NULL);
    assert(in_tick==0);

    list_entry_t *selected = NULL, *p = head;
    // 遍历链表，找到第一个 <0,0>
    do {
        if (GET_ACCESSED_FLAG(mm->pgdir, p) == 0 && GET_DIRTY_FLAG(mm->pgdir, p) == 0) {
            selected = p;
            break;
        }
        p = list_next(p);
    } while (p != head);

    // 遍历链表，找到第一个 <0,1>，设为 <0,0>
    if (selected == NULL)
    {
        do {
            if (GET_ACCESSED_FLAG(mm->pgdir, p) == 0 && GET_DIRTY_FLAG(mm->pgdir, p)) {
                selected = p;
                break;
            }
            CLEAR_ACCESSED_FLAG(mm->pgdir, p);
            p = list_next(p);
        } while (p != head);
    }

    // 遍历链表，找到第一个 <0,0>
    if (selected == NULL)
    {
        do {
            if (GET_ACCESSED_FLAG(mm->pgdir, p) == 0 && GET_DIRTY_FLAG(mm->pgdir, p) == 0) {
                selected = p;
                break;
            }
            p = list_next(p);
        } while (p != head);
    }

    // 遍历链表，找到第一个 <0,1>
    if (selected == NULL)
    {
        do {
            if (GET_ACCESSED_FLAG(mm->pgdir, p) == 0 && GET_DIRTY_FLAG(mm->pgdir, p)) {
                selected = p;
                break;
            }
            p = list_next(p);
        } while (p != head);
    }
    
    // 把换掉的页逐出
    head = selected;
    if (list_empty(head)) {
        mm->sm_priv = NULL;
    } else {
        mm->sm_priv = list_next(head);
        list_del(head);
    }
    *ptr_page = le2page(head, pra_page_link);
    return 0;
}

static int
_clock_check_swap(void) {
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==4);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==4);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==4);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==4);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==5);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==6);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==6);
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==7);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==8);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==9);
    cprintf("write Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==9);
    cprintf("read Virt Page b in fifo_check_swap\n");
    assert(*(unsigned char *)0x2000 == 0x0b);
    assert(pgfault_num==10);
    cprintf("read Virt Page c in fifo_check_swap\n");
    assert(*(unsigned char *)0x3000 == 0x0c);
    assert(pgfault_num==11);
    cprintf("read Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    assert(pgfault_num==12);
    cprintf("read Virt Page d in fifo_check_swap\n");
    assert(*(unsigned char *)0x4000 == 0x0d);
    assert(pgfault_num==13);
    cprintf("read Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(*(unsigned char *)0x3000 == 0x0c);
    assert(*(unsigned char *)0x4000 == 0x0d);
    assert(*(unsigned char *)0x5000 == 0x0e);
    assert(*(unsigned char *)0x2000 == 0x0b);
    assert(pgfault_num==14);
    return 0;
}

static int
_clock_init(void)
{ return 0; }

static int
_clock_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{ return 0; }

static int
_clock_tick_event(struct mm_struct *mm)
{ return 0; }

struct swap_manager swap_manager_clock =
{
    .name            = "clock swap manager",
    .init            = &_clock_init,
    .init_mm         = &_clock_init_mm,
    .tick_event      = &_clock_tick_event,
    .map_swappable   = &_clock_map_swappable,
    .set_unswappable = &_clock_set_unswappable,
    .swap_out_victim = &_clock_swap_out_victim,
    .check_swap      = &_clock_check_swap,
};