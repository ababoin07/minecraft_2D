#include "includes.h"
#include "hash_table.h"

struct HashTable* create_hash_table() {
    struct HashTable* ptr = malloc(sizeof(struct HashTable));
    if (NULL == ptr) return NULL;
    for (int tmp = 0; tmp < HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE; tmp++) {
        ptr->buckets[tmp].chunk_ptr = NULL;
    }
    return ptr;
}

int hash_pair(int64_t x, int64_t y) {
    uint64_t ux = (uint64_t)x;
    uint64_t uy = (uint64_t)y;

    ux ^= uy + 0x9e3779b9 + (ux << 6) + (ux >> 2);
    
    return (int)ux;
}

void store_chunk(struct Chunk* chunk_input, struct HashTable* hash_table) {
    int hash = HASH_TABLE_BUCKET_SIZE * (hash_pair(chunk_input->position_x, chunk_input->position_y) & (HASH_TABLE_BUCKETS - 1));
    for (int idx = hash; idx < hash + HASH_TABLE_BUCKET_SIZE; idx++) {
        if (NULL == hash_table->buckets[idx].chunk_ptr) {
            hash_table->buckets[idx].chunk_ptr = chunk_input;
            hash_table->buckets[idx].age = GetTime();
            return;
        }
    }

    double min_age = INFINITY;
    int min_idx = 0;
    for (int idx = hash; idx < hash + HASH_TABLE_BUCKET_SIZE; idx++) {
        if (hash_table->buckets[idx].age < min_age) {
            min_age = hash_table->buckets[idx].age;
            min_idx = idx;
        }
    }

    // TODO: save in a file the chunk
    destroy_chunk(hash_table->buckets[min_idx].chunk_ptr);
    hash_table->buckets[min_idx].chunk_ptr = chunk_input;
}

struct Chunk* get_chunk(int64_t x, int64_t y, struct HashTable* hash_table) {
    int hash = HASH_TABLE_BUCKET_SIZE * (hash_pair(x, y) & (HASH_TABLE_BUCKETS - 1));
    for (int idx = hash; idx < hash + HASH_TABLE_BUCKET_SIZE; idx++) {
        if (NULL == hash_table->buckets[idx].chunk_ptr) continue;
        if (hash_table->buckets[idx].chunk_ptr->position_x == x && hash_table->buckets[idx].chunk_ptr->position_y == y) {
            return hash_table->buckets[idx].chunk_ptr;
        }
    }
    return NULL;
}