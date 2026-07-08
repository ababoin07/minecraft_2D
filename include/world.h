#ifndef WORLD_H
#define WORLD_H

#include <pthread.h>
#include <stdbool.h>

#include "includes.h"
#include "FastNoiseLite.h"
#include "hash_table.h"

enum ChunkGenSteps {
    CHUNK_GEN_UNSTARTED,
    CHUNK_GEN_BASE,
    CHUNK_GEN_CAVES,
    CHUNK_GEN_STRUCTURES,
    CHUNK_GEN_VEGETATION,
    CHUNK_GEN_FINISHED
};

struct Chunk {
    chunk_gen_steps_t generation;
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
    hash_table_t* chunks;
    pthread_t worker_thread;
    volatile bool worker_running;
};

void sanitize_filename(char*);
chunk_t* create_chunk(int64_t, int64_t, fnl_state*);
void chunk_generate_base(chunk_t*);
void chunk_generate_caves(chunk_t*);
_Bool generate_next(chunk_t*);
world_t* create_world(int, char*);
void destroy_world(world_t*);
void destroy_chunk(chunk_t*);
void save_chunk(chunk_t*);
chunk_t* load_chunk(world_t*, int64_t, int64_t);

#endif // WORLD_H