#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>

#define BUDDY_MAX_DEPTH 30
static unsigned int* buddy_page; // 存当前段内最长可供分配的连续内存块大小
static unsigned int buddy_page_num; // 存储buddy本身需要的页
static unsigned int max_pages; // buddy储存的页，叶节点数
static struct Page* buddy_allocatable_base; // 页首地址

#define max(a, b) ((a) > (b) ? (a) : (b))

/*
zkw线段树有一些很好的性质
0  1         0       1
 2   3   ->     10       11
4 5 6 7      100  101 110  111
下标为 n
子节点为 n<<1 和 n<<1|1
父节点为 n>>1

init:
       8
   4       4
 2   2   2   2
1 1 1 1 1 1 1 1

allocate/free:
       4
   2       4
 0   2   2   2
0 0 1 1 1 1 1 1

       4
   1       4
 0   1   2   2
0 0 0 1 1 1 1 1
*/

static void
buddy_init(void) {}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    
    // max_pages为总页数n向下取最近的2的幂
    // 此外还需要额外预留 max_pages*4B*2/4096B 个页存二叉树
    // 其中4B为一个unsigned int的大小，4096B为一个页的大小
    // 乘2是因为二叉树的节点接近其叶节点的两倍
    max_pages = 1;
    for (int i = 1; i < BUDDY_MAX_DEPTH; ++i, max_pages <<= 1)
        if (max_pages + (max_pages >> 9) >= n)
            break;
    max_pages >>= 1;
    buddy_page_num = (max_pages >> 9) + 1;
    cprintf("buddy init: total %d, use %d, free %d\n", n, buddy_page_num, max_pages);
    
    // 将数据结构本身，即前buddy_page_num个页设置为reserved
    for (int i = 0; i < buddy_page_num; ++i)
        SetPageReserved(base + i);
    
    // 将之后的max_pages个页设置为property
    buddy_allocatable_base = base + buddy_page_num;
    for (struct Page *p = buddy_allocatable_base; p != base + n; ++p) {
        ClearPageReserved(p);
        SetPageProperty(p);
        set_page_ref(p, 0);
    }
    
    // 初始化buddy_page，叶节点置1，每个节点是子节点乘2 
    buddy_page = (unsigned int*)KADDR(page2pa(base));
    for (int i = max_pages; i < max_pages << 1; ++i)
        buddy_page[i] = 1;
    for (int i = max_pages - 1; i > 0; --i)
        buddy_page[i] = buddy_page[i << 1] << 1;
}

static struct
Page* buddy_alloc_pages(size_t n) {
    assert(n > 0);

    // 左子节点优先地、logn地搜索最紧的节点
    // index是该节点对应下标，size是相应大小
    if (n > buddy_page[1]) return NULL;
    unsigned int index = 1, size = max_pages;
    for (; size >= n; size >>= 1) {
        if (buddy_page[index << 1] >= n) index <<= 1;
        else if (buddy_page[index << 1 | 1] >= n) index = index << 1 | 1;
        else break;
    }
    buddy_page[index] = 0;

    // 从下标index开始分配size个页
    struct Page* new_page = buddy_allocatable_base + index * size - max_pages;
    for (struct Page* p = new_page; p != new_page + size; ++p) {
        set_page_ref(p, 0);
        ClearPageProperty(p);
    }

    // 反向更新父节点
    // 一旦某节点下的一个后代节点开始分配，其被更新为子节点的MAX而非SUM
    for (; (index >>= 1) > 0; )
        buddy_page[index] = max(buddy_page[index << 1], buddy_page[index << 1 | 1]);
    return new_page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);

    // 将base映射到buddy[index]上
    unsigned int index = (unsigned int)(base - buddy_allocatable_base) + max_pages;
    unsigned int size = 1;

    // free所有页
    for (struct Page *p = base; p != base + n; ++p) {
        assert(!PageReserved(p) && !PageProperty(p));
        SetPageProperty(p);
        set_page_ref(p, 0);
    }

    // 向上找到buddy_page[index] == 0的最高节点
    for (; buddy_page[index] > 0; index >>= 1, size <<= 1);

    // 向上调整buddy_page
    for (buddy_page[index] = size; size <<= 1, (index >>= 1) > 0;)
        buddy_page[index] =
            (buddy_page[index << 1] + buddy_page[index << 1 | 1] == size) ?
            size :
            max(buddy_page[index << 1], buddy_page[index << 1 | 1]);
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_page[1];
}

static void
buddy_check(void) {
    int all_pages = nr_free_pages();
    struct Page* p0, *p1, *p2, *p3;
    assert(alloc_pages(all_pages + 1) == NULL);

    p0 = alloc_pages(1);
    assert(p0 != NULL);
    p1 = alloc_pages(2);
    assert(p1 == p0 + 2);
    assert(!PageReserved(p0) && !PageProperty(p0));
    assert(!PageReserved(p1) && !PageProperty(p1));

    p2 = alloc_pages(1);
    assert(p2 == p0 + 1);
    p3 = alloc_pages(2);
    assert(p3 == p0 + 4);
    assert(!PageProperty(p3) && !PageProperty(p3 + 1) && PageProperty(p3 + 2));

    free_pages(p1, 2);
    assert(PageProperty(p1) && PageProperty(p1 + 1));
    assert(p1->ref == 0);

    free_pages(p0, 1);
    free_pages(p2, 1);

    p2 = alloc_pages(2);
    assert(p2 == p0);
    free_pages(p2, 2);
    assert((*(p2 + 1)).ref == 0);
    assert(nr_free_pages() == all_pages >> 1);

    free_pages(p3, 2);
    p1 = alloc_pages(129);
    free_pages(p1, 256);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};