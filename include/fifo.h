#ifndef FIFO_H
#define FIFO_H

#include "includes.h"
#include "world.h"
#include <pthread.h>

struct FifoWorldGen {
    int64_t x[FIFO_WORLD_GEN_SIZE];
    int64_t y[FIFO_WORLD_GEN_SIZE];
    uint32_t start, end, capacity;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

fifo_world_gen_t* FIFO_world_gen_create();
void FIFO_world_gen_destroy(fifo_world_gen_t*);
void request_gen(int64_t, int64_t, fifo_world_gen_t*);

#endif // FIFO_H