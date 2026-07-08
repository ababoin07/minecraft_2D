#ifndef DESTROY_STACK_H
#define DESTROY_STACK_H

#include "global.h"
#include "world.h"


void init_destroy_stack();
void destroy_destroy_stack();
void schedule_chunk_destruction(struct Chunk*);
void process_destroy_stack();

#endif // DESTROY_STACK_H