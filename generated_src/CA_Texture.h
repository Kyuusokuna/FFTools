#pragma once
#include "../src/types.h"
#include "../src/Craft_Jobs.h"
#include "../src/Craft_Actions.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const f32x4 Craft_Job_Icon_uvs[NUM_JOBS];
extern const f32x4 Craft_Action_Icon_uvs[NUM_JOBS][NUM_ACTIONS];

#define CA_TEXTURE_WIDTH (960)
#define CA_TEXTURE_HEIGHT (800)
#define CA_TEXTURE_MIP_LEVELS (1)
#define CA_TEXTURE_ARRAY_SIZE (1)
#define CA_TEXTURE_FORMAT (98)
#define CA_TEXTURE_PIXEL_DATA (CA_TEXTURE_pixel_data)

extern unsigned char CA_TEXTURE_pixel_data[768000];

extern bool Textures_decompress();

#ifdef __cplusplus
}
#endif
