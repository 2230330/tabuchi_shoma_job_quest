#pragma once
#include"i_render_dx12_system.h"

#include<DirectXMath.h>

//背景用の雲描画をDX12で行う
class RenderCloudDX12System :public IRenderDX12System
{
public:
	RenderCloudDX12System();
	~RenderCloudDX12System();

	//DX12用の描画関数
	void RenderDX12(ID3D12GraphicsCommandList* cmd_list) override;

    void GetSkyTexture(ID3D12Resource* sky_texture);
private:
    //定数バッファ
    struct CloudRayMarchingConstants
    {
        //風邪の制御
        DirectX::XMFLOAT2 wind_direction = { 1.0f, 0.75f };
        DirectX::XMFLOAT2 cloud_altitudes_min_max = { 6371500.0f, 6373000.0f }; // highest and lowest altitudes at which clouds are distributed
        float wind_speed = 1.0f; // [0.0, 20.0]

        //密度
        float density_scale = 0.10f; // [0.01, 0.2]
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

    };
    CloudRayMarchingConstants* mapped_cloud_raymarching_constants_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> cloud_raymarching_constant_buffer_ = nullptr;


	//各テクスチャのリソースを保持する
	Microsoft::WRL::ComPtr<ID3D12Resource> sky_texture_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> low_freq_perlin_worley_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mid_freq_worley_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> high_freq_worley_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> weather_map_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> curl_noise_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> gradient_cumulonimbus_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> gradient_cumulus_srv_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> gradient_stratus_srv_ = nullptr;
};