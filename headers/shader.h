#ifndef PART2_SHADER_H
#define PART2_SHADER_H
#include<d3d11.h>

namespace shader_from_cso
{
    HRESULT CreateVsFromCso(ID3D11Device* device, const char* csoName, ID3D11VertexShader** vertexShader,
        ID3D11InputLayout** inputLayout, D3D11_INPUT_ELEMENT_DESC* inputElementDesc, UINT numElenemts);

    HRESULT CreatePsFromCso(ID3D11Device* device, const char* csoName, ID3D11PixelShader** pixelShader);
}
#endif // !PART2_SHADER_H