#include "mem.h"

#include <malloc.h>

void* mem_alloc(size_t size) {
    // todo: log?
    return malloc(size);
}

void* mem_calloc(size_t nmemb, size_t size) {
    // todo: log?

    return calloc(nmemb, size);
}

void* mem_realloc(void* mem, size_t size) {
    // todo: log?
    return realloc(mem, size);
}

void mem_free(void* block) {
    // todo: log?
    return free(block);
}
