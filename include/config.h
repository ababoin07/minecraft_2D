#ifndef CONFIG_H
#define CONFIG_H

/** @defgroup config Configuration constants */

/** @{ */
#define CHUNK_SIZE 16                 /**< Side length of a chunk (in blocks). */
#define HASH_TABLE_BUCKETS 2048       /**< Number of hash buckets (must be power of two). */
#define HASH_TABLE_BUCKET_SIZE 12     /**< Slots per bucket (linear probing). */
#define OLD_RENDERER 0                /**< 0 = texture‑based, 1 = per‑block drawing. */
#define FIFO_WORLD_GEN_SIZE 16384     /**< FIFO size for chunk generation requests (power of two). */
#define LIGHT_SOURCES_MAX_AMOUNT 1048576 /**< Max pending light updates (power of two). */
/** @} */

#endif // CONFIG_H