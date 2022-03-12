#pragma once
#include <stdint.h>
#include "../src/Craft_Actions.h"
#include "Items.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t NUM_RECIPES[NUM_JOBS];
#define MAX_NUM_RECIPES (1674)

typedef struct {
    Item result;
    Item ingredients[10];
    
    int32_t durability;
    int32_t progress;
    int32_t quality;
    
    int8_t progress_divider;
    int8_t progress_modifier;
    int8_t quality_divider;
    int8_t quality_modifier;
    
    int8_t level;
} Recipe;

extern Recipe Recipes[NUM_JOBS][MAX_NUM_RECIPES];

extern bool Recipes_decompress();

#ifdef __cplusplus
}
#endif
