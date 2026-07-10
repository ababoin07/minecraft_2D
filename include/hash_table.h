#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "includes.h"
#include "world.h"
#include <pthread.h>

/**
 * @brief One slot in a hash bucket (holds a chunk pointer and its age).
 */
struct HashTableSlot
{
    double age;          /**< Timestamp used for LRU eviction. */
    chunk_t *chunk_ptr;  /**< Pointer to the chunk, or NULL if empty. */
};

/**
 * @brief Hash table that maps (chunk_x,chunk_y) to a chunk pointer.
 * @note Uses open addressing with linear probing; eviction based on age.
 */
struct HashTable
{
    hash_table_slot_t buckets[HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE];
    pthread_mutex_t mutex;
};

/**
 * @brief Create a new hash table.
 * @return Pointer to allocated table, or NULL on failure.
 */
hash_table_t *create_hash_table();

/**
 * @brief Destroy a hash table (does not free chunks inside).
 * @param table Pointer to the table.
 */
void destroy_hash_table(hash_table_t *);

/**
 * @brief Compute a hash for (x,y).
 * @return A 32‑bit hash value.
 */
int hash_pair(int64_t, int64_t);

/**
 * @brief Store a chunk in the hash table, evicting an old chunk if necessary.
 * @param chunk Pointer to the chunk to store.
 * @param table Pointer to the hash table.
 * @note The evicted chunk is saved and scheduled for destruction.
 */
void store_chunk(chunk_t *, hash_table_t *);

/**
 * @brief Retrieve a chunk from the hash table by coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param table Pointer to the hash table.
 * @return Pointer to the chunk, or NULL if not found.
 */
chunk_t *get_chunk(int64_t, int64_t, hash_table_t *);

#endif // HASH_TABLE_H