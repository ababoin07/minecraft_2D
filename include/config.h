#ifndef CONFIG_H
#define CONFIG_H

#define CHUNK_SIZE 16
#define HASH_TABLE_BUCKETS 2048//262144     // Must be a power of 2
#define HASH_TABLE_BUCKET_SIZE 8
#define OLD_RENDERER 0
#define FIFO_WORLD_GEN_SIZE 16384     // Must be a power of 2
#define LIGHT_SOURCES_MAX_AMOUNT 1048576 // Must be a power of 2

#endif // CONFIG_H