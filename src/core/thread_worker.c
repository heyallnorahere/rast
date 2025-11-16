#include "thread_worker.h"

#include "core/mem.h"
#include "core/list.h"

#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>

struct worker_thread {
    pthread_t id;
    thread_worker_t* worker;
};

typedef struct thread_worker {
    pthread_mutex_t mutex;

    pthread_cond_t new_job;
    struct list jobs;

    uint32_t thread_count;
    struct worker_thread* threads;

    pthread_cond_t thread_stopped;
    bool should_stop;
    uint32_t stopped_threads;

    thread_worker_func callback;
    void* user_data;
} thread_worker_t;

static void* worker_thread_routine(void* user_data) {
    struct worker_thread* thread = user_data;

    while (true) {
        pthread_mutex_lock(&thread->worker->mutex);

        while (!thread->worker->jobs.head) {
            if (thread->worker->should_stop) {
                break;
            }

            pthread_cond_wait(&thread->worker->new_job, &thread->worker->mutex);
        }

        if (thread->worker->should_stop) {
            thread->worker->stopped_threads++;
            pthread_mutex_unlock(&thread->worker->mutex);

            pthread_cond_signal(&thread->worker->thread_stopped);
            break;
        }

        struct list_node* head = thread->worker->jobs.head;
        void* job = head->data;
        list_remove(&thread->worker->jobs, head);

        pthread_mutex_unlock(&thread->worker->mutex);
        thread->worker->callback(thread->worker->user_data, job);
    }

    return NULL;
}

thread_worker_t* thread_worker_start(thread_worker_func callback, void* user_data) {
    thread_worker_t* worker = mem_alloc(sizeof(thread_worker_t));
    pthread_mutex_init(&worker->mutex, NULL);

    worker->callback = callback;
    worker->user_data = user_data;

    pthread_cond_init(&worker->new_job, NULL);
    list_init(&worker->jobs);

    worker->thread_count = sysconf(_SC_NPROCESSORS_ONLN);
    worker->threads = mem_alloc(sizeof(struct worker_thread) * worker->thread_count);

    pthread_cond_init(&worker->thread_stopped, NULL);
    worker->should_stop = false;
    worker->stopped_threads = 0;

    for (uint32_t i = 0; i < worker->thread_count; i++) {
        struct worker_thread* thread = &worker->threads[i];

        thread->worker = worker;
        pthread_create(&thread->id, NULL, worker_thread_routine, thread);
    }

    return worker;
}

void thread_worker_stop(thread_worker_t* worker) {
    if (!worker) {
        return;
    }

    pthread_mutex_lock(&worker->mutex);
    worker->should_stop = true;

    while (worker->stopped_threads < worker->thread_count) {
        // we do this so that waiting threads arent left hanging
        pthread_cond_signal(&worker->new_job);

        pthread_cond_wait(&worker->thread_stopped, &worker->mutex);
    }

    pthread_mutex_unlock(&worker->mutex);

    pthread_mutex_destroy(&worker->mutex);
    pthread_cond_destroy(&worker->new_job);
    pthread_cond_destroy(&worker->thread_stopped);

    // shouldnt do anything but just to be safe ig
    list_free(&worker->jobs);

    mem_free(worker->threads);
    mem_free(worker);
}

uint32_t thread_worker_get_thread_count(const thread_worker_t* worker) {
    return worker->thread_count;
}

void thread_worker_push_job(thread_worker_t* worker, void* job) {
    pthread_mutex_lock(&worker->mutex);
    list_append(&worker->jobs, job);
    pthread_mutex_unlock(&worker->mutex);

    pthread_cond_signal(&worker->new_job);
}
