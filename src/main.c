#include "includes.h"
#include "data.h"
#include "world.h"
#include "camera.h"
#include "render.h"
#include "fifo.h"
#include "destroy_stack.h"

/**
 * @brief Critical error handler – prints message and exits.
 */
void MINECRAFT_2D_CRITICAL_ERROR(char *error)
{
    printf("\nCritical error: %s", error);
    fflush(stdout);
    exit(-1);
}

/* Global variables defined here. */
Texture2D Textures_Atlas;
fifo_world_gen_t *fifo_world_gen;
atomic_uint Chunks_Amount;
world_t *world;
uint64_t tick;

int main()
{
    tick = 0;
    float dt = 0.016f;
    camera_impl_t camera = {0.0, 0.0, CHUNK_SIZE * 2};

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);

    InitWindow(800, 600, "Minecraft 2D");
    init_destroy_stack();

    pthread_mutex_init(&pending_light_table.mutex, NULL);
    for (int i = 0; i < PENDING_LIGHT_BUCKETS; i++)
        pending_light_table.buckets[i] = NULL;

    SetWindowMinSize(800, 600);

    Textures_Atlas = LoadTexture("assets/textures/blocks_texture.png");
    fifo_world_gen = FIFO_world_gen_create();
    Texture2D cursor_texture = LoadTexture("assets/textures/cursor_texture.png");

    Chunks_Amount = 0;

    world = create_world(42, "test-world");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();
        int screen_center_x = screen_width / 2;
        int screen_center_y = screen_height / 2;

        /* Camera movement and zoom. */
        camera.x += ((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))) * dt * 256 / camera.zoom;
        camera.y += ((IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) - (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))) * dt * 256 / camera.zoom;
        _Bool key_pressed_q = IsKeyDown(KEY_Q);
        _Bool key_pressed_e = IsKeyDown(KEY_E);
        float factor = key_pressed_e == key_pressed_q ? 1 : (key_pressed_q ? 0.5f : 2.f);
        camera.zoom *= pow(factor, dt);
        camera.zoom = CLAMP(camera.zoom, 2.0, 80.0);

        render_world(world, camera, screen_center_x, screen_center_y, screen_width, screen_height);

        /* Compute the block under the mouse cursor. */
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

        /* Place a dirt block when '0' is pressed. */
        if (IsKeyDown(KEY_ZERO))
        {
            int local_x = (int)fmod(fmod((double)grid_x, CHUNK_SIZE) + CHUNK_SIZE, CHUNK_SIZE);
            int local_y = (int)fmod(fmod((double)grid_y, CHUNK_SIZE) + CHUNK_SIZE, CHUNK_SIZE);

            int block_idx = local_x * CHUNK_SIZE + local_y;

            int64_t chunk_x = (int64_t)floor((double)grid_x / CHUNK_SIZE);
            int64_t chunk_y = (int64_t)floor((double)grid_y / CHUNK_SIZE) + 1;

            chunk_t *chunk_ptr = get_chunk((int64_t)chunk_x, (int64_t)chunk_y, world->chunks);
            if (NULL != chunk_ptr)
            {
                chunk_ptr->blocks[block_idx] = DIRT;
                BeginTextureMode(chunk_ptr->texture);
                DrawTexturePro(Textures_Atlas, (Rectangle){DIRT * 16, 16, 16, -16}, (Rectangle){local_x * 16, local_y * 16, 16, 16}, (Vector2){0, 0}, 0, WHITE);
                EndTextureMode();
                chunk_ptr->is_source[block_idx] = true;
                register_light_source(chunk_x, chunk_y, block_idx, true, true, true, true, 255, 255, 255);
            }
        }

        /* Draw the cursor. */
        DrawTexturePro(cursor_texture, (Rectangle){0, 0, 16, 16}, (Rectangle){draw_x, draw_y, camera.zoom, camera.zoom}, (Vector2){0, 0}, 0, WHITE);
        EndDrawing();

        process_destroy_stack();
        process_light_sources(2500);
        dt = GetFrameTime();
        ++tick;
    }

    destroy_world(world);

    UnloadTexture(Textures_Atlas);
    FIFO_world_gen_destroy(fifo_world_gen);
    UnloadTexture(cursor_texture);
    process_destroy_stack();
    cleanup_pending_light_table();
    destroy_destroy_stack();
    CloseWindow();

    return 0;
}