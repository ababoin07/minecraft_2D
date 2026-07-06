#ifndef RENDER_H
#define RENDER_H
#include "includes.h"

void render_chunk(float, float, struct Chunk*, float);
void generate_chunk_texture_render(struct Chunk*);
void render_world(struct World*, struct CameraImpl, int, int, int, int);
#endif // RENDER_H