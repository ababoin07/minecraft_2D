#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "includes.h"
#include "world.h"
#include <pthread.h>

struct HashTableSlot {
    double age;
    struct Chunk* chunk_ptr;
};

struct HashTable {
    struct HashTableSlot buckets[HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE];
    pthread_mutex_t mutex;
};

struct HashTable* create_hash_table();
void destroy_hash_table(struct HashTable*);
int hash_pair(int64_t, int64_t);
void store_chunk(struct Chunk*, struct HashTable*);
struct Chunk* get_chunk(int64_t, int64_t, struct HashTable*);

#endif // HASH_TABLE_H