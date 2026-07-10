#ifndef DATA_H
#define DATA_H

/**
 * @brief Block type identifiers.
 */
enum BlockName
{
    AIR,          /**< Air (transparent). */
    DIRT,         /**< Dirt block. */
    GRASS,        /**< Grass block. */
    STONE,        /**< Stone block. */
    BEDROCK,      /**< Bedrock (indestructible). */
    WATER_STAND,  /**< Water (still). */
    LIGHT_SOURCE, /**< Artificial light source. */
    BLOCK_COUNT   /**< Number of block types. */
};

/** Global attenuation colour for each block type (used in lighting). */
extern Color attenuation_color[BLOCK_COUNT];

#endif // DATA_H