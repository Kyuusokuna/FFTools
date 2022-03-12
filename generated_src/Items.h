#pragma once
#include <stdint.h>
#include "../src/string.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_ITEMS (36701)

typedef uint16_t Item;
extern ROString_Literal Item_to_name[NUM_ITEMS];

extern bool Items_decompress();

#ifdef __cplusplus
}
#endif
