#ifndef FIFO_H
#define FIFO_H

#include "includes.h"
#include "world.h"

struct FifoWorldGen {
    int64_t x[FIFO_WORLD_GEN_SIZE];
    int64_t y[FIFO_WORLD_GEN_SIZE];
    uint32_t start, end, capacity;
};

struct FifoWorldGen* FIFO_world_gen_create();
void FIFO_world_gen_destroy(struct FifoWorldGen*);
void request_gen(int64_t, int64_t, struct FifoWorldGen*);

#endif // FIFO_H