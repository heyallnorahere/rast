#include "semaphore.h"

#include "core/mem.h"

#include <pthread.h>

struct semaphore {
    uint64_t value;
    pthread_mutex_t mutex;
    pthread_cond_t signal;
};

semaphore_t* semaphore_create() {
    semaphore_t* semaphore = mem_alloc(sizeof(semaphore_t));
    semaphore->value = 0;

    pthread_mutex_init(&semaphore->mutex, NULL);
    pthread_cond_init(&semaphore->signal, NULL);

    return semaphore;
}

void semaphore_destroy(semaphore_t* semaphore) {
    if (!semaphore) {
        return;
    }

    pthread_mutex_destroy(&semaphore->mutex);
    pthread_cond_destroy(&semaphore->signal);

    mem_free(semaphore);
}

void semaphore_signal(semaphore_t *semaphore) {
    pthread_mutex_lock(&semaphore->mutex);
    semaphore->value++;
    pthread_mutex_unlock(&semaphore->mutex);

    pthread_cond_signal(&semaphore->signal);
}

void semaphore_wait_for_value(semaphore_t *semaphore, uint64_t target) {
    pthread_mutex_lock(&semaphore->mutex);

    while (semaphore->value < target) {
        pthread_cond_wait(&semaphore->signal, &semaphore->mutex);
    }

    semaphore->value -= target;
    pthread_mutex_unlock(&semaphore->mutex);
}
