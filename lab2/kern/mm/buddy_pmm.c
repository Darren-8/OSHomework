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
要进行测试，需要作两处修改：
kern/mm/pmm.c(153): pmm_manager = &default_pmm_manager;
tools/grade.sh(325): 'memory management: default_pmm_manager'

内存布局：
+0                          其他数据结构
+1                          buddy_page
+buddy_page_num             .
+buddy_page_num+1           buddy_allocatable_base
+buddy_page_num+max_pages   .
+buddy_page_num+max_pages+1 空闲

zkw线段树有一些很好的性质
   1                 1
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
    if(buddy_page[index << 1] + buddy_page[index << 1 | 1] == size){
        buddy_page[index] = size;
    } else {
        buddy_page[index] = max(buddy_page[index << 1], buddy_page[index << 1 | 1]);
    }
}

// 根节点就是空闲页数
static size_t
buddy_nr_free_pages(void) {
    return buddy_page[1];
}

static void
buddy_check(void) {
    int all_pages = nr_free_pages();                //    4
    assert(alloc_pages(all_pages + 1) == NULL);     //  2   2
    struct Page* pa, *pb, *pc, *pd;                 // 1 1 1 1

    pa = alloc_pages(1);                            //    2
    assert(pa != NULL);                             //  1   2
    assert(!PageReserved(pa) && !PageProperty(pa)); // a 1 1 1

    pb = alloc_pages(2);                            //    1
    assert(pb == pa + 2);                           //  1   0
    assert(!PageReserved(pb) && !PageProperty(pb)); // a 1 b .

    pc = alloc_pages(1);                            //    0
    assert(pc == pa + 1);                           //  0   0
                                                    // a c b .

    pd = alloc_pages(2);                                //        2
    assert(pd == pa + 4);                               //    0       2
    assert(!PageProperty(pd) &&                         //  0   0   0   2
        !PageProperty(pd + 1) && PageProperty(pd + 2)); // a c b . d . 1 1
    
    free_pages(pb, 2);                                  //        4
    assert(PageProperty(pb) && PageProperty(pb + 1));   //    2       2
    assert(pb->ref == 0);                               //  0   2   0   2
    // pb still here                                    // a c b 1 d . 1 1

    free_pages(pa, 1);  //        4
    free_pages(pc, 1);  //    4       2
                        //  2   2   0   2
                        // a c b 1 d . 1 1

    pc = alloc_pages(2);                        //         2
    assert(pc == pa);                           //     2       2
                                                //   0   2   0   2
                                                // ac . b 1 d . 1 1

    free_pages(pc, 2);                          //         4
    assert((*(pc + 1)).ref == 0);               //     4       2
                                                //   2   2   0   2
                                                // ac 1 b 1 d . 1 1

    free_pages(pd, 2); // all clear
    pb = alloc_pages(129);
    assert(!PageProperty(pb) && !PageProperty(pb + 200));
    free_pages(pb, 256);
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