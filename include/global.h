#ifndef GLOBAL_H
#define GLOBAL_H

#include "includes.h"
#include <raylib.h>
#include <stdatomic.h>

#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>

#define DESTROY_STACK_SIZE 4096

struct DestroyStack {
    chunk_t* chunks[DESTROY_STACK_SIZE];
    uint32_t top;
    pthread_mutex_t mutex;
};

extern destroy_stack_t destroy_stack;

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0777)
#endif

extern Texture2D Textures_Atlas;
extern fifo_world_gen_t* fifo_world_gen;
extern atomic_uint Chunks_Amount;
extern world_t* world;

#endif // GLOBAL_H