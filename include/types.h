#ifndef TYPES_H
#define TYPES_H

/**
 * @brief Critical error handler – prints message and exits.
 * @param error Error message.
 */
void MINECRAFT_2D_CRITICAL_ERROR(char *);

/* Forward declarations for all main struct types. */
typedef struct PendingLightNode pending_light_node_t;
typedef struct PendingLightTable pending_light_table_t;
typedef struct Chunk chunk_t;
typedef enum ChunkGenSteps chunk_gen_steps_t;
typedef struct World world_t;
typedef enum BlockName block_name_t;
typedef struct FifoWorldGen fifo_world_gen_t;
typedef struct DestroyStack destroy_stack_t;
typedef struct HashTableSlot hash_table_slot_t;
typedef struct HashTable hash_table_t;
typedef struct CameraImpl camera_impl_t;
typedef struct LightUpdateSlot light_update_slot_t;

#endif // TYPES_H