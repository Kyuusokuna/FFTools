#pragma once
#include <d3d11.h>

ID3D11ShaderResourceView *create_texture(ID3D11Device *device, UINT width, UINT height, DXGI_FORMAT format, void *data, UINT pitch, UINT array_size, UINT mip_levels);