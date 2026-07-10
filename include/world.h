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
    CHUNK_GEN_FINISHED,
    CHUNK_GEN_AMOUT
};

struct Chunk {
    chunk_gen_steps_t generation;
    uint8_t blocks[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t light_r[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t light_g[CHUNK_SIZE * CHUNK_SIZE];
    uint8_t light_b[CHUNK_SIZE * CHUNK_SIZE];
    bool light_sun[CHUNK_SIZE * CHUNK_SIZE];
    bool is_source[CHUNK_SIZE * CHUNK_SIZE];
    int64_t position_x, position_y;
    RenderTexture2D texture;
    fnl_state* noise_generator;
    bool texture_generated;
};

struct LightUpdateSlot {
    int64_t chunk_x;
    int64_t chunk_y;
    uint8_t block_idx, force_r, force_g, force_b;
    bool force_set, dirty_r, dirty_g, dirty_b;
};

struct World {
    char* name;
    int seed;
    fnl_state noise_generator;
    hash_table_t* chunks;
    pthread_t worker_thread;
    volatile bool worker_running;
    light_update_slot_t fifo_lighting_data[LIGHT_SOURCES_MAX_AMOUNT];
    uint32_t ligthing_start;
    uint32_t lighting_end;
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
void register_light_source(int64_t, int64_t, uint8_t, bool, bool, bool, bool, uint8_t, uint8_t, uint8_t);
void process_light_sources(uint32_t);

#endif // WORLD_H