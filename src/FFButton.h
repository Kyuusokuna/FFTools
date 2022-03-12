#pragma once
#include "types.h"

typedef enum : uint8_t {
    FFButton_Disabled = 0,
    FFButton_Enabled = 1,
    FFButton_Empty = 2,
    FFButton_Selected = 3,

    NUM_FFButtons = 4,
} Action_Slot;


extern const f32x4 FFButton_uvs[NUM_FFButtons];