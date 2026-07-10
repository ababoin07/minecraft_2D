#ifndef GLOBAL_H
#define GLOBAL_H

#include "includes.h"
#include <raylib.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>

#define DESTROY_STACK_SIZE 4096 /**< Max number of chunks that can be queued for destruction. */

/**
 * @brief Stack of chunks pending destruction (thread‑safe).
 */
struct DestroyStack
{
    chunk_t *chunks[DESTROY_STACK_SIZE]; /**< Array of chunk pointers. */
    uint32_t top;                        /**< Stack pointer. */
    pthread_mutex_t mutex;               /**< Mutex for concurrent access. */
};

/** Global destroy stack instance. */
extern destroy_stack_t destroy_stack;

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0777)
#endif

extern Texture2D Textures_Atlas;          /**< Global block texture atlas. */
extern fifo_world_gen_t *fifo_world_gen;  /**< Global FIFO for chunk generation. */
extern atomic_uint Chunks_Amount;         /**< Total number of loaded chunks (atomic). */
extern world_t *world;                    /**< Global world pointer. */
extern uint64_t tick;                     /**< Global tick counter. */
extern pending_light_table_t pending_light_table; /**< Global pending light table. */

#endif // GLOBAL_H