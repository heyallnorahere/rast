#ifndef MEM_H_
#define MEM_H_

#include <stddef.h>

void* mem_alloc(size_t size);
void* mem_calloc(size_t nmemb, size_t size);
void* mem_realloc(void* mem, size_t size);
void mem_free(void* block);

#endif
