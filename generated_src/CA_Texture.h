#pragma once
#include "../src/types.h"
#include "../src/Craft_Jobs.h"
#include "../src/Craft_Actions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum : u8 {
    FFUI_ActionButton_Disabled,
    FFUI_ActionButton_Enabled,
    FFUI_ActionButton_Empty,
    FFUI_ActionButton_Selected,
    
    FFUI_Button_left,
    FFUI_Button_middle,
    FFUI_Button_right,
    
    FFUI_Checkbox_checked,
    FFUI_Checkbox_unchecked,
    
    NUM_FFUI_UVS,
} FFUI_uv_type;

extern const f32x4 Craft_Job_Icon_uvs[NUM_JOBS];
extern const f32x4 Craft_Action_Icon_uvs[NUM_JOBS][NUM_ACTIONS];

extern const f32x4 FFUI_uvs[NUM_FFUI_UVS];

#define CA_TEXTURE_WIDTH (960)
#define CA_TEXTURE_HEIGHT (808)
#define CA_TEXTURE_MIP_LEVELS (1)
#define CA_TEXTURE_ARRAY_SIZE (1)
#define CA_TEXTURE_FORMAT (98)
#define CA_TEXTURE_PIXEL_DATA (CA_TEXTURE_pixel_data)

extern unsigned char CA_TEXTURE_pixel_data[775680];

extern bool Textures_decompress();

#ifdef __cplusplus
}
#endif
