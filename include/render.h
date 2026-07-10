#ifndef RENDER_H
#define RENDER_H

#include "includes.h"

/**
 * @brief Render a single chunk using its pre‑generated texture.
 * @param origin_x Screen X coordinate of the chunk's top‑left corner.
 * @param origin_y Screen Y coordinate.
 * @param chunk Pointer to the chunk.
 * @param zoom Current zoom factor (pixels per block).
 */
void render_chunk(float, float, chunk_t*, float);

/**
 * @brief Generate/update the render texture for a chunk based on its current block and light data.
 * @param chunk Pointer to the chunk.
 * @note The texture is created if not yet present.
 */
void generate_chunk_texture_render(chunk_t*);

/**
 * @brief Render the visible part of the world.
 * @param world Pointer to the world.
 * @param camera Current camera state.
 * @param center_x Screen centre X.
 * @param center_y Screen centre Y.
 * @param screen_width Current window width.
 * @param screen_height Current window height.
 * @details This function determines which chunks are visible, requests generation of missing ones,
 *          and renders them (with fallback placeholders).
 */
void render_world(world_t*, camera_impl_t, int, int, int, int);

#endif // RENDER_H