#ifndef WORLD_H
#define WORLD_H

#include <pthread.h>
#include <stdbool.h>
#include "includes.h"
#include "FastNoiseLite.h"
#include "hash_table.h"

/**
 * @brief Generation stages for a chunk.
 */
enum ChunkGenSteps
{
    CHUNK_GEN_UNSTARTED,   /**< Not generated at all. */
    CHUNK_GEN_BASE,        /**< Base terrain (heightmap). */
    CHUNK_GEN_CAVES,       /**< Caves carved. */
    CHUNK_GEN_STRUCTURES,  /**< (Reserved) structures. */
    CHUNK_GEN_VEGETATION,  /**< (Reserved) vegetation. */
    CHUNK_GEN_FINISHED,    /**< Fully generated. */
    CHUNK_GEN_AMOUT
};

/**
 * @brief Main chunk data structure.
 */
struct Chunk
{
    chunk_gen_steps_t generation;        /**< Current generation stage. */
    uint8_t blocks[CHUNK_SIZE * CHUNK_SIZE]; /**< Block IDs (0 = AIR). */
    uint8_t light_r[CHUNK_SIZE * CHUNK_SIZE]; /**< Red light level (0‑255). */
    uint8_t light_g[CHUNK_SIZE * CHUNK_SIZE]; /**< Green light level. */
    uint8_t light_b[CHUNK_SIZE * CHUNK_SIZE]; /**< Blue light level. */
    bool light_sun[CHUNK_SIZE * CHUNK_SIZE]; /**< True if the block receives sunlight. */
    bool is_source[CHUNK_SIZE * CHUNK_SIZE]; /**< True if the block is an artificial light source. */
    int64_t position_x, position_y;      /**< Chunk coordinates. */
    RenderTexture2D texture;             /**< Pre‑rendered texture (16×16 pixels per block). */
    fnl_state *noise_generator;          /**< Pointer to the world's noise state. */
    bool texture_generated;              /**< Whether the texture is up‑to‑date. */
};

/**
 * @brief One entry in the lighting update FIFO.
 */
struct LightUpdateSlot
{
    int64_t chunk_x;
    int64_t chunk_y;
    uint8_t block_idx;
    uint8_t force_r, force_g, force_b;   /**< Forced colour (if force_set is true). */
    bool force_set;                      /**< If true, use forced colour instead of attenuation. */
    bool dirty_r, dirty_g, dirty_b;      /**< Which colour channels should be updated. */
};

/**
 * @brief Node in a linked list of pending light updates for chunks not yet loaded.
 */
typedef struct PendingLightNode
{
    int64_t chunk_x, chunk_y;
    uint8_t block_idx;
    bool dirty_r, dirty_g, dirty_b;
    bool force_set;
    uint8_t force_r, force_g, force_b;
    struct PendingLightNode *next;
} pending_light_node_t;

#define PENDING_LIGHT_BUCKETS 16384

/**
 * @brief Hash table of pending light updates (keyed by chunk coordinates).
 */
typedef struct PendingLightTable
{
    pending_light_node_t *buckets[PENDING_LIGHT_BUCKETS];
    pthread_mutex_t mutex;
} pending_light_table_t;

/**
 * @brief World container.
 */
struct World
{
    char *name;                          /**< World name (also used for directory). */
    int seed;                            /**< World generation seed. */
    fnl_state noise_generator;           /**< FastNoiseLite state. */
    hash_table_t *chunks;                /**< Chunk hash table. */
    pthread_t worker_thread;             /**< Background thread for chunk generation. */
    volatile bool worker_running;        /**< Set to false to stop the worker. */
    light_update_slot_t fifo_lighting_data[LIGHT_SOURCES_MAX_AMOUNT]; /**< Circular buffer for light updates. */
    uint32_t lighting_start;             /**< Read index for light FIFO. */
    uint32_t lighting_end;               /**< Write index for light FIFO. */
    pthread_mutex_t lighting_mutex;      /**< Protects the light FIFO. */
};

/* World and chunk manipulation functions. */

/**
 * @brief Replace '-' with '_' in a path string (safe for filesystem).
 * @param path String to sanitise (modified in‑place).
 */
void sanitize_filename(char *);

/**
 * @brief Create a new, uninitialised chunk.
 * @param offset_x X chunk coordinate.
 * @param offset_y Y chunk coordinate.
 * @param noise_generator Pointer to the world noise state.
 * @return Pointer to the new chunk, or NULL on failure.
 */
chunk_t *create_chunk(int64_t, int64_t, fnl_state *);

/**
 * @brief Generate the base terrain for a chunk (heightmap).
 * @param chunk Pointer to the chunk.
 */
void chunk_generate_base(chunk_t *);

/**
 * @brief Carve caves into a chunk (after base generation).
 * @param chunk Pointer to the chunk.
 */
void chunk_generate_caves(chunk_t *);

/**
 * @brief Perform the next generation step (reserved, currently always returns 1).
 * @param chunk Pointer to the chunk.
 * @return Always 1 (placeholder).
 */
_Bool generate_next(chunk_t *);

/**
 * @brief Create a new world.
 * @param seed World seed.
 * @param name World name (used for folder name).
 * @return Pointer to the new world, or NULL on failure.
 * @note Starts the background chunk generation thread.
 */
world_t *create_world(int, char *);

/**
 * @brief Destroy a world, saving all chunks and cleaning up.
 * @param world Pointer to the world.
 */
void destroy_world(world_t *);

/**
 * @brief Destroy a chunk (free memory and render texture).
 * @param chunk Pointer to the chunk.
 */
void destroy_chunk(chunk_t *);

/**
 * @brief Save a chunk to disk.
 * @param chunk Pointer to the chunk.
 */
void save_chunk(chunk_t *);

/**
 * @brief Load a chunk from disk.
 * @param world Pointer to the world.
 * @param cx X chunk coordinate.
 * @param cy Y chunk coordinate.
 * @return Pointer to the loaded chunk, or NULL if not found.
 */
chunk_t *load_chunk(world_t *, int64_t, int64_t);

/**
 * @brief Register a light source update.
 * @param chunk_x X chunk coordinate.
 * @param chunk_y Y chunk coordinate.
 * @param block_idx Index of the block within the chunk.
 * @param r,g,b Whether to update red/green/blue channels.
 * @param force_color If true, use r_input,g_input,b_input as absolute values.
 * @param r_input,g_input,b_input Forced colour values.
 * @note The update is pushed into the light FIFO.
 */
void register_light_source(int64_t, int64_t, uint8_t, bool, bool, bool, bool, uint8_t, uint8_t, uint8_t);

/**
 * @brief Process up to a given number of pending light updates (time‑limited).
 * @param microseconds Maximum time to spend (in microseconds).
 */
void process_light_sources(uint32_t);

/**
 * @brief Add a pending light update for a chunk that is not currently loaded.
 * @param cx,cy Chunk coordinates.
 * @param block_idx Block index.
 * @param dirty_r,g,b Which channels to update.
 * @param force_set Whether to force colour.
 * @param force_r,g,b Forced colour.
 * @note The update is stored in the pending_light_table.
 */
void add_pending_light(int64_t, int64_t, uint8_t, bool, bool, bool, bool, uint8_t, uint8_t, uint8_t);

/**
 * @brief Apply all pending light updates for a specific chunk.
 * @param chunk Pointer to the chunk that has just been loaded.
 */
void apply_pending_lights_for_chunk(chunk_t *);

/**
 * @brief Free all pending light nodes and destroy the mutex.
 */
void cleanup_pending_light_table(void);

#endif // WORLD_H