#ifndef DESTROY_STACK_H
#define DESTROY_STACK_H

#include "includes.h"
#include "global.h"
#include "world.h"

/**
 * @brief Initialise the chunk destruction stack.
 */
void init_destroy_stack();

/**
 * @brief Destroy the chunk destruction stack (free mutex).
 */
void destroy_destroy_stack();

/**
 * @brief Schedule a chunk for later destruction.
 * @param chunk Pointer to the chunk to destroy.
 * @note The chunk is pushed onto a thread‑safe stack.
 */
void schedule_chunk_destruction(chunk_t *);

/**
 * @brief Process all pending chunk destructions.
 * @note This function pops chunks from the stack and calls destroy_chunk().
 */
void process_destroy_stack();

#endif // DESTROY_STACK_H