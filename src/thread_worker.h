#ifndef MT_WORKER_H_
#define MT_WORKER_H_

typedef struct thread_worker thread_worker_t;

typedef void (*thread_worker_func)(void* user_data, void* job);

thread_worker_t* thread_worker_start(thread_worker_func callback, void* user_data);
void thread_worker_stop(thread_worker_t* worker);

void thread_worker_wait_idle(const thread_worker_t* worker);

void thread_worker_push_job(const thread_worker_t* worker, void* job);

#endif
