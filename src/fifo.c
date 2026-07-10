#include "includes.h"
#include "fifo.h"
#include "world.h"

/**
 * @brief Create and initialise a new FIFO.
 * @return Pointer to allocated FIFO, or NULL.
 */
fifo_world_gen_t *FIFO_world_gen_create()
{
    fifo_world_gen_t *ptr = malloc(sizeof(fifo_world_gen_t));
    if (NULL == ptr)
        return NULL;
    ptr->start = 0;
    ptr->end = 0;
    ptr->capacity = FIFO_WORLD_GEN_SIZE;
    pthread_mutex_init(&ptr->mutex, NULL);
    pthread_cond_init(&ptr->cond, NULL);
    return ptr;
}

/**
 * @brief Destroy a FIFO.
 */
void FIFO_world_gen_destroy(fifo_world_gen_t *ptr)
{
    pthread_mutex_destroy(&ptr->mutex);
    pthread_cond_destroy(&ptr->cond);
    free(ptr);
}

/**
 * @brief Request generation of a chunk; ignore duplicates.
 */
void request_gen(int64_t x, int64_t y, fifo_world_gen_t *fifo)
{
    pthread_mutex_lock(&fifo->mutex);
    uint32_t tmp = fifo->start;
    /* Check for duplicate request. */
    while (tmp != fifo->end)
    {
        if (fifo->x[tmp] == x && fifo->y[tmp] == y)
        {
            pthread_mutex_unlock(&fifo->mutex);
            return;
        }
        tmp = (tmp + 1) & (FIFO_WORLD_GEN_SIZE - 1);
    }
    /* Append new request. */
    fifo->x[fifo->end] = x;
    fifo->y[fifo->end] = y;
    fifo->end = (fifo->end + 1) & (FIFO_WORLD_GEN_SIZE - 1);
    pthread_cond_signal(&fifo->cond);
    pthread_mutex_unlock(&fifo->mutex);
}