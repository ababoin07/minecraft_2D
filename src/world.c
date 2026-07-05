#include "includes.h"

#include "world.h"
#include "data.h"
#include "FastNoiseLite.h"
#include "render.h"

struct Chunk* create_chunk(int64_t offset_x, int64_t offset_y, fnl_state* noise_generator) {
    struct Chunk* ptr = malloc(sizeof(struct Chunk));
    if (NULL == ptr) return NULL;
    for (size_t tmp = 0; tmp < CHUNK_SIZE * CHUNK_SIZE; tmp++) 
        ptr->blocks[tmp] = 0;
    ptr->position_x = offset_x;
    ptr->position_y = offset_y;
    ptr->texture = LoadRenderTexture(16 * CHUNK_SIZE, 16 * CHUNK_SIZE);
    ptr->generation = CHUNK_GEN_UNSTARTED;
    ptr->noise_generator = noise_generator;
    generate_chunk_texture_render(ptr);
    return ptr;
};

void chunk_generate_base(struct Chunk* chunk_input) {
    chunk_input->noise_generator->frequency;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        int height_noise = (int)(16 * fnlGetNoise2D(chunk_input->noise_generator, (float)chunk_input->position_x * (float)CHUNK_SIZE + (float)x, 32768.f));
        int height = CHUNK_SIZE * chunk_input->position_y;
        for (int y = 0; y < CHUNK_SIZE; y++) {
            chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)(height > height_noise? AIR: (height == height_noise? GRASS: STONE));
            ++height;
        }
    }
}

void chunk_generate_caves(struct Chunk* chunk_input) {
    chunk_input->noise_generator->octaves = 8;
    float start_x = chunk_input->position_x * CHUNK_SIZE;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        float start_y = chunk_input->position_y * CHUNK_SIZE;
        for (int y = 0; y < CHUNK_SIZE; y++) {
            float noise;
            noise = fnlGetNoise2D(chunk_input->noise_generator, (float)x + start_x, (float)y + start_y);
            if (noise > 0.8f) {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
            noise = fnlGetNoise2D(chunk_input->noise_generator, ((float)x + start_x) / 0.5f, ((float)y + start_y) / 0.5f);
            if (-0.15f < noise && noise < 0.15f) {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;                
            }
        }
    }
}

_Bool generate_next(struct Chunk* chunk) {
    return 1;
}

struct World* create_world(int seed) {
    struct World* ptr = malloc(sizeof(struct World));
    if (NULL == ptr) return NULL;
    ptr->seed = seed;
    ptr->noise_generator = fnlCreateState();
    ptr->noise_generator.seed = seed;
    ptr->noise_generator.noise_type = FNL_NOISE_OPENSIMPLEX2;
    ptr->offset_x = 0;
    ptr->offset_y = 0;
    for (int64_t x = 0; x < LOADED_WORLD_WIDTH; x++) {
        for (int64_t y = 0; y < LOADED_WORLD_HEIGHT; y++) {
            ptr->active_chunks[x * LOADED_WORLD_HEIGHT + y] = create_chunk(x - LOADED_WORLD_CENTER_X + ptr->offset_x, y - LOADED_WORLD_CENTER_Y + ptr->offset_y, &ptr->noise_generator);
            chunk_generate_base(ptr->active_chunks[x * LOADED_WORLD_HEIGHT + y]);
            chunk_generate_caves(ptr->active_chunks[x * LOADED_WORLD_HEIGHT + y]);
            generate_chunk_texture_render(ptr->active_chunks[x * LOADED_WORLD_HEIGHT + y]);
            // generate_until(ptr->active_chunks[x * LOADED_WORLD_HEIGHT + y], CHUNK_GEN_BASE);
        }
    }
    return ptr;
}

void destroy_world(struct World* world_input) {
    for (size_t tmp = 0; tmp < LOADED_WORLD_WIDTH * LOADED_WORLD_HEIGHT; tmp++) {
        destroy_chunk(world_input->active_chunks[tmp]);
    }
    free(world_input);
    printf("INFO: WORLD: Destroyed World from RAM\n");
}

void destroy_chunk(struct Chunk* chunk_input) {
    UnloadRenderTexture(chunk_input->texture);
    free(chunk_input);
}