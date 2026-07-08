#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "includes.h"
#include "world.h"
#include <pthread.h>

struct HashTableSlot {
    double age;
    chunk_t* chunk_ptr;
};

struct HashTable {
    hash_table_slot_t buckets[HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE];
    pthread_mutex_t mutex;
};

hash_table_t* create_hash_table();
void destroy_hash_table(hash_table_t*);
int hash_pair(int64_t, int64_t);
void store_chunk(chunk_t*, hash_table_t*);
chunk_t* get_chunk(int64_t, int64_t, hash_table_t*);

#endif // HASH_TABLE_H