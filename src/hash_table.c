#include "includes.h"
#include "hash_table.h"
#include "destroy_stack.h"

/**
 * @brief Create a new hash table.
 */
hash_table_t *create_hash_table()
{
    hash_table_t *ptr = malloc(sizeof(hash_table_t));
    if (NULL == ptr)
        return NULL;
    for (int tmp = 0; tmp < HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE; tmp++)
    {
        ptr->buckets[tmp].chunk_ptr = NULL;
    }
    pthread_mutex_init(&ptr->mutex, NULL);
    return ptr;
}

/**
 * @brief Destroy a hash table (mutex only; chunks are freed elsewhere).
 */
void destroy_hash_table(hash_table_t *table)
{
    pthread_mutex_destroy(&table->mutex);
    free(table);
}

/**
 * @brief Hash function for (x,y).
 */
int hash_pair(int64_t x, int64_t y)
{
    uint64_t ux = (uint64_t)x;
    uint64_t uy = (uint64_t)y;
    ux ^= uy + 0x9e3779b9 + (ux << 6) + (ux >> 2);
    return (int)ux;
}

/**
 * @brief Store a chunk, evicting an old one if the bucket is full.
 * @details Uses linear probing within each bucket. If no empty slot is found,
 *          the chunk with the smallest age is evicted (saved and scheduled for destruction).
 */
void store_chunk(chunk_t *chunk_input, hash_table_t *hash_table)
{
    pthread_mutex_lock(&hash_table->mutex);
    int hash = HASH_TABLE_BUCKET_SIZE * (hash_pair(chunk_input->position_x, chunk_input->position_y) & (HASH_TABLE_BUCKETS - 1));
    /* Try to find an empty slot. */
    for (int idx = hash; idx < hash + HASH_TABLE_BUCKET_SIZE; idx++)
    {
        if (NULL == hash_table->buckets[idx].chunk_ptr)
        {
            hash_table->buckets[idx].chunk_ptr = chunk_input;
            hash_table->buckets[idx].age = GetTime();
            pthread_mutex_unlock(&hash_table->mutex);
            return;
        }
    }

    /* No empty slot: evict the oldest chunk in this bucket. */
    double min_age = INFINITY;
    int min_idx = 0;
    for (int idx = hash; idx < hash + HASH_TABLE_BUCKET_SIZE; idx++)
    {
        if (hash_table->buckets[idx].age < min_age)
        {
            min_age = hash_table->buckets[idx].age;
            min_idx = idx;
        }
    }

    save_chunk(hash_table->buckets[min_idx].chunk_ptr);
    schedule_chunk_destruction(hash_table->buckets[min_idx].chunk_ptr);
    hash_table->buckets[min_idx].chunk_ptr = chunk_input;
    hash_table->buckets[min_idx].age = GetTime();
    pthread_mutex_unlock(&hash_table->mutex);

    /* Apply any pending light updates that were queued for this chunk. */
    apply_pending_lights_for_chunk(chunk_input);
}

/**
 * @brief Retrieve a chunk by coordinates.
 */
chunk_t *get_chunk(int64_t x, int64_t y, hash_table_t *hash_table)
{
    pthread_mutex_lock(&hash_table->mutex);
    int hash = HASH_TABLE_BUCKET_SIZE * (hash_pair(x, y) & (HASH_TABLE_BUCKETS - 1));
    chunk_t *result = NULL;
    for (int idx = hash; idx < hash + HASH_TABLE_BUCKET_SIZE; idx++)
    {
        if (NULL == hash_table->buckets[idx].chunk_ptr)
            continue;
        if (hash_table->buckets[idx].chunk_ptr->position_x == x && hash_table->buckets[idx].chunk_ptr->position_y == y)
        {
            result = hash_table->buckets[idx].chunk_ptr;
            break;
        }
    }
    pthread_mutex_unlock(&hash_table->mutex);
    return result;
}