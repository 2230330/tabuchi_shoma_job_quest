#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include<DirectXMath.h>
#include<memory>

class ComponentManager;
class FrameBuffer;
class FullscreenQuad;

class RenderFogSystem :public IRenderSystem
{
public:
    RenderFogSystem(ComponentManager& comp_mng, RenderPass render_pass);

    void Render()override;

    void SetObjectDepthView(ID3D11ShaderResourceView* depth_map);

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
        float fog_steps{ 32 };
        float fog_max_distance{ 300 };
        float noise_scale{ 0.015 };
        float fog_density{ 0.005 };

    };

    FogConstants fog_constant_{};
    Microsoft::WRL::ComPtr<ID3D11Buffer> fog_constant_buffer_ = nullptr;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>fog_ps_ = nullptr;
    //オブジェクトの深度情報を持ったマップ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>depth_map_ = nullptr;
    //ノイズマップ、雲のノイズマップを流用
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>noise_map_ = nullptr;

    //フルスクリーン描画
    std::unique_ptr<FullscreenQuad>fullscreen_quad_ = nullptr;
    //フレームバッファ
    std::unique_ptr<FrameBuffer>frame_buffer_ = nullptr;

    //GPU負荷計測用
    static const int QUERY_BUFFER_COUNT = 8;
    int write_index_ = 0;
    Microsoft::WRL::ComPtr<ID3D11Query> dis_joint_query_[QUERY_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D11Query>time_stamp_start_query_[QUERY_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D11Query>time_stamp_end_query_[QUERY_BUFFER_COUNT];
    double gpu_time_ms_ = 0.0;

};