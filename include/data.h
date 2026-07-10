#ifndef DATA_H
#define DATA_H

enum BlockName {
    AIR,
    DIRT,
    GRASS,
    STONE,
    BEDROCK,
    WATER_STAND,
    LIGHT_SOURCE,
    BLOCK_COUNT
};

extern Color attenuation_color[BLOCK_COUNT];

#endif // DATA_H