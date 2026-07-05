#ifndef WORLD_H
#define WORLD_H

#include "includes.h"
#include "FastNoiseLite.h"

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
    int64_t offset_x, offset_y;
    fnl_state noise_generator;
    struct Chunk* active_chunks[LOADED_WORLD_WIDTH * LOADED_WORLD_HEIGHT];
};

struct Chunk* create_chunk(int64_t, int64_t, fnl_state*);
void chunk_generate_base(struct Chunk*);
void chunk_generate_caves(struct Chunk*);
_Bool generate_next(struct Chunk*);
struct World* create_world(int);
void destroy_world(struct World*);
void destroy_chunk(struct Chunk*);

#endif // WORLD_H