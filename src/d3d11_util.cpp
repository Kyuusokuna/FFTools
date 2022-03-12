#include "d3d11_util.h"

ID3D11ShaderResourceView *create_texture(ID3D11Device *device, UINT width, UINT height, DXGI_FORMAT format, void *data, UINT pitch, UINT array_size, UINT mip_levels) {
    ID3D11ShaderResourceView *result = 0;

    D3D11_TEXTURE2D_DESC texture_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = mip_levels,
        .ArraySize = array_size,
        .Format = format,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .MiscFlags = 0,
    };

    D3D11_SUBRESOURCE_DATA texture_initial_data = {
        .pSysMem = data,
        .SysMemPitch = pitch,
    };

    ID3D11Texture2D *texture = 0;
    if (FAILED(device->CreateTexture2D(&texture_desc, &texture_initial_data, &texture)))
        return 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC CA_texture_srv_desc = {
        .Format = format,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = mip_levels,
        },
    };

    if (FAILED(device->CreateShaderResourceView(texture, &CA_texture_srv_desc, &result))) {
        texture->Release();
        return 0;
    }
    texture->Release();

    //set_debug_name(texture, "CA_Texture");
    return result;
}