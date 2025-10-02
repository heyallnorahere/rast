#include "mem.h"

#include <glib.h>

void* mem_alloc(size_t size) {
    // todo: log?
    return g_malloc(size);
}

void* mem_calloc(size_t nmemb, size_t size) {
    // todo: log?

    return g_malloc0_n(nmemb, size);
}

void* mem_realloc(void* mem, size_t size) {
    // todo: log?
    return g_realloc(mem, size);
}

void mem_free(void* block) {
    // todo: log?
    return g_free(block);
}
