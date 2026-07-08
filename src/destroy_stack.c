#include "global.h"
#include "world.h"

destroy_stack_t destroy_stack;

void init_destroy_stack() {
    destroy_stack.top = 0;
    pthread_mutex_init(&destroy_stack.mutex, NULL);
}

void destroy_destroy_stack() {
    pthread_mutex_destroy(&destroy_stack.mutex);
}

void schedule_chunk_destruction(chunk_t* chunk) {
    if (!chunk) return;
    pthread_mutex_lock(&destroy_stack.mutex);
    if (destroy_stack.top < DESTROY_STACK_SIZE) {
        destroy_stack.chunks[destroy_stack.top++] = chunk;
    } else {
        fprintf(stderr, "Destroy stack full, chunk leaked!\n");
    }
    pthread_mutex_unlock(&destroy_stack.mutex);
}

void process_destroy_stack() {
    pthread_mutex_lock(&destroy_stack.mutex);
    while (destroy_stack.top > 0) {
        chunk_t* chunk = destroy_stack.chunks[--destroy_stack.top];
        pthread_mutex_unlock(&destroy_stack.mutex);
        destroy_chunk(chunk);
        pthread_mutex_lock(&destroy_stack.mutex);
    }
    pthread_mutex_unlock(&destroy_stack.mutex);
}