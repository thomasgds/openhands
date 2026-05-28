#include "memory.h"
#include "kernel_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char s_heap[KMEM_HEAP_SIZE];
static size_t s_used = 0;
static bool s_use_internal = false;

void memory_init(void)
{
    s_use_internal = true;
    LOG_INFO("Memory initialized: heap=%d bytes, pool=%d bytes",
             KMEM_HEAP_SIZE, KMEM_POOL_SIZE);
}

void *kalloc(size_t size)
{
    if (!s_use_internal) return malloc(size);
    if (s_used + size > KMEM_HEAP_SIZE) return NULL;
    void *ptr = &s_heap[s_used];
    s_used += size;
    return ptr;
}

void kfree(void *ptr) { if (!s_use_internal) free(ptr); (void)ptr; }
void *kcalloc(size_t nmemb, size_t size) {
    void *p = kalloc(nmemb * size);
    if (p) memset(p, 0, nmemb * size);
    return p;
}
void *krealloc(void *ptr, size_t size) { (void)ptr; (void)size; return NULL; }
size_t kalloc_available(void) { return KMEM_HEAP_SIZE - s_used; }
