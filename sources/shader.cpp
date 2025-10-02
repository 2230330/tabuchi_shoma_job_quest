#include "../headers/shader.h"

#include <WICTextureLoader.h>

#include<sstream>
#include<iostream>

#include"../headers/misc.h"

HRESULT shader_from_cso::CreateVsFromCso(
    ID3D11Device* device,
    const char* csoName,
    ID3D11VertexShader** vertexShader,
    ID3D11InputLayout** inputLayout,
    D3D11_INPUT_ELEMENT_DESC* inputElementDesc,
    UINT numElenemts)
{
    FILE* fp=nullptr;
    fopen_s(&fp, csoName, "rb");
    _ASSERT_EXPR_A(fp, "vs cso flie not found");

    //    // ① マルチバイト → ワイド文字列に変換
    //int len = MultiByteToWideChar(CP_UTF8, 0, csoName, -1, nullptr, 0);
    //std::wstring wCsoName(len, L'\0');
    //MultiByteToWideChar(CP_UTF8, 0, csoName, -1, &wCsoName[0], len);

    //// ② ファイルを開く（全角文字もOK）
    //FILE* fp = nullptr;
    //_wfopen_s(&fp, wCsoName.c_str(), L"rb");
    //_ASSERT_EXPR(fp, L"vs cso file not found");

    fseek(fp, 0, SEEK_END);
    long file_sz{ ftell(fp) };
    fseek(fp, 0, SEEK_SET);

    size_t cso_sz = static_cast<size_t>(file_sz);

    std::unique_ptr<unsigned char[]>cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
    fread(cso_data.get(), cso_sz, 1, fp);
    fclose(fp);

    HRESULT hr{ S_OK };
    hr = device->CreateVertexShader(cso_data.get(), cso_sz, nullptr, vertexShader);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    if (inputLayout)
    {
        hr = device->CreateInputLayout(inputElementDesc, numElenemts, cso_data.get(), cso_sz, inputLayout);
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    return hr;

}

HRESULT shader_from_cso::CreatePsFromCso(ID3D11Device* device, const char* csoName, ID3D11PixelShader** pixelShader)
{
    //// ① マルチバイト → ワイド文字列に変換
    //int len = MultiByteToWideChar(CP_UTF8, 0, csoName, -1, nullptr, 0);
    //std::wstring wCsoName(len, L'\0');
    //MultiByteToWideChar(CP_UTF8, 0, csoName, -1, &wCsoName[0], len);

    //// ② ファイルを開く（全角文字もOK）
    //FILE* fp = nullptr;
    //_wfopen_s(&fp, wCsoName.c_str(), L"rb");
    //_ASSERT_EXPR(fp, L"ps cso file not found");

    FILE* fp = nullptr;
    fopen_s(&fp, csoName, "rb");
    _ASSERT_EXPR_A(fp, "ps cso flie not fount");

    fseek(fp, 0, SEEK_END);
    long file_sz{ ftell(fp) };
    fseek(fp, 0, SEEK_SET);

    size_t cso_sz = static_cast<size_t>(file_sz);

    std::unique_ptr<unsigned char[]>cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
    size_t read_count= fread(cso_data.get(), cso_sz, 1, fp);
    fclose(fp);

    HRESULT hr{ S_OK };
    hr = device->CreatePixelShader(cso_data.get(), cso_sz, nullptr, pixelShader);
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    return hr;
}
