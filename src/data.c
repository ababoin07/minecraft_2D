#include <raylib.h>

#include "data.h"

Color attenuation_color[BLOCK_COUNT] = {
    [AIR] = (Color){2, 2, 2, 255},
    [DIRT] = (Color){96, 60, 40, 255},
    [GRASS] = (Color){50, 150, 50, 255},
    [STONE] = (Color){128, 128, 128, 255},
    [BEDROCK] = (Color){32, 32, 32, 255},
    [WATER_STAND] = (Color){10, 15, 5, 255},
    [LIGHT_SOURCE] = (Color){0, 0, 0, 255},
};