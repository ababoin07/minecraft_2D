#include "includes.h"
#include "world.h"
#include "camera.h"

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
    float chunk_size = CHUNK_SIZE * camera.zoom;
    size_t chunk_idx = (size_t)-1;
    size_t counter_temporary = 0;
    for (int64_t x = 0; x < LOADED_WORLD_WIDTH; x++) {
        for (int64_t y = 0; y < LOADED_WORLD_HEIGHT; y++) {
            chunk_idx++;
            if (NULL == world->active_chunks[chunk_idx]) continue;
            float position_x = (world->active_chunks[chunk_idx]->position_x - camera.x) * camera.zoom * CHUNK_SIZE + center_x;
            float position_y = (camera.y - world->active_chunks[chunk_idx]->position_y) * camera.zoom * CHUNK_SIZE + center_y;
            if (position_x > center_x + width / 2 || position_y > center_y + height / 2) continue;
            if (position_x < center_x - width / 2 - chunk_size || position_y < center_y - height / 2 - chunk_size) continue;
            render_chunk(position_x, position_y, world->active_chunks[chunk_idx], camera.zoom / CHUNK_SIZE);
            ++counter_temporary;
        }
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Drawn chunks: %zu", counter_temporary);
    DrawText(buffer, 0, 0, 40, DARKBLUE);
    DrawFPS(0, 45);
}