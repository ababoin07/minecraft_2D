#include "includes.h"

#include "data.h"
#include "world.h"
#include "camera.h"
#include "render.h"

Texture2D Textures_Atlas;

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    float dt = 0.016f;
    struct CameraImpl camera = {0.0, 0.0, CHUNK_SIZE * 2};

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    InitWindow(800, 600, "Minecraft 2D");
    SetWindowMinSize(800, 600);

    Textures_Atlas = LoadTexture("assets/textures/blocks_texture.png");
    Texture2D cursor_texture = LoadTexture("assets/textures/cursor_texture.png");
    struct World* world = create_world(123);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();
        int screen_center_x = screen_width / 2;
        int screen_center_y = screen_height / 2;

        camera.x += ((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))) * dt * 16 / camera.zoom;
        camera.y += ((IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) - (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))) * dt * 16 / camera.zoom;
        _Bool key_pressed_q = IsKeyDown(KEY_Q);
        _Bool key_pressed_e = IsKeyDown(KEY_E);
        float factor = key_pressed_e == key_pressed_q ? 1 : (key_pressed_q ? 0.5f : 2.f);
        camera.zoom *= pow(factor, dt);

        render_world(world, camera, screen_center_x, screen_center_y, screen_width, screen_height);

        float camera_block_x = camera.x * CHUNK_SIZE;
        float camera_block_y = camera.y * CHUNK_SIZE;

        float mouse_screen_x = GetMouseX();
        float mouse_screen_y = GetMouseY();

        float world_block_x = camera_block_x + (mouse_screen_x - screen_center_x) / camera.zoom;
        float world_block_y = camera_block_y - (mouse_screen_y - screen_center_y) / camera.zoom;

        float grid_x = floorf(world_block_x);
        float grid_y = floorf(world_block_y);

        float draw_x = (grid_x - camera_block_x) * camera.zoom + screen_center_x;
        float draw_y = (camera_block_y - grid_y - 1) * camera.zoom + screen_center_y;

        if (IsKeyDown(KEY_ZERO)) {
            
            int local_x = (int)fmod(fmod((double)grid_x, CHUNK_SIZE) + CHUNK_SIZE, CHUNK_SIZE);
            int local_y = (int)fmod(fmod((double)grid_y, CHUNK_SIZE) + CHUNK_SIZE, CHUNK_SIZE);

            int block_idx = local_x * CHUNK_SIZE + local_y;

            int64_t chunk_x = (int64_t)floor((double)grid_x / CHUNK_SIZE);
            int64_t chunk_y = (int64_t)floor((double)grid_y / CHUNK_SIZE) + 1;

            struct Chunk* chunk_ptr = get_chunk((int64_t)chunk_x, (int64_t)chunk_y, world->chunks);
            if (NULL != chunk_ptr) {
                    chunk_ptr->blocks[block_idx] = DIRT;
                    BeginTextureMode(chunk_ptr->texture);
                        DrawTexturePro(Textures_Atlas, (Rectangle){DIRT * 16, 16, 16, -16}, (Rectangle){local_x * 16, local_y * 16, 16, 16}, (Vector2){0, 0}, 0, WHITE);
                    EndTextureMode();
            }
        }

        if (IsKeyPressed(KEY_ONE)) printf("(%f, %f);\n", grid_x, grid_y);

        if (IsKeyPressed(KEY_SPACE)) {
            
        }

        DrawTexturePro(cursor_texture, (Rectangle){0, 0, 16, 16}, (Rectangle){draw_x, draw_y, camera.zoom, camera.zoom}, (Vector2){0, 0}, 0, WHITE);
        EndDrawing();
        dt = GetFrameTime();
    }

    destroy_world(world);
    world = NULL;

    UnloadTexture(Textures_Atlas);
    CloseWindow(); 
    
    return 0;
}