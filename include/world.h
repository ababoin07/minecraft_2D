#ifndef WORLD_H
#define WORLD_H

#include "includes.h"
#include "FastNoiseLite.h"
#include "hash_table.h"

enum ChunkGenSteps {
    CHUNK_GEN_UNSTARTED,
    CHUNK_GEN_BASE,
    CHUNK_GEN_WATER,
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
};

struct World {
    int seed;
    fnl_state noise_generator;
    struct HashTable* chunks;
};

struct Chunk* create_chunk(int64_t, int64_t, fnl_state*);
void chunk_generate_base(struct Chunk*);
void chunk_generate_caves(struct Chunk*);
_Bool generate_next(struct Chunk*);
struct World* create_world(int);
void destroy_world(struct World*);
void destroy_chunk(struct Chunk*);
void eat_fifo(struct FifoWorldGen*, struct World*, int);

#endif // WORLD_H