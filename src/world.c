#include "includes.h"
#include "world.h"
#include "data.h"
#include "FastNoiseLite.h"
#include "render.h"
#include "fifo.h"
#include "files_api.h"
#include <pthread.h>
#include <stdatomic.h>

void sanitize_filename(char* path) {
    if (path == NULL) return;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '-') {
            path[i] = '_';
        }
    }
}

static void build_chunk_path(const world_t* world, int64_t cx, int64_t cy, char* out_path, size_t out_size) {
    int rx = (int)(cx / 32);
    int ry = (int)(cy / 32);
    snprintf(out_path, out_size, "worlds/%s/region_%d_%d/chunk_%" PRId64 "_%" PRId64 ".dat", world->name, rx, ry, cx, cy);
    sanitize_filename(out_path);
}

chunk_t* load_chunk(world_t* world, int64_t cx, int64_t cy) {
    char path[512];
    build_chunk_path(world, cx, cy, path, sizeof(path));

    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    uint32_t magic, version;
    int64_t pos_x, pos_y;
    uint8_t generation;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || fread(&version, sizeof(version), 1, f) != 1 || fread(&pos_x, sizeof(pos_x), 1, f) != 1 || fread(&pos_y, sizeof(pos_y), 1, f) != 1 || fread(&generation, sizeof(generation), 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    const uint32_t EXPECTED_MAGIC = 0x43484B;
    if (magic != EXPECTED_MAGIC || version != 1 || pos_x != cx || pos_y != cy) {
        fclose(f);
        return NULL;
    }

    chunk_t* chunk = malloc(sizeof(chunk_t));
    if (!chunk) { fclose(f); return NULL; }

    if (fread(chunk->blocks, sizeof(chunk->blocks), 1, f) != 1) {
        fclose(f);
        free(chunk);
        return NULL;
    }
    fclose(f);

    chunk->position_x = cx;
    chunk->position_y = cy;
    chunk->generation = (chunk_gen_steps_t)generation;
    chunk->texture = (RenderTexture2D){0};
    chunk->texture_generated = false;
    chunk->noise_generator = &world->noise_generator;

    atomic_fetch_add(&Chunks_Amount, 1);
    return chunk;
}
static void* worker_main(void* arg) {
    world_t* world = (world_t*)arg;
    extern fifo_world_gen_t* fifo_world_gen;

    while (world->worker_running) {
        pthread_mutex_lock(&fifo_world_gen->mutex);
        while (fifo_world_gen->start == fifo_world_gen->end && world->worker_running) {
            pthread_cond_wait(&fifo_world_gen->cond, &fifo_world_gen->mutex);
        }
        if (!world->worker_running) {
            pthread_mutex_unlock(&fifo_world_gen->mutex);
            break;
        }
        int64_t x = fifo_world_gen->x[fifo_world_gen->start];
        int64_t y = fifo_world_gen->y[fifo_world_gen->start];
        fifo_world_gen->start = (fifo_world_gen->start + 1) & (FIFO_WORLD_GEN_SIZE - 1);
        pthread_mutex_unlock(&fifo_world_gen->mutex);

        chunk_t* chunk = load_chunk(world, x, y);
        if (!chunk) {
            chunk = create_chunk(x, y, &world->noise_generator);
            chunk_generate_base(chunk);
            chunk_generate_caves(chunk);
        }
        if (chunk) {
            store_chunk(chunk, world->chunks);
        }
    }
    return NULL;
}

world_t* create_world(int seed, char* name) {
    world_t* ptr = malloc(sizeof(world_t));
    if (NULL == ptr) return NULL;
    if (!directory_exists("worlds")) MKDIR("worlds");
    char path[256];
    snprintf(path, sizeof(path), "worlds/%s", name);
    sanitize_filename(path);
    if (!directory_exists(path)) MKDIR(path);
    ptr->name = name;
    ptr->seed = seed;
    ptr->noise_generator = fnlCreateState();
    ptr->noise_generator.seed = seed;
    ptr->noise_generator.noise_type = FNL_NOISE_OPENSIMPLEX2;
    ptr->chunks = create_hash_table();

    for (int64_t x = -HALF_INITIAL_WORLD_SIZE; x < HALF_INITIAL_WORLD_SIZE; x++) {
        for (int64_t y = -HALF_INITIAL_WORLD_SIZE; y < HALF_INITIAL_WORLD_SIZE; y++) {
            chunk_t* chunk = load_chunk(ptr, x, y);
            if (!chunk) {
                chunk = create_chunk(x, y, &ptr->noise_generator);
                chunk_generate_base(chunk);
                chunk_generate_caves(chunk);
            }
            store_chunk(chunk, ptr->chunks);
        }
    }

    ptr->worker_running = true;
    pthread_create(&ptr->worker_thread, NULL, worker_main, ptr);
    return ptr;
}

chunk_t* create_chunk(int64_t offset_x, int64_t offset_y, fnl_state* noise_generator) {
    chunk_t* ptr = malloc(sizeof(chunk_t));
    if (NULL == ptr) return NULL;
    for (size_t tmp = 0; tmp < CHUNK_SIZE * CHUNK_SIZE; tmp++) 
        ptr->blocks[tmp] = 0;
    ptr->position_x = offset_x;
    ptr->position_y = offset_y;
    ptr->texture = (RenderTexture2D){0};
    ptr->generation = CHUNK_GEN_UNSTARTED;
    ptr->noise_generator = noise_generator;
    ptr->texture_generated = false;
    atomic_fetch_add(&Chunks_Amount, 1);
    return ptr;
};

void chunk_generate_base(chunk_t* chunk_input) {
    chunk_input->noise_generator->frequency;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        int height_noise = (int)(8 * fnlGetNoise2D(chunk_input->noise_generator, (float)chunk_input->position_x * (float)CHUNK_SIZE + (float)x, 16.f));
        height_noise += (int)(96 * fnlGetNoise2D(chunk_input->noise_generator, ((float)chunk_input->position_x * (float)CHUNK_SIZE + (float)x) / 8.f, 32.f));
        int height = CHUNK_SIZE * chunk_input->position_y;
        for (int y = 0; y < CHUNK_SIZE; y++) {
            chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)(height >= height_noise? (height < -8? WATER_STAND: AIR): STONE);
            ++height;
        }
    }
    chunk_input->generation = CHUNK_GEN_BASE;
}

void chunk_generate_caves(chunk_t* chunk_input) {
    float start_x = chunk_input->position_x * CHUNK_SIZE;
    float noise;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        float start_y = chunk_input->position_y * CHUNK_SIZE;
        for (int y = 0; y < CHUNK_SIZE; y++) {
            if (STONE != chunk_input->blocks[x * CHUNK_SIZE + y]) continue;
            noise = fnlGetNoise2D(chunk_input->noise_generator, (float)x + start_x, (float)y + start_y);
            if (noise > 0.8f) {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
            noise = fnlGetNoise2D(chunk_input->noise_generator, ((float)x + start_x + 8192.f) / 2.f, ((float)y + start_y) / 2.5f);
            if (-0.05f < noise && noise < 0.05f) {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
            noise = fnlGetNoise2D(chunk_input->noise_generator, ((float)x + start_x - 8192.f) * 2.f, ((float)y + start_y) * 2.f);
            if (-0.1f < noise && noise < 0.1f) {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
        }
    }
    chunk_input->generation = CHUNK_GEN_CAVES;
}

_Bool generate_next(chunk_t* chunk) {
    return 1;
}

void destroy_world(world_t* world_input) {
    world_input->worker_running = false;
    extern  fifo_world_gen_t* fifo_world_gen;
    pthread_mutex_lock(&fifo_world_gen->mutex);
    pthread_cond_signal(&fifo_world_gen->cond);
    pthread_mutex_unlock(&fifo_world_gen->mutex);
    pthread_join(world_input->worker_thread, NULL);

    for (size_t i = 0; i < HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE; i++) {
        chunk_t* chunk = world_input->chunks->buckets[i].chunk_ptr;
        if (chunk != NULL) {
            save_chunk(chunk);
        }
    }

    for (size_t i = 0; i < HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE; i++) {
        chunk_t* chunk = world_input->chunks->buckets[i].chunk_ptr;
        if (chunk != NULL) {
            destroy_chunk(chunk);
        }
    }

    destroy_hash_table(world_input->chunks);
    free(world_input);
}

void destroy_chunk(chunk_t* chunk_input) {
    if (chunk_input->texture.id != 0) {
        UnloadRenderTexture(chunk_input->texture);
    }
    free(chunk_input);
    atomic_fetch_sub(&Chunks_Amount, 1);
}

void save_chunk(chunk_t* chunk_input) {
    if (!chunk_input) return;

    extern world_t* world;

    int rx = (int)(chunk_input->position_x / 32);
    int ry = (int)(chunk_input->position_y / 32);
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "worlds/%s/region_%d_%d", world->name, rx, ry);
    sanitize_filename(dir_path);
    if (!directory_exists(dir_path)) {
        MKDIR(dir_path);
    }

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/chunk_%" PRId64 "_%" PRId64 ".dat", dir_path,
             chunk_input->position_x, chunk_input->position_y);
    sanitize_filename(file_path);

    FILE* f = fopen(file_path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to save chunk (%" PRId64 ", %" PRId64 ")\n",
                chunk_input->position_x, chunk_input->position_y);
        return;
    }

    const uint32_t magic = 0x43484B;
    const uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&chunk_input->position_x, sizeof(chunk_input->position_x), 1, f);
    fwrite(&chunk_input->position_y, sizeof(chunk_input->position_y), 1, f);
    uint8_t gen = (uint8_t)chunk_input->generation;
    fwrite(&gen, sizeof(gen), 1, f);

    fwrite(chunk_input->blocks, sizeof(chunk_input->blocks), 1, f);

    fclose(f);
}