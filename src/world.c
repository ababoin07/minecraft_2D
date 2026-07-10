#include "includes.h"
#include "world.h"
#include "data.h"
#include "FastNoiseLite.h"
#include "render.h"
#include "fifo.h"
#include "files_api.h"
#include <pthread.h>
#include <stdatomic.h>

/** Global pending light table. */
pending_light_table_t pending_light_table;

/**
 * @brief Replace '-' with '_' in a path string.
 */
void sanitize_filename(char *path)
{
    if (path == NULL)
        return;
    for (int i = 0; path[i] != '\0'; i++)
    {
        if (path[i] == '-')
            path[i] = '_';
    }
}

/**
 * @brief Build the full file system path for a chunk.
 * @param world Pointer to the world.
 * @param cx,cy Chunk coordinates.
 * @param out_path Buffer to write the path.
 * @param out_size Size of the buffer.
 */
static void build_chunk_path(const world_t *world, int64_t cx, int64_t cy, char *out_path, size_t out_size)
{
    int rx = (int)(cx / 32);
    int ry = (int)(cy / 32);
    snprintf(out_path, out_size, "worlds/%s/region_%d_%d/chunk_%" PRId64 "_%" PRId64 ".dat", world->name, rx, ry, cx, cy);
    sanitize_filename(out_path);
}

/**
 * @brief Load a chunk from disk.
 * @return Pointer to loaded chunk, or NULL if not found.
 */
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

    /* Read block data and light arrays. */
    if (fread(chunk->blocks, sizeof(chunk->blocks), 1, f) != 1 ||
        fread(chunk->light_r, sizeof(chunk->light_r), 1, f) != 1 ||
        fread(chunk->light_g, sizeof(chunk->light_g), 1, f) != 1 ||
        fread(chunk->light_b, sizeof(chunk->light_b), 1, f) != 1 ||
        fread(chunk->light_sun, sizeof(chunk->light_sun), 1, f) != 1 ||
        fread(chunk->is_source, sizeof(chunk->is_source), 1, f) != 1)
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

/**
 * @brief Background worker thread: processes chunk generation requests.
 */
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

        /* Try to load from disk; if not found, generate from scratch. */
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

/**
 * @brief Create a new world.
 */
world_t *create_world(int seed, char *name)
{
    world_t *ptr = malloc(sizeof(world_t));
    if (NULL == ptr)
        return NULL;
    if (!directory_exists("worlds"))
        MKDIR("worlds");
    pthread_mutex_init(&ptr->lighting_mutex, NULL);
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

    ptr->lighting_start = 0;
    ptr->lighting_end = 0;

    ptr->worker_running = true;
    pthread_create(&ptr->worker_thread, NULL, worker_main, ptr);
    return ptr;
}

/**
 * @brief Create a new (empty) chunk.
 */
chunk_t *create_chunk(int64_t offset_x, int64_t offset_y, fnl_state *noise_generator)
{
    chunk_t *ptr = malloc(sizeof(chunk_t));
    if (NULL == ptr)
        return NULL;
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
}

/**
 * @brief Generate base terrain using 2D noise.
 */
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
                /* Set sky light (blue tint). */
                chunk_input->light_r[block_idx] = 135;
                chunk_input->light_g[block_idx] = 206;
                chunk_input->light_b[block_idx] = 235;
                chunk_input->light_sun[block_idx] = true;
                /* Register sunlight sources at the surface. */
                if ((height == height_noise && height >= -8) || height == -8)
                    register_light_source(chunk_input->position_x, chunk_input->position_y, block_idx, true, true, true, false, 0, 0, 0);
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

    /* Register sunlight for adjacent chunks’ surface edges. */
    if (NULL != get_chunk(chunk_input->position_x, chunk_input->position_y + 1, world->chunks))
        for (int i = 0; i < CHUNK_SIZE; i++)
            register_light_source(chunk_input->position_x, chunk_input->position_y + 1, i * CHUNK_SIZE, true, true, true, false, 0, 0, 0);
    if (NULL != get_chunk(chunk_input->position_x, chunk_input->position_y - 1, world->chunks))
        for (int i = 0; i < CHUNK_SIZE; i++)
            register_light_source(chunk_input->position_x, chunk_input->position_y - 1, i * CHUNK_SIZE + CHUNK_SIZE - 1, true, true, true, false, 0, 0, 0);
    if (NULL != get_chunk(chunk_input->position_x + 1, chunk_input->position_y, world->chunks))
        for (int i = 0; i < CHUNK_SIZE; i++)
            register_light_source(chunk_input->position_x + 1, chunk_input->position_y + 1, i, true, true, true, false, 0, 0, 0);
    if (NULL != get_chunk(chunk_input->position_x - 1, chunk_input->position_y, world->chunks))
        for (int i = 0; i < CHUNK_SIZE; i++)
            register_light_source(chunk_input->position_x - 1, chunk_input->position_y + 1, (CHUNK_SIZE - 1) * CHUNK_SIZE + i, true, true, true, false, 0, 0, 0);
}

/**
 * @brief Carve caves using several octaves of noise.
 */
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

/**
 * @brief Placeholder for further generation steps.
 */
_Bool generate_next(chunk_t *chunk)
{
    return 1;
}

/**
 * @brief Destroy the world, saving all chunks and stopping the worker.
 */
void destroy_world(world_t *world_input)
{
    world_input->worker_running = false;
    extern fifo_world_gen_t *fifo_world_gen;
    pthread_mutex_lock(&fifo_world_gen->mutex);
    pthread_cond_signal(&fifo_world_gen->cond);
    pthread_mutex_unlock(&fifo_world_gen->mutex);
    pthread_join(world_input->worker_thread, NULL);

    /* Save and destroy all chunks in the hash table. */
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
    pthread_mutex_destroy(&world_input->lighting_mutex);
    free(world_input);
}

/**
 * @brief Destroy a chunk (free memory and render texture).
 */
void destroy_chunk(chunk_t *chunk_input)
{
    if (chunk_input->texture.id != 0)
        UnloadRenderTexture(chunk_input->texture);
    free(chunk_input);
    atomic_fetch_sub(&Chunks_Amount, 1);
}

/**
 * @brief Save a chunk to disk.
 */
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
        MKDIR(dir_path);

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

/**
 * @brief Register a light source update (push onto the light FIFO).
 */
void register_light_source(int64_t chunk_x, int64_t chunk_y, uint8_t block_idx,
                           bool r, bool g, bool b, bool force_color,
                           uint8_t r_input, uint8_t g_input, uint8_t b_input)
{
    pthread_mutex_lock(&world->lighting_mutex);

    uint32_t next_end = (world->lighting_end + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
    if (next_end == world->lighting_start)  // FIFO full
    {
        pthread_mutex_unlock(&world->lighting_mutex);
        return;
    }

    light_update_slot_t *slot = &world->fifo_lighting_data[world->lighting_end];
    slot->chunk_x = chunk_x;
    slot->chunk_y = chunk_y;
    slot->block_idx = block_idx;
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

    world->lighting_end = next_end;
    pthread_mutex_unlock(&world->lighting_mutex);
}

/**
 * @brief Add a pending light update for a chunk not yet loaded.
 * @details The update is stored in a hash table (by chunk coordinates) as a linked list.
 */
void add_pending_light(int64_t cx, int64_t cy, uint8_t block_idx, bool dirty_r, bool dirty_g, bool dirty_b, bool force_set, uint8_t force_r, uint8_t force_g, uint8_t force_b)
{
    pending_light_node_t *node = malloc(sizeof(pending_light_node_t));
    if (!node)
    {
        fprintf(stderr, "Failed to allocate pending light node\n");
        return;
    }
    *node = (pending_light_node_t){
        .chunk_x = cx,
        .chunk_y = cy,
        .block_idx = block_idx,
        .dirty_r = dirty_r,
        .dirty_g = dirty_g,
        .dirty_b = dirty_b,
        .force_set = force_set,
        .force_r = force_r,
        .force_g = force_g,
        .force_b = force_b,
        .next = NULL};

    int hash = (hash_pair(cx, cy) & (PENDING_LIGHT_BUCKETS - 1));
    pthread_mutex_lock(&pending_light_table.mutex);
    node->next = pending_light_table.buckets[hash];
    pending_light_table.buckets[hash] = node;
    pthread_mutex_unlock(&pending_light_table.mutex);
}

/**
 * @brief Apply all pending light updates for a specific chunk.
 * @details The updates are removed from the pending table and fed into the main light FIFO.
 */
void apply_pending_lights_for_chunk(chunk_t *chunk)
{
    if (!chunk)
        return;
    int64_t cx = chunk->position_x, cy = chunk->position_y;
    int hash = (hash_pair(cx, cy) & (PENDING_LIGHT_BUCKETS - 1));

    pending_light_node_t *local_list = NULL;

    /* Atomically extract the linked list for this chunk. */
    pthread_mutex_lock(&pending_light_table.mutex);
    pending_light_node_t **prev = &pending_light_table.buckets[hash];
    pending_light_node_t *current = *prev;
    while (current)
    {
        if (current->chunk_x == cx && current->chunk_y == cy)
        {
            *prev = current->next;
            current->next = local_list;
            local_list = current;
            current = *prev;
        }
        else
        {
            prev = &current->next;
            current = current->next;
        }
    }
    pthread_mutex_unlock(&pending_light_table.mutex);

    /* Re‑register each update (now the chunk is loaded). */
    pending_light_node_t *node = local_list;
    while (node)
    {
        pending_light_node_t *next = node->next;
        register_light_source(cx, cy, node->block_idx, node->dirty_r, node->dirty_g, node->dirty_b, node->force_set, node->force_r, node->force_g, node->force_b);
        free(node);
        node = next;
    }
}

/**
 * @brief Free all pending light nodes and destroy the mutex.
 */
void cleanup_pending_light_table(void)
{
    for (int i = 0; i < PENDING_LIGHT_BUCKETS; i++)
    {
        pending_light_node_t *node = pending_light_table.buckets[i];
        while (node)
        {
            pending_light_node_t *next = node->next;
            free(node);
            node = next;
        }
        pending_light_table.buckets[i] = NULL;
    }
    pthread_mutex_destroy(&pending_light_table.mutex);
}

/**
 * @brief Process light updates from the FIFO, propagating light to neighbours.
 * @param microseconds Maximum time (μs) to spend processing.
 * @details For each update:
 *          - If the chunk is not loaded, the update is moved to pending_light_table.
 *          - Otherwise, the block's light is updated (using attenuation or forced colour).
 *          - Then light is propagated to the four orthogonal neighbours (if they are brighter).
 *          - This is the core of the flood‑fill lighting algorithm.
 */
void process_light_sources(uint32_t microseconds)
{
    double start_time = GetTime();
    double seconds = microseconds / 1e6;
    uint32_t processed = 0;

    pthread_mutex_lock(&world->lighting_mutex);

    while (world->lighting_start != world->lighting_end && (GetTime() - start_time) <= seconds)
    {
        light_update_slot_t *slot = &world->fifo_lighting_data[world->lighting_start];
        int64_t chunk_x = slot->chunk_x;
        int64_t chunk_y = slot->chunk_y;
        uint8_t block_idx = slot->block_idx;

        chunk_t *chunk = get_chunk(chunk_x, chunk_y, world->chunks);
        if (NULL == chunk)
        {
            /* Chunk not loaded → defer update. */
            pthread_mutex_unlock(&world->lighting_mutex);
            add_pending_light(chunk_x, chunk_y, block_idx,
                              slot->dirty_r, slot->dirty_g, slot->dirty_b,
                              slot->force_set, slot->force_r, slot->force_g, slot->force_b);
            pthread_mutex_lock(&world->lighting_mutex);
            world->lighting_start = (world->lighting_start + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
            continue;
        }

        uint8_t prev_r = chunk->light_r[block_idx];
        uint8_t prev_g = chunk->light_g[block_idx];
        uint8_t prev_b = chunk->light_b[block_idx];

        /* Apply forced colour if requested. */
        if (slot->force_set)
        {
            chunk->light_r[block_idx] = MAX(slot->force_r, prev_r);
            chunk->light_g[block_idx] = MAX(slot->force_g, prev_g);
            chunk->light_b[block_idx] = MAX(slot->force_b, prev_b);
            prev_r = chunk->light_r[block_idx];
            prev_g = chunk->light_g[block_idx];
            prev_b = chunk->light_b[block_idx];
        }

        uint8_t new_r = prev_r, new_g = prev_g, new_b = prev_b;
        if (!chunk->is_source[block_idx])
        {
            /* Apply attenuation (subtract block colour). */
            Color att = attenuation_color[chunk->blocks[block_idx]];
            if (slot->dirty_r)
                new_r = (prev_r > att.r) ? (prev_r - att.r) : 0;
            if (slot->dirty_g)
                new_g = (prev_g > att.g) ? (prev_g - att.g) : 0;
            if (slot->dirty_b)
                new_b = (prev_b > att.b) ? (prev_b - att.b) : 0;
        }

        int block_x = block_idx / CHUNK_SIZE;
        int block_y = block_idx % CHUNK_SIZE;

        /* Neighbour offsets. */
        const int8_t dx_list[] = {-1, 1, 0, 0};
        const int8_t dy_list[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; i++)
        {
            int8_t dx = dx_list[i];
            int8_t dy = dy_list[i];
            int new_block_x = block_x + dx;
            int new_block_y = block_y + dy;
            int64_t new_chunk_x = chunk_x;
            int64_t new_chunk_y = chunk_y;

            /* Wrap around chunk borders. */
            if (new_block_x < 0)
            {
                new_block_x += CHUNK_SIZE;
                new_chunk_x--;
            }
            else if (new_block_x >= CHUNK_SIZE)
            {
                new_block_x -= CHUNK_SIZE;
                new_chunk_x++;
            }
            if (new_block_y < 0)
            {
                new_block_y += CHUNK_SIZE;
                new_chunk_y--;
            }
            else if (new_block_y >= CHUNK_SIZE)
            {
                new_block_y -= CHUNK_SIZE;
                new_chunk_y++;
            }

            uint16_t new_block_idx = new_block_x * CHUNK_SIZE + new_block_y;

            chunk_t *target_chunk = (new_chunk_x == chunk_x && new_chunk_y == chunk_y) ? chunk : get_chunk(new_chunk_x, new_chunk_y, world->chunks);
            if (target_chunk == NULL)
            {
                /* Neighbour chunk not loaded → register a new light source for it. */
                pthread_mutex_unlock(&world->lighting_mutex);
                register_light_source(new_chunk_x, new_chunk_y, new_block_idx, slot->dirty_r, slot->dirty_g, slot->dirty_b, true, new_r, new_g, new_b);
                pthread_mutex_lock(&world->lighting_mutex);
                continue;
            }

            target_chunk->texture_generated = false;  // mark for re‑render

            if (target_chunk->is_source[new_block_idx])
                continue;  // light sources are fixed

            /* Sunlight propagation (only upwards, dy == -1). */
            if (dy == -1 && chunk->light_sun[block_idx] && target_chunk->blocks[new_block_idx] == AIR)
            {
                bool update_r = slot->dirty_r && (prev_r > target_chunk->light_r[new_block_idx]);
                bool update_g = slot->dirty_g && (prev_g > target_chunk->light_g[new_block_idx]);
                bool update_b = slot->dirty_b && (prev_b > target_chunk->light_b[new_block_idx]);
                if (update_r)
                    target_chunk->light_r[new_block_idx] = prev_r;
                if (update_g)
                    target_chunk->light_g[new_block_idx] = prev_g;
                if (update_b)
                    target_chunk->light_b[new_block_idx] = prev_b;
                target_chunk->light_sun[new_block_idx] = true;
                if (update_r || update_g || update_b)
                {
                    pthread_mutex_unlock(&world->lighting_mutex);
                    register_light_source(new_chunk_x, new_chunk_y, new_block_idx, update_r, update_g, update_b, false, 0, 0, 0);
                    pthread_mutex_lock(&world->lighting_mutex);
                }
                continue;
            }

            /* Normal light propagation (only if the neighbour is dimmer). */
            bool update_r = slot->dirty_r && (new_r > target_chunk->light_r[new_block_idx]);
            bool update_g = slot->dirty_g && (new_g > target_chunk->light_g[new_block_idx]);
            bool update_b = slot->dirty_b && (new_b > target_chunk->light_b[new_block_idx]);
            if (!update_r && !update_g && !update_b)
                continue;

            if (update_r)
                target_chunk->light_r[new_block_idx] = new_r;
            if (update_g)
                target_chunk->light_g[new_block_idx] = new_g;
            if (update_b)
                target_chunk->light_b[new_block_idx] = new_b;

            pthread_mutex_unlock(&world->lighting_mutex);
            register_light_source(new_chunk_x, new_chunk_y, new_block_idx, update_r, update_g, update_b, false, 0, 0, 0);
            pthread_mutex_lock(&world->lighting_mutex);
        }

        world->lighting_start = (world->lighting_start + 1) & (LIGHT_SOURCES_MAX_AMOUNT - 1);
        processed++;
    }

    pthread_mutex_unlock(&world->lighting_mutex);
}