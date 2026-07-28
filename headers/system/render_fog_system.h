#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>
#include<memory>
#include<cstdint>

class ComponentManager;
class FrameBuffer;
class FullscreenQuad;

class RenderFogSystem :public IRenderSystem
{
public:
    RenderFogSystem(ComponentManager& comp_mng, RenderPass render_pass);

    void Render()override;

    void SetObjectDepthView(ID3D11ShaderResourceView* depth_map);

    //オブジェクトのレンダーマップの解像度を取得
    //これは、フォグの描画解像度を下げ、処理を軽くするため
    void SetObjectResolution(float width, float height);

private:
    //GPU負荷計測用
    void InitializeGpuTimer(ID3D11Device* device);
    void BeginGpuFrame(int write_index);
    void UpdateGpuTimer(int read_index);
    void EndGpuFrame(int write_index);
private:

    ComponentManager& comp_mng_;

    //コンスタントバッファ
    struct FogConstants
    {
        int fog_steps{ 32 };
        float fog_max_distance{ 1000.f };
        float noise_scale{ 0.003f };
        float fog_density{ 0.001f };

        float fog_max_height{ 100.f };
        float fog_intensity{ 1.f };
        float object_resolution_width{ 0.f };
        float object_resolution_height{ 0.f };

        DirectX::XMFLOAT4 fog_color{ 0.5f,0.5f,0.5f,1.0f };
    };

    FogConstants fog_constant_{};
    Microsoft::WRL::ComPtr<ID3D11Buffer> fog_constant_buffer_ = nullptr;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>fog_ps_ = nullptr;
    //オブジェクトの深度情報を持ったマップ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>depth_map_ = nullptr;
    //ノイズマップ、雲のノイズマップを流用
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>noise_map_ = nullptr;
    //ディザリング用のノイズマップ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>dither_noise_map_ = nullptr;

    //フルスクリーン描画
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;

    //GPU負荷計測用
    static const int QUERY_BUFFER_COUNT = 16;
    int write_index_ = 0;
    Microsoft::WRL::ComPtr<ID3D11Query> dis_joint_query_[QUERY_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D11Query>time_stamp_start_query_[QUERY_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D11Query>time_stamp_end_query_[QUERY_BUFFER_COUNT];
    double gpu_time_ms_ = 0.0;

};