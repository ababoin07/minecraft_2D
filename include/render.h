#ifndef RENDER_H
#define RENDER_H

#include "includes.h"

void render_chunk(float, float, chunk_t*, float);
void generate_chunk_texture_render(chunk_t*);
void render_world(world_t*, camera_impl_t, int, int, int, int);
#endif // RENDER_H