#include <raylib.h>
#include "data.h"

/** @brief Attenuation colour for each block type. */
Color attenuation_color[BLOCK_COUNT] = {
    [AIR] = (Color){2, 2, 2, 255},
    [DIRT] = (Color){48, 30, 20, 255},
    [GRASS] = (Color){25, 75, 25, 255},
    [STONE] = (Color){64, 64, 64, 255},
    [BEDROCK] = (Color){255, 255, 255, 255},
    [WATER_STAND] = (Color){10, 15, 5, 255},
    [LIGHT_SOURCE] = (Color){0, 0, 0, 255},
};