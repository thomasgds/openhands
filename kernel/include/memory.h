#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KMEM_POOL_SIZE (32 * 1024)
#define KMEM_HEAP_SIZE (512 * 1024)

void memory_init(void);
void *kalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
size_t kalloc_available(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_H */
