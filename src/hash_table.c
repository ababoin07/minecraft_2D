#include "includes.h"
#include "hash_table.h"

struct HashTable* create_hash_table() {
    struct HashTable* ptr = malloc(sizeof(struct HashTable));
    if (NULL == ptr) return NULL;
    for (int a = 0; a < HASH_TABLE_BUCKETS; a++) {
        ptr->buckets[a].free_buckets = 0;
        for (int b = 0; b < HASH_TABLE_BUCKET_SIZE; b++) {
            ptr->buckets[a].slots[b].age = -1.0;
            ptr->buckets[a].slots[b].chunk_ptr = NULL;
        }
    }
}

int hash_pair(int64_t x, int64_t y) {
    uint64_t ux = (uint64_t)x;
    uint64_t uy = (uint64_t)y;

    ux ^= uy + 0x9e3779b9 + (ux << 6) + (ux >> 2);
    
    return (int)(ux % HASH_TABLE_BUCKETS);
}