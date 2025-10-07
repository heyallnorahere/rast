#include "thread_worker.h"

#include "mem.h"

#include <glib.h>

typedef struct thread_worker {
    GThreadPool* pool;

    thread_worker_func callback;
    void* user_data;
} thread_worker_t;

static void thread_worker_job(gpointer job, gpointer user_data) {
    uint64_t* job_id = job;

    thread_worker_t* worker = user_data;
    worker->callback(worker->user_data, *job_id);

    mem_free(job_id);
}

thread_worker_t* thread_worker_start(thread_worker_func callback, void* user_data) {
    thread_worker_t* worker = mem_alloc(sizeof(thread_worker_t));
    worker->callback = callback;
    worker->user_data = user_data;

    guint num_processors = g_get_num_processors();
    worker->pool = g_thread_pool_new(thread_worker_job, worker, (gint)num_processors, TRUE, NULL);

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

    g_thread_pool_free(worker->pool, FALSE, TRUE);
    mem_free(worker);
}

void thread_worker_set_data(thread_worker_t* worker, void* user_data) {
    worker->user_data = user_data;
}

void thread_worker_push_job(const thread_worker_t* worker, uint64_t job) {
    uint64_t* job_id = mem_alloc(sizeof(uint64_t));
    *job_id = job;

    g_thread_pool_push(worker->pool, job_id, NULL);
}
