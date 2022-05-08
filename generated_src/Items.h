#pragma once
#include <stdint.h>
#include "../src/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t Item;

enum Item_Flag : uint8_t {
    Item_Flag_Craftable = (1 << 0),
    Item_Flag_Collectable = (1 << 1),
};

#define NUM_ITEMS (38001)
extern ROString_Literal Item_to_name[NUM_ITEMS];
extern Item_Flag Item_to_flags[NUM_ITEMS];

extern bool Items_decompress();

#ifdef __cplusplus
}
#endif
