#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "includes.h"
#include "world.h"

struct HashTableSlot {
    double age;
    int64_t x, y;
    struct Chunk* chunk_ptr;
};

struct HashTableBucket {
    int free_buckets;
    struct HashTableSlot slots[HASH_TABLE_BUCKET_SIZE];
};

struct HashTable {
    struct HashTableBucket buckets[HASH_TABLE_BUCKETS];
};

struct HashTable* create_hash_table();
int hash_pair(int64_t, int64_t);

#endif // HASH_TABLE_H