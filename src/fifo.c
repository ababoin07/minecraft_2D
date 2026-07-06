#include "includes.h"
#include "fifo.h"
#include "world.h"

struct FifoWorldGen* FIFO_world_gen_create() {
    struct FifoWorldGen* ptr = malloc(sizeof(struct FifoWorldGen));
    if (NULL == ptr) return NULL;
    ptr->start = 0;
    ptr->end = 0;
    ptr->capacity = FIFO_WORLD_GEN_SIZE;

}

void FIFO_world_gen_destroy(struct FifoWorldGen* ptr) {
    free(ptr);
}

void request_gen(int64_t x, int64_t y, struct FifoWorldGen* fifo) {
    uint32_t tmp = fifo->start;
    while (tmp != fifo->end) {
        if (fifo->x[tmp] == x && fifo->y[tmp] == y) return;
        tmp = (tmp + 1) & (FIFO_WORLD_GEN_SIZE - 1);
    }
    fifo->x[fifo->end] = x;
    fifo->y[fifo->end] = y;
    fifo->end = (fifo->end + 1) & (FIFO_WORLD_GEN_SIZE - 1);
}


