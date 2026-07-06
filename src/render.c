#include "includes.h"
#include "world.h"
#include "camera.h"
#include "fifo.h"

#include "render.h"

#if OLD_RENDERER == 0
void render_chunk(float origin_x, float origin_y, struct Chunk* chunk, float zoom) {
    DrawTexturePro(
        chunk->texture.texture,
        (Rectangle){ 0, 0, chunk->texture.texture.width, chunk->texture.texture.height },
        (Rectangle){ origin_x, origin_y, CHUNK_SIZE * 16 * zoom, CHUNK_SIZE * 16 * zoom },
        (Vector2){ 0, 0 }, 0, WHITE
    );
}
#else
void render_chunk(float origin_x, float origin_y, struct Chunk* chunk, float zoom) {
    Vector2 origin = {0, 0};
    float zoom_times_16 = zoom * 16;
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            DrawTexturePro(Textures_Atlas, (Rectangle){chunk->blocks[y + x * CHUNK_SIZE] * 16, 0, 16, 16}, (Rectangle){origin_x + x * zoom_times_16, origin_y + (CHUNK_SIZE - 1 - y) * zoom_times_16, zoom_times_16, zoom_times_16}, (Vector2){0, 0}, 0, WHITE);
        }
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "(%" PRId64 ", %" PRId64")", chunk->position_x, chunk->position_y);
    DrawText(buffer, (int)origin_x, (int)origin_y, (int)30 * zoom, RED);
}
#endif


void generate_chunk_texture_render(struct Chunk* chunk) {
    BeginTextureMode(chunk->texture);
        ClearBackground(BLANK);
        
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                int block_id = chunk->blocks[y + x * CHUNK_SIZE];
                
                DrawTexturePro(Textures_Atlas, (Rectangle){block_id * 16, 16, 16, -16}, (Rectangle){x * 16, y * 16, 16, 16}, (Vector2){0, 0}, 0, WHITE);
            }
        }
    EndTextureMode();
}

void render_world(struct World* world, struct CameraImpl camera, int center_x, int center_y, int width, int height) {
    float scaled_chunk_size = CHUNK_SIZE * camera.zoom;
    
    float half_w = (width / 2.0f) / (camera.zoom * CHUNK_SIZE);
    float half_h = (height / 2.0f) / (camera.zoom * CHUNK_SIZE);

    int64_t min_x = (int64_t)floorf(camera.x - half_w);
    int64_t max_x = (int64_t)ceilf(camera.x + half_w);
    int64_t min_y = (int64_t)floorf(camera.y - half_h);
    int64_t max_y = (int64_t)ceilf(camera.y + half_h);

    size_t counter_temporary = 0;

    for (int64_t x = min_x; x <= max_x; x++) {
        for (int64_t y = min_y; y <= max_y; y++) {
            struct Chunk* chunk = get_chunk(x, y, world->chunks);
            if (NULL == chunk) {
                request_gen(x, y, fifo_world_gen);
                continue;
            }

            float screen_x = (chunk->position_x - camera.x) * camera.zoom * CHUNK_SIZE + center_x;
            float screen_y = (camera.y - chunk->position_y) * camera.zoom * CHUNK_SIZE + center_y;

            if (screen_x + scaled_chunk_size < center_x - width / 2.0f || 
                screen_x > center_x + width / 2.0f ||
                screen_y + scaled_chunk_size < center_y - height / 2.0f || 
                screen_y > center_y + height / 2.0f) {
                continue;
            }

            render_chunk(screen_x, screen_y, chunk, camera.zoom / CHUNK_SIZE);
            ++counter_temporary;
        }
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Drawn chunks: %zu", counter_temporary);
    DrawText(buffer, 0, 0, 20, DARKBLUE);
    DrawFPS(0, 25);
    int amount = fifo_world_gen->end - fifo_world_gen->start;
    if (amount < 0) amount += FIFO_WORLD_GEN_SIZE;
    snprintf(buffer, sizeof(buffer), "Waiting chunks amount: %i", amount);
    DrawText(buffer, 0, 50, 20, BLUE);
    snprintf(buffer, sizeof(buffer), "Chunks amount: %" PRIu32, Chunks_Amount);
    DrawText(buffer, 0, 75, 20, RED);
} 