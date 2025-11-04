#include "semaphore.h"

#include "mem.h"

#include <glib.h>

struct semaphore {
    uint64_t value;
    GMutex mutex;
    GCond signal;
};

semaphore_t* semaphore_create() {
    semaphore_t* semaphore = mem_alloc(sizeof(semaphore_t));
    semaphore->value = 0;

    g_mutex_init(&semaphore->mutex);
    g_cond_init(&semaphore->signal);

    return semaphore;
}

void semaphore_destroy(semaphore_t* semaphore) {
    if (!semaphore) {
        return;
    }

    g_mutex_clear(&semaphore->mutex);
    g_cond_clear(&semaphore->signal);

    mem_free(semaphore);
}

void semaphore_signal(semaphore_t *semaphore) {
    g_mutex_lock(&semaphore->mutex);
    semaphore->value++;
    g_mutex_unlock(&semaphore->mutex);

    g_cond_signal(&semaphore->signal);
}

void semaphore_wait_for_value(semaphore_t *semaphore, uint64_t target) {
    g_mutex_lock(&semaphore->mutex);

    while (semaphore->value < target) {
        g_cond_wait(&semaphore->signal, &semaphore->mutex);
    }

    semaphore->value -= target;
    g_mutex_unlock(&semaphore->mutex);
}
