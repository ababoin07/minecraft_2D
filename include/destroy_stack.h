#ifndef DESTROY_STACK_H
#define DESTROY_STACK_H

#include "includes.h"

#include "global.h"
#include "world.h"


void init_destroy_stack();
void destroy_destroy_stack();
void schedule_chunk_destruction(chunk_t*);
void process_destroy_stack();

#endif // DESTROY_STACK_H