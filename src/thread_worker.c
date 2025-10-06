#include "thread_worker.h"

#include "mem.h"

#include <glib.h>

#include <stdbool.h>

typedef struct thread_worker {
    GThread* thread;
    GAsyncQueue* jobs;

    thread_worker_func callback;
    thread_worker_data_free data_free;
    void* user_data;

    gint stop;
    bool active;
} thread_worker_t;

static gpointer thread_worker_entrypoint(gpointer user_data) {
    thread_worker_t* worker = user_data;

    while (true) {
        worker->active = false;

        gint stop = g_atomic_int_get(&worker->stop);
        if (stop != 0) {
            break;
        }

        void* job = g_async_queue_pop(worker->jobs);

        worker->active = true;
        worker->callback(worker->user_data, job);
    }

    return NULL;
}

thread_worker_t* thread_worker_start(thread_worker_func callback,
                                     thread_worker_data_free data_free) {
    thread_worker_t* worker = mem_alloc(sizeof(thread_worker_t));
    worker->jobs = g_async_queue_new();
    worker->callback = callback;
    worker->data_free = data_free;
    worker->user_data = NULL;
    worker->stop = 0;

    // start the thread AFTER we set remaining context
    worker->thread = g_thread_new("thread worker", NULL, worker);

    return worker;
}

void thread_worker_stop(thread_worker_t* worker) {
    if (!worker) {
        return;
    }

    g_atomic_int_exchange(&worker->stop, 1);
    g_thread_join(worker->thread);

    g_thread_unref(worker->thread);
    g_async_queue_unref(worker->jobs);

    if (worker->data_free) {
        worker->data_free(worker->user_data);
    }

    mem_free(worker);
}

void thread_worker_wait_idle(const thread_worker_t* worker) {
    while (true) {
        gint pending_jobs = g_async_queue_length(worker->jobs);
        bool working = worker->active || pending_jobs <= 0;

        if (!working) {
            break;
        }

        // 0.1ms
        g_usleep(100);
    }
}
