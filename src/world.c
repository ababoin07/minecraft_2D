#include "includes.h"
#include "world.h"
#include "data.h"
#include "FastNoiseLite.h"
#include "render.h"
#include "fifo.h"
#include "files_api.h"
#include <pthread.h>
#include <stdatomic.h>

void sanitize_filename(char *path)
{
    if (path == NULL)
        return;
    for (int i = 0; path[i] != '\0'; i++)
    {
        if (path[i] == '-')
        {
            path[i] = '_';
        }
    }
}

static void build_chunk_path(const world_t *world, int64_t cx, int64_t cy, char *out_path, size_t out_size)
{
    int rx = (int)(cx / 32);
    int ry = (int)(cy / 32);
    snprintf(out_path, out_size, "worlds/%s/region_%d_%d/chunk_%" PRId64 "_%" PRId64 ".dat", world->name, rx, ry, cx, cy);
    sanitize_filename(out_path);
}

chunk_t *load_chunk(world_t *world, int64_t cx, int64_t cy)
{
    char path[512];
    build_chunk_path(world, cx, cy, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    uint32_t magic, version;
    int64_t pos_x, pos_y;
    uint8_t generation;
    if (fread(&magic, sizeof(magic), 1, f) != 1 ||
        fread(&version, sizeof(version), 1, f) != 1 ||
        fread(&pos_x, sizeof(pos_x), 1, f) != 1 ||
        fread(&pos_y, sizeof(pos_y), 1, f) != 1 ||
        fread(&generation, sizeof(generation), 1, f) != 1)
    {
        fclose(f);
        return NULL;
    }

    const uint32_t EXPECTED_MAGIC = 0x43484B;
    if (magic != EXPECTED_MAGIC || version != 1 || pos_x != cx || pos_y != cy)
    {
        fclose(f);
        return NULL;
    }

    chunk_t *chunk = malloc(sizeof(chunk_t));
    if (!chunk)
    {
        fclose(f);
        return NULL;
    }

    if (fread(chunk->blocks, sizeof(chunk->blocks), 1, f) != 1)
    {
        fclose(f);
        free(chunk);
        return NULL;
    }
    if (fread(chunk->light_r, sizeof(chunk->light_r), 1, f) != 1)
    {
        fclose(f);
        free(chunk);
        return NULL;
    }
    if (fread(chunk->light_g, sizeof(chunk->light_g), 1, f) != 1)
    {
        fclose(f);
        free(chunk);
        return NULL;
    }
    if (fread(chunk->light_b, sizeof(chunk->light_b), 1, f) != 1)
    {
        fclose(f);
        free(chunk);
        return NULL;
    }
    if (fread(chunk->light_sun, sizeof(chunk->light_sun), 1, f) != 1)
    {
        fclose(f);
        free(chunk);
        return NULL;
    }
    if (fread(chunk->is_source, sizeof(chunk->is_source), 1, f) != 1)
    {
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

static void *worker_main(void *arg)
{
    world_t *world = (world_t *)arg;
    extern fifo_world_gen_t *fifo_world_gen;

    while (world->worker_running)
    {
        pthread_mutex_lock(&fifo_world_gen->mutex);
        while (fifo_world_gen->start == fifo_world_gen->end && world->worker_running)
        {
            pthread_cond_wait(&fifo_world_gen->cond, &fifo_world_gen->mutex);
        }
        if (!world->worker_running)
        {
            pthread_mutex_unlock(&fifo_world_gen->mutex);
            break;
        }
        int64_t x = fifo_world_gen->x[fifo_world_gen->start];
        int64_t y = fifo_world_gen->y[fifo_world_gen->start];
        fifo_world_gen->start = (fifo_world_gen->start + 1) & (FIFO_WORLD_GEN_SIZE - 1);
        pthread_mutex_unlock(&fifo_world_gen->mutex);

        chunk_t *chunk = load_chunk(world, x, y);
        if (!chunk)
        {
            chunk = create_chunk(x, y, &world->noise_generator);
            chunk_generate_base(chunk);
            chunk_generate_caves(chunk);
        }
        if (chunk)
        {
            store_chunk(chunk, world->chunks);
        }
    }
    return NULL;
}

world_t *create_world(int seed, char *name)
{
    world_t *ptr = malloc(sizeof(world_t));
    if (NULL == ptr)
        return NULL;
    if (!directory_exists("worlds"))
        MKDIR("worlds");
    char path[256];
    snprintf(path, sizeof(path), "worlds/%s", name);
    sanitize_filename(path);
    if (!directory_exists(path))
        MKDIR(path);
    ptr->name = name;
    ptr->seed = seed;
    ptr->noise_generator = fnlCreateState();
    ptr->noise_generator.seed = seed;
    ptr->noise_generator.noise_type = FNL_NOISE_OPENSIMPLEX2;
    ptr->chunks = create_hash_table();

    ptr->ligthing_start = 0;
    ptr->lighting_end = 0;

    ptr->worker_running = true;
    pthread_create(&ptr->worker_thread, NULL, worker_main, ptr);
    return ptr;
}

chunk_t *create_chunk(int64_t offset_x, int64_t offset_y, fnl_state *noise_generator)
{
    chunk_t *ptr = malloc(sizeof(chunk_t));
    if (NULL == ptr)
        return NULL;
    for (size_t tmp = 0; tmp < CHUNK_SIZE * CHUNK_SIZE; tmp++)
    {
        ptr->blocks[tmp] = 0;
    }
    ptr->position_x = offset_x;
    ptr->position_y = offset_y;
    ptr->texture = (RenderTexture2D){0};
    ptr->generation = CHUNK_GEN_UNSTARTED;
    ptr->noise_generator = noise_generator;
    ptr->texture_generated = false;
    atomic_fetch_add(&Chunks_Amount, 1);
    return ptr;
};

void chunk_generate_base(chunk_t *chunk_input)
{
    int block_idx;
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        int height_noise = (int)(8 * fnlGetNoise2D(chunk_input->noise_generator, (float)chunk_input->position_x * (float)CHUNK_SIZE + (float)x, 16.f));
        height_noise += (int)(96 * fnlGetNoise2D(chunk_input->noise_generator, ((float)chunk_input->position_x * (float)CHUNK_SIZE + (float)x) / 8.f, 32.f));
        int height = CHUNK_SIZE * chunk_input->position_y;
        for (int y = 0; y < CHUNK_SIZE; y++)
        {
            block_idx = x * CHUNK_SIZE + y;
            chunk_input->is_source[block_idx] = false;
            chunk_input->blocks[block_idx] = (uint8_t)(height >= height_noise ? (height < -8 ? WATER_STAND : AIR) : STONE);
            if (chunk_input->blocks[block_idx] == AIR)
            {
                chunk_input->light_r[block_idx] = 135;
                chunk_input->light_g[block_idx] = 206;
                chunk_input->light_b[block_idx] = 235;

                chunk_input->light_sun[block_idx] = true;

                if ((height == height_noise && height >= -8) || height == -8)
                {
                    register_light_source(chunk_input->position_x, chunk_input->position_y, block_idx, true, true, true, false, 0, 0, 0);
                }
            }
            else
            {
                chunk_input->light_r[block_idx] = 0;
                chunk_input->light_g[block_idx] = 0;
                chunk_input->light_b[block_idx] = 0;
                chunk_input->light_sun[block_idx] = false;
            }
            ++height;
        }
    }
    chunk_input->generation = CHUNK_GEN_BASE;
    chunk_t *tmp_chunk_ptr = get_chunk(chunk_input->position_x, chunk_input->position_y + 1, world->chunks);
    if (NULL != tmp_chunk_ptr)
    {
        for (int i = 0; i < CHUNK_SIZE; i++)
            register_light_source(chunk_input->position_x, chunk_input->position_y + 1, i * CHUNK_SIZE + CHUNK_SIZE - 1, true, true, true, false, 0, 0, 0);
    }
}

void chunk_generate_caves(chunk_t *chunk_input)
{
    float start_x = chunk_input->position_x * CHUNK_SIZE;
    float noise;
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        float start_y = chunk_input->position_y * CHUNK_SIZE;
        for (int y = 0; y < CHUNK_SIZE; y++)
        {
            if (STONE != chunk_input->blocks[x * CHUNK_SIZE + y])
                continue;
            noise = fnlGetNoise2D(chunk_input->noise_generator, (float)x + start_x, (float)y + start_y);
            if (noise > 0.8f)
            {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
            noise = fnlGetNoise2D(chunk_input->noise_generator, ((float)x + start_x + 8192.f) / 2.f, ((float)y + start_y) / 2.5f);
            if (-0.05f < noise && noise < 0.05f)
            {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
            noise = fnlGetNoise2D(chunk_input->noise_generator, ((float)x + start_x - 8192.f) * 2.f, ((float)y + start_y) * 2.f);
            if (-0.1f < noise && noise < 0.1f)
            {
                chunk_input->blocks[x * CHUNK_SIZE + y] = (uint8_t)AIR;
                continue;
            }
        }
    }
    chunk_input->generation = CHUNK_GEN_CAVES;
}

_Bool generate_next(chunk_t *chunk)
{
    return 1;
}

void destroy_world(world_t *world_input)
{
    world_input->worker_running = false;
    extern fifo_world_gen_t *fifo_world_gen;
    pthread_mutex_lock(&fifo_world_gen->mutex);
    pthread_cond_signal(&fifo_world_gen->cond);
    pthread_mutex_unlock(&fifo_world_gen->mutex);
    pthread_join(world_input->worker_thread, NULL);

    for (size_t i = 0; i < HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE; i++)
    {
        chunk_t *chunk = world_input->chunks->buckets[i].chunk_ptr;
        if (chunk != NULL)
        {
            save_chunk(chunk);
        }
    }

    for (size_t i = 0; i < HASH_TABLE_BUCKETS * HASH_TABLE_BUCKET_SIZE; i++)
    {
        chunk_t *chunk = world_input->chunks->buckets[i].chunk_ptr;
        if (chunk != NULL)
        {
            destroy_chunk(chunk);
        }
    }

    destroy_hash_table(world_input->chunks);
    free(world_input);
}

void destroy_chunk(chunk_t *chunk_input)
{
    if (chunk_input->texture.id != 0)
    {
        UnloadRenderTexture(chunk_input->texture);
    }
    free(chunk_input);
    atomic_fetch_sub(&Chunks_Amount, 1);
}

void save_chunk(chunk_t *chunk_input)
{
    if (!chunk_input)
        return;

    extern world_t *world;

    int rx = (int)(chunk_input->position_x / 32);
    int ry = (int)(chunk_input->position_y / 32);
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "worlds/%s/region_%d_%d", world->name, rx, ry);
    sanitize_filename(dir_path);
    if (!directory_exists(dir_path))
    {
        MKDIR(dir_path);
    }

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/chunk_%" PRId64 "_%" PRId64 ".dat", dir_path,
             chunk_input->position_x, chunk_input->position_y);
    sanitize_filename(file_path);

    FILE *f = fopen(file_path, "wb");
    if (!f)
    {
        fprintf(stderr, "Failed to save chunk (%" PRId64 ", %" PRId64 ")\n", chunk_input->position_x, chunk_input->position_y);
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

    fwrite(&chunk_input->light_r, sizeof(chunk_input->light_r), 1, f);
    fwrite(&chunk_input->light_g, sizeof(chunk_input->light_g), 1, f);
    fwrite(&chunk_input->light_b, sizeof(chunk_input->light_b), 1, f);
    fwrite(&chunk_input->light_sun, sizeof(chunk_input->light_sun), 1, f);
    fwrite(&chunk_input->is_source, sizeof(chunk_input->is_source), 1, f);

    fclose(f);
}

void register_light_source(int64_t chunk_x, int64_t chunk_y, uint8_t block_idx, bool r, bool g, bool b, bool force_color, uint8_t r_input, uint8_t g_input, uint8_t b_input)
{
    light_update_slot_t *slot = &world->fifo_lighting_data[world->lighting_end];
    slot->block_idx = block_idx;
    slot->chunk_x = chunk_x;
    slot->chunk_y = chunk_y;
    slot->force_set = force_color;

    slot->dirty_r = r;
    slot->dirty_g = g;
    slot->dirty_b = b;

    if (force_color)
    {
        slot->force_r = r_input;
        slot->force_g = g_input;
        slot->force_b = b_input;
    }
    world->lighting_end = (world->lighting_end + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
}

void process_light_sources(uint32_t microseconds) {
    double start_time = GetTime();
    double seconds = microseconds / 1e6;
    uint32_t did_amount = 0;
    uint32_t iters_amount = 0;
    chunk_t *chunk_ptr, *tmp_chunk_ptr;
    uint8_t prev_r, prev_g, prev_b;
    uint8_t new_r, new_g, new_b;
    int block_x, block_y, new_block_x, new_block_y;
    uint16_t block_idx, new_block_idx;
    Color attenuation;
    int64_t chunk_x, chunk_y, new_chunk_x, new_chunk_y;
    int8_t dx, dy;
    bool update_r, update_g, update_b;
    light_update_slot_t* light_slot;

    const int8_t dx_list[] = {-1, 1, 0, 0};
    const int8_t dy_list[] = {0, 0, -1, 1};

    while (GetTime() - start_time <= seconds && iters_amount < LIGHT_SOURCES_MAX_AMOUNT) {
        iters_amount++;

        if (world->ligthing_start == world->lighting_end)
            return;

        light_slot = &world->fifo_lighting_data[world->ligthing_start];
        chunk_x = light_slot->chunk_x;
        chunk_y = light_slot->chunk_y;
        chunk_ptr = get_chunk(chunk_x, chunk_y, world->chunks);

        if (NULL == chunk_ptr) {
            uint32_t next_end = (world->lighting_end + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
            if (next_end != world->ligthing_start) {
                memcpy(&world->fifo_lighting_data[world->lighting_end],
                       &world->fifo_lighting_data[world->ligthing_start],
                       sizeof(light_update_slot_t));
                world->lighting_end = next_end;
            }
            world->ligthing_start = (world->ligthing_start + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
            continue;
        }

        block_idx = light_slot->block_idx;

        prev_r = chunk_ptr->light_r[block_idx];
        prev_g = chunk_ptr->light_g[block_idx];
        prev_b = chunk_ptr->light_b[block_idx];

        if (light_slot->force_set) {
            chunk_ptr->light_r[block_idx] = MAX(light_slot->force_r, prev_r);
            chunk_ptr->light_g[block_idx] = MAX(light_slot->force_g, prev_g);
            chunk_ptr->light_b[block_idx] = MAX(light_slot->force_b, prev_b);
            prev_r = chunk_ptr->light_r[block_idx];
            prev_g = chunk_ptr->light_g[block_idx];
            prev_b = chunk_ptr->light_b[block_idx];
        }

        new_r = prev_r;
        new_g = prev_g;
        new_b = prev_b;

        if (chunk_ptr->is_source[block_idx]) {
            new_r = prev_r;
            new_g = prev_g;
            new_b = prev_b;
        } else {
            attenuation = attenuation_color[chunk_ptr->blocks[block_idx]];
            if (light_slot->dirty_r) {
                new_r = (prev_r > attenuation.r) ? (prev_r - attenuation.r) : 0;
            }
            if (light_slot->dirty_g) {
                new_g = (prev_g > attenuation.g) ? (prev_g - attenuation.g) : 0;
            }
            if (light_slot->dirty_b) {
                new_b = (prev_b > attenuation.b) ? (prev_b - attenuation.b) : 0;
            }
        }

        block_x = block_idx / CHUNK_SIZE;
        block_y = block_idx % CHUNK_SIZE;

        for (int i = 0; i < 4; i++) {
            dx = dx_list[i];
            dy = dy_list[i];

            new_block_x = block_x + dx;
            new_block_y = block_y + dy;
            new_chunk_x = chunk_x;
            new_chunk_y = chunk_y;

            if (new_block_x < 0) {
                new_block_x += CHUNK_SIZE;
                new_chunk_x--;
            } else if (new_block_x >= CHUNK_SIZE) {
                new_block_x -= CHUNK_SIZE;
                new_chunk_x++;
            }
            if (new_block_y < 0) {
                new_block_y += CHUNK_SIZE;
                new_chunk_y--;
            } else if (new_block_y >= CHUNK_SIZE) {
                new_block_y -= CHUNK_SIZE;
                new_chunk_y++;
            }

            new_block_idx = (uint16_t)(new_block_x * CHUNK_SIZE + new_block_y);

            if (new_chunk_x == chunk_x && new_chunk_y == chunk_y) {
                tmp_chunk_ptr = chunk_ptr;
            } else {
                tmp_chunk_ptr = get_chunk(new_chunk_x, new_chunk_y, world->chunks);
            }

            if (NULL == tmp_chunk_ptr) {
                register_light_source(new_chunk_x, new_chunk_y, new_block_idx,
                                      light_slot->dirty_r, light_slot->dirty_g, light_slot->dirty_b,
                                      true, new_r, new_g, new_b);
                continue;
            }

            tmp_chunk_ptr->texture_generated = false;

            if (tmp_chunk_ptr->is_source[new_block_idx])
                continue;

            if (dy == -1 && chunk_ptr->light_sun[block_idx] && tmp_chunk_ptr->blocks[new_block_idx] == AIR) {
                update_r = light_slot->dirty_r && (prev_r > tmp_chunk_ptr->light_r[new_block_idx]);
                update_g = light_slot->dirty_g && (prev_g > tmp_chunk_ptr->light_g[new_block_idx]);
                update_b = light_slot->dirty_b && (prev_b > tmp_chunk_ptr->light_b[new_block_idx]);

                if (update_r) tmp_chunk_ptr->light_r[new_block_idx] = prev_r;
                if (update_g) tmp_chunk_ptr->light_g[new_block_idx] = prev_g;
                if (update_b) tmp_chunk_ptr->light_b[new_block_idx] = prev_b;

                tmp_chunk_ptr->light_sun[new_block_idx] = true;

                if (update_r || update_g || update_b) {
                    register_light_source(new_chunk_x, new_chunk_y, new_block_idx, update_r, update_g, update_b, false, 0, 0, 0);
                }
                continue;
            }

            update_r = light_slot->dirty_r && (new_r > tmp_chunk_ptr->light_r[new_block_idx]);
            update_g = light_slot->dirty_g && (new_g > tmp_chunk_ptr->light_g[new_block_idx]);
            update_b = light_slot->dirty_b && (new_b > tmp_chunk_ptr->light_b[new_block_idx]);

            if (!update_r && !update_g && !update_b)
                continue;

            if (update_r) tmp_chunk_ptr->light_r[new_block_idx] = new_r;
            if (update_g) tmp_chunk_ptr->light_g[new_block_idx] = new_g;
            if (update_b) tmp_chunk_ptr->light_b[new_block_idx] = new_b;

            register_light_source(new_chunk_x, new_chunk_y, new_block_idx,
                                  update_r, update_g, update_b,
                                  false, 0, 0, 0);
        }

        world->ligthing_start = (world->ligthing_start + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
        did_amount++;
    }
}