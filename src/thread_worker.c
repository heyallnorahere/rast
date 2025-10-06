#include "thread_worker.h"

#include "mem.h"

#include <glib.h>

#include <stdbool.h>

typedef struct thread_worker {
    GThreadPool* pool;

    thread_worker_func callback;
    void* user_data;
} thread_worker_t;

static void thread_worker_entrypoint(gpointer user_data, gpointer job) {
    thread_worker_t* worker = user_data;
    worker->callback(worker->user_data, job);
}

thread_worker_t* thread_worker_start(thread_worker_func callback, void* user_data) {
    thread_worker_t* worker = mem_alloc(sizeof(thread_worker_t));
    worker->callback = callback;
    worker->user_data = NULL;

    GError* error = NULL;
    guint num_processors = g_get_num_processors();

    worker->pool =
        g_thread_pool_new(thread_worker_entrypoint, worker, (gint)num_processors, FALSE, &error);

    if (!worker->pool) {
        mem_free(worker);
        return NULL;
    }

    return worker;
}

void thread_worker_stop(thread_worker_t* worker) {
    if (!worker) {
        return;
    }

    g_thread_pool_free(worker->pool, TRUE, TRUE);
    mem_free(worker);
}

void thread_worker_wait_idle(const thread_worker_t* worker) {
    while (true) {
        guint unprocessed = g_thread_pool_unprocessed(worker->pool);
        if (unprocessed == 0) {
            break;
        }

        // 0.1ms
        g_usleep(100);
    }
}

void thread_worker_push_job(const thread_worker_t* worker, void* job) {
    g_thread_pool_push(worker->pool, job, NULL);
}
