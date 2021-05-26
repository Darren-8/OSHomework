#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <pmm.h>
#include <list.h>

#define CACHE_NAMELEN 16

// 每种对象由cache（可以理解为仓库）进行统一管理
struct kmem_cache_t {
    // Linux 的slab 可有三种状态：全满、部分空闲、全空
    // slab分配器首先从部分空闲的slab进行分配，如没有，则从物理连续页上分配新的slab，并把它赋给一个cache，然后再从新slab分配空间
    list_entry_t slabs_full;	                        // 全满Slab链表
    list_entry_t slabs_partial;                         // 部分空闲Slab链表
    list_entry_t slabs_free;                            // 全空闲Slab链表
    uint16_t objsize;		                            // 对象大小
    uint16_t num;                                   	// 每个Slab保存的对象数目
    // 这里由于限制Slab大小为一页，所以数据对象和每页对象数据不会超过4096=2^12，所以使用16位整数
    void (*ctor)(void*, struct kmem_cache_t *, size_t); // 构造函数
    void (*dtor)(void*, struct kmem_cache_t *, size_t); // 析构函数
    char name[CACHE_NAMELEN];                       	// cache名称
    list_entry_t cache_link;	                        // cache链表，方便遍历
};

struct kmem_cache_t *
kmem_cache_create(const char *name, size_t size,
                       void (*ctor)(void*, struct kmem_cache_t *, size_t),
                       void (*dtor)(void*, struct kmem_cache_t *, size_t));
void kmem_cache_destroy(struct kmem_cache_t *cachep);
void *kmem_cache_alloc(struct kmem_cache_t *cachep);
void *kmem_cache_zalloc(struct kmem_cache_t *cachep);
void kmem_cache_free(struct kmem_cache_t *cachep, void *objp);
size_t kmem_cache_size(struct kmem_cache_t *cachep);
const char *kmem_cache_name(struct kmem_cache_t *cachep);
int kmem_cache_shrink(struct kmem_cache_t *cachep);
int kmem_cache_reap();
void *kmalloc(size_t size);
void kfree(void *objp);
size_t ksize(void *objp);

void kmem_int();

#endif /* ! __KERN_MM_SLUB_H__ */