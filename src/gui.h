#pragma once
#include <d3d11.h>

namespace GUI {
    const char *init(ID3D11Device *device);
    bool per_frame();
}