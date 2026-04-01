#pragma once
#include"i_render_system.h"

#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"
#include"../fullscreen_quad.h"
#include"../framebuffer.h"

class SkyRenderSystem:public IRenderSystem
{
public:
    SkyRenderSystem(ComponentManager& comp_mng,RenderPass render_pass);

    //スカイキューブを受け取る
    void SetSkyCubeSRV(ID3D11ShaderResourceView* sky_cube_srv){
        sky_srv_ = sky_cube_srv;
    }

    void Render()override;

    bool GetSkyFlag() { return sky_flag_; }
private:

    ComponentManager& comp_mng_;    

    //頂点構造
    struct SkyVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>index_buffer_;

    // メッシュ生成パラメータ
    const unsigned int latitude_segments_ = 16;
    const unsigned int longitude_segments_ = 32;
    const float radius_ = 1000.f;
    UINT index_count_;

    std::vector<SkyVertex> vertices_;
    std::vector<uint32_t> indices_;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> sky_vs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>sky_input_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> sky_ps_;

    //定数バッファ
    // 参考資料
    //https://speakerdeck.com/fadis/konpiyutagurahuikusunokong?slide=26
    struct SkyAtmosphereCB
    {
        float rayleigh_scale_height{ 8000.f };
        float mie_scale_height{ 1200.f };
        float ozone_scale_half_width{ 15000.f };
        float ozone_center_height{ 50000.f };
        float earth_height{ 6360000.0f }; // 地球半径 [m]
        float sun_distance{ 150000000000.0f }; // 太陽までの距離 [m]
        float atmosphere_height{ 100000.0f }; // 大気の高さ [m]
        int max_sample{ 64 };
        float height{ 0.f };//自身の高度

        DirectX::XMFLOAT3 dummy;
    };
    SkyAtmosphereCB sky_atmosphere_constant;
    Microsoft::WRL::ComPtr<ID3D11Buffer>rayleigh_constant_buffer_;

    //スカイテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sky_srv_=nullptr;


    bool sky_flag_ = false;

};