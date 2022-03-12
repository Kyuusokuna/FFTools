#include "FFButton.h"
#include "../data/ActionSlots.h"

#define UV_COORD_TEXURE_WIDTH (ACTION_SLOTS_WIDTH)
#define UV_COORD_TEXURE_HEIGHT (ACTION_SLOTS_HEIGHT)
#define UV_COORD(uv_min_x, uv_min_y, uv_width, uv_height) {(((float)(uv_min_x)) / ((float)(UV_COORD_TEXURE_WIDTH))), (((float)(uv_min_y)) / ((float)(UV_COORD_TEXURE_HEIGHT))), (((float)((uv_min_x) + (uv_width))) / ((float)(UV_COORD_TEXURE_WIDTH))), (((float)((uv_min_y) + (uv_height))) / ((float)(UV_COORD_TEXURE_HEIGHT)))}

const f32x4 FFButton_uvs[NUM_FFButtons] = {
    [FFButton_Disabled] = UV_COORD(0 * 96, 0, 96, 96),
    [FFButton_Enabled] = UV_COORD(1 * 96, 0, 96, 96),
    [FFButton_Empty] = UV_COORD(2 * 96, 0, 96, 96),
    [FFButton_Selected] = UV_COORD(3 * 96, 0, 144, 144),
};