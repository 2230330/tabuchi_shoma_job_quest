#pragma once
#include"i_render_system.h"
#include<d3d11.h>
#include<wrl.h>
#include"../component/component_manager.h"
#include"../fullscreen_quad.h"
#include"../framebuffer.h"

//ボリュームクラウドの描画システム
//このシステムは、ComponentVolumetricCloudを持つエンティティが存在する場合に、
// フルスクリーンクワッドを使用してボリュームクラウドを描画します。
//シェーダーはshaderers/volumetric_cloud_ps.csoを使用
//レイマーチングを使用して、空に雲を描画します。
class CloudRenderSystem :public IRenderSystem
{
public:
    CloudRenderSystem(ComponentManager&comp_mng,RenderPass render_pass);
    
    //スカイカラーをセットする関数
    void SetSkyColorSRV(ID3D11ShaderResourceView* sky_color_srv) {
        sky_color_srv_ = sky_color_srv;
    }
    //オブジェクトの深度情報をセットする関数
    void SetObjectDepthSRV(ID3D11ShaderResourceView* object_depth_srv) {
        object_depth_srv_ = object_depth_srv;
    }

    void SetObjectResolution(float width, float height) {
        cloud_ray_marching_constant_.object_resolution = DirectX::XMFLOAT2(width, height);
    }

    ID3D11ShaderResourceView* GetCloudShadowSRV() { return shadow_map_->GetShaderResourceView(0).Get(); }

    void Render()override;

    bool HasRenderableCloud() { return enable_cloud_; }

private:
    //enable cloud
    bool enable_cloud_ = false;

    //初期に1度だけ呼び出し、ノイズマップを作成
    void CreateNoiseTextures(ID3D11Device* device);
    void UpdateConstants(const ComponentVolumetricCloud& cloud);

    ComponentManager& comp_mng_;

    //シェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader>cloud_ps_;

    //定数バッファ
    struct CloudRayMarchingConstants
    {
        //風邪の制御
        DirectX::XMFLOAT2 wind_direction = { 1.0f, 0.75f };
        DirectX::XMFLOAT2 cloud_altitudes_min_max = { 6371500.0f, 6373000.0f }; // highest and lowest altitudes at which clouds are distributed
        float wind_speed = 1.0f; // [0.0, 20.0]

        //密度
        float density_scale = 0.05f; // [0.01, 0.2]
        //雲の覆う割合
        float cloud_coverage_scale = 0.25f; 
        //雨雲の光吸収の強さ
        float rain_cloud_absorption_scale = 0.5;
        //雲の種類、厚さを調整する
        float cloud_type_scale = 1.0f;

        float earth_radius = 6370000.0f; // earth radius
        float horizon_distance_scale = 1.0f;
        float low_frequency_perlin_worley_sampling_scale = 0.00004f;
        float high_frequency_worley_sampling_scale = 0.0004f;
        float cloud_density_long_distance_scale = 18.0f;
        int enable_powdered_sugar_efffect = false;

        int ray_marching_steps = 128;
        int auto_ray_marching_steps = false;

        DirectX::XMFLOAT2 object_resolution;//16バイトアラインメントのためのダミー
        //DirectX::XMFLOAT2 dummy;//16バイトアラインメントのためのダミー

    }cloud_ray_marching_constant_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>cloud_ray_marching_constant_buffer_;

    //ノイズテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>low_freq_perlin_worley_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>mid_freq_worley_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>high_freq_worley_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>weather_map_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>curl_noise_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>gradient_cumulonimbus_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>gradient_cumulus_srv_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>gradient_stratus_srv_ = nullptr;

    //低周波ノイズ
    const int low_freq_perlin_worley_dimensions_{ 256 };
    const int low_freq_perlin_worley_numthreads_{ 8 };
    //高周波ノイズ
    const int high_freq_worley_dimensions_{ 64 };
    const int high_freq_worley_numthreads_{ 8 };


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

    //スカイカラー
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>sky_color_srv_ = nullptr;

    //オブジェクトの深度情報
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>object_depth_srv_ = nullptr;

    //フルスクリーンクワッド
    std::unique_ptr<FullscreenQuad>fullscreen_quad_;

    //シャドウマップ用
    const int SHADOW_RES = 512;
    std::unique_ptr<FrameBuffer>shadow_map_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>cloud_screen_shadow_ps_ = nullptr;

};