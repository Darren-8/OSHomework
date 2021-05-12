#include <defs.h>
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
*/

list_entry_t list_head; // XII
list_entry_t *pointer; // 时钟指针
bool full; // 是否已经装满

static int
_clock_init_mm(struct mm_struct *mm)
{
    list_init(&list_head);
    pointer = mm->sm_priv = &list_head;
    full = 0;
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
    if (full) return 0;
    // 头结点和待插入节点
    list_entry_t* head = (list_entry_t*) (mm->sm_priv);
    list_entry_t* entry = &(page->pra_page_link);
    assert(entry != NULL && pointer != NULL);
    // 因为是循环链表，直接插在头结点之前
    list_add_before(head, entry);
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
    full = 1;
    assert(in_tick == 0);
    list_entry_t *head = mm->sm_priv, *next;
    assert(pointer != NULL);
    if (pointer == head) pointer = list_next(pointer);
    
    // 选择逐出页面
    for (; ; pointer = list_next(pointer)) {
        if (pointer == head) continue;
        struct Page* page = le2page(pointer, pra_page_link);
        uintptr_t va = page->pra_vaddr;
        pte_t *ptep = get_pte(mm->pgdir, va, 0);
        assert(*ptep & PTE_P);

        // 获取A和D并打印
        uintptr_t A = *ptep & PTE_A, D = *ptep & PTE_D;
        cprintf("visit vaddr: 0x%08x %c%c\n", page->pra_vaddr, A ? 'A' : '-', D ? 'D' : '-');
        if (A == 0 && D == 0) { // 00, 替换
            *ptr_page = page;
            break;
        } else {
            if (A == 0) { // 01 -> 00
                // (va / PGSIZE + 1) << 8 是对应PTE
                swapfs_write((va / PGSIZE + 1) << 8, page);
                cprintf("write page vaddr 0x%x to swap %d\n", va, va / PGSIZE + 1);
                *ptep &= ~PTE_D;
            } else { // 10, 11 -> 00, 01
                *ptep &= ~PTE_A;
            }
            tlb_invalidate(mm->pgdir, va);
        }
    }
    pointer = list_next(pointer);
    return 0;
}

/**
 * 以下样例来自课程PPT
 * 在此之前，check_content_set(void)里已经有了
 * 0x1000 : 0x0a
 * 0x1010 : 0x0a
 * 0x2000 : 0x0b
 * 0x2010 : 0x0b
 * 0x3000 : 0x0c
 * 0x3010 : 0x0c
 * 0x4000 : 0x0d
 * 0x4010 : 0x0d
 * 并为它们产生了四次缺页中断
*/
static int
_clock_check_swap(void) {
    // 清空所有 AD
    pde_t *pgdir = KADDR((pde_t*) rcr3());
    for (int i = 1; i <= 4; i++) {
        pte_t *ptep = get_pte(pgdir, i * 0x1000, 0);
        swapfs_write((i * 0x1000 / PGSIZE + 1) << 8, pte2page(*ptep));
        *ptep &= ~(PTE_A | PTE_D);
        tlb_invalidate(pgdir, i * 0x1000);
    }
    assert(pgfault_num == 4);

    cprintf("read Virt Page c in clock_check_swap\n");  // 1 A b c d
    assert(*(unsigned char *)0x3000 == 0x0c);           // A 0 0 1 0
    assert(pgfault_num == 4);                           // D 0 0 0 0

    cprintf("write Virt Page a in clock_check_swap\n"); // 2 A b c d
    assert(*(unsigned char *)0x1000 == 0x0a);           // A 1 0 1 0
    *(unsigned char *)0x1000 = 0x0a;                    // D 1 0 0 0
    assert(pgfault_num == 4);

    cprintf("read Virt Page d in clock_check_swap\n");  // 3 A b c d
    assert(*(unsigned char *)0x4000 == 0x0d);           // A 1 0 1 1
    assert(pgfault_num == 4);                           // D 1 0 0 0
    
    cprintf("write Virt Page b in clock_check_swap\n"); // 4 A b c d
    assert(*(unsigned char *)0x2000 == 0x0b);           // A 1 1 1 1
    *(unsigned char *)0x2000 = 0x0b;                    // D 1 1 0 0
    assert(pgfault_num == 4);
    
    cprintf("read Virt Page e in clock_check_swap\n");  // 5 a b e D
    unsigned e = *(unsigned char *)0x5000;              // A 0 1 1 0
    cprintf("e = 0x%04x\n", e);                         // D 0 0 0 0
    assert(pgfault_num == 5);
    
    cprintf("read Virt Page b in clock_check_swap\n");  // 6 a b e D
    assert(*(unsigned char *)0x2000 == 0x0b);           // A 0 1 1 0
    assert(pgfault_num == 5);                           // D 0 0 0 0
    
    cprintf("write Virt Page a in clock_check_swap\n"); // 7 a b e D
    assert(*(unsigned char *)0x1000 == 0x0a);           // A 1 1 1 0
    *(unsigned char *)0x1000 = 0x0a;                    // D 1 0 0 0
    assert(pgfault_num == 5);
    
    cprintf("read Virt Page b in clock_check_swap\n");  // 8 a b e D
    assert(*(unsigned char *)0x2000 == 0x0b);           // A 1 1 1 0
    assert(pgfault_num == 5);                           // D 1 0 0 0
    
    cprintf("read Virt Page c in clock_check_swap\n");  // 9 A b e c
    assert(*(unsigned char *)0x3000 == 0x0c);           // A 1 1 1 1
    assert(pgfault_num == 6);                           // D 1 0 0 0
    
    cprintf("read Virt Page d in clock_check_swap\n");  //10 a b E c
    assert(*(unsigned char *)0x4000 == 0x0d);           // A 0 1 0 0
    assert(pgfault_num == 7);                           // D 0 0 0 0
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