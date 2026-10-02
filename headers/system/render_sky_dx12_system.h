#pragma once
#include"i_render_dx12_system.h"

#include<d3d12.h>
#include<wrl.h>

class RenderSkyDX12System :public IRenderDX12System
{
public:
	RenderSkyDX12System();
	~RenderSkyDX12System();
	void RenderDX12(ID3D12GraphicsCommandList* cmd_list) override;//DX12用の描画関数

private:
	//定数バッファ
	//algnas(16)を付与して、16バイト境界に配置されるようにする
	struct alignas(16) SkyAtmosphereConstants
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
		int dummy[3];
	};
	Microsoft::WRL::ComPtr<ID3D12Resource>atmosphere_upload_buffer_;
	SkyAtmosphereConstants* mapped_atmosphere_constants_ = nullptr;
};