#ifndef FIFO_H
#define FIFO_H

#include "includes.h"
#include "world.h"
#include <pthread.h>

/**
 * @brief Lock‑free (mutex‑protected) FIFO for chunk generation requests.
 * @note The FIFO is circular; capacity must be a power of two.
 */
struct FifoWorldGen {
    int64_t x[FIFO_WORLD_GEN_SIZE]; /**< X coordinates of requested chunks. */
    int64_t y[FIFO_WORLD_GEN_SIZE]; /**< Y coordinates of requested chunks. */
    uint32_t start;                /**< Read index. */
    uint32_t end;                  /**< Write index. */
    uint32_t capacity;             /**< Fixed capacity (FIFO_WORLD_GEN_SIZE). */
    pthread_mutex_t mutex;         /**< Protects access to the FIFO. */
    pthread_cond_t cond;           /**< Signalled when a new request is added. */
};

/**
 * @brief Create and initialise a new FIFO.
 * @return Pointer to allocated FIFO, or NULL on failure.
 */
fifo_world_gen_t* FIFO_world_gen_create();

/**
 * @brief Destroy a FIFO and free its memory.
 * @param fifo Pointer to the FIFO.
 */
void FIFO_world_gen_destroy(fifo_world_gen_t*);

/**
 * @brief Request generation of a chunk at (x,y).
 * @param x X chunk coordinate.
 * @param y Y chunk coordinate.
 * @param fifo Pointer to the FIFO.
 * @note Duplicate requests are ignored.
 */
void request_gen(int64_t, int64_t, fifo_world_gen_t*);

#endif // FIFO_H