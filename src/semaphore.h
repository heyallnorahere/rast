#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

#include <stdint.h>

typedef struct semaphore semaphore_t;

semaphore_t* semaphore_create();
void semaphore_destroy(semaphore_t* semaphore);

void semaphore_signal(semaphore_t* semaphore);
void semaphore_wait_for_value(semaphore_t* semaphore, uint64_t target);

#define semaphore_wait(semaphore) semaphore_wait_for_value(semaphore, 1)

#endif
