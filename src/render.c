#include "includes.h"
#include "world.h"
#include "camera.h"
#include "fifo.h"
#include "render.h"
#include <stdatomic.h>

static int texture_gen_quota = 32; /**< Max textures to generate per frame. */

/**
 * @brief Render a chunk using its pre‑generated texture.
 */
#if OLD_RENDERER == 0
void render_chunk(float origin_x, float origin_y, struct Chunk *chunk, float zoom)
{
    DrawTexturePro(
        chunk->texture.texture,
        (Rectangle){0, 0, chunk->texture.texture.width, chunk->texture.texture.height},
        (Rectangle){origin_x, origin_y, CHUNK_SIZE * CHUNK_SIZE * zoom, CHUNK_SIZE * CHUNK_SIZE * zoom},
        (Vector2){0, 0}, 0, WHITE);
}
#else
void render_chunk(float origin_x, float origin_y, struct Chunk *chunk, float zoom)
{
    Vector2 origin = {0, 0};
    float zoom_times_16 = zoom * 16;
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int y = 0; y < CHUNK_SIZE; y++)
        {
            DrawTexturePro(Textures_Atlas, (Rectangle){chunk->blocks[y + x * CHUNK_SIZE] * 16, 0, 16, 16}, (Rectangle){origin_x + x * zoom_times_16, origin_y + (CHUNK_SIZE - 1 - y) * zoom_times_16, zoom_times_16, zoom_times_16}, (Vector2){0, 0}, 0, (Color){chunk->light_r[y + x * CHUNK_SIZE], chunk->light_g[y + x * CHUNK_SIZE], chunk->light_b[y + x * CHUNK_SIZE], 255});
        }
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "(%" PRId64 ", %" PRId64 ")", chunk->position_x, chunk->position_y);
    DrawText(buffer, (int)origin_x, (int)origin_y, (int)30 * zoom, RED);
}
#endif

/**
 * @brief Generate the render texture for a chunk.
 */
void generate_chunk_texture_render(chunk_t *chunk)
{
    if (!chunk->texture_generated)
    {
        if (chunk->texture.id == 0)
        {
            chunk->texture = LoadRenderTexture(16 * CHUNK_SIZE, 16 * CHUNK_SIZE);
            if (chunk->texture.id == 0)
                return;
        }
        BeginTextureMode(chunk->texture);
        ClearBackground(BLANK);
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int y = 0; y < CHUNK_SIZE; y++)
            {
                int block_idx = y + x * CHUNK_SIZE;
                int block_id = chunk->blocks[block_idx];
                /* Draw each block with its current light colour. */
                DrawTexturePro(Textures_Atlas, (Rectangle){block_id * 16, 16, 16, -16}, (Rectangle){x * 16, y * 16, 16, 16}, (Vector2){0, 0}, 0, (Color){chunk->light_r[block_idx], chunk->light_g[block_idx], chunk->light_b[block_idx], 255});
            }
        }
        EndTextureMode();
        chunk->texture_generated = true;
    }
}

/**
 * @brief Render a grey placeholder for a chunk whose texture is not ready.
 */
static void render_chunk_placeholder(float origin_x, float origin_y, float zoom)
{
    DrawRectangle((int)origin_x, (int)origin_y, (int)(CHUNK_SIZE * CHUNK_SIZE * zoom) + 1, (int)(CHUNK_SIZE * CHUNK_SIZE * zoom) + 1, (Color){20, 20, 40, 255});
}

/**
 * @brief Render the visible world.
 * @details Determines visible chunks, requests missing ones (limited to 16 per frame),
 *          generates textures for loaded chunks (up to quota), and draws them.
 */
void render_world(world_t *world, camera_impl_t camera, int center_x, int center_y, int width, int height)
{
    float scaled_chunk_size = CHUNK_SIZE * camera.zoom;

    float half_w = (width / 2.0f) / (camera.zoom * CHUNK_SIZE);
    float half_h = (height / 2.0f) / (camera.zoom * CHUNK_SIZE);

    int64_t min_x = (int64_t)floorf(camera.x - half_w);
    int64_t max_x = (int64_t)ceilf(camera.x + half_w);
    int64_t min_y = (int64_t)floorf(camera.y - half_h);
    int64_t max_y = (int64_t)ceilf(camera.y + half_h);

    size_t counter_temporary = 0;
    int request_count = 0;
    int texture_gen_count = 0;

    /* First pass: request missing chunks (up to 16). */
    for (int64_t x = min_x; x <= max_x && request_count < 16; x++)
    {
        for (int64_t y = min_y; y <= max_y && request_count < 16; y++)
        {
            chunk_t *chunk = get_chunk(x, y, world->chunks);
            if (NULL == chunk)
            {
                int fifo_usage;
                pthread_mutex_lock(&fifo_world_gen->mutex);
                fifo_usage = fifo_world_gen->end - fifo_world_gen->start;
                if (fifo_usage < 0)
                    fifo_usage += FIFO_WORLD_GEN_SIZE;
                pthread_mutex_unlock(&fifo_world_gen->mutex);
                if (fifo_usage < FIFO_WORLD_GEN_SIZE - 8)
                {
                    request_gen(x, y, fifo_world_gen);
                    request_count++;
                }
            }
        }
    }

    /* Second pass: render all visible chunks. */
    for (int64_t x = min_x; x <= max_x; x++)
    {
        for (int64_t y = min_y; y <= max_y; y++)
        {
            chunk_t *chunk = get_chunk(x, y, world->chunks);
            if (NULL == chunk)
                continue;

            /* Generate texture if not yet done (limited quota). */
            if (!chunk->texture_generated && texture_gen_count < texture_gen_quota)
            {
                generate_chunk_texture_render(chunk);
                texture_gen_count++;
            }

            float screen_x = (chunk->position_x - camera.x) * camera.zoom * CHUNK_SIZE + center_x;
            float screen_y = (camera.y - chunk->position_y) * camera.zoom * CHUNK_SIZE + center_y;

            /* Culling: skip if completely outside viewport. */
            if (screen_x + scaled_chunk_size < center_x - width / 2.0f ||
                screen_x > center_x + width / 2.0f ||
                screen_y + scaled_chunk_size < center_y - height / 2.0f ||
                screen_y > center_y + height / 2.0f)
                continue;

            if (chunk->texture_generated)
                render_chunk(screen_x, screen_y, chunk, camera.zoom / CHUNK_SIZE);
            else
                render_chunk_placeholder(screen_x, screen_y, camera.zoom / CHUNK_SIZE);

            ++counter_temporary;
        }
    }

    /* Draw statistics. */
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Drawn chunks: %zu", counter_temporary);
    DrawText(buffer, 0, 0, 20, DARKBLUE);
    DrawFPS(0, 25);

    int amount = 0;
    pthread_mutex_lock(&fifo_world_gen->mutex);
    amount = fifo_world_gen->end - fifo_world_gen->start;
    if (amount < 0)
        amount += FIFO_WORLD_GEN_SIZE;
    pthread_mutex_unlock(&fifo_world_gen->mutex);
    snprintf(buffer, sizeof(buffer), "Waiting chunks amount: %i", amount);
    DrawText(buffer, 0, 50, 20, BLUE);

    uint32_t chunks_total = atomic_load(&Chunks_Amount);
    snprintf(buffer, sizeof(buffer), "Chunks amount: %" PRIu32, chunks_total);
    DrawText(buffer, 0, 75, 20, RED);

    snprintf(buffer, sizeof(buffer), "Len of lighting queue: %" PRId32, world->lighting_end - world->lighting_start);
    DrawText(buffer, 0, 100, 20, RED);
}