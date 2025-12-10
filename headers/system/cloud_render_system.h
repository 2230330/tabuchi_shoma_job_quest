#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"
#include"../fullscreen_quad.h"

class CloudRenderSystem :public IRenderSystem
{
public:
    CloudRenderSystem(ComponentManager&comp_mng,RenderPass render_pass);

    void Render()override;

private:
    //初期に1度だけ呼び出し、ノイズマップを作成
    void CreateNoiseTextures(ID3D11Device* device);
    void UpdateConstants(const ComponentCloudDome& cloud);

    ComponentManager& comp_mng_;

    //ドーム頂点構造
    struct SkyVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer>vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>index_buffer_;

    // ドームメッシュ生成パラメータ
    const float radius_ = 1000.f;
    UINT index_count_;

    std::vector<SkyVertex> vertices_;
    std::vector<uint32_t> indices_;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>cloud_ps_;


    //定数バッファ
    struct CloudRayMarchingConstants
    {
        //風邪の制御
        DirectX::XMFLOAT2 wind_direction = { 1.0f, 0.0f };
        DirectX::XMFLOAT2 cloud_altitudes_min_max = { 6001500.0f, 6004000.0f }; // highest and lowest altitudes at which clouds are distributed
        float wind_speed = 1.0f; // [0.0, 20.0]

        //密度
        float density_scale = 0.05f; // [0.01, 0.2]
        //雲の覆う割合
        float cloud_coverage_scale = 0.2f; // [0.1, 1.0]
        //雨雲の光吸収の強さ
        float rain_cloud_absorption_scale = 0.5;
        //雲の種類、厚さを調整する
        float cloud_type_scale = 1.0f;

        float earth_radius = 6000000.0f; // earth radius
        float horizon_distance_scale = 1.0f;
        float low_frequency_perlin_worley_sampling_scale = 0.00008f;
        float high_frequency_worley_sampling_scale = 0.001f;
        float cloud_density_long_distance_scale = 18.0f;
        int enable_powdered_sugar_efffect = false;

        int ray_marching_steps = 128;
        int auto_ray_marching_steps = false;
    }cloud_ray_marching_constant_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>cloud_ray_marching_constant_buffer_;

    //ノイズテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>low_freq_perlin_worley_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>mid_freq_worley_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>high_freq_worley_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>weather_map_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>curl_noise_srv_ = nullptr;

    //低周波ノイズ
    const int low_freq_perlin_worley_dimensions{ 128 };
    const int low_freq_perlin_worley_numthreads{ 8 };
    //中周波
    const int mid_freq_perlin_worley_dimensions{ 64 };
    const int mid_freq_perlin_worley_numthreads{ 8 };
    //高周波ノイズ
    const int high_freq_worley_dimensions{ 32 };
    const int high_freq_worley_numthreads{ 8 };
    //curlノイズ
    const int curl_dimensions{ 128 };
    const int curl_numthreads{ 8 };
    //雲の高度分布テクスチャ
    const int layout_cloud_height_profile_dimensions{ 64 };
    const int layout_cloud_height_profile_numthreads{ 8 };


    struct CurlParams
    {
        float frequency; //base_frequency(1.0f)
        float amplitude; //出力ベクトルの振れ幅(40.f)
        float eps_world; //差分幅（正規化座標系の割合）
        unsigned int dim;//実際のボリューム寸法(curl_dimension)と同値
        float time_offset;//アニメーション用オフセット
        unsigned int seed; //ノイズシード
        float dummy[2];
    };
    CurlParams curl_params_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>curl_params_buffer_;


    std::unique_ptr<FullscreenQuad>fullscreen_quad_;
};