#ifndef TYPES_H
#define TYPES_H

void MINECRAFT_2D_CRITICAL_ERROR(char*);

// struct Chunk
struct Chunk;
typedef struct Chunk chunk_t;

// enum ChunkGenSteps
enum ChunkGenSteps;
typedef enum ChunkGenSteps chunk_gen_steps_t;

// struct World
struct World;
typedef struct World world_t;

// enum BlockName
enum BlockName;
typedef enum BlockName block_name_t;

// struct FifoWorldGen
struct FifoWorldGen;
typedef struct FifoWorldGen fifo_world_gen_t;

// struct DestroyStack
struct DestroyStack;
typedef struct DestroyStack destroy_stack_t;

// struct HashTableSlot
struct HashTableSlot;
typedef struct HashTableSlot hash_table_slot_t;

// struct HashTable
struct HashTable;
typedef struct HashTable hash_table_t;

// struct CameraImpl
struct CameraImpl;
typedef struct CameraImpl camera_impl_t;

// struct LightUpdateSlot
struct LightUpdateSlot;
typedef struct LightUpdateSlot light_update_slot_t;

#endif // TYPES_H