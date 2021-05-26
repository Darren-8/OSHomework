#include <slub.h>
#include <list.h>
#include <defs.h>
#include <string.h>
#include <stdio.h>

// slab分配算法采用cache存储内核对象。当创建cache时，起初包括若干标记为空闲的对象
// 对象的数量与slab的大小有关。当需要内核数据结构的对象时，可以直接从cache上直接获取，并将对象初始化为使用
// Slab对应的内存页分为两部分
// 保存空闲信息的bufcnt以及可用内存区域buf[len(obj)]

struct slab_t {
    int ref;                        // 页的引用次数（保留）
    struct kmem_cache_t *cachep;    // cache对象指针
    uint16_t inuse;                 // 已经分配对象数目
    uint16_t free;                  // 下一个空闲对象偏移量
    list_entry_t slab_link;         // Slab链表
};

// The number of sized cache : 16, 32, 64, 128, 256, 512, 1024, 2048
#define SIZED_CACHE_NUM     8
#define SIZED_CACHE_MIN     16
#define SIZED_CACHE_MAX     2048

#define le2slab(le,link)    ((struct slab_t*)le2page((struct Page*)le,link))
#define slab2kva(slab)      (page2kva((struct Page*)slab))

static list_entry_t cache_chain;
static struct kmem_cache_t cache_cache;
static struct kmem_cache_t *sized_caches[SIZED_CACHE_NUM];
static char *cache_cache_name = "cache";
static char *sized_cache_name = "sized";

// 申请一页内存，初始化空闲链表bufctl，构造buf中的对象
// 更新Slab元数据，最后将新的Slab加入到仓库的空闲Slab表中
static void *
kmem_cache_grow(struct kmem_cache_t *cachep) {
    struct Page *page = alloc_page();
    void *kva = page2kva(page);
    // Init slub meta data
    struct slab_t *slab = (struct slab_t *) page;
    slab->cachep = cachep;
    slab->inuse = slab->free = 0;
    list_add(&(cachep->slabs_free), &(slab->slab_link));
    // Init bufctl
    int16_t *bufctl = kva;
    for (int i = 1; i < cachep->num; i++)
        bufctl[i-1] = i;
    bufctl[cachep->num-1] = -1;
    // Init cache 
    void *buf = bufctl + cachep->num;
    if (cachep->ctor) 
        for (void *p = buf; p < buf + cachep->objsize * cachep->num; p += cachep->objsize)
            cachep->ctor(p, cachep, cachep->objsize);
    return slab;
}

// 析构buf中的对象后将内存页归还
static void
kmem_slab_destroy(struct kmem_cache_t *cachep, struct slab_t *slab) {
    // Destruct cache
    struct Page *page = (struct Page *) slab;
    int16_t *bufctl = page2kva(page);
    void *buf = bufctl + cachep->num;
    if (cachep->dtor)
        for (void *p = buf; p < buf + cachep->objsize * cachep->num; p += cachep->objsize)
            cachep->dtor(p, cachep, cachep->objsize);
    // Return slub page 
    page->property = page->flags = 0;
    list_del(&(page->page_link));
    free_page(page);
}

static int 
kmem_sized_index(size_t size) {
    // Round up 
    size_t rsize = ROUNDUP(size, 2);
    if (rsize < SIZED_CACHE_MIN)
        rsize = SIZED_CACHE_MIN;
    // Find index
    int index = 0;
    for (int t = rsize / 32; t; t /= 2)
        index ++;
    return index;
}

// ! Test code
#define TEST_OBJECT_LENTH 2046
#define TEST_OBJECT_CTVAL 0x22
#define TEST_OBJECT_DTVAL 0x11

static const char *test_object_name = "test";

struct test_object {
    char test_member[TEST_OBJECT_LENTH];
};

static void
test_ctor(void* objp, struct kmem_cache_t * cachep, size_t size) {
    char *p = objp;
    for (int i = 0; i < size; i++)
        p[i] = TEST_OBJECT_CTVAL;
}

static void
test_dtor(void* objp, struct kmem_cache_t * cachep, size_t size) {
    char *p = objp;
    for (int i = 0; i < size; i++)
        p[i] = TEST_OBJECT_DTVAL;
}

static size_t 
list_length(list_entry_t *listelm) {
    size_t len = 0;
    list_entry_t *le = listelm;
    while ((le = list_next(le)) != listelm)
        len ++;
    return len;
}

static void 
check_kmem() {

    assert(sizeof(struct Page) == sizeof(struct slab_t));

    size_t fp = nr_free_pages();

    // Create a cache 
    struct kmem_cache_t *cp0 = kmem_cache_create(test_object_name, sizeof(struct test_object), test_ctor, test_dtor);
    assert(cp0 != NULL);
    assert(kmem_cache_size(cp0) == sizeof(struct test_object));
    assert(strcmp(kmem_cache_name(cp0), test_object_name) == 0);
    // Allocate six objects
    struct test_object *p0, *p1, *p2, *p3, *p4, *p5;
    char *p;
    assert((p0 = kmem_cache_alloc(cp0)) != NULL);
    assert((p1 = kmem_cache_alloc(cp0)) != NULL);
    assert((p2 = kmem_cache_alloc(cp0)) != NULL);
    assert((p3 = kmem_cache_alloc(cp0)) != NULL);
    assert((p4 = kmem_cache_alloc(cp0)) != NULL);
    p = (char *) p4;
    for (int i = 0; i < sizeof(struct test_object); i++)
        assert(p[i] == TEST_OBJECT_CTVAL);
    assert((p5 = kmem_cache_zalloc(cp0)) != NULL);
    p = (char *) p5;
    for (int i = 0; i < sizeof(struct test_object); i++)
        assert(p[i] == 0);
    assert(nr_free_pages()+3 == fp);
    assert(list_empty(&(cp0->slabs_free)));
    assert(list_empty(&(cp0->slabs_partial)));
    assert(list_length(&(cp0->slabs_full)) == 3);
    // Free three objects 
    kmem_cache_free(cp0, p3);
    kmem_cache_free(cp0, p4);
    kmem_cache_free(cp0, p5);
    assert(list_length(&(cp0->slabs_free)) == 1);
    assert(list_length(&(cp0->slabs_partial)) == 1);
    assert(list_length(&(cp0->slabs_full)) == 1);
    // Shrink cache 
    assert(kmem_cache_shrink(cp0) == 1);
    assert(nr_free_pages()+2 == fp);
    assert(list_empty(&(cp0->slabs_free)));
    p = (char *) p4;
    for (int i = 0; i < sizeof(struct test_object); i++)
        assert(p[i] == TEST_OBJECT_DTVAL);
    // Reap cache 
    kmem_cache_free(cp0, p0);
    kmem_cache_free(cp0, p1);
    kmem_cache_free(cp0, p2);
    assert(kmem_cache_reap() == 2);
    assert(nr_free_pages() == fp);
    // Destory a cache 
    kmem_cache_destroy(cp0);

    // Sized alloc 
    assert((p0 = kmalloc(2048)) != NULL);
    assert(nr_free_pages()+1 == fp);
    kfree(p0);
    assert(kmem_cache_reap() == 1);
    assert(nr_free_pages() == fp);

    cprintf("check_kmem() succeeded!\n");

}
// ! End of test code

// 从kmem_cache_t中获得一个对象，初始化成员，最后将对象加入cache链表
// 由于空闲表每一项占用2字节，所以每个Slab的对象数目就是：4096字节/(2字节+对象大小)
struct kmem_cache_t *
kmem_cache_create(const char *name, size_t size,
                       void (*ctor)(void*, struct kmem_cache_t *, size_t),
                       void (*dtor)(void*, struct kmem_cache_t *, size_t)) {
    assert(size <= (PGSIZE - 2));
    struct kmem_cache_t *cachep = kmem_cache_alloc(&(cache_cache));
    if (cachep != NULL) {
        cachep->objsize = size;
        cachep->num = PGSIZE / (sizeof(int16_t) + size);
        cachep->ctor = ctor;
        cachep->dtor = dtor;
        memcpy(cachep->name, name, CACHE_NAMELEN);
        list_init(&(cachep->slabs_full));
        list_init(&(cachep->slabs_partial));
        list_init(&(cachep->slabs_free));
        list_add(&(cache_chain), &(cachep->cache_link));
    }
    return cachep;
}

// 释放cache中所有的Slab，释放kmem_cache_t
void 
kmem_cache_destroy(struct kmem_cache_t *cachep) {
    list_entry_t *head, *le;
    // Destory full slabs
    head = &(cachep->slabs_full);
    le = list_next(head);
    while (le != head) {
        list_entry_t *temp = le;
        le = list_next(le);
        kmem_slab_destroy(cachep, le2slab(temp, page_link));
    }
    // Destory partial slabs 
    head = &(cachep->slabs_partial);
    le = list_next(head);
    while (le != head) {
        list_entry_t *temp = le;
        le = list_next(le);
        kmem_slab_destroy(cachep, le2slab(temp, page_link));
    }
    // Destory free slabs 
    head = &(cachep->slabs_free);
    le = list_next(head);
    while (le != head) {
        list_entry_t *temp = le;
        le = list_next(le);
        kmem_slab_destroy(cachep, le2slab(temp, page_link));
    }
    // Free kmem_cache 
    kmem_cache_free(&(cache_cache), cachep);
}   

// 先查找slabs_partial，如果没找到空闲区域则查找slabs_free，还是没找到就申请一个新的slab
// 从slab分配一个对象后，如果slab变满，那么将slab加入slabs_full
void *
kmem_cache_alloc(struct kmem_cache_t *cachep) {
    list_entry_t *le = NULL;
    // Find in partial list 
    if (!list_empty(&(cachep->slabs_partial)))
        le = list_next(&(cachep->slabs_partial));
    // Find in empty list 
    else {
        if (list_empty(&(cachep->slabs_free)) && kmem_cache_grow(cachep) == NULL)
            return NULL;
        le = list_next(&(cachep->slabs_free));
    }
    // Alloc 
    list_del(le);
    struct slab_t *slab = le2slab(le, page_link);
    void *kva = slab2kva(slab);
    int16_t *bufctl = kva;
    void *buf = bufctl + cachep->num;
    void *objp = buf + slab->free * cachep->objsize;
    // Update slab
    slab->inuse ++;
    slab->free = bufctl[slab->free];
    if (slab->inuse == cachep->num)
        list_add(&(cachep->slabs_full), le);
    else 
        list_add(&(cachep->slabs_partial), le);
    return objp;
}

// 使用kmem_cache_alloc分配一个对象之后将对象内存区域初始化为零
void *
kmem_cache_zalloc(struct kmem_cache_t *cachep) {
    void *objp = kmem_cache_alloc(cachep);
    memset(objp, 0, cachep->objsize);
    return objp;
}

// 将对象从Slab中释放，也就是将对象空间加入空闲链表，更新Slab元信息
// 如果Slab变空，那么将Slab加入slabs_partial链表
void 
kmem_cache_free(struct kmem_cache_t *cachep, void *objp) {
    // Get slab of object 
    void *base = page2kva(pages);
    void *kva = ROUNDDOWN(objp, PGSIZE);
    struct slab_t *slab = (struct slab_t *) &pages[(kva-base)/PGSIZE];
    // Get offset in slab
    int16_t *bufctl = kva;
    void *buf = bufctl + cachep->num;
    int offset = (objp - buf) / cachep->objsize;
    // Update slab 
    list_del(&(slab->slab_link));
    bufctl[offset] = slab->free;
    slab->inuse --;
    slab->free = offset;
    if (slab->inuse == 0)
        list_add(&(cachep->slabs_free), &(slab->slab_link));
    else 
        list_add(&(cachep->slabs_partial), &(slab->slab_link));
}

// 获得仓库中对象的大小
size_t 
kmem_cache_size(struct kmem_cache_t *cachep) {
    return cachep->objsize;
}

// 获得cache的名称
const char *
kmem_cache_name(struct kmem_cache_t *cachep) {
    return cachep->name;
}

// 将cache中slabs_free中所有Slab释放
int 
kmem_cache_shrink(struct kmem_cache_t *cachep) {
    int count = 0;
    list_entry_t *le = list_next(&(cachep->slabs_free));
    while (le != &(cachep->slabs_free)) {
        list_entry_t *temp = le;
        le = list_next(le);
        kmem_slab_destroy(cachep, le2slab(temp, page_link));
        count ++;
    }
    return count;
}

// 遍历cache链表，对每一个cache进行kmem_cache_shrink操作
int 
kmem_cache_reap() {
    int count = 0;
    list_entry_t *le = &(cache_chain);
    while ((le = list_next(le)) != &(cache_chain))
        count += kmem_cache_shrink(to_struct(le, struct kmem_cache_t, cache_link));
    return count;
}

// 找到大小最合适的内置cache，申请一个对象
void *
kmalloc(size_t size) {
    assert(size <= SIZED_CACHE_MAX);
    return kmem_cache_alloc(sized_caches[kmem_sized_index(size)]);
}

// 释放内置cache对象
void 
kfree(void *objp) {
    void *base = slab2kva(pages);
    void *kva = ROUNDDOWN(objp, PGSIZE);
    struct slab_t *slab = (struct slab_t *) &pages[(kva-base)/PGSIZE];
    kmem_cache_free(slab->cachep, objp);
}

void
kmem_int() {

    // 初始化kmem_cache_t
    cache_cache.objsize = sizeof(struct kmem_cache_t);
    cache_cache.num = PGSIZE / (sizeof(int16_t) + sizeof(struct kmem_cache_t));
    cache_cache.ctor = NULL;
    cache_cache.dtor = NULL;
    memcpy(cache_cache.name, cache_cache_name, CACHE_NAMELEN);
    list_init(&(cache_cache.slabs_full));
    list_init(&(cache_cache.slabs_partial));
    list_init(&(cache_cache.slabs_free));
    list_init(&(cache_chain));
    list_add(&(cache_chain), &(cache_cache.cache_link));

    // 初始化8个固定大小的内置cache
    for (int i = 0, size = 16; i < SIZED_CACHE_NUM; i++, size <<= 1)
        sized_caches[i] = kmem_cache_create(sized_cache_name, size, NULL, NULL); 

    check_kmem();
}