#ifndef WORLD_H
#define WORLD_H

#include "includes.h"
#include "FastNoiseLite.h"
#include "hash_table.h"
#include <pthread.h>
#include <stdbool.h>

enum ChunkGenSteps {
    CHUNK_GEN_UNSTARTED,
    CHUNK_GEN_BASE,
    CHUNK_GEN_CAVES,
    CHUNK_GEN_STRUCTURES,
    CHUNK_GEN_VEGETATION,
    CHUNK_GEN_FINISHED
};

struct Chunk {
    enum ChunkGenSteps generation;
    uint8_t blocks[CHUNK_SIZE * CHUNK_SIZE];
    int64_t position_x, position_y;
    RenderTexture2D texture;
    fnl_state* noise_generator;
    bool texture_generated;
};

struct World {
    char* name;
    int seed;
    fnl_state noise_generator;
    struct HashTable* chunks;
    pthread_t worker_thread;
    volatile bool worker_running;
};

void sanitize_filename(char*);
struct Chunk* create_chunk(int64_t, int64_t, fnl_state*);
void chunk_generate_base(struct Chunk*);
void chunk_generate_caves(struct Chunk*);
_Bool generate_next(struct Chunk*);
struct World* create_world(int, char*);
void destroy_world(struct World*);
void destroy_chunk(struct Chunk*);
void save_chunk(struct Chunk*);
struct Chunk* load_chunk(struct World*, int64_t, int64_t);

#endif // WORLD_H